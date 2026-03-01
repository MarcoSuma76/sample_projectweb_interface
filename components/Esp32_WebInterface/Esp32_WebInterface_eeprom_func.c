/*
  Esp32_WebInterface_storage_func.c
*/

#include "Esp32_WebInterface.h"
#include "esp_wifi.h"          // Per esp_wifi_stop()
#include "esp_log.h"           // Per ESP_LOGI, ESP_LOGW
#include "nvs_flash.h"         // Per nvs_open, nvs_erase_key, ecc.
#include "freertos/FreeRTOS.h" // Per vTaskDelay
#include "freertos/task.h"

#define NVS_NAMESPACE "storage"
// Tag per il log
static const char *TAG = "STORAGE";

// --- FUNZIONE: InizializzaDati (Lettura all'avvio) ---
// Questa funzione viene chiamata all'avvio per caricare i dati salvati in NVS e popolare le variabili del programma
void WebInterface_inizializzaDati(WebInterface_t *self)
{
    nvs_handle_t my_handle;
    esp_err_t err;

    // Apriamo lo storage in modalità sola lettura
    err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &my_handle);
    if (err != ESP_OK)
        return;

    int32_t magic = 0;
    err = nvs_get_i32(my_handle, "magic", &magic);

    // Se il Magic Number è corretto (32767), procediamo al caricamento
    if (err == ESP_OK && magic == 32767)
    {
        for (int a = 0; a < self->num_oggetti; a++)
        {
            char key[16];
            char key_lab[16];
            snprintf(key, sizeof(key), "v%d", a);         // Chiave per il Valore
            snprintf(key_lab, sizeof(key_lab), "l%d", a); // Chiave per l'Etichetta

            Oggetto_t *obj = &self->oggetti[a];

            // --- 1. CARICAMENTO ETICHETTA (Friendly Name) ---
            // Lo facciamo per tutti gli oggetti: se esiste in NVS, sovrascriviamo quella di default
            size_t lab_size = sizeof(obj->etichetta);
            if (nvs_get_str(my_handle, key_lab, obj->etichetta, &lab_size) == ESP_OK)
            {
                // Opzionale: log di debug per confermare il caricamento del nome amichevole
                // ESP_LOGD("STORAGE", "Caricata etichetta per %s: %s", obj->nome, obj->etichetta);
            }

            // --- 2. CARICAMENTO VALORI SPECIFICI PER TIPO ---

            // --- CARICAMENTO BOOL ---
            if (strcmp(obj->tipo, "bool_set") == 0 && obj->Pbool != NULL)
            {
                uint8_t val;
                if (nvs_get_u8(my_handle, key, &val) == ESP_OK)
                {
                    *obj->Pbool = (val != 0);
                }
            }
            // --- CARICAMENTO INT ---
            else if (strcmp(obj->tipo, "int_set") == 0 && obj->Pint != NULL)
            {
                int32_t val;
                if (nvs_get_i32(my_handle, key, &val) == ESP_OK)
                {
                    *obj->Pint = (int)val;
                }
            }
            // --- CARICAMENTO FLOAT ---
            else if (strcmp(obj->tipo, "float_set") == 0 && obj->Pfloat != NULL)
            {
                float val;
                size_t size = sizeof(float);
                if (nvs_get_blob(my_handle, key, &val, &size) == ESP_OK)
                {
                    *obj->Pfloat = val;
                }
            }
            // --- CARICAMENTO CHAR (Zero-Copy + Sanitize) ---
            else if (strcmp(obj->tipo, "char_set") == 0 && obj->Pchar != NULL)
            {
                size_t required_size = obj->LcharP;
                // Leggiamo la stringa direttamente nel puntatore Pchar
                if (nvs_get_str(my_handle, key, obj->Pchar, &required_size) == ESP_OK)
                {
                    // Sanitize post-caricamento: sostituiamo eventuali '|' residui
                    for (int i = 0; obj->Pchar[i] != '\0'; i++)
                    {
                        if (obj->Pchar[i] == '|')
                            obj->Pchar[i] = '-';
                    }
                }
            }
        }

        // --- AGGIUNTA PER LO SWITCH MQTT ---
        uint8_t m_en = 1; // Default a 1 (abilitato) se la chiave non esiste
        if (nvs_get_u8(my_handle, "mqtt_en", &m_en) == ESP_OK)
        {
            self->mqtt_enabled = (m_en == 1);
        }
        else
        {
            self->mqtt_enabled = true; // Primo avvio assoluto
        }

        WebInterface_Log(self, TAG, "Dati ed Etichette caricati con successo dalla NVS");
    }
    nvs_close(my_handle);
}

