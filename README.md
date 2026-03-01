![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)
![Framework: ESP-IDF](https://img.shields.io/badge/Framework-ESP--IDF-blue.svg)
![Platform: ESP32](https://img.shields.io/badge/Platform-ESP32-red.svg)

# ESP32 WebInterface & Home Assistant
**Sistema di controllo domotico professionale per ESP32.**

Questo componente permette di:
- Configurare il Wi-Fi tramite Access Point.
- Gestire fino a 40 oggetti (sensori/interruttori) da una Dashboard Web.
- Integrare tutto in Home Assistant via MQTT con auto-discovery.

  <img width="1170" height="787" alt="image" src="https://github.com/user-attachments/assets/376ce582-993a-4a71-8b6f-301fb8e66289" />


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
