/*
  Esp32_WebInterface_server_func.c
*/

#include "Esp32_WebInterface.h"
#include "esp_log.h"
#include "esp_wifi.h"  // Per esp_wifi_scan_start, wifi_ap_record_t, ecc.
#include "esp_event.h" // Necessario per la gestione degli eventi wifi
#include "esp_ota_ops.h"
#include "esp_flash_partitions.h"
#include "esp_partition.h"
#include <sys/param.h> //per il MIN usato nell'OTA

#include "index_html.h" // Questo includerà la stringa generata dal Python

// Tag per il log
static const char *TAG = "WEB_SERVER";

// --- FUNZIONI DI SUPPORTO

// --- getValue restituisce il valore della variabile puntata come stringa, pronto per essere inviato al web
const char *WebInterface_getValue(WebInterface_t *self, int indice)
{
    Oggetto_t *obj = &self->oggetti[indice];

    // --- CASO 1: PUNTATORI A STRINGA (Char, Char_Set, Debug) ---
    if (obj->Pchar != NULL)
    {
        if (obj->Pchar[0] == '\0')
            return "-!x";

        // Se è un normale "char" o "string", facciamo una pulizia veloce dei pipe
        // solo se necessario, per non rompere il protocollo.
        // Se il log/stringa è molto lungo, questo ciclo è veloce su ESP32.
        for (int i = 0; obj->Pchar[i] != '\0'; i++)
        {
            if (obj->Pchar[i] == '|')
                obj->Pchar[i] = '-';
        }

        return obj->Pchar; // Restituisce direttamente la RAM del main.c
    }

    // --- CASO 2: VALORI NUMERICI / BOOL ---
    // Questi vengono convertiti nel piccolo buffer obj->value (32 byte)
    if (strstr(obj->tipo, "bool"))
        WebInterface_ValueFromProgramByPbool(self, indice);
    else if (strstr(obj->tipo, "int") || strstr(obj->tipo, "count"))
        WebInterface_ValueFromProgramByPint(self, indice);
    else if (strstr(obj->tipo, "float"))
        WebInterface_ValueFromProgramByPfloat(self, indice);

    return obj->value;
}

// --- setValue setta il valore ricevuto in formato char dal web alla variabile puntata (se char)
// o se è int/float/bool  aggiorna e il buffer .value dell'oggetto e poi
// chiama la funzione di conversione per aggiornare la variabile puntata
void WebInterface_setValue(WebInterface_t *self, int indice, const char *_value)
{
    if (self == NULL || indice < 0 || indice >= self->num_oggetti)
        return;

    const char *tipo = self->oggetti[indice].tipo;
    char valore_validato[32];
    const char *p_final_value = _value;

    // --- 1. VALIDAZIONE RANGE PER NUMERICI (INT e FLOAT) ---
    if (strstr(tipo, "int") || strstr(tipo, "count") || strstr(tipo, "float"))
    {
        float val = atof(_value); // Converte in float per il controllo universale
        bool corretto = false;

        if (val < self->oggetti[indice].min)
        {
            val = self->oggetti[indice].min;
            corretto = true;
        }
        if (val > self->oggetti[indice].max)
        {
            val = self->oggetti[indice].max;
            corretto = true;
        }

        // Se abbiamo dovuto correggere il valore, generiamo una nuova stringa pulita
        if (corretto)
        {
            if (strstr(tipo, "float"))
                snprintf(valore_validato, sizeof(valore_validato), "%.2f", val);
            else
                snprintf(valore_validato, sizeof(valore_validato), "%d", (int)val);

            p_final_value = valore_validato;
        }
    }

    // --- 2. GESTIONE STRINGHE (Pchar diretto) ---
    if (strstr(tipo, "char"))
    {
        WebInterface_ValueFromWebByPchar(self, indice, p_final_value);
    }
    // --- 3. GESTIONE NUMERICI/BOOL (Usa .value come appoggio) ---
    else
    {
        // Copiamo il valore validato nel buffer interno dell'oggetto
        strncpy(self->oggetti[indice].value, p_final_value, sizeof(self->oggetti[indice].value) - 1);
        self->oggetti[indice].value[sizeof(self->oggetti[indice].value) - 1] = '\0';

        if (strstr(tipo, "bool"))
            WebInterface_ValueFromWebByPbool(self, indice);
        else if (strstr(tipo, "int") || strstr(tipo, "count"))
            WebInterface_ValueFromWebByPint(self, indice);
        else if (strstr(tipo, "float"))
            WebInterface_ValueFromWebByPfloat(self, indice);
        else if (strcmp(tipo, "button") == 0)
            WebInterface_exeFunzione(self, indice);
    }

    // --- 4. AGGIUNTA PER AUTOMAZIONE HA ---
    // Pubblichiamo subito il cambio su HA (se il valore è stato corretto, HA riceverà quello giusto)
    if (self->HA_Set && self->oggetti[indice].HA && self->mqtt_client != NULL)
    {
        WebInterface_publishState(self, indice);
    }
}
// --- HANDLER DEGLI URI (Standard ESP-IDF) ---

