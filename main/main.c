// Sviluppata da [Suma MArco] - [Il Tuo Sito Web o Contatto]
// main.c - Esempio di utilizzo della libreria Esp32_WebInterface
// questo ocomponente serve a creare un interfaccia web per monitorare e controllare variabili e funzioni su un ESP32,
// con integrazione opzionale per Home Assistant tramite MQTT.

// --- CARATTERISTICHE PRINCIPALI ---
// - Registrazione di variabili di diversi tipi (char, int, float, bool) con puntatori diretti alla memoria del programma
// - Supporto per variabili "set" con limiti personalizzati e unità di misura
// - Aggiunta di bottoni con funzioni di callback
// Questa è la funzione principale dell'applicazione, dove viene inizializzata l'interfaccia web,
// registrate le variabili e avviato il sistema. Puoi personalizzare questa funzione per adattarla alle tue esigenze specifiche,
// aggiungendo più variabili, funzioni di callback o logica di controllo.

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "Esp32_WebInterface.h"

// istanzio la struttura principale del Web Interface
// Questa struttura contiene tutte le informazioni e i dati necessari per gestire l'interfaccia web,
// il server HTTP, la connessione MQTT e i vari oggetti registrati
// al primo avvio troverai l'interfaccia al indirizzo 192.168.4.1 (modalità AP)
// dall'interfaccia puoi cercare le reti WiFi disponibili, inserire le credenziali e connetterti alla tua rete locale,
// dopo di che l'indirizzo IP cambierà in quello assegnato dal router tuttii dati inseriti e le variabili registrate rimarranno
// salvati in memoria e saranno accessibili dall'interfaccia web  anche dopo un riavvio del dispositivo.
// se lo desideri dall'interfaccia puoi anche attivare la connessione mqtt e configurare i parametri per l'integrazione con Home Assistant,
// altrimenti il sistema funzionerà comunque come interfaccia web locale
WebInterface_t myInterface;

// VARIABILI GLOBALI
int valore_intero = 25;
float valore_float = 12.5;
bool stato_bool = false;
char NomeLuce[30] = "Luce Principale";
int secondi_uptime = 0;

// azione del bottone di esempio, che scrive un messaggio di debug quando viene premuto
void azione_bottone(void *arg)
{
    WebInterface_Debug(&myInterface, "Log di main", "Bottone! premuto!\n");
}

void app_main(void)
{
    int idx = 0; // Variabile per memorizzare l'indice dell'oggetto registrato, se necessario

    // 1. Definizione della struttura Web Interface e inizializzazione con il titolo dell'interfaccia web (es. "Centralina Smart")
    // passo il puntatore alla struttura principale e il titolo dell'interfaccia web (es. "Centralina Smart")
    // il sistema funzionera solo come interfaccia web locale se non configuri la parte MQTT/Home Assistant,
    // finche non viene aperto lato client il resto del programma continua a funzionare regolarmente,

    WebInterface_Init(&myInterface, "Centralina Smart");

    // 2. Registrazione delle variabili (AddVar...)
    idx = WebInterface_AddDebug(&myInterface, "Log di main", 512);

    // Esempio di registrazione di una variabile intera con limiti personalizzati e unità di misura (UDM)
    // passo l'interfacia, il nome della variabile, il puntatore alla variabile, il valore di default (come stringa),
    // se è per HA e l'unità di misura (UDM)
    idx = WebInterface_AddVar_Set_Int(&myInterface, "Soglia_Nuova", &valore_intero, "25", true, "C");
    if (idx != -1)
    {
        // 2. Personalizzi i limiti specifici per questo sensore
        myInterface.oggetti[idx].min = -30.0f;
        myInterface.oggetti[idx].max = 30.0f;
    }

    // Esempio di registrazione di una variabile float
    // passo l'interfacia, il nome della variabile, il puntatore alla variabile, il valore di default (come stringa),
    // se è per HA e l'unità di misura (UDM)
    idx = WebInterface_AddVar_Set_Float(&myInterface, "Pressione", &valore_float, "12.5", true, "bar");

    // Esempio di registrazione di una variabile bool con valore di default e visibilità su Home Assistant
    // passo l'interfacia, il nome della variabile, il puntatore alla variabile, il valore di default (come stringa), se è per HA
    idx = WebInterface_AddVar_Set_Bool(&myInterface, "Ventola", &stato_bool, "false", true);

    // Esempio di registrazione di una variabile stringa con lunghezza massima e valore di default
    // passo l'interfacia, il nome della variabile, il puntatore alla variabile, la lunghezza massima della stringa,
    // il valore di default (come stringa)
    idx = WebInterface_AddVar_Set_Char(&myInterface, "Nome Luce", NomeLuce, 30, "Luce Principale");

    // Esempio di registrazione di un contatore di secondi di uptime, esposto anche su Home Assistant con unità di misura "s"
    // passo l'interfacia, il nome della variabile, il puntatore alla variabile, se è per HA e l'unità di misura (UDM)
    idx = WebInterface_AddCountDHMS(&myInterface, "Uptime", &secondi_uptime, true, "s");

    // 3. Registrazione di un bottone (AddButton)
    // passo l'interfaccia, il nome del bottone, il valore da mostrare sul bottone, la funzione di callback e se è per HA
    idx = WebInterface_AddButton(&myInterface, "Svuota_Log", "Esegui", &azione_bottone, true);

    // 4. Configurazione MQTT per Home Assistant (SetupHA) - OPZIONALE
    // passo l'interfaccia, l'indirizzo del broker MQTT, la porta, l'username, la password e l'intervallo di refresh in secondi
    WebInterface_SetupHA(&myInterface,
                         "mqtt://192.168.1.156", // Indirizzo Broker
                         1883,                   // Porta
                         "IL_TUO_USER",          // Username
                         "LA_TUA_PASSWORD",      // Password
                         60);                    // Refresh secondi

    // AVVIO TOTALE (Tutto in uno)
    WebInterface_Start(&myInterface);

    // 5. Ora puoi aggiornare le variabili in loop o da altre funzioni, e il sistema si occuperà di aggiornare l'interfaccia web e inviare i dati a Home Assistant se configurato

    // Esempio di aggiornamento del log di debug con una stringa formattata
    WebInterface_Debug(&myInterface, "Log di main", "Ciao!\n");

    // Simulazione di un contatore di uptime che si aggiorna ogni secondo
    // in background, il sistema si occuperà di aggiornare l'interfaccia web e inviare i dati a Home Assistant se configurato
    while (1)
    {
        secondi_uptime = (int)(xTaskGetTickCount() * portTICK_PERIOD_MS / 1000);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}