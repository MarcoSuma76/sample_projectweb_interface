// Esp32_WebInterface.h - Interfaccia Web per ESP32 con supporto MQTT e Home Assistant
// Versione 2.0 - 2024-06-01
// Questa libreria consente di creare un'interfaccia web per monitorare e controllare variabili e funzioni
// su un ESP32, con integrazione opzionale per Home Assistant tramite MQTT.
// Sviluppata da [Suma MArco] - [Il Tuo Sito Web o Contatto]
// --- CARATTERISTICHE PRINCIPALI ---
// - Registrazione di variabili di diversi tipi (char, int, float, bool) con puntatori diretti alla memoria del programma
// - Supporto per variabili "set" con limiti personalizzati e unità di misura
// - Aggiunta di bottoni con funzioni di callback
// - Integrazione con Home Assistant tramite MQTT, con supporto per Discovery e aggiornamenti automatici
// - Interfaccia web responsive e personalizzabile

#ifndef Esp32_WebInterface_h
#define Esp32_WebInterface_h

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>
#include "esp_http_server.h"
#include "esp_wifi.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include "mqtt_client.h"

#ifdef __cplusplus
extern "C"
{
#endif

// --- COSTANTI OTTIMIZZATE ---
#define MAX_OGGETTI 40      // Massimo numero di variabili registrabili
#define MAX_NOME 30         // Lunghezza max nome etichetta
#define MAX_VALORE 32       // Lunghezza max stringa valore
#define MAX_TIPO 20         // Lunghezza max stringa tipo (es. "int_set")
#define MAX_TOPIC 150       // Lunghezza max topic MQTT
#define MAX_HA_TIPO 20      // Tipo dispositivo Home Assistant (es. "sensor")
#define MAX_UDM 8           // Unità di misura (es. "°C", "bar", "V")
#define MAX_MQTT_SERVER 100 // Lunghezza max indirizzo server MQTT
#define MAX_MQTT_USER 30    // Lunghezza max username MQTT
#define MAX_MQTT_PW 30      // Lunghezza max password MQTT
#define MAX_SSID 32         // Lunghezza max SSID WiFi
#define MAX_PASS 64         // Lunghezza max password WiFi
#define MAX_D_UID 13        // <-- REINSERITO PER COMPATIBILITÀ RIGHE 77
#define BUFFER_JSON 1024    // <-- REINSERITO PER SICUREZZA

    // --- STRUTTURE ---
    typedef struct
    {
        char nome[MAX_NOME];         // Nome visualizzato nell'interfaccia web
        char etichetta[MAX_NOME];    // Nome leggibile (es: "Luce Cantina")
        char tipo[MAX_TIPO];         // Tipo di dato (es. "char", "int_set", "button", "system-log")
        char value[MAX_VALORE];      // Valore attuale (sempre come stringa, convertita in base al tipo)
        unsigned int LcharP;         // Lunghezza massima per char_set
        char *Pchar;                 // Puntatore a variabile char (se tipo è char o char_set o debug)
        bool *Pbool;                 // Puntatore a variabile bool (se tipo è bool o bool_set)
        int *Pint;                   // Puntatore a variabile int (se tipo è int o int_set)
        float *Pfloat;               // Puntatore a variabile float (se tipo è float o float_set)
        void (*Pfunzione)();         // Puntatore a funzione (se tipo è button)
        bool HA;                     // Flag per indicare se la variabile è esposta a Home Assistant
        const char *valore_default;  // Valore di default per reset (sempre come stringa)
        char ha_tipo[MAX_HA_TIPO];   // Tipo dispositivo Home Assistant (es. "sensor", "switch")
        char UDM[MAX_UDM];           // Unità di misura (es. "°C", "bar", "V")
        char state_topic[MAX_TOPIC]; // Topic MQTT per lo stato (se HA è true)
        char CmdTopic[MAX_TOPIC];    // Topic MQTT per i comandi (se HA è true e tipo è set)
        float min;                   // Valore minimo (per int_set e float_set)
        float max;                   // Valore massimo (per int_set e float_set)
    } Oggetto_t;

    typedef struct
    {
        httpd_handle_t server;                // Handle del server HTTP
        Oggetto_t oggetti[MAX_OGGETTI];       // Array di oggetti registrati
        int num_oggetti;                      // Numero di oggetti attualmente registrati
        char titolo[MAX_NOME];                // Titolo dell'interfaccia web
        bool HA_Set;                          // Flag per indicare se la configurazione Home Assistant è stata impostata
        char mqtt_server[MAX_MQTT_SERVER];    // Indirizzo del server MQTT
        char mqtt_user[MAX_MQTT_USER];        // Username MQTT
        char mqtt_pw[MAX_MQTT_PW];            // Password MQTT
        int mqtt_port;                        // Porta MQTT
        int mqtt_refresh;                     // Intervallo di aggiornamento MQTT in secondi
        esp_mqtt_client_handle_t mqtt_client; // Handle del client MQTT
        TaskHandle_t mqtt_task_handle;        //  Handle del task per il refresh MQTT
        bool mqtt_enabled;                    // Flag per indicare se il client MQTT è abilitato/disabilitato
        bool mqtt_connected;                  // Flag per indicare se il client MQTT è connesso
        bool ota_in_progress;                 // flag true quando l'OTA è attivo
        char system_log[2048];                // Buffer per il log di sistema, con dimensione aumentata a 2048 caratteri
        char mqtt_uid[MAX_D_UID];             // ID univoco generato dal MAC, usato per MQTT Discovery
    } WebInterface_t;

    // --- PROTOTIPI PUBBLICI ---

    // inizializzazione e configurazione passando il titolo dell'interfaccia web (es. "Centralina Smart")
    void WebInterface_Init(WebInterface_t *self, const char *titolo);

    // eventuale impostazione dela configurazione per l'integrazione con Home Assistant (MQTT)
    // - se non la usi, il sistema funziona comunque come interfaccia web locale
    void WebInterface_SetupHA(WebInterface_t *self, const char *_mqtt_server, int _mqtt_port, const char *_mqtt_user, const char *_mqtt_pw, int _mqtt_refresh);

    // Avvio dell'interfaccia web: carica dati, avvia Wi-Fi e server HTTP
    void WebInterface_Start(WebInterface_t *self);

    //-------------------------------------------------

    // Aggiunta Variabili

    int WebInterface_AddVar_Char(WebInterface_t *self, const char *nome, char *Pchar);
    int WebInterface_AddVar_Bool(WebInterface_t *self, const char *nome, bool *Pbool, bool HA);
    int WebInterface_AddVar_Int(WebInterface_t *self, const char *nome, int *Pint, bool HA, const char *UDM);
    int WebInterface_AddVar_Float(WebInterface_t *self, const char *nome, float *Pfloat, bool HA, const char *UDM);

    int WebInterface_AddVar_Set_Char(WebInterface_t *self, const char *nome, char *Pchar, unsigned int LcharP, const char *default_fisso);
    int WebInterface_AddVar_Set_Int(WebInterface_t *self, const char *nome, int *Pint, const char *default_fisso, bool HA, const char *UDM);
    int WebInterface_AddVar_Set_Float(WebInterface_t *self, const char *nome, float *Pfloat, const char *default_fisso, bool HA, const char *UDM);
    int WebInterface_AddVar_Set_Bool(WebInterface_t *self, const char *nome, bool *Pbool, const char *default_fisso, bool HA);

    int WebInterface_AddButton(WebInterface_t *self, const char *nome, const char *valore, void (*Pfunzione)(), bool HA);
    int WebInterface_AddDebug(WebInterface_t *self, const char *nome, unsigned int dimensione);
    int WebInterface_AddCountDHMS(WebInterface_t *self, const char *nome, int *Pint, bool HA, const char *UDM);
    int WebInterface_AddCount(WebInterface_t *self, const char *nome, int *Pint, bool HA, const char *UDM);

    //--------------- Gestione Valori

    // Queste funzione imposta il valore di un oggetto passandolo come stringa
    void WebInterface_setValue(WebInterface_t *self, int indice, const char *_valore);
    // Questa funzione restituisce il valore di un oggetto come stringa
    const char *WebInterface_getValue(WebInterface_t *self, int indice);
    // Questa funzione esegue la funzione associata a un button, se presente
    void WebInterface_exeFunzione(WebInterface_t *self, int indice);

    // --- PROTOTIPI INTERNI ---
    void WebInterface_setNome(WebInterface_t *self, int indice, const char *_nome);
    void WebInterface_setTipo(WebInterface_t *self, int indice, const char *_tipo);
    void WebInterface_setUDM(WebInterface_t *self, int indice, const char *_UDM);
    void WebInterface_setHaTipo(WebInterface_t *self, int indice, const char *_ha_tipo);

    // contolli e funzioni di utilità
    bool WebInterface_nomedoppio(WebInterface_t *self, const char *nome);
    void WebInterface_Sanitize(char *str);
    void WebInterface_Sanitize_name(char *str);

    // Trasmissione dati verso Web
    const char *WebInterface_ValueFromProgramByPchar(WebInterface_t *self, int indice);
    void WebInterface_ValueFromProgramByPbool(WebInterface_t *self, int indice);
    void WebInterface_ValueFromProgramByPint(WebInterface_t *self, int indice);
    void WebInterface_ValueFromProgramByPfloat(WebInterface_t *self, int indice);

    // Ricezione dati dal Web
    void WebInterface_ValueFromWebByPchar(WebInterface_t *self, int indice, const char *valore_dal_web);
    void WebInterface_ValueFromWebByPbool(WebInterface_t *self, int i);
    void WebInterface_ValueFromWebByPint(WebInterface_t *self, int i);
    void WebInterface_ValueFromWebByPfloat(WebInterface_t *self, int i);

    // Funzioni di sistema
    void WebInterface_resetEsp32(WebInterface_t *self);
    void WebInterface_memorizzaDati(WebInterface_t *self);
    void WebInterface_azzeraDati(WebInterface_t *self);
    void WebInterface_setUpEeprom(WebInterface_t *self);

    // Funzioni Wi-Fi & DNS
    void WebInterface_WiFi_Init(WebInterface_t *self);
    void WebInterface_StartDNS(WebInterface_t *self);
    void WebInterface_StopDNS(WebInterface_t *self);
    esp_err_t WebInterface_WiFi_GetScanResults(wifi_ap_record_t *ap_info, uint16_t *ap_count, uint16_t max_networks);
    void WebInterface_WiFi_SaveConfig(WebInterface_t *self, const char *ssid, const char *pass);
    void WebInterface_WiFi_ClearConfig(void);

    // Avvio server e imposta handler
    void WebInterface_Server_Start(WebInterface_t *self);

    // Debug Log
    void WebInterface_Log(WebInterface_t *self, const char *tag, const char *formato, ...);
    void WebInterface_Debug(WebInterface_t *self, const char *nome_debug, const char *testo);

    // Avvia manualmente il client MQTT (chiamata internamente da WebInterface_Start).
    void WebInterface_StartMQTT(WebInterface_t *self);

    // Ferma e distrugge il client MQTT, se attivo, per poi avviarne uno nuovo con la configurazione aggiornata.
    void WebInterface_RestartMQTT(WebInterface_t *self);

    // Invia lo stato di un singolo oggetto al broker MQTT.
    bool WebInterface_publishState(WebInterface_t *self, int indice);

    // pubblica lo stato di un oggetto passato attraverso il nome
    bool WebInterface_publishStateByName(WebInterface_t *self, const char *_nome);

    // Invia lo stato di tutti gli oggetti configurati con HA = true.
    void WebInterface_publishAllState(WebInterface_t *self);

    // Pubblica il messaggio di Discovery per un oggetto specifico, permettendo a Home Assistant
    // di riconoscerlo e configurarlo automaticamente.
    bool WebInterface_publishDiscovery(WebInterface_t *self, int indice);

#ifdef __cplusplus
}
#endif

#endif