// Funzione di utilità per decodificare URL-encoded strings (es. spazi, caratteri speciali)
void url_decode(char *src)
{
    char *dst = src;
    while (*src)
    {
        if (*src == '%' && src[1] && src[2])
        {
            int value;
            if (sscanf(src + 1, "%02x", &value) == 1)
            {
                *dst++ = (char)value;
                src += 3;
            }
            else
            {
                *dst++ = *src++;
            }
        }
        else if (*src == '+')
        {
            *dst++ = ' ';
            src++;
        }
        else
        {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}

// 1. Pagina Principale (/) carica index.html(l'interfaccia)
static esp_err_t index_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, index_html, HTTPD_RESP_USE_STRLEN);
}

// 2. getOggettiWeb (Configurazione iniziale)
// viene chiamata dal client all'apertura dell'intervaccia e invia tutti gli oggetti legati alle variabili locali da creare sulla pagina
static esp_err_t getOggettiWeb_handler(httpd_req_t *req)
{
    WebInterface_t *self = (WebInterface_t *)req->user_ctx;

    // --- LOGICA DIAGNOSTICA WI-FI (Originale) ---
    // Questa parte è stata mantenuta per fornire un feedback immediato sullo stato della connessione Wi-Fi all'apertura dell'interfaccia.
    wifi_ap_record_t ap_info;
    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK)
    {
        char info_wifi[100];
        esp_netif_ip_info_t ip_info;
        esp_netif_t *netif = esp_netif_get_default_netif();
        // Se siamo connessi e abbiamo un IP valido, mostriamo anche l'indirizzo IP. Altrimenti, indichiamo che non abbiamo un IP.
        if (netif != NULL && esp_netif_get_ip_info(netif, &ip_info) == ESP_OK)
        {
            char ip_str[16];
            esp_ip4addr_ntoa(&ip_info.ip, ip_str, sizeof(ip_str));

            // ABBIAMO TOLTO IL CARATTERE '|' PER NON ROMPERE IL PARSING
            snprintf(info_wifi, sizeof(info_wifi), "Rete: %s -- IP: %s",
                     (char *)ap_info.ssid, ip_str);
        }
        else
        {
            snprintf(info_wifi, sizeof(info_wifi), "Rete: %s -- IP: 0.0.0.0", (char *)ap_info.ssid);
        }

        WebInterface_Log(self, "STATUS", info_wifi);
    }
    else
    {
        WebInterface_Log(self, "STATUS", "Wi-Fi non connesso o in modalità AP");
    }

    // --- COSTRUZIONE PACCHETTO AGGIORNATO ---
    // Aumentiamo leggermente il buffer se hai molti oggetti con etichette lunghe
    char *pacchetto = malloc(8192);
    if (pacchetto == NULL)
        return ESP_FAIL;
    pacchetto[0] = '\0';

    char riga[256]; // Buffer temporaneo per ogni oggetto

    // --- AGGIUNTA: INVIO DATI MQTT ---
    // Inviamo i dati nel formato tipo:nome:etichetta=host,porta,user,pw|
    snprintf(riga, sizeof(riga), "mqttconfig:broker:config=%s,%d,%s,%s|",
             self->mqtt_server,
             self->mqtt_port,
             self->mqtt_user,
             self->mqtt_pw);
    strcat(pacchetto, riga);
    // Il client JS userà queste info per precompilare i campi di configurazione MQTT
    for (int a = 0; a < self->num_oggetti; a++)
    {
        /* NUOVO FORMATO: tipo,nome,etichetta=valore|
           In questo modo il JS riceve 3 metadati (tipo, nome tecnico, etichetta)
           e il valore separato dall'uguale.
        */
        // Usiamo il formato tipo : nome : etichetta = valore |
        // I due punti sono separatori molto più stabili della virgola
        snprintf(riga, sizeof(riga), "%s:%s:%s=%s|",
                 self->oggetti[a].tipo,
                 self->oggetti[a].nome,
                 self->oggetti[a].etichetta,
                 WebInterface_getValue(self, a));

        strcat(pacchetto, riga);
    }

    httpd_resp_set_type(req, "text/plain");
    esp_err_t res = httpd_resp_send(req, pacchetto, HTTPD_RESP_USE_STRLEN);

    free(pacchetto);
    return res;
}

