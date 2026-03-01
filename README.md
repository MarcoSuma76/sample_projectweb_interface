![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)
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
- MQTT Broker (Mosquitto or Home Assistant Add-on)
