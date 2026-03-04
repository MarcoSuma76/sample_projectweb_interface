/*
  Esp32_WebInterface_HA_func.c
  Conversione INTEGRALE (Discovery + Connection + Loop)
  Include la logica di Esp32_WebInterface_HA_object.cpp
*/

#include "Esp32_WebInterface.h"
#include "mqtt_client.h"
#include "esp_log.h"

static const char *TAG = "HA_INTERFACE";

static const char *MF = "Suma Marco";
static const char *MDL = "ESP32_WEBINTERFACE";

// --- FUNZIONI DI SUPPORTO ---

const char *WebInterface_getHA_Binary_Value(WebInterface_t *self, int indice)
{
    WebInterface_ValueFromProgramByPbool(self, indice);
    return (strcmp(self->oggetti[indice].value, "true") == 0) ? "ON" : "OFF";
}

// --- LOGICA DISCOVERY (Ex HA_object.cpp) ---

/**
 * NOTA TECNICA: Struttura Discovery Home Assistant
 * -------------------------------------------------------------------------
 * Il Discovery permette a HA di configurare l'entità automaticamente.
 * * Topic di Configurazione:
 * homeassistant/[TIPO]/[DEVICE_ID]/[NOME_VAR]/config
 * * Esempio JSON inviato (Prefisso ESP32-WI):
 * {
 * "name": "Ventola",
 * "uniq_id": "004B123C11F8_Ventola",
 * "stat_t": "ESP32-WI/004B123C11F8/Ventola/stat_t",
 * "cmd_t": "ESP32-WI/004B123C11F8/Ventola/cmd_t",
 * "unit_of_meas": "°C",
 * "dev": {
 * "ids": ["004B123C11F8"],
 * "name": "Centralina Smart",
 * "mf": "Suma Marco",
 * "mdl": "C_Interface"
 * }
 * }
 * -------------------------------------------------------------------------
 */

// questa funzione costruisce il payload di Discovery e lo pubblica sul topic corretto per ogni oggetto esposto a Home Assistant
bool WebInterface_publishDiscovery(WebInterface_t *self, int indice)
{
    if (indice < 0 || indice >= self->num_oggetti)
        return false;

    Oggetto_t *obj = &self->oggetti[indice];
    char t_conf[200];
    char json_buffer[BUFFER_JSON];

    // 1. Variabili locali per pulizia
    char uid[20];
    char nome[MAX_NOME];
    strncpy(uid, self->mqtt_uid, sizeof(uid) - 1);
    uid[sizeof(uid) - 1] = '\0';
    strncpy(nome, obj->nome, sizeof(nome) - 1);
    nome[sizeof(nome) - 1] = '\0';

    // 2. Costruzione TOPIC con NUOVO PREFISSO: ESP32-WI
    // Esempio: ESP32-WI/004B123C11F8/Ventola/stat_t
    snprintf(obj->state_topic, MAX_TOPIC, "ESP32-WI/%s/%s/stat_t", uid, nome);
    snprintf(obj->CmdTopic, MAX_TOPIC, "ESP32-WI/%s/%s/cmd_t", uid, nome);

    // Topic di configurazione (Discovery) rimane sotto homeassistant/ perché è lì che HA ascolta
    snprintf(t_conf, sizeof(t_conf), "homeassistant/%s/%s/%s/config",
             obj->ha_tipo, uid, nome);

    // Prepariamo stringhe extra per i numeri
    char extra_number_config[128] = "";
    if (strcmp(obj->ha_tipo, "number") == 0)
    {
        // Inseriamo min, max e uno step predefinito (es. 1 per int, 0.1 per float)
        float step = (strstr(obj->tipo, "float")) ? 0.1f : 1.0f;

        snprintf(extra_number_config, sizeof(extra_number_config),
                 ",\"min\":%.1f,\"max\":%.1f,\"step\":%.1f,\"mode\":\"box\"",
                 obj->min, obj->max, step);
    }

    // 3. Costruzione JSON aggiornata
    snprintf(json_buffer, sizeof(json_buffer),
             "{"
             "\"name\":\"%s\","       // qui metto l'etichetta, che è quella che voglio vedere su Home Assistant, non il nome pulito usato nei topic
             "\"uniq_id\":\"%s_%s\"," // ID univoco per HA, composto da mqtt_uid + nome variabile
             "\"stat_t\":\"%s\","
             "\"cmd_t\":\"%s\","
             "\"unit_of_meas\":\"%s\"" // <--- L'unità di misura è qui
             "%s,"                     // <--- Qui inseriamo min/max/step se è un numero
             "\"dev\":{"
             "\"ids\":[\"%s\"],"
             "\"name\":\"%s\","
             "\"mf\":\"%s\","
             "\"mdl\":\"%s\""
             "}"
             "}",
             obj->etichetta,
             uid, nome,
             obj->state_topic,
             obj->CmdTopic,
             obj->UDM,            // Assicurati che obj->UDM contenga "°C" o "bar"
             extra_number_config, // Inserisce i parametri extra
             uid,
             self->titolo,
             MF,
             MDL);

    int msg_id = esp_mqtt_client_publish(self->mqtt_client, t_conf, json_buffer, 0, 1, 1);

    vTaskDelay(pdMS_TO_TICKS(50)); // Piccola pausa per evitare sovraccarichi in caso di molti oggetti

    return (msg_id != -1);
}
// --- GESTIONE STATI ---