// 3. getDati (Aggiornamento valori - Chiamata ogni secondo via AJAX)
// viene chiamata dal client ogni secondo e invia i valori legati alle variabili locali
static esp_err_t getDati_handler(httpd_req_t *req)
{
    WebInterface_t *self = (WebInterface_t *)req->user_ctx; // Recuperiamo il contesto passato durante la registrazione dell'handler

    // Allocazione buffer
    char *pacchetto = malloc(3000);
    if (pacchetto == NULL)
        return ESP_FAIL;
    pacchetto[0] = '\0';
    // Ciclo su tutti gli oggetti registrati per costruire il pacchetto di risposta
    for (int a = 0; a < self->num_oggetti; a++)
    {
        // --- 1. GESTIONE SYSTEM-LOG ---
        if (strcmp(self->oggetti[a].tipo, "system-log") == 0)
        {
            strcat(pacchetto, "system_log=");
            if (self->system_log[0] != '\0' && strcmp(self->system_log, "-!x") != 0)
            {
                strcat(pacchetto, self->system_log);
                strcpy(self->system_log, "-!x");
            }
            else
            {
                strcat(pacchetto, "-!x");
            }
            strcat(pacchetto, "|");
        }
        // --- 2. TUTTI GLI ALTRI OGGETTI ---
        else
        {
            const char *val = WebInterface_getValue(self, a);

            // 1. Aggiungiamo il valore al pacchetto che andrà al browser
            strcat(pacchetto, self->oggetti[a].nome);
            strcat(pacchetto, "=");
            strcat(pacchetto, val);
            strcat(pacchetto, "|");

            // LOGICA DI CONSUMO SOLO PER DEBUG
            // Se è un debug, lo azzeriamo dopo averlo copiato nel pacchetto
            if (strcmp(self->oggetti[a].tipo, "debug") == 0)
            {
                if (self->oggetti[a].Pchar != NULL)
                {
                    self->oggetti[a].Pchar[0] = '\0';
                }
            }
            // Se è un 'char' normale, NON entriamo qui e la stringa resta nel main.c
        }
    } // <-- Fine del ciclo FOR

    // 1. Aggiungiamo il valore per rss
    int rssi = 0;
    wifi_ap_record_t ap;
    if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK)
    {
        rssi = ap.rssi; // Ottiene il valore in dBm (es. -65)
    }
    char rssi_str[10];
    snprintf(rssi_str, sizeof(rssi_str), "%d", rssi);
    strcat(pacchetto, "wifi_rssi=");
    strcat(pacchetto, rssi_str);
    strcat(pacchetto, "|");

    // 2. Aggiungiamo lo stato MQTT (1 o 0) al pacchetto
    bool mqtt_conn = (self->mqtt_connected); // La tua variabile di stato MQTT
    strcat(pacchetto, "mqtt_status=");
    strcat(pacchetto, mqtt_conn ? "1" : "0");
    strcat(pacchetto, "|");

    // Aggiungi anche lo stato dell'abilitazione (se il servizio deve essere attivo)
    strcat(pacchetto, "mqtt_en=");
    strcat(pacchetto, self->mqtt_connected ? "1" : "0");
    strcat(pacchetto, "|");

    httpd_resp_set_type(req, "text/plain");
    esp_err_t res = httpd_resp_send(req, pacchetto, HTTPD_RESP_USE_STRLEN);

    free(pacchetto);
    return res;
}

// 4. putDati?nome=valore
// viene chiamata dal client quando un utente ,modifica il valore sull'oggetto web legato alla variabile locale
static esp_err_t putDati_handler(httpd_req_t *req)
{
    WebInterface_t *self = (WebInterface_t *)req->user_ctx;

    // Allocazione dinamica del buffer in base alla lunghezza del contenuto ricevuto
    // Aggiungiamo +1 per il terminatore null
    size_t size = req->content_len + 1;
    char *buf = malloc(size);
    if (buf == NULL)
        return ESP_FAIL;

    int ret = httpd_req_recv(req, buf, req->content_len);
    if (ret <= 0)
    {
        free(buf);
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    // Parsing della stringa "chiave=valore"
    char *key = strtok(buf, "=");
    char *value = strtok(NULL, "=");

    if (key && value)
    {
        for (int a = 0; a < self->num_oggetti; a++)
        {
            if (strcmp(self->oggetti[a].nome, key) == 0)
            {
                // Scrivo il valore direttamente (Zero-Copy per le stringhe)
                WebInterface_setValue(self, a, value);

                // Aggiornamento per la risposta (opzionale se usi Pchar diretto)
                WebInterface_getValue(self, a);

                ESP_LOGI("SERVER", "Update: %s , valore: %s", key, value);
                break;
            }
        }
    }

    free(buf); // Fondamentale liberare la memoria!
    return httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);
}

