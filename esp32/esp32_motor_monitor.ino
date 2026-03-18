#include <WiFi.h>
#include <WebServer.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// ==============================
// User Configuration
// ==============================
const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
const char* MQTT_BROKER = "192.168.1.10"; // Raspberry Pi IP
const uint16_t MQTT_PORT = 1883;

const char* AP_SSID = "MotorController_Setup";
const char* AP_PASSWORD = "12345678";

// Publish intervals
const unsigned long STATUS_PUBLISH_INTERVAL_MS = 2000;
const unsigned long WEB_REFRESH_INTERVAL_MS = 1000;
const unsigned long OFFLINE_GRACE_MS = 10000;

// DMX DIP switch pins (6 bits -> 1..63 usable, clamp to 1..50)
const int DIP_PINS[6] = {32, 33, 25, 26, 27, 14};

// Optional: status LED
const int LED_PIN = 2;

WebServer server(80);
WiFiClient espClient;
PubSubClient mqttClient(espClient);

uint8_t motorId = 1;
unsigned long lastStatusPublish = 0;
unsigned long lastWebUpdate = 0;
unsigned long lastMqttReconnectAttempt = 0;

bool wifiConnected = false;
bool apMode = false;

// Motor state placeholders (replace with real PD Stepper integration)
float currentPositionMm = 0.0f;
float speedMmPerSec = 0.0f;
bool motorOnline = true;
bool hasError = false;
String errorText = "none";

// ==============================
// Utility
// ==============================
uint8_t readMotorIdFromDip() {
  uint8_t value = 0;
  for (int i = 0; i < 6; i++) {
    pinMode(DIP_PINS[i], INPUT_PULLUP);
    int bitVal = digitalRead(DIP_PINS[i]) == LOW ? 1 : 0;
    value |= (bitVal << i);
  }

  if (value == 0) value = 1;
  if (value > 50) value = 50;
  return value;
}

float getRamUsagePercent() {
  uint32_t totalHeap = ESP.getHeapSize();
  uint32_t freeHeap = ESP.getFreeHeap();
  if (totalHeap == 0) return 0.0f;
  return (100.0f * (float)(totalHeap - freeHeap)) / (float)totalHeap;
}

float getCpuTemperatureC() {
  // ESP32 Arduino core provides temperatureRead() on many boards.
  return temperatureRead();
}

void updateMotorTelemetry() {
  // TODO: Replace this simulation with real PD Stepper controller values.
  // Example behavior for demo:
  currentPositionMm += speedMmPerSec * 0.1f;
  if (currentPositionMm > 1000.0f) currentPositionMm = 0.0f;

  static float sign = 1.0f;
  speedMmPerSec += sign * 2.0f;
  if (speedMmPerSec > 150.0f) sign = -1.0f;
  if (speedMmPerSec < 10.0f) sign = 1.0f;

  hasError = false;
  errorText = "none";
}

String statusAsJson() {
  StaticJsonDocument<384> doc;
  doc["motor_id"] = motorId;
  doc["online"] = motorOnline;
  doc["position_mm"] = currentPositionMm;
  doc["speed_mm_s"] = speedMmPerSec;
  doc["ram_usage_percent"] = getRamUsagePercent();
  doc["cpu_temp_c"] = getCpuTemperatureC();
  doc["error"] = hasError;
  doc["error_text"] = errorText;
  doc["timestamp_ms"] = millis();

  String payload;
  serializeJson(doc, payload);
  return payload;
}

String getMqttTopic() {
  return "motors/" + String(motorId) + "/status";
}

void publishStatus() {
  if (!mqttClient.connected()) return;
  String topic = getMqttTopic();
  String payload = statusAsJson();
  mqttClient.publish(topic.c_str(), payload.c_str(), true);
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  // Optional command handling.
  String msg;
  for (unsigned int i = 0; i < length; i++) {
    msg += (char)payload[i];
  }

  // Example command topic: motors/<id>/cmd
  if (msg == "reset_error") {
    hasError = false;
    errorText = "none";
  }
}

bool connectMqtt() {
  String clientId = "esp32-motor-" + String(motorId) + "-" + String((uint32_t)ESP.getEfuseMac(), HEX);
  String willTopic = getMqttTopic();
  String willPayload = "{\"motor_id\":" + String(motorId) + ",\"online\":false}";

  bool connected = mqttClient.connect(clientId.c_str(), willTopic.c_str(), 1, true, willPayload.c_str());
  if (connected) {
    String cmdTopic = "motors/" + String(motorId) + "/cmd";
    mqttClient.subscribe(cmdTopic.c_str());
    publishStatus();
  }
  return connected;
}

void setupWiFiWithFallbackAP() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  const int maxAttempts = 3;
  int attempts = 0;

  while (WiFi.status() != WL_CONNECTED && attempts < maxAttempts) {
    attempts++;
    delay(3000);
  }

  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    apMode = false;
    return;
  }

  // Fallback Access Point mode
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  wifiConnected = false;
  apMode = true;
}

String htmlPage() {
  String html = "<!doctype html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>";
  html += "<title>ESP32 Motor Monitor</title><style>body{font-family:Arial;margin:20px;} .ok{color:green;} .warn{color:#c9a000;} .err{color:red;} code{background:#f0f0f0;padding:2px 4px;}</style></head><body>";
  html += "<h2>ESP32 Motor Monitor</h2>";
  html += "<p><b>Motor ID:</b> " + String(motorId) + "</p>";
  html += "<p><b>Mode:</b> " + String(apMode ? "Access Point (192.168.4.1)" : "WiFi Client") + "</p>";
  html += "<p><b>IP:</b> " + String(apMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString()) + "</p>";
  html += "<p><b>MQTT:</b> " + String(mqttClient.connected() ? "<span class='ok'>connected</span>" : "<span class='err'>disconnected</span>") + "</p>";
  html += "<p><b>Status topic:</b> <code>" + getMqttTopic() + "</code></p>";
  html += "<pre>" + statusAsJson() + "</pre>";
  html += "<p><a href='/json'>JSON API</a></p>";
  html += "<script>setTimeout(()=>location.reload()," + String(WEB_REFRESH_INTERVAL_MS) + ");</script>";
  html += "</body></html>";
  return html;
}

void setupWebServer() {
  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html", htmlPage());
  });

  server.on("/json", HTTP_GET, []() {
    server.send(200, "application/json", statusAsJson());
  });

  server.begin();
}

void setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  Serial.begin(115200);
  delay(500);

  motorId = readMotorIdFromDip();

  setupWiFiWithFallbackAP();

  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);

  setupWebServer();

  if (!apMode) {
    connectMqtt();
  }
}

void loop() {
  server.handleClient();

  if (!apMode) {
    if (!mqttClient.connected()) {
      unsigned long now = millis();
      if (now - lastMqttReconnectAttempt > 5000) {
        lastMqttReconnectAttempt = now;
        connectMqtt();
      }
    } else {
      mqttClient.loop();
    }
  }

  unsigned long now = millis();
  if (now - lastStatusPublish >= STATUS_PUBLISH_INTERVAL_MS) {
    lastStatusPublish = now;
    updateMotorTelemetry();
    publishStatus();

    // LED heartbeat
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
  }
}