// --- FUNZIONE: MemorizzaDati (Salva i valori correnti) ---
// Questa funzione viene chiamata quando vogliamo salvare i dati correnti in NVS
// viene chiamata dal web quando si preme il pulsante "Salva Dati" e salva i valori attuali delle variabili modificabili
void WebInterface_memorizzaDati(WebInterface_t *self)
{
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err != ESP_OK)
        return;

    // Scriviamo il Magic Number per validare il database (32767 come da tua inizializzazione)
    nvs_set_i32(my_handle, "magic", 32767);

    for (int a = 0; a < self->num_oggetti; a++)
    {
        char key[16];
        char key_lab[16];
        snprintf(key, sizeof(key), "v%d", a);         // Chiave per il Valore
        snprintf(key_lab, sizeof(key_lab), "l%d", a); // Chiave per l'Etichetta

        Oggetto_t *obj = &self->oggetti[a];

        // --- 1. SALVATAGGIO ETICHETTA (Friendly Name) ---
        // Salviamo l'etichetta per ogni oggetto, così se l'utente la cambia da web rimane persistente
        nvs_set_str(my_handle, key_lab, obj->etichetta);

        // --- 2. SALVATAGGIO VALORI SPECIFICI PER TIPO ---

        // --- SALVATAGGIO BOOL ---
        if (strcmp(obj->tipo, "bool_set") == 0 && obj->Pbool != NULL)
        {
            nvs_set_u8(my_handle, key, *obj->Pbool ? 1 : 0);
        }
        // --- SALVATAGGIO INT ---
        else if (strcmp(obj->tipo, "int_set") == 0 && obj->Pint != NULL)
        {
            nvs_set_i32(my_handle, key, (int32_t)*obj->Pint);
        }
        // --- SALVATAGGIO FLOAT ---
        else if (strcmp(obj->tipo, "float_set") == 0 && obj->Pfloat != NULL)
        {
            // Usiamo blob per salvare i 4 byte del float così come sono
            nvs_set_blob(my_handle, key, obj->Pfloat, sizeof(float));
        }
        // --- SALVATAGGIO CHAR ---
        else if (strcmp(obj->tipo, "char_set") == 0 && obj->Pchar != NULL)
        {
            nvs_set_str(my_handle, key, obj->Pchar);
        }
    }

    // --- AGGIUNTA PER LO SWITCH MQTT ---
    // Salviamo mqtt_enabled come intero (1 = ON, 0 = OFF)
    nvs_set_u8(my_handle, "mqtt_en", self->mqtt_enabled ? 1 : 0);

    err = nvs_commit(my_handle);
    if (err == ESP_OK)
    {
        WebInterface_Log(self, TAG, "Tutte le variabili ed etichette sono state salvate in NVS");
    }
    nvs_close(my_handle);
}

// --- FUNZIONE: AzzeraDati (Pulisce la memoria e resetta) ---
void WebInterface_azzeraDati(WebInterface_t *self)
{
    nvs_handle_t my_handle;

    ESP_LOGW("SISTEMA", "Inizio procedura di azzeramento totale...");

    if (nvs_open("storage", NVS_READWRITE, &my_handle) == ESP_OK)
    {
        // 1. Rimuoviamo la chiave 'magic'
        nvs_erase_key(my_handle, "magic");
        ESP_LOGI("SISTEMA", "Chiave 'magic' rimossa.");

        // 2. Cancelliamo variabili (v) ed etichette (l)
        for (int i = 0; i < self->num_oggetti; i++)
        {
            char key_val[16];
            char key_lab[16];
            snprintf(key_val, sizeof(key_val), "v%d", i);
            snprintf(key_lab, sizeof(key_lab), "l%d", i);

            // --- CANCELLAZIONE VALORE ---
            // Solo per i tipi che vengono effettivamente salvati
            const char *tipo = self->oggetti[i].tipo;
            if (strcmp(tipo, "button") != 0 && strcmp(tipo, "debug") != 0 && strcmp(tipo, "lab") != 0)
            {
                if (nvs_erase_key(my_handle, key_val) == ESP_OK)
                {
                    ESP_LOGI("SISTEMA", "Cancellata chiave dati: %s", key_val);
                }
            }

            // --- CANCELLAZIONE ETICHETTA ---
            // L'etichetta la cancelliamo SEMPRE se presente,
            // per riportare ogni oggetto al suo nome originale definito nel codice.
            if (nvs_erase_key(my_handle, key_lab) == ESP_OK)
            {
                ESP_LOGI("SISTEMA", "Cancellata etichetta: %s", key_lab);
            }
        }

        nvs_commit(my_handle);
        nvs_close(my_handle);
        ESP_LOGW("SISTEMA", "Memoria NVS (Dati + Etichette) pulita con successo.");
    }

    // Ultimo respiro per i log e poi reset
    vTaskDelay(pdMS_TO_TICKS(500));
    ESP_LOGE("SISTEMA", "--- RIAVVIO ORA ---");
    esp_restart();
}

// --- FUNZIONE: ResetEsp32 (Semplice riavvio) ---
void WebInterface_resetEsp32(WebInterface_t *self)
{
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();
}

// --- SETUP INIZIALE ---
// Questa funzione viene chiamata all'avvio per inizializzare la partizione NVS e caricare i dati salvati
void WebInterface_setUpEeprom(WebInterface_t *self)
{
    // Inizializza la partizione NVS di default
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    WebInterface_inizializzaDati(self);
}