// 5,6,7. Comandi di sistema (Reset, Memorizza, Azzera)
// viene chiamata dal client quando un utente ,preme uno dei tasti funzione
static esp_err_t system_handler(httpd_req_t *req)
{
    WebInterface_t *self = (WebInterface_t *)req->user_ctx;

    if (strcmp(req->uri, "/memorizzaDati") == 0)
    {
        WebInterface_memorizzaDati(self);
        httpd_resp_sendstr(req, "Dati salvati correttamente in NVS.");
        WebInterface_Log(self, TAG, "Dati salvati su richiesta web.");
    }
    else if (strcmp(req->uri, "/azzeraDati") == 0)
    {
        // RISPOSTA IMMEDIATA: evita che il browser ricarichi all'infinito
        httpd_resp_sendstr(req, "OK: Azzeramento e riavvio in corso...");
        // PAUSA: tempo tecnico per inviare il pacchetto HTTP sulla rete
        vTaskDelay(pdMS_TO_TICKS(1000));
        // ESECUZIONE
        WebInterface_azzeraDati(self);
    }
    else if (strcmp(req->uri, "/resetEsp32") == 0)
    {
        httpd_resp_sendstr(req, "OK: Riavvio in corso...");
        ESP_LOGW(TAG, "Riavvio hardware richiesto.");
        vTaskDelay(pdMS_TO_TICKS(1500));
        esp_restart();
    }

    return ESP_OK;
}

// 8. Scan WiFi - Avvia la scansione, genera un JSON e lo invia al browser
// viene chiamata dal client quando un utente apre la pagina di configurazione WiFi
static esp_err_t scanWiFi_handler(httpd_req_t *req)
{
    WebInterface_t *self = (WebInterface_t *)req->user_ctx;
    uint16_t max_entries = 10;
    wifi_ap_record_t ap_info[10];
    uint16_t ap_count = 0;

    // CHIAMATA ALLA FUNZIONE ESTERNA
    esp_err_t err = WebInterface_WiFi_GetScanResults(ap_info, &ap_count, max_entries);

    if (err != ESP_OK)
    {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    // --- COSTRUZIONE DEL JSON (Logica Server) ---
    char *json_response = malloc(1024);
    if (json_response == NULL)
        return ESP_ERR_NO_MEM;

    strcpy(json_response, "[");
    for (int i = 0; i < ap_count; i++)
    {
        char entry[128];
        snprintf(entry, sizeof(entry),
                 "{\"ssid\":\"%s\",\"rssi\":%d}%s",
                 (char *)ap_info[i].ssid, ap_info[i].rssi,
                 (i == ap_count - 1) ? "" : ",");
        strcat(json_response, entry);
    }
    strcat(json_response, "]");

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_response);

    free(json_response);
    WebInterface_Log(self, TAG, "Reti scansionate: %d", ap_count);
    return ESP_OK;
}

// 9. Setup WiFi - Riceve SSID e Password
// viene chiamata dal client quando un utente ,inserisce ssid e password e preme connetti e poi viene eseguita la connessione alla rete WiFi
static esp_err_t setupWiFi_handler(httpd_req_t *req)
{
    WebInterface_t *self = (WebInterface_t *)req->user_ctx; // Recuperiamo il contesto
    char buf[256];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0)
    {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT)
        {
            httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    char *ssid = NULL;
    char *pass = NULL;

    // Estrazione parametri (Parsing semplice)
    char *p = strtok(buf, "&");
    while (p != NULL)
    {
        if (strncmp(p, "ssid=", 5) == 0)
            ssid = p + 5;
        if (strncmp(p, "pass=", 5) == 0)
            pass = p + 5;
        p = strtok(NULL, "&");
    }

    if (ssid != NULL && pass != NULL)
    {
        // Applichiamo la decodifica per gestire spazi e caratteri speciali
        url_decode(ssid);
        url_decode(pass);

        ESP_LOGI("WIFI_SETUP", "SSID Ricevuto: %s", ssid);
        ESP_LOGI("WIFI_SETUP", "Pass Ricevuta: %s", pass);

        // 1. Invia prima la risposta al browser (così l'utente vede il messaggio)
        // Prepariamo un messaggio pulito per l'alert
        char resp_msg[128];
        snprintf(resp_msg, sizeof(resp_msg),
                 "Ricevuto! L'ESP32 sta tentando la connessione a: %s. Verificare i log o il nuovo IP.",
                 ssid);

        // Invio come plain text
        httpd_resp_set_type(req, "text/plain");
        httpd_resp_sendstr(req, resp_msg);

        // 2. Attendi un brevissimo istante per assicurarti che il pacchetto TCP parta
        vTaskDelay(pdMS_TO_TICKS(500));

        // 3. Salva e avvia la connessione (usando la tua funzione nel file .c)
        WebInterface_WiFi_SaveConfig(self, ssid, pass);
    }
    else
    {
        // CORREZIONE QUI: Usiamo la funzione corretta per l'errore 400
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "SSID o Password mancanti");
    }
    return ESP_OK;
}

