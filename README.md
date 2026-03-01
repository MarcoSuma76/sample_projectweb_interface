![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)
![Framework: ESP-IDF](https://img.shields.io/badge/Framework-ESP--IDF-blue.svg)
![Platform: ESP32](https://img.shields.io/badge/Platform-ESP32-red.svg)

# ESP32 WebInterface & Home Assistant
**Sistema di controllo domotico professionale per ESP32.**

Questo componente permette di:
- Configurare il Wi-Fi tramite Access Point.
- Gestire fino a 40 oggetti (sensori/interruttori) da una Dashboard Web.
- Integrare tutto in Home Assistant via MQTT con auto-discovery.


#####SPIEGAZIONE SOMMARIA

questo ocomponente serve a creare un interfaccia web per monitorare e controllare variabili e funzioni su un ESP32,
con integrazione opzionale per Home Assistant tramite MQTT.

 --- CARATTERISTICHE PRINCIPALI ---
- Registrazione di variabili di diversi tipi (char, int, float, bool) con puntatori diretti alla memoria del programma
  se aperta lato client l'interfaccia si aggiorna con periodi selezionabili(0.5-1-3 ") con le variabili in memoria a cui e collegata.
  se le variabili sono state collegate all'interfaccia con l'opzione "SET" si possono modificare dall'interfaccia, altrimenti vengono solo visualizzate.
  Inoltre è possibile collegare Bool, Int, Float, Button a un broker Mqtt(Mosquitto ,Home Assistant) per gestire tutto da lì.
  (Autodiscovery come Swicth, Number,Sensor e Button).

al primo avvio troverai l'interfaccia al indirizzo 192.168.4.1 (modalità AP)
dall'interfaccia puoi cercare le reti WiFi disponibili, inserire le credenziali e connetterti alla tua rete locale,
dopo di che l'indirizzo IP cambierà in quello assegnato dal router e tutti i dati inseriti e le variabili registrate rimarranno
salvati in memoria e saranno accessibili dall'interfaccia web  anche dopo un riavvio del dispositivo.
se lo desideri dall'interfaccia puoi anche attivare la connessione mqtt e configurare i parametri per l'integrazione con Home Assistant,
altrimenti il sistema funzionerà comunque come interfaccia web locale.

Esempio:
  
  <img width="1170" height="787" alt="image" src="https://github.com/user-attachments/assets/376ce582-993a-4a71-8b6f-301fb8e66289" />



 Schede di sistema espandibili:
  <img width="606" height="798" alt="image" src="https://github.com/user-attachments/assets/b939758f-c628-446c-8717-3497c4a37b07" />



### Installazione
Copia la cartella `Esp32_WebInterface` nella cartella `components` del tuo progetto ESP-IDF.

# 🚀 ESP32 Smart Centralina (Web Interface + MQTT)
**Sviluppata da: Suma Marco**

Questo progetto trasforma un ESP32 in una centralina domotica completa con interfaccia web moderna.

## 🌐 Funzionalità Web
- **Dashboard**: Visualizzazione stati e controllo switch in tempo reale.
- **Configurazione MQTT**: Pagina dedicata per IP Broker, Porta e Credenziali.
- **Status Link**: Icona dinamica 🔗 (connesso) / 🔓 (scollegato).
- **Reset Intelligenti**: Pulsanti separati per reset Dati, WiFi e MQTT.

## 🏠 Integrazione Home Assistant
Grazie al protocollo MQTT con **Auto-Discovery**, la centralina appare automaticamente tra i dispositivi di Home Assistant senza dover scrivere codice YAML.

## 🛠️ Requisiti
- ESP-IDF v5.x
- Broker MQTT (es. Mosquitto o add-on di Home Assistant)
