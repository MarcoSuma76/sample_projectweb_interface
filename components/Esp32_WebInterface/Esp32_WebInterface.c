/*
  Esp32_WebInterface.c
*/

#include "Esp32_WebInterface.h"
#include "esp_wifi.h"
#include "esp_mac.h"
#include <stdarg.h>
#include <stdio.h>
#include "esp_log.h"
#include "Esp32_WebInterface.h"

// Inizializzazione dell'interfaccia
void WebInterface_Init(WebInterface_t *self, const char *titolo_val)
{
    // --- 1. INIZIALIZZAZIONE HARDWARE ---
    // Inizializzazione NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Inizializzazione stack di rete e loop eventi
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // --- 2. PULIZIA STRUTTURA ---
    memset(self, 0, sizeof(WebInterface_t));

    self->num_oggetti = 0;

    // --- 3. REGISTRAZIONE OGGETTI SPECIALI (Logica Originale) ---

    // A. IL PRIMO OGGETTO È SEMPRE IL TITOLO
    WebInterface_setNome(self, self->num_oggetti, "titolo");
    WebInterface_setTipo(self, self->num_oggetti, "titolo");

    strncpy(self->oggetti[self->num_oggetti].value, titolo_val, MAX_VALORE - 1);
    self->oggetti[self->num_oggetti].value[MAX_VALORE - 1] = '\0';

    strncpy(self->titolo, titolo_val, MAX_NOME - 1);
    self->titolo[MAX_NOME - 1] = '\0';

    // SANIFICAZIONE: Puliamo entrambi i buffer dove abbiamo appena copiato il titolo
    WebInterface_Sanitize(self->oggetti[self->num_oggetti].value);
    WebInterface_Sanitize(self->titolo);

    self->num_oggetti++; // ora vale 1, il titolo è registrato come primo oggetto (indice 0)

    // B. IL SECONDO OGGETTO È IL SYSTEM-LOG
    memset(self->system_log, 0, sizeof(self->system_log));
    snprintf(self->system_log, sizeof(self->system_log), "Log di sistema avviato...\n");

    WebInterface_setNome(self, self->num_oggetti, "system_log");
    WebInterface_setTipo(self, self->num_oggetti, "system-log");
    strncpy(self->oggetti[self->num_oggetti].value, "INIT", MAX_VALORE - 1);

    self->num_oggetti++; // ora vale 2, il system-log è registrato come secondo oggetto (indice 1)

    // --- 4. STATI FINALI ---
    self->HA_Set = false;
    self->mqtt_client = NULL;
    self->ota_in_progress = false;

    printf("[WEB INTERFACE] Hardware pronto e oggetti base registrati.\n");
}

// Configurazione Home Assistant (MQTT)
void WebInterface_SetupHA(WebInterface_t *self, const char *_mqtt_server, int _mqtt_port, const char *_mqtt_user, const char *_mqtt_pw, int _mqtt_refresh)
{
    if (self == NULL)
        return;

    // 1. Salvataggio parametri connessione
    nvs_handle_t h;
    bool caricato_da_nvs = false;

    // Apriamo in lettura
    if (nvs_open("storage", NVS_READONLY, &h) == ESP_OK)
    {
        size_t size_server = sizeof(self->mqtt_server);
        // Proviamo a leggere l'host. Se c'è quello, presumiamo ci sia tutto il resto
        if (nvs_get_str(h, "mqtt_host", self->mqtt_server, &size_server) == ESP_OK)
        {
            // FIX ERRORE 1: Usiamo un cast a (int32_t*) per la porta
            nvs_get_i32(h, "mqtt_port", (int32_t *)&self->mqtt_port);

            size_t size_u = sizeof(self->mqtt_user);
            nvs_get_str(h, "mqtt_user", self->mqtt_user, &size_u);

            size_t size_p = sizeof(self->mqtt_pw);
            nvs_get_str(h, "mqtt_pw", self->mqtt_pw, &size_p);

            caricato_da_nvs = true;
            ESP_LOGI("HA", "Configurazione MQTT caricata da NVS");
        }
        nvs_close(h);
    }

    //  Se NON caricato da NVS, usiamo i parametri passati (server, porta, ecc.)
    if (!caricato_da_nvs)
    {
        strncpy(self->mqtt_server, _mqtt_server, MAX_MQTT_SERVER - 1);
        self->mqtt_port = _mqtt_port;
        strncpy(self->mqtt_user, _mqtt_user, MAX_MQTT_USER - 1);
        strncpy(self->mqtt_pw, _mqtt_pw, MAX_MQTT_PW - 1);
        ESP_LOGI("HA", "Uso parametri MQTT di default (Main)");
    }
    self->mqtt_refresh = _mqtt_refresh;

    self->HA_Set = true; // Indichiamo che la configurazione HA è stata impostata (anche se magari caricata da NVS)

    WebInterface_Log(self, "HA_INTERFACE", "Broker configurato: %s:%d", self->mqtt_server, self->mqtt_port);

    // 2. Generazione UID Unico dal MAC Address
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);

    // Scriviamo nel nuovo campo mqtt_uid
    snprintf(self->mqtt_uid, sizeof(self->mqtt_uid), "%02X%02X%02X%02X%02X%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    WebInterface_Log(self, "HA_INTERFACE", "Configurazione salvata. Device ID: %s", self->mqtt_uid);
}