// 10. Reset WiFi e riavvio
// viene chiamata dal client quando un utente ,preme il tasto per cancellare i dati WiFi e riavviare l'ESP32
static esp_err_t clearWiFi_handler(httpd_req_t *req)
{
    ESP_LOGW(TAG, "Richiesta cancellazione totale WiFi ricevuta dal Web");

    // Rispondiamo al browser
    httpd_resp_sendstr(req, "Memoria cancellata. Riavvio in corso...\n\n"
                            "Passero' allo stato AP\n"
                            "SSID: ESP32_Dashboard\n"
                            "IP: 192.168.4.1");

    // Chiamiamo la funzione di cancellazione
    WebInterface_WiFi_ClearConfig();

    // Attendiamo un attimo e resettiamo
    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_restart();

    return ESP_OK;
}

// 11 OTA
//  viene chiamata dal client quando un utente ,carica un nuovo firmware tramite la pagina di aggiornamento OTA
esp_err_t ota_update_post_handler(httpd_req_t *req)
{
    char buf[1024];
    esp_ota_handle_t update_handle = 0;

    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);

    WebInterface_t *self = (WebInterface_t *)req->user_ctx;

    if (update_partition == NULL)
    {
        WebInterface_Log(self, "OTA", "Errore: Partizione OTA non trovata! Controlla partitions.csv");
        return ESP_FAIL;
    }

    // ATTIVA IL BLOCCO
    self->ota_in_progress = true;
    WebInterface_Log(self, "OTA", "Inizio ricezione firmware...");

    esp_err_t err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &update_handle);
    if (err != ESP_OK)
    {
        WebInterface_Log(self, "OTA", "Errore inizio OTA (0x%X)", err);
        return ESP_FAIL;
    }

    int remaining = req->content_len;
    bool is_first_chunk = true;

    while (remaining > 0)
    {
        int recv_len = httpd_req_recv(req, buf, MIN(remaining, sizeof(buf)));
        if (recv_len <= 0)
        {
            if (recv_len == HTTPD_SOCK_ERR_TIMEOUT)
                continue;
            esp_ota_end(update_handle);
            return ESP_FAIL;
        }

        // --- PROTEZIONE: Controllo Header ---
        if (is_first_chunk)
        {
            // Un firmware ESP32 valido inizia sempre con il byte 0xE9
            if (buf[0] != 0xE9)
            {
                WebInterface_Log(self, "OTA", "Errore: Il file non è un firmware ESP32 valido!");
                esp_ota_abort(update_handle);
                httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "File non valido");
                return ESP_FAIL;
            }
            is_first_chunk = false;
        }
        // -------------------------------------

        esp_ota_write(update_handle, buf, recv_len);
        remaining -= recv_len;
    }

    // DISATTIVA IL BLOCCO prima del riavvio (o in caso di fine operazione)
    self->ota_in_progress = false;

    if (esp_ota_end(update_handle) != ESP_OK || esp_ota_set_boot_partition(update_partition) != ESP_OK)
    {
        WebInterface_Log(self, "OTA", "Errore validazione firmware!");
        return ESP_FAIL;
    }

    WebInterface_Log(self, "OTA", "Aggiornamento riuscito! Riavvio...");
    httpd_resp_sendstr(req, "OK");

    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_restart();
    return ESP_OK;
}