// Questa funzione pubblica lo stato attuale di un oggetto su MQTT, se è esposto a Home Assistant
bool WebInterface_publishState(WebInterface_t *self, int indice)
{
    if (!self->HA_Set || self->mqtt_client == NULL)
        return false;
    Oggetto_t *obj = &self->oggetti[indice];
    if (!obj->HA)
        return false;

    const char *val;
    if (strcmp(obj->ha_tipo, "binary_sensor") == 0 || strcmp(obj->ha_tipo, "switch") == 0)
    {
        val = WebInterface_getHA_Binary_Value(self, indice);
    }
    else
    {
        val = WebInterface_getValue(self, indice);
    }

    esp_mqtt_client_publish(self->mqtt_client, obj->state_topic, val, 0, 1, 1);

    return true;
}

// pubblica lo stato di un oggetto passato attraverso il nome
bool WebInterface_publishStateByName(WebInterface_t *self, const char *_nome)
{
    // 3. Gestione e SANIFICAZIONE del nome
    char nome_pulito[MAX_NOME];
    strncpy(nome_pulito, _nome, MAX_NOME - 1);
    nome_pulito[MAX_NOME - 1] = '\0';

    // assicuriamoci che il nome sia pulito da caratteri che potrebbero creare problemi nel web (es. '|')
    // e con home assistant (es. spazi)
    // --- DISCRIMINAZIONE SANIFICAZIONE ---
    WebInterface_Sanitize_name(nome_pulito);

    for (int i = 0; i < self->num_oggetti; i++)
    {
        if (strcmp(self->oggetti[i].nome, nome_pulito) == 0)
        {
            WebInterface_publishState(self, i);
            return true;
        }
    }
    return false;
}

// Questa funzione pubblica lo stato di tutti gli oggetti esposti a Home Assistant (chiamata ad esempio all'avvio o alla riconnessione)
void WebInterface_publishAllState(WebInterface_t *self)
{
    for (int i = 2; i < self->num_oggetti; i++)
    {
        if (self->oggetti[i].HA)
            WebInterface_publishState(self, i);
    }
}

// --- EVENT HANDLER (Connection & Data) ---

// Funzione di callback per gli eventi MQTT (connessione, disconnessione, dati ricevuti)
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    WebInterface_t *self = (WebInterface_t *)handler_args;
    esp_mqtt_event_handle_t event = event_data;

    switch ((esp_mqtt_event_id_t)event_id)
    {
    case MQTT_EVENT_CONNECTED:
        self->mqtt_connected = true;
        WebInterface_Log(self, TAG, "Broker connesso");

        for (int i = 2; i < self->num_oggetti; i++)
        {
            if (self->oggetti[i].HA)
            {
                if (WebInterface_publishDiscovery(self, i))
                {
                    WebInterface_Log(self, TAG, "Discovery pubblicato per %s", self->oggetti[i].nome);
                }

                // --- PUNTO 1: AGGIUNTA SOTTOSCRIZIONE ---
                // Se l'oggetto ha un CmdTopic (es. switch o button), dobbiamo ascoltarlo
                if (strlen(self->oggetti[i].CmdTopic) > 0)
                {
                    esp_mqtt_client_subscribe(self->mqtt_client, self->oggetti[i].CmdTopic, 1);
                    WebInterface_Log(self, TAG, "Sottoscritto al topic: %s\n", self->oggetti[i].CmdTopic);
                }
            }
        }
        WebInterface_publishAllState(self);
        break;

    case MQTT_EVENT_DATA:
    {
        char topic[128], data[128];
        snprintf(topic, sizeof(topic), "%.*s", event->topic_len, event->topic);
        snprintf(data, sizeof(data), "%.*s", event->data_len, event->data);

        for (int i = 2; i < self->num_oggetti; i++)
        {
            if (self->oggetti[i].HA && strcmp(self->oggetti[i].CmdTopic, topic) == 0)
            {
                if (strcmp(self->oggetti[i].ha_tipo, "switch") == 0)
                {
                    WebInterface_setValue(self, i, (strcmp(data, "ON") == 0) ? "true" : "false");
                }
                else if (strcmp(self->oggetti[i].ha_tipo, "button") == 0)
                {
                    WebInterface_exeFunzione(self, i);
                }
                else
                {
                    WebInterface_setValue(self, i, data);
                }
                WebInterface_publishState(self, i);
                break;
            }
        }
        break;
    }
    case MQTT_EVENT_DISCONNECTED:
        self->mqtt_connected = false;
        WebInterface_Log(self, TAG, "Broker disconnesso");
        break;
    default:
        break;
    }
}

// --- AVVIO ---

// Funzione interna al file (static) per il task
static void mqtt_refresh_task(void *pvParameters);