// Avvio dell'interfaccia web: carica dati, avvia Wi-Fi e server HTTP
void WebInterface_Start(WebInterface_t *self)
{
    if (self == NULL)
        return;

    // 1. CARICAMENTO DATI DA NVS
    WebInterface_setUpEeprom(self);

    // 2. AVVIO Wi-Fi
    WebInterface_WiFi_Init(self);

    // 3. AVVIO SERVER HTTP
    WebInterface_Server_Start(self);

    // 4. AVVIO DNS (Captive Portal)
    WebInterface_StartDNS(self);

    WebInterface_Log(self, "SYSTEM", "Interfaccia avviata: NVS, WiFi, Server e DNS pronti.");

    // --- AGGIUNTA PER AUTOMAZIONE HA ---
    // Se è stato chiamato SetupHA, avviamo MQTT
    if (self->HA_Set)
    {
        WebInterface_Log(self, "HA", "Configurazione rilevata. Avvio connessione al Broker...");
        WebInterface_StartMQTT(self);
    }
}

// sostituisce i caratteri non validi (es. '|') con un trattino '-' per evitare problemi di parsing e visualizzazione
void WebInterface_Sanitize(char *str)
{
    if (str == NULL)
        return;
    while (*str)
    {
        if (*str == '|')
            *str = '-';
        str++;
    }
}

// Sostituisce i caratteri non validi nei nomi (es. spazio, punto, trattino, slash, percento) con underscore e converte in minuscolo
// Questo è importante per evitare problemi di parsing e per standardizzare i nomi degli oggetti, specialmente quelli esposti a Home Assistant
// Esempio: "Soglia Nuova" diventa "soglia_nuova", "Temperatura-Max" diventa "temperatura_max"
void WebInterface_Sanitize_name(char *str)
{
    if (str == NULL)
        return;

    for (int i = 0; str[i] != '\0'; i++)
    {
        // 1. Converti in minuscolo (lavoriamo sempre su str[i])
        if (str[i] >= 'A' && str[i] <= 'Z')
        {
            str[i] = str[i] + ('a' - 'A');
        }

        // 2. Sostituisce i caratteri speciali con underscore
        if (str[i] == ' ' || str[i] == '.' || str[i] == '-' ||
            str[i] == '/' || str[i] == '%' || str[i] == '|')
        {
            str[i] = '_';
        }
    }
}

// Verifica che il nome non sia doppio
bool WebInterface_nomedoppio(WebInterface_t *self, const char *_nome)
{
    for (int i = 0; i < self->num_oggetti; i++)
    {
        if (strcmp(self->oggetti[i].nome, _nome) == 0)
        {
            return true;
        }
    }
    return false;
}

// --- FUNZIONI GETTER ---

char *WebInterface_getNome(WebInterface_t *self, int indice)
{
    return self->oggetti[indice].nome;
}

char *WebInterface_getTipo(WebInterface_t *self, int indice)
{
    return self->oggetti[indice].tipo;
}

// --- FUNZIONI SETTER ---

void WebInterface_setNome(WebInterface_t *self, int indice, const char *_nome)
{
    strncpy(self->oggetti[indice].nome, _nome, MAX_NOME - 1);
    self->oggetti[indice].nome[MAX_NOME - 1] = '\0';
}

void WebInterface_setTipo(WebInterface_t *self, int indice, const char *_tipo)
{
    strncpy(self->oggetti[indice].tipo, _tipo, MAX_TIPO - 1);
    self->oggetti[indice].tipo[MAX_TIPO - 1] = '\0';
}