// 12. Reset Singolo - Riceve il nome di una variabile e la resetta al valore di default
// viene chiamata dal client quando un utente ,preme il tasto di reset accanto ad ogni variabile
// per riportarla al valore di default definito nel codice
static esp_err_t resetSingolo_handler(httpd_req_t *req)
{
    WebInterface_t *self = (WebInterface_t *)req->user_ctx;
    char buf[128];
    memset(buf, 0, sizeof(buf));

    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0)
        return ESP_FAIL;
    buf[ret] = '\0';

    // 1. Parsing più robusto
    char *nome_var = NULL;
    char *p = strstr(buf, "nome="); // Cerchiamo esplicitamente la chiave
    if (p)
    {
        nome_var = p + 5; // Saltiamo "nome="
    }
    else
    {
        nome_var = strchr(buf, '='); // Fallback sul vecchio metodo se non c'è "nome="
        if (nome_var)
            nome_var++;
    }

    if (nome_var)
    {
        for (int a = 0; a < self->num_oggetti; a++)
        {
            if (strcmp(self->oggetti[a].nome, nome_var) == 0)
            {
                // 2. SICUREZZA: Se è una stringa, azzeriamo il buffer prima del reset
                if (strcmp(self->oggetti[a].tipo, "char") == 0 && self->oggetti[a].Pchar != NULL)
                {
                    memset(self->oggetti[a].Pchar, 0, self->oggetti[a].LcharP);
                }

                // 3. Ripristino
                WebInterface_setValue(self, a, self->oggetti[a].valore_default);

                // 4. Sincronizzazione
                WebInterface_memorizzaDati(self);

                // 5: aggiungi un log anche alla WebInterface se vuoi vederlo sul browser
                WebInterface_Log(self, "SERVER", "Reset: %s ripristinato", nome_var);

                break;
            }
        }
    }
    return httpd_resp_sendstr(req, "OK");
}

// 13 Set Label - Riceve il nome di una variabile e la nuova etichetta da visualizzare
// viene chiamata dal client quando un utente ,modifica l'etichetta di una variabile dalla pagina di configurazione
// per aggiornare dinamicamente l'etichetta sia sulla pagina che su Home Assistant (se HA è attivo)
static esp_err_t setLabel_handler(httpd_req_t *req)
{
    WebInterface_t *self = (WebInterface_t *)req->user_ctx;
    char buf[128];
    int ret, remaining = req->content_len;

    if (remaining >= sizeof(buf))
        return ESP_FAIL;

    ret = httpd_req_recv(req, buf, remaining);
    if (ret <= 0)
        return ESP_FAIL;
    buf[ret] = '\0';

    char nome[MAX_NOME] = {0};
    char label[MAX_NOME] = {0};

    // Estraiamo i dati dalla query POST (form-urlencoded)
    // Più semplicemente, visto che usiamo URLSearchParams nel JS:
    httpd_query_key_value(buf, "nome", nome, sizeof(nome));
    httpd_query_key_value(buf, "label", label, sizeof(label));

    // 1. Decodifichiamo i %20 e i simboli (usando la tua funzione esistente)
    url_decode(label);

    // 2. SANITIZZAZIONE: Sostituiamo i caratteri separatori critici
    for (int i = 0; label[i] != '\0'; i++)
    {
        if (label[i] == ':' || label[i] == '|' || label[i] == '=')
        {
            label[i] = '-'; // Sostituiamo con un trattino per sicurezza
        }
    }

    // Cerchiamo l'oggetto e aggiorniamo l'etichetta
    for (int i = 0; i < self->num_oggetti; i++)
    {
        if (strcmp(self->oggetti[i].nome, nome) == 0)
        {
            strncpy(self->oggetti[i].etichetta, label, MAX_NOME - 1);
            WebInterface_Log(self, TAG, "Etichetta aggiornata per %s -> %s", nome, label);

            // Salviamo subito in NVS
            WebInterface_memorizzaDati(self);

            // 2. CONTROLLO HA: Aggiorniamo Home Assistant SOLO se l'oggetto è abilitato
            if (self->oggetti[i].HA == true)
            {
                WebInterface_Log(self, TAG, "Oggetto HA attivo: invio nuovo Discovery per %s", nome);
                WebInterface_publishDiscovery(self, i);
            }
            else
            {
                WebInterface_Log(self, TAG, "Oggetto HA disabilitato: solo aggiornamento locale per %s", nome);
            }

            httpd_resp_sendstr(req, "OK");

            // Opzionale: riavvio per forzare Home Assistant a vedere il cambio
            // WebInterface_resetEsp32(self);
            return ESP_OK;
        }
    }

    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Oggetto non trovato");
    return ESP_OK;
}

