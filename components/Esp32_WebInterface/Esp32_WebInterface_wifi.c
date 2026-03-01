#include "Esp32_WebInterface.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "WIFI_SERVICE";

// --- EVENT HANDLER PER GESTIONE DINAMICA del wi-fi lavora in background---
// Questa funzione viene chiamata automaticamente dal sistema quando succede qualcosa al WiFi
static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    WebInterface_t *self = (WebInterface_t *)arg;
    static int retry_count = 0;

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        if (retry_count < 5)
        {
            ESP_LOGW(TAG, "Tentativo di riconnessione %d/5...", retry_count + 1);
            esp_wifi_connect();
            retry_count++;
        }
        else
        {
            ESP_LOGE(TAG, "Impossibile connettersi. Attivo Captive Portal di soccorso...");
            esp_wifi_set_mode(WIFI_MODE_APSTA);
            WebInterface_StartDNS(self);
            retry_count = 0; // Reset per il prossimo ciclo dopo riconfigurazione
        }
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Connesso! IP: " IPSTR, IP2STR(&event->ip_info.ip));

        retry_count = 0;
        WebInterface_StopDNS(self);
        esp_wifi_set_mode(WIFI_MODE_STA);
    }
}

// --- LOGICA DI SCANSIONE ---
// Questa funzione esegue la scansione delle reti WiFi disponibili e restituisce i risultati al chiamante (es. pagina web)
esp_err_t WebInterface_WiFi_GetScanResults(wifi_ap_record_t *ap_info, uint16_t *ap_count, uint16_t max_networks)
{
    // 1. DISABILITA POWER SAVE per liberare la radio
    esp_wifi_set_ps(WIFI_PS_NONE);

    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = false,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time.active.min = 120,
        .scan_time.active.max = 250};

    ESP_LOGI(TAG, "Avvio scansione hardware...");
    esp_err_t err = esp_wifi_scan_start(&scan_config, true);

    // Ripristina Power Save
    esp_wifi_set_ps(WIFI_PS_MIN_MODEM);

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Errore hardware scansione: %d", err);
        return err;
    }

    // Ottieni il numero effettivo di reti trovate
    esp_wifi_scan_get_ap_num(ap_count);

    // Limita il numero di record alla dimensione del buffer fornito
    if (*ap_count > max_networks)
        *ap_count = max_networks;

    // Copia i record nell'array passato come parametro
    return esp_wifi_scan_get_ap_records(ap_count, ap_info);
}

// --- SALVATAGGIO CREDENZIALI wi-fi nella flash e riavvio della connessione---
// Questa funzione esegue il salvataggio delle credenziali WiFi nella memoria flash (NVS) e avvia un nuovo tentativo di connessione con i nuovi dati
void WebInterface_WiFi_SaveConfig(WebInterface_t *self, const char *ssid, const char *pass)
{
    nvs_handle_t my_handle;
    if (nvs_open("storage", NVS_READWRITE, &my_handle) == ESP_OK)
    {
        nvs_set_str(my_handle, "wifi_ssid", ssid);
        nvs_set_str(my_handle, "wifi_pass", pass);
        nvs_commit(my_handle);
        nvs_close(my_handle);
        ESP_LOGI(TAG, "Configurazione WiFi salvata in NVS.");

        // --- AGGIUNGI QUESTE RIGHE PER AVVIARE LA CONNESSIONE ---
        wifi_config_t wifi_config = {0};
        strncpy((char *)wifi_config.sta.ssid, ssid, 32);
        strncpy((char *)wifi_config.sta.password, pass, 64);

        // Applica la nuova configurazione alla stazione
        esp_wifi_set_config(WIFI_IF_STA, &wifi_config);

        ESP_LOGI(TAG, "Avvio tentativo di connessione a: %s", ssid);
        esp_wifi_connect();
    }
}

// --- INIZIALIZZAZIONE Wi-Fi ---
// Questa funzione configura e avvia il WiFi in modalità AP+STA, gestisce le credenziali salvate e registra gli event handler per la gestione dinamica della connessione
void WebInterface_WiFi_Init(WebInterface_t *self)
{
    // Inizializzazione interfacce
    esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Caricamento credenziali
    char nvs_ssid[32] = "";
    char nvs_pass[64] = "";
    nvs_handle_t my_handle;
    bool creds_found = false;

    if (nvs_open("storage", NVS_READONLY, &my_handle) == ESP_OK)
    {
        size_t s_len = sizeof(nvs_ssid), p_len = sizeof(nvs_pass);
        if (nvs_get_str(my_handle, "wifi_ssid", nvs_ssid, &s_len) == ESP_OK &&
            nvs_get_str(my_handle, "wifi_pass", nvs_pass, &p_len) == ESP_OK)
        {
            creds_found = true;
        }
        nvs_close(my_handle);
    }

    wifi_config_t wifi_config_sta = {0};
    if (creds_found)
    {
        strncpy((char *)wifi_config_sta.sta.ssid, nvs_ssid, 32);
        strncpy((char *)wifi_config_sta.sta.password, nvs_pass, 64);
    }

    wifi_config_t wifi_config_ap = {
        .ap = {
            .ssid = "ESP32_Dashboard",
            .max_connection = 4,
            .authmode = WIFI_AUTH_OPEN},
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    if (creds_found)
        esp_wifi_set_config(WIFI_IF_STA, &wifi_config_sta);
    esp_wifi_set_config(WIFI_IF_AP, &wifi_config_ap);

    // --- REGISTRAZIONE DEGLI EVENTI ---
    // Importante: permette all'ESP32 di reagire alla connessione o disconnessione
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, self, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, self, NULL));

    ESP_ERROR_CHECK(esp_wifi_start());

    // --- AVVIO INTELLIGENTE ---
    if (creds_found)
    {
        ESP_LOGI(TAG, "Tentativo di connessione automatica...");
        esp_wifi_connect();
    }
    else
    {
        ESP_LOGW(TAG, "Nessuna credenziale salvata. Attivazione Captive Portal.");
        WebInterface_StartDNS(self);
    }
}

// --- funzione che cancella dalla flash i dati di connessione alla rete wi-fi ---
// Questa funzione cancella le credenziali WiFi salvate nella memoria flash (NVS) e prepara l'ESP32 per una nuova configurazione
void WebInterface_WiFi_ClearConfig(void)
{
    nvs_handle_t my_handle;
    if (nvs_open("storage", NVS_READWRITE, &my_handle) == ESP_OK)
    {
        nvs_erase_key(my_handle, "wifi_ssid");
        nvs_erase_key(my_handle, "wifi_pass");
        nvs_commit(my_handle);
        nvs_close(my_handle);
        ESP_LOGW(TAG, "Dati Wi-Fi cancellati. Riavvio...");
    }
}