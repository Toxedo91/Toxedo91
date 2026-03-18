# Motor Monitoring System (ESP32 + Raspberry Pi)

Dieses Projekt enthält eine komplette Vorlage für ein verteiltes Motor-Monitoring-System mit bis zu 50 ESP32-Controllern und einem Raspberry Pi als Monitoring-Server.

## Struktur

- `esp32/esp32_motor_monitor.ino` – Arduino Firmware für ESP32
- `raspberry_pi/setup.md` – Setup-Anleitung für Raspberry Pi
- `raspberry_pi/server/app.py` – Flask + MQTT + WebSocket Monitoring Server
- `raspberry_pi/server/templates/index.html` – Dashboard HTML
- `raspberry_pi/server/static/style.css` – Dashboard Styling
- `raspberry_pi/server/static/app.js` – Dashboard Logik (Live Updates)

## Features

### ESP32
- WLAN Verbindung zum Router
- 3 Verbindungsversuche, dann AP-Fallback auf `192.168.4.1`
- Webinterface mit JSON Endpoint
- MQTT Statuspublishing zum Raspberry Pi Broker (`1883`)
- Motor-ID über DIP/DMX-Schalter
- Regelmäßiger Versand von:
  - Motor ID
  - Online Status
  - Position
  - Geschwindigkeit
  - RAM Nutzung
  - CPU Temperatur
  - Fehlerstatus

### Raspberry Pi
- Mosquitto MQTT Broker
- Python Flask Server mit Socket.IO
- Dashboard mit:
  - Online (grün)
  - Offline (rot)
  - Warnungen (gelb)
  - CPU / RAM Anzeige
  - Live Updates via WebSocket
  - Auto-Hide für lange offline Motoren
  - Systemübersicht (online/offline/warnings)

## Schnellstart
1. ESP32 Sketch öffnen, WLAN/MQTT Konfiguration anpassen, flashen.
2. Raspberry Pi nach `raspberry_pi/setup.md` einrichten.
3. Monitoring Server starten und Dashboard im Browser öffnen.