// 14 Set MQTT - Riceve i parametri di configurazione MQTT (host, port, user, pass) e li salva in NVS
static esp_err_t setMQTT_handler(httpd_req_t *req)
{
    WebInterface_t *self = (WebInterface_t *)req->user_ctx;

    char buf[256];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0)
        return ESP_FAIL;
    buf[ret] = '\0';

    char host[MAX_MQTT_SERVER], port_str[10], user[MAX_MQTT_USER], pass[MAX_MQTT_PW];

    httpd_query_key_value(buf, "host", host, sizeof(host));
    httpd_query_key_value(buf, "port", port_str, sizeof(port_str));
    httpd_query_key_value(buf, "user", user, sizeof(user));
    httpd_query_key_value(buf, "pass", pass, sizeof(pass));

    url_decode(host); // Usando la tua funzione url_decode
    url_decode(user);
    url_decode(pass);

    nvs_handle_t h;
    if (nvs_open("storage", NVS_READWRITE, &h) == ESP_OK)
    {
        nvs_set_str(h, "mqtt_host", host);
        nvs_set_i32(h, "mqtt_port", atoi(port_str));
        nvs_set_str(h, "mqtt_user", user);
        nvs_set_str(h, "mqtt_pw", pass);
        nvs_commit(h);
        nvs_close(h);
    }

    // AGGIORNAMENTO DINAMICO:
    if (self->mqtt_enabled)
    {
        WebInterface_Log(self, "MQTT", "Nuovi parametri salvati. Riavvio client...");
        WebInterface_RestartMQTT(self);
    }
    else
    {
        WebInterface_Log(self, "MQTT", "Parametri salvati (MQTT è disabilitato)");
    }

    httpd_resp_sendstr(req, "Parametri salvati. Riavvio...");
    return ESP_OK;
}

// 15 Reset MQTT - Cancella i dati MQTT da NVS e riavvia
static esp_err_t resetMQTT_handler(httpd_req_t *req)
{
    WebInterface_t *self = (WebInterface_t *)req->user_ctx;
    nvs_handle_t h;
    if (nvs_open("storage", NVS_READWRITE, &h) == ESP_OK)
    {
        nvs_erase_key(h, "mqtt_host");
        nvs_erase_key(h, "mqtt_port");
        nvs_erase_key(h, "mqtt_user");
        nvs_erase_key(h, "mqtt_pass");
        nvs_commit(h);
        nvs_close(h);
    }
    // AGGIORNAMENTO DINAMICO: se il client è attivo, lo fermiamo e distruggiamo subito per evitare problemi di connessione con dati parziali
    if (self->mqtt_client != NULL)
    {
        esp_mqtt_client_stop(self->mqtt_client);
        esp_mqtt_client_destroy(self->mqtt_client);
        self->mqtt_client = NULL;
        self->mqtt_connected = false;
        WebInterface_Log(self, "MQTT", "Configurazione resettata e client rimosso.");
    }
    httpd_resp_sendstr(req, "MQTT Resettato.");
    return ESP_OK;
}