// Questa funzione avvia la connessione MQTT e crea un task per il refresh periodico dei dati
void WebInterface_StartMQTT(WebInterface_t *self)
{
    // Se non ci sono i parametri minimi, usciamo
    if (!self->HA_Set)
        return;

    // 1. GESTIONE CLIENT MQTT
    // Se il client esiste già, non lo ricreiamo: lo facciamo solo ripartire
    if (self->mqtt_client != NULL)
    {
        esp_mqtt_client_start(self->mqtt_client);
    }
    else
    {
        esp_mqtt_client_config_t mqtt_cfg = {
            .broker.address.uri = self->mqtt_server,
            .broker.address.port = self->mqtt_port,
            .credentials.username = self->mqtt_user,
            .credentials.authentication.password = self->mqtt_pw,
            .credentials.client_id = self->mqtt_uid,
            .session.keepalive = 60,
        };

        self->mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
        esp_mqtt_client_register_event(self->mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, self);
        esp_mqtt_client_start(self->mqtt_client);
    }

    // 2. GESTIONE TASK DI REFRESH (Sotto il cofano)
    // Verifichiamo se l'handle del task è NULL. Se lo è, il task non è mai partito.
    if (self->mqtt_task_handle == NULL)
    {
        xTaskCreate(mqtt_refresh_task,
                    "mqtt_refresh_task",
                    4096,
                    self,
                    5,
                    &self->mqtt_task_handle); // Salviamo l'handle qui

        ESP_LOGI("MQTT", "Task di refresh creato con successo.");
    }
    else
    {
        ESP_LOGD("MQTT", "Task di refresh già attivo, salto creazione.");
    }
}

// Task che si occupa di inviare periodicamente i dati su MQTT (stato e discovery)
// Viene eseguito in loop e rispetta l'intervallo definito in self->mqtt_refresh
// Inoltre, controlla lo stato del Wi-Fi e del broker per evitare di inviare dati quando non è possibile
// Questo task è fondamentale per mantenere aggiornati i dati su Home Assistant, soprattutto dopo una riconnessione
// La logica prevede un controllo dinamico dello stato del Wi-Fi e del broker, e un invio periodico dei dati solo quando è possibile

static void mqtt_refresh_task(void *pvParameters)
{
    WebInterface_t *self = (WebInterface_t *)pvParameters;

    // Aspettiamo che il sistema si stabilizzi all'avvio (10 secondi)
    vTaskDelay(pdMS_TO_TICKS(10000));

    int discovery_counter = 0;

    while (1)
    {
        // --- CONTROLLO ABILITAZIONE UTENTE ---
        // Eseguiamo la logica solo se l'interruttore MQTT sulla dashboard è ON
        if (self->mqtt_enabled)
        {
            // --- CONTROLLO STATO WIFI ---
            wifi_mode_t mode;
            esp_err_t err = esp_wifi_get_mode(&mode);

            // Procediamo solo se il Wi-Fi è in modalità Station o Dual (STA+AP)
            // E se l'evento MQTT ci ha confermato la connessione al broker
            if (err == ESP_OK && (mode == WIFI_MODE_STA || mode == WIFI_MODE_APSTA))
            {
                if (self->mqtt_connected)
                {
                    for (int i = 2; i < self->num_oggetti; i++)
                    {
                        if (self->oggetti[i].HA)
                        {
                            // 1. PUBBLICA LO STATO (Telemetria)
                            WebInterface_publishState(self, i);
                            vTaskDelay(pdMS_TO_TICKS(50));

                            // 2. PUBBLICA IL DISCOVERY (Configurazione - ogni 30 cicli)
                            if (discovery_counter == 0)
                            {
                                WebInterface_publishDiscovery(self, i);
                                vTaskDelay(pdMS_TO_TICKS(150));
                            }
                        }
                    }

                    // Incrementiamo il contatore per il prossimo invio Discovery
                    discovery_counter++;
                    if (discovery_counter >= 30)
                        discovery_counter = 0;
                }
            }
            else
            {
                // Se il Wi-Fi è spento o in solo AP, resettiamo per forzare Discovery al ritorno
                discovery_counter = 0;
            }
        }
        else
        {
            // Se MQTT è disabilitato dall'interruttore, resettiamo il counter
            discovery_counter = 0;
        }

        // --- ATTESA DINAMICA ---
        // Utilizziamo l'intervallo definito dall'utente (self->mqtt_refresh)
        int delay_ms = self->mqtt_refresh * 1000;
        if (delay_ms < 1000)
            delay_ms = 1000;

        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
}

// Funzione per resettare completamente il client MQTT (stop + destroy) e poi riavviarlo
// Utile ad esempio quando si cambiano i parametri di connessione per forzare una nuova connessione con i nuovi dati
void WebInterface_RestartMQTT(WebInterface_t *self)
{
    if (self->mqtt_client != NULL)
    {
        esp_mqtt_client_stop(self->mqtt_client);
        esp_mqtt_client_destroy(self->mqtt_client);
        self->mqtt_client = NULL; // Cruciale affinché StartMQTT ne crei uno nuovo
        self->mqtt_connected = false;
    }
    WebInterface_StartMQTT(self);
}