// debug
//  Questa funzione permette di loggare un messaggio sia sulla seriale (con il TAG) che sull'oggetto debug del Web Client
void WebInterface_Log(WebInterface_t *self, const char *tag, const char *formato, ...)
{
    char corpo_messaggio[256];
    char messaggio_web[300];

    // 1. Elaborazione del testo (printf style)
    va_list args;
    va_start(args, formato);
    vsnprintf(corpo_messaggio, sizeof(corpo_messaggio), formato, args);
    va_end(args);

    // 2. LOG SERIALE: Sostituiamo esp_log_write con ESP_LOGI per thread-safety
    // Questo evita che i caratteri si intreccino con altri log di sistema
    ESP_LOGI(tag, "%s", corpo_messaggio);

    if (self == NULL)
        return;

    // 3. LOG WEB: Sanificazione e preparazione
    WebInterface_Sanitize(corpo_messaggio);
    // Usiamo snprintf per sicurezza e salviamo la lunghezza reale
    int len_nuova = snprintf(messaggio_web, sizeof(messaggio_web), "[%s] %s\n", tag, corpo_messaggio);

    // Protezione: se snprintf fallisce o taglia, len_nuova potrebbe essere falsata
    if (len_nuova < 0 || len_nuova >= sizeof(messaggio_web))
        len_nuova = strlen(messaggio_web);

    for (int i = 0; i < self->num_oggetti; i++)
    {
        if (strcmp(self->oggetti[i].tipo, "system-log") == 0)
        {
            // Importante: usiamo un buffer locale per non manipolare direttamente self->system_log
            // finché non siamo pronti, o assicuriamoci che sia sempre terminato
            size_t len_attuale = strlen(self->system_log);
            const size_t limite_max = 2000;

            if (len_attuale + len_nuova >= limite_max)
            {
                size_t spazio_da_liberare = len_nuova + 256;
                if (spazio_da_liberare > len_attuale)
                    spazio_da_liberare = len_attuale;

                char *taglio = strchr(self->system_log + spazio_da_liberare, '\n');

                if (taglio != NULL)
                {
                    taglio++;
                    size_t offset = taglio - self->system_log;
                    size_t rimanenti = len_attuale - offset;

                    memmove(self->system_log, taglio, rimanenti);
                    len_attuale = rimanenti;
                }
                else
                {
                    len_attuale = 0;
                }
                // PUNTO CRITICO 1: Terminazione immediata dopo memmove
                self->system_log[len_attuale] = '\0';
            }

            // PUNTO CRITICO 2: strcat ha bisogno che la destinazione sia una stringa valida
            // Se len_attuale è 0, assicuriamoci che il primo byte sia '\0'
            if (len_attuale == 0)
                self->system_log[0] = '\0';

            strcat(self->system_log, messaggio_web);
            break;
        }
    }
}

// Questa funzione permette di loggare un messaggio di debug su un oggetto di tipo "debug" definito dall'utente
// Scrittura nel log con sanificazione e protezione overflow
void WebInterface_Debug(WebInterface_t *self, const char *nome_debug, const char *testo)
{
    // 1. Controllo di sicurezza iniziale
    if (self == NULL || nome_debug == NULL || testo == NULL)
        return;

    // --- AGGIUNTA: Sanificazione locale del nome cercato ---
    char nome_cercato[MAX_NOME];
    strncpy(nome_cercato, nome_debug, MAX_NOME - 1);
    nome_cercato[MAX_NOME - 1] = '\0';

    // Sanifichiamo il nome ricevuto per farlo corrispondere a quello salvato
    WebInterface_Sanitize_name(nome_cercato);

    for (int i = 0; i < self->num_oggetti; i++)
    {
        // Ora confrontiamo il tipo "debug" e il nome sanificato
        if (strcmp(self->oggetti[i].tipo, "debug") == 0 && strcmp(self->oggetti[i].nome, nome_cercato) == 0)
        {
            char *buffer = self->oggetti[i].Pchar;
            unsigned int max_size = self->oggetti[i].LcharP;

            // 2. Controllo di sicurezza buffer
            if (buffer == NULL || max_size == 0)
            {
                ESP_LOGE("DEBUG", "Errore: Buffer NULL per %s", nome_cercato);
                return;
            }

            // 3. Gestione segnale vuoto
            if (strcmp(buffer, "-!x") == 0)
            {
                buffer[0] = '\0';
            }

            size_t current_len = strlen(buffer);
            size_t testo_len = strlen(testo);

            // 4. LOGICA BUFFER CIRCOLARE
            for (size_t j = 0; j < testo_len; j++)
            {
                char c = (testo[j] == '|') ? '-' : testo[j];

                if (current_len < max_size - 1)
                {
                    buffer[current_len++] = c;
                }
                else
                {
                    buffer[0] = '>';
                    current_len = 1;
                    buffer[current_len++] = c;
                }
            }

            // 5. Chiusura stringa
            buffer[current_len] = '\0';
            return;
        }
    }
}