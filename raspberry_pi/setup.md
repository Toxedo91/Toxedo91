# Raspberry Pi 4 Setup (Monitoring Server)

## 1) System vorbereiten
```bash
sudo apt update
sudo apt upgrade -y
sudo apt install -y python3 python3-pip python3-venv mosquitto mosquitto-clients
```

Mosquitto beim Start aktivieren:
```bash
sudo systemctl enable mosquitto
sudo systemctl start mosquitto
sudo systemctl status mosquitto
```

Optional: Broker testen
```bash
# Terminal 1
mosquitto_sub -h localhost -t 'motors/+/status' -v

# Terminal 2
mosquitto_pub -h localhost -t 'motors/1/status' -m '{"motor_id":1,"online":true}'
```

## 2) Monitoring Server installieren
```bash
cd ~/motor-monitoring/raspberry_pi/server
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
```

## 3) Konfiguration
Umgebungsvariablen (optional):
```bash
export MQTT_HOST=localhost
export MQTT_PORT=1883
export FLASK_HOST=0.0.0.0
export FLASK_PORT=5000
```

## 4) Starten
```bash
cd ~/motor-monitoring/raspberry_pi/server
source .venv/bin/activate
python app.py
```

Dashboard aufrufen:
- `http://<RPI-IP>:5000`

## 5) Als Service (optional)
`/etc/systemd/system/motor-monitor.service`
```ini
[Unit]
Description=Motor Monitoring Flask Server
After=network.target mosquitto.service

[Service]
Type=simple
User=pi
WorkingDirectory=/home/pi/motor-monitoring/raspberry_pi/server
Environment=MQTT_HOST=localhost
Environment=MQTT_PORT=1883
Environment=FLASK_HOST=0.0.0.0
Environment=FLASK_PORT=5000
ExecStart=/home/pi/motor-monitoring/raspberry_pi/server/.venv/bin/python app.py
Restart=always
RestartSec=3

[Install]
WantedBy=multi-user.target
```

Aktivieren:
```bash
sudo systemctl daemon-reload
sudo systemctl enable motor-monitor.service
sudo systemctl start motor-monitor.service
sudo systemctl status motor-monitor.service
```
