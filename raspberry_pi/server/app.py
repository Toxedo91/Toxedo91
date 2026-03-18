import json
import os
import threading
import time
from dataclasses import dataclass, asdict
from typing import Dict

from flask import Flask, jsonify, render_template
from flask_socketio import SocketIO
import paho.mqtt.client as mqtt

MQTT_HOST = os.getenv("MQTT_HOST", "localhost")
MQTT_PORT = int(os.getenv("MQTT_PORT", "1883"))
FLASK_HOST = os.getenv("FLASK_HOST", "0.0.0.0")
FLASK_PORT = int(os.getenv("FLASK_PORT", "5000"))
MAX_MOTORS = int(os.getenv("MAX_MOTORS", "50"))
OFFLINE_TIMEOUT_S = int(os.getenv("OFFLINE_TIMEOUT_S", "8"))
HIDE_OFFLINE_AFTER_S = int(os.getenv("HIDE_OFFLINE_AFTER_S", "30"))

app = Flask(__name__)
socketio = SocketIO(app, cors_allowed_origins="*")


@dataclass
class MotorState:
    motor_id: int
    online: bool = False
    position_mm: float = 0.0
    speed_mm_s: float = 0.0
    ram_usage_percent: float = 0.0
    cpu_temp_c: float = 0.0
    error: bool = False
    error_text: str = "none"
    last_seen: float = 0.0


motor_states: Dict[int, MotorState] = {i: MotorState(motor_id=i) for i in range(1, MAX_MOTORS + 1)}
lock = threading.Lock()


def mqtt_on_connect(client, userdata, flags, reason_code):
    print(f"[MQTT] Connected with reason code={reason_code}")
    client.subscribe("motors/+/status")


def mqtt_on_disconnect(client, userdata, reason_code):
    print(f"[MQTT] Disconnected reason={reason_code}. Auto-reconnect active.")


def mqtt_on_message(client, userdata, message):
    try:
        payload = json.loads(message.payload.decode("utf-8"))
        motor_id = int(payload.get("motor_id", 0))
        if motor_id < 1 or motor_id > MAX_MOTORS:
            return

        with lock:
            state = motor_states[motor_id]
            state.online = bool(payload.get("online", True))
            state.position_mm = float(payload.get("position_mm", state.position_mm))
            state.speed_mm_s = float(payload.get("speed_mm_s", state.speed_mm_s))
            state.ram_usage_percent = float(payload.get("ram_usage_percent", state.ram_usage_percent))
            state.cpu_temp_c = float(payload.get("cpu_temp_c", state.cpu_temp_c))
            state.error = bool(payload.get("error", False))
            state.error_text = str(payload.get("error_text", "none"))
            state.last_seen = time.time()

        emit_snapshot()
    except Exception as exc:
        print(f"[MQTT] Parse error: {exc}")


def emit_snapshot():
    socketio.emit("dashboard_update", snapshot_data())


def snapshot_data():
    now = time.time()
    rows = []

    with lock:
        online_count = 0
        warning_count = 0

        for motor_id in sorted(motor_states.keys()):
            st = motor_states[motor_id]
            age = now - st.last_seen if st.last_seen > 0 else 10**9
            is_online = st.online and age <= OFFLINE_TIMEOUT_S
            hidden = (not is_online) and (age > HIDE_OFFLINE_AFTER_S)
            warn = st.error or st.cpu_temp_c > 75.0 or st.ram_usage_percent > 85.0

            if is_online:
                online_count += 1
            if warn:
                warning_count += 1

            rows.append(
                {
                    **asdict(st),
                    "is_online": is_online,
                    "warning": warn,
                    "age_s": round(age, 1),
                    "hidden": hidden,
                }
            )

    return {
        "summary": {
            "max_motors": MAX_MOTORS,
            "online_count": online_count,
            "offline_count": MAX_MOTORS - online_count,
            "warning_count": warning_count,
            "timestamp": int(now),
        },
        "motors": rows,
    }


@app.route("/")
def index():
    return render_template("index.html")


@app.route("/api/status")
def api_status():
    return jsonify(snapshot_data())


def mqtt_thread():
    client = mqtt.Client(client_id="rpi-monitor-server", clean_session=True)
    client.on_connect = mqtt_on_connect
    client.on_message = mqtt_on_message
    client.on_disconnect = mqtt_on_disconnect

    client.reconnect_delay_set(min_delay=1, max_delay=10)
    client.connect_async(MQTT_HOST, MQTT_PORT, keepalive=30)
    client.loop_forever(retry_first_connection=True)


def watchdog_thread():
    while True:
        socketio.sleep(1)
        emit_snapshot()


if __name__ == "__main__":
    threading.Thread(target=mqtt_thread, daemon=True).start()
    socketio.start_background_task(watchdog_thread)
    socketio.run(app, host=FLASK_HOST, port=FLASK_PORT)
