![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)
![Framework: ESP-IDF](https://img.shields.io/badge/Framework-ESP--IDF-blue.svg)
![Platform: ESP32](https://img.shields.io/badge/Platform-ESP32-red.svg)

# ESP32 WebInterface & Home Assistant
**Sistema di controllo domotico professionale per ESP32.**

Questo componente permette di:
- Configurare il Wi-Fi tramite Access Point.
- Gestire fino a 40 oggetti (sensori/in![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)
![Framework: ESP-IDF](https://img.shields.io/badge/Framework-ESP--IDF-blue.svg)
![Platform: ESP32](https://img.shields.io/badge/Platform-ESP32-red.svg)

# 🚀 ESP32 Smart WebInterface & Home Assistant Connector
**Developed by: Suma Marco**

[Italian version below / Versione italiana in basso]

A professional-grade component for **ESP-IDF v5.x** that creates a modern web dashboard to monitor and control variables in real-time, with seamless **Home Assistant MQTT Auto-Discovery** integration.

## 🌟 Key Features
- **Real-time Monitoring:** Connect memory pointers (char, int, float, bool) directly to the web UI with selectable refresh rates (0.5s - 3s).
- **Interactive Controls:** Toggle switches, input numbers, or trigger buttons directly from the browser.
- **Smart Connectivity:** Built-in WiFi Manager (Access Point mode at 192.168.4.1) to scan and connect to local networks.
- **HA Auto-Discovery:** Automatically exports variables as Switches, Sensors, or Numbers to Home Assistant via MQTT.
- **Persistent Storage:** All configurations (WiFi, MQTT, Object settings) are saved in NVS memory.

## 🏠 Home Assistant Integration
No YAML coding required! The device uses MQTT Discovery to appear automatically in your Home Assistant dashboard.

---

## 🇮🇹 Versione Italiana

Questo componente trasforma un ESP32 in una centralina domotica completa. Permette di registrare variabili (int, float, bool) con puntatori diretti alla memoria e visualizzarle/modificarle tramite una dashboard web moderna.

### Caratteristiche Principali
- **Configurazione WiFi:** Al primo avvio, l'ESP32 crea un Access Point (192.168.4.1) per l'inserimento delle credenziali.
- **Dashboard Dinamica:** Aggiornamento in tempo reale dei dati visualizzati.
- **Integrazione MQTT:** Supporto completo per Mosquitto/Home Assistant con autodiscovery degli oggetti.
- **Status Link:** Icone dinamiche 🔗/🔓 per il monitoraggio della connessione in tempo reale.

### Esempio Interfaccia / UI Examples
<img width="1170" alt="Dashboard" src="https://github.com/user-attachments/assets/376ce582-993a-4a71-8b6f-301fb8e66289" />

### Schede di sistema / System Tabs
<img width="606" alt="Settings" src="https://github.com/user-attachments/assets/b939758f-c628-446c-8717-3497c4a37b07" />

## 🛠️ Installation / Installazione
1. Copy the `Esp32_WebInterface` folder into the `components` directory of your ESP-IDF project.
2. Inizializza il componente nel tuo `main.c`.

## 📜 Requirements / Requisiti
- ESP-IDF v5.x
- MQTT Broker (Mosquitto or Home Assistant Add-on)terruttori) da una Dashboard Web.
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