// 16 Abilita/Disabilita MQTT - Attiva o disattiva il servizio MQTT senza cancellare i dati
static esp_err_t setMqttEnable_handler(httpd_req_t *req)
{
    WebInterface_t *self = (WebInterface_t *)req->user_ctx;
    char buf[32];
    int ret = httpd_req_recv(req, buf, sizeof(buf));
    if (ret <= 0)
        return ESP_FAIL;
    buf[ret] = '\0';

    if (strstr(buf, "enabled=1"))
    {
        self->mqtt_enabled = true;
        // Se il client esiste già, lo facciamo solo ripartire (molto veloce)
        if (self->mqtt_client != NULL)
        {
            esp_mqtt_client_start(self->mqtt_client);
            WebInterface_Log(self, "MQTT", "Servizio ripreso");
        }
        else
        {
            // Se è nullo (es. dopo un reset o primo avvio), usiamo la funzione completa
            WebInterface_StartMQTT(self);
            WebInterface_Log(self, "MQTT", "Servizio avviato");
        }
    }
    else
    {
        self->mqtt_enabled = false;
        // Non distruggiamo il client, lo mettiamo solo in pausa
        if (self->mqtt_client != NULL)
        {
            esp_mqtt_client_stop(self->mqtt_client);
            self->mqtt_connected = false; // Importante per l'icona 🔴 sulla UI
            WebInterface_Log(self, "MQTT", "Servizio sospeso");
        }
    }

    // Ricordati di salvare lo stato in NVS se vuoi che al riavvio l'ESP ricordi ON/OFF
    // WebInterface_SaveSystemConfig(self);

    httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// --- AVVIO DEL SERVER ---
// Questa funzione viene chiamata dal main.c per avviare il server web e registrare tutti gli handler
void WebInterface_Server_Start(WebInterface_t *self)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 8192;     // Più memoria per gestire i JSON
    config.max_uri_handlers = 16; // Più spazio per tutti i tuoi URL

    if (httpd_start(&self->server, &config) == ESP_OK)
    {

        // 1 Handler principale, pagina iniziale
        httpd_uri_t uri_root = {.uri = "/", .method = HTTP_GET, .handler = index_handler, .user_ctx = self};
        httpd_register_uri_handler(self->server, &uri_root);

        // 2 Handler per ottenere la configurazione iniziale degli oggetti
        httpd_uri_t uri_getOggetti = {.uri = "/getOggettiWeb", .method = HTTP_GET, .handler = getOggettiWeb_handler, .user_ctx = self};
        httpd_register_uri_handler(self->server, &uri_getOggetti);

        // 3 Handler per ottenere i dati aggiornati (AJAX)
        httpd_uri_t uri_getDati = {.uri = "/getDati", .method = HTTP_GET, .handler = getDati_handler, .user_ctx = self};
        httpd_register_uri_handler(self->server, &uri_getDati);

        // 4 Handler per ricevere i dati modificati dal web
        httpd_uri_t uri_putDati = {.uri = "/putDati", .method = HTTP_POST, .handler = putDati_handler, .user_ctx = self};
        httpd_register_uri_handler(self->server, &uri_putDati);

        // 5 Handler per i comandi di sistema (memorizza)
        httpd_uri_t uri_mem = {.uri = "/memorizzaDati", .method = HTTP_POST, .handler = system_handler, .user_ctx = self};
        httpd_register_uri_handler(self->server, &uri_mem);

        // 6 Handler per i comandi di sistema (azzera dati)
        httpd_uri_t uri_azz = {.uri = "/azzeraDati", .method = HTTP_POST, .handler = system_handler, .user_ctx = self};
        httpd_register_uri_handler(self->server, &uri_azz);

        // 7 Handler per i comandi di sistema (reset ESP32)
        httpd_uri_t uri_res = {.uri = "/resetEsp32", .method = HTTP_POST, .handler = system_handler, .user_ctx = self};
        httpd_register_uri_handler(self->server, &uri_res);

        // 8 Handler per la scansione WiFi
        httpd_uri_t uri_scan = {.uri = "/scanWiFi", .method = HTTP_POST, .handler = scanWiFi_handler, .user_ctx = self};
        httpd_register_uri_handler(self->server, &uri_scan);

        // 9 Handler per la configurazione WiFi (POST)
        httpd_uri_t uri_setup = {.uri = "/setupWiFi", .method = HTTP_POST, .handler = setupWiFi_handler, .user_ctx = self};
        httpd_register_uri_handler(self->server, &uri_setup);

        // 10 Handler per cancellare i dati WiFi e riavviare
        httpd_uri_t uri_clear_wifi = {.uri = "/clearWiFi", .method = HTTP_POST, .handler = clearWiFi_handler, .user_ctx = self};
        httpd_register_uri_handler(self->server, &uri_clear_wifi);

        // 11 Handler per OTA
        httpd_uri_t uri_ota = {.uri = "/update", .method = HTTP_POST, .handler = ota_update_post_handler, .user_ctx = self};
        httpd_register_uri_handler(self->server, &uri_ota);

        // 12 Handler per reset singolo
        httpd_uri_t uri_resetS = {.uri = "/resetSingolo", .method = HTTP_POST, .handler = resetSingolo_handler, .user_ctx = self};
        httpd_register_uri_handler(self->server, &uri_resetS);

        // 13 Handler per set label
        httpd_uri_t uri_setLabel = {.uri = "/setLabel", .method = HTTP_POST, .handler = setLabel_handler, .user_ctx = self};
        httpd_register_uri_handler(self->server, &uri_setLabel);

        // 14 Handler per salvare i dati MQTT
        httpd_uri_t setMQTT_uri = {.uri = "/setMQTT", .method = HTTP_POST, .handler = setMQTT_handler, .user_ctx = self};
        httpd_register_uri_handler(self->server, &setMQTT_uri);

        // 15 Handler per resettare i dati MQTT
        httpd_uri_t resetMQTT_uri = {.uri = "/resetMQTT", .method = HTTP_POST, .handler = resetMQTT_handler, .user_ctx = self};
        httpd_register_uri_handler(self->server, &resetMQTT_uri);

        // 16 Handler per abilitare/disabilitare MQTT
        httpd_uri_t uri_mqtt_en = {.uri = "/setMqttEnable", .method = HTTP_POST, .handler = setMqttEnable_handler, .user_ctx = self};
        httpd_register_uri_handler(self->server, &uri_mqtt_en);

        ESP_LOGI("WEB_SERVER", "Server Web Online con 16 handler registrati.");

        // --- AGGIUNTE PER DEBUG OTA E VERSIONING ---
        // invio al client la partizione su cui giro(ota1 o ota2)  e data e ora di compilazione del firmware,
        // così da avere un riscontro immediato dopo l'OTA se il nuovo firmware è effettivamente online
        // o se si è verificato qualche errore durante l'aggiornamento
        const esp_partition_t *running = esp_ota_get_running_partition();
        WebInterface_Log(self, "SISTEMA", "Firmware Online (Build: %s %s)", __DATE__, __TIME__);
        WebInterface_Log(self, "SISTEMA", "Partizione attiva: %s", running->label);
        // --------------------------------------------
    }
}