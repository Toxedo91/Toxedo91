/*
 * PD Stepper Webpage + MQTT Split Telemetry Dashboard 7.1
 * ------------------------------------------------------
 * Eigenschaften:
 * - Heim-WLAN mit 3 Versuchen
 * - AP-Fallback (192.168.4.1)
 * - Weboberfläche direkt im ESP (gleich in AP- und STA-Modus)
 * - MQTT mit getrennter Fast-/Slow-Telemetry für Dashboard und Systemwerte
 * - Node-RED Dashboard ist read-only
 * - mDNS / Hostname: http://pdstepper-<id>.local
 */

#include <Arduino.h>
#include <esp_dmx.h>
#include <Adafruit_MCP23X17.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <TMC2209.h>
#include <Preferences.h>
#include <Wire.h>
#include <esp_timer.h>
#include <PubSubClient.h>

// =====================================================
// =================== HTML DIREKT EINBAUEN ============
// =====================================================

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="de">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>PD Stepper ESP32 7.1</title>
<style>
body{
  font-family:Arial,sans-serif;
  background:#111827;
  color:#e5e7eb;
  margin:0;
  padding:20px;
}
.container{
  max-width:1000px;
  margin:auto;
}
.card{
  background:#1f2937;
  border-radius:12px;
  padding:20px;
  margin-bottom:20px;
}
h1{color:#fb923c;margin-top:0;}
h2{color:#f3f4f6;}
.row{
  display:flex;
  justify-content:space-between;
  align-items:center;
  padding:10px 0;
  border-bottom:1px solid rgba(255,255,255,0.05);
}
.label{color:#9ca3af;}
.value{
  font-weight:bold;
  display:flex;
  align-items:center;
  gap:8px;
  word-break:break-word;
}
button{
  background:#fb923c;
  border:none;
  color:white;
  padding:10px 14px;
  margin-right:10px;
  margin-top:10px;
  border-radius:8px;
  cursor:pointer;
}
button:hover{background:#ea580c;}
.badge{
  padding:4px 8px;
  border-radius:6px;
  font-size:12px;
}
.green{background:#16a34a;}
.yellow{background:#facc15;color:#000;}
.red{background:#dc2626;}
.gray{background:#6b7280;}
input, select{
  background:#111827;
  color:#fff;
  border:1px solid #374151;
  border-radius:8px;
  padding:8px;
}
.grid2{
  display:grid;
  grid-template-columns:1fr 1fr;
  gap:12px;
}
@media(max-width:800px){
  .row{
    flex-direction:column;
    align-items:flex-start;
    gap:6px;
  }
  .grid2{
    grid-template-columns:1fr;
  }
}
</style>
</head>
<body>
<div class="container">

  <div class="card">
    <h1>PD Stepper ESP32 7.1</h1>

    <div class="row"><div class="label">Motor ID / DMX Adresse</div><div class="value" id="motor_id">-</div></div>
    <div class="row"><div class="label">Hostname</div><div class="value" id="hostname">-</div></div>
    <div class="row"><div class="label">WLAN</div><div class="value" id="wifi">-</div></div>
    <div class="row"><div class="label">IP</div><div class="value" id="ip">-</div></div>
    <div class="row"><div class="label">MQTT</div><div class="value" id="mqtt">-</div></div>
    <div class="row"><div class="label">Power Good</div><div class="value" id="powergood">-</div></div>
    <div class="row"><div class="label">DMX Value</div><div class="value" id="dmx">-</div></div>
    <div class="row"><div class="label">Position</div><div class="value" id="position">-</div></div>
    <div class="row"><div class="label">Voltage</div><div class="value" id="voltage">-</div></div>
    <div class="row">
      <div class="label">CPU Temp</div>
      <div class="value">
        <span id="cpu_temp">-</span>
        <span id="temp_state" class="badge gray">OK</span>
      </div>
    </div>
    <div class="row">
      <div class="label">Driver Status</div>
      <div class="value">
        <span id="driver_status">-</span>
        <span id="driver_state" class="badge gray">OK</span>
      </div>
    </div>
    <div class="row"><div class="label">Motor Enabled</div><div class="value" id="motor_enabled">-</div></div>
    <div class="row"><div class="label">RSSI</div><div class="value" id="rssi">-</div></div>
  </div>

  <div class="card">
    <h2>Schnellbefehle lokal am ESP</h2>
    <button onclick="sendCmd('ENABLE=1')">Enable</button>
    <button onclick="sendCmd('ENABLE=0')">Disable</button>
    <button onclick="sendCmd('BUTTONRESET=1')">Stop / Reset</button>
    <button onclick="sendCmd('MODE=POSITION')">Position Mode</button>
    <button onclick="sendCmd('MODE=VELOCITY')">Velocity Mode</button>
  </div>

  <div class="card">
    <h2>Einstellungen</h2>
    <form action="/save" method="POST">
      <div class="grid2">
        <div>
          <label>Enabled</label><br>
          <input type="checkbox" name="enabled1" %ENABLED_CHECKED%>
        </div>

        <div>
          <label>Set Voltage</label><br>
          <select name="setvoltage">
            <option value="5" %SEL_V5%>5V</option>
            <option value="9" %SEL_V9%>9V</option>
            <option value="12" %SEL_V12%>12V</option>
            <option value="15" %SEL_V15%>15V</option>
            <option value="20" %SEL_V20%>20V</option>
          </select>
        </div>

        <div>
          <label>Microsteps</label><br>
          <input type="text" name="microsteps" value="%MICROSTEPS%">
        </div>

        <div>
          <label>Current</label><br>
          <input type="text" name="current" value="%CURRENT%">
        </div>

        <div>
          <label>Standstill Mode</label><br>
          <select name="standstill_mode">
            <option value="NORMAL" %SS_NORMAL%>NORMAL</option>
            <option value="FREEWHEELING" %SS_FREE%>FREEWHEELING</option>
            <option value="BRAKING" %SS_BRAKE%>BRAKING</option>
            <option value="STRONG_BRAKING" %SS_STRONGBRAKE%>STRONG_BRAKING</option>
          </select>
        </div>

        <div>
          <label>DMX Mode</label><br>
          <select name="DMX_mode">
            <option value="POSITION" %MODE_POSITION%>POSITION</option>
            <option value="VELOCITY" %MODE_VELOCITY%>VELOCITY</option>
          </select>
        </div>

        <div>
          <label>Max Speed</label><br>
          <input type="text" name="max_speed" value="%MAX_SPEED%">
        </div>

        <div>
          <label>Acceleration</label><br>
          <input type="text" name="acceleration" value="%ACCEL%">
        </div>

        <div>
          <label>Deceleration</label><br>
          <input type="text" name="deceleration" value="%DECEL%">
        </div>

        <div>
          <label>Init Position</label><br>
          <select name="init_pos">
            <option value="START" %INIT_START%>START</option>
            <option value="MIDDLE" %INIT_MIDDLE%>MIDDLE</option>
            <option value="END" %INIT_END%>END</option>
          </select>
        </div>

        <div>
          <label>Position Angle</label><br>
          <input type="text" name="pos_angle" value="%POS_ANGLE%">
        </div>

        <div>
          <label>Steps per Rev</label><br>
          <input type="text" name="steps_per_rev" value="%STEPS_PER_REV%">
        </div>
      </div>

      <br>
      <button type="submit">Speichern</button>
    </form>
  </div>

</div>

<script>
async function loadStatus(){
  try{
    const r = await fetch('/api/status');
    const s = await r.json();

    document.getElementById("motor_id").innerHTML = s.motor_id;
    document.getElementById("hostname").innerHTML = (s.hostname || "-") + ((s.hostname && s.hostname !== "-") ? ".local" : "");
    document.getElementById("wifi").innerHTML = s.wifi;
    document.getElementById("ip").innerHTML = s.ip;
    document.getElementById("mqtt").innerHTML = s.mqtt;
    document.getElementById("powergood").innerHTML = s.powergood;
    document.getElementById("dmx").innerHTML = s.dmx_value;
    document.getElementById("position").innerHTML = s.position;
    document.getElementById("voltage").innerHTML = s.voltage;
    document.getElementById("cpu_temp").innerHTML = s.cpu_temp;
    document.getElementById("driver_status").innerHTML = s.driver_status;
    document.getElementById("motor_enabled").innerHTML = s.motor_enabled;
    document.getElementById("rssi").innerHTML = s.rssi;

    let t = parseFloat(String(s.cpu_temp || "").replace("°C","").replace(",","."));
    let tempBadge = document.getElementById("temp_state");

    if (isNaN(t)) {
      tempBadge.innerHTML = "N/A";
      tempBadge.className = "badge gray";
    } else if (t < 60) {
      tempBadge.innerHTML = "OK";
      tempBadge.className = "badge green";
    } else if (t < 75) {
      tempBadge.innerHTML = "WARNING";
      tempBadge.className = "badge yellow";
    } else {
      tempBadge.innerHTML = "CRITICAL";
      tempBadge.className = "badge red";
    }

    let ds = String(s.driver_status || "").toLowerCase();
    let driverBadge = document.getElementById("driver_state");

    if (ds.includes("shutdown") || ds.includes("critical")) {
      driverBadge.innerHTML = "CRITICAL";
      driverBadge.className = "badge red";
    } else if (ds.includes("warning")) {
      driverBadge.innerHTML = "WARNING";
      driverBadge.className = "badge yellow";
    } else if (ds.includes("disabled")) {
      driverBadge.innerHTML = "DISABLED";
      driverBadge.className = "badge gray";
    } else {
      driverBadge.innerHTML = "OK";
      driverBadge.className = "badge green";
    }
  } catch(e){}
}

async function sendCmd(cmd){
  await fetch('/api/cmd?value=' + encodeURIComponent(cmd));
}

setInterval(loadStatus,1000);
loadStatus();
</script>
</body>
</html>
)rawliteral";

// =====================================================
// =================== DMX SETUP ========================
// =====================================================

int transmitPin = 14;
int receivePin = 13;
int enablePin = -1;

dmx_port_t dmxPort = 1;
byte data[DMX_PACKET_SIZE];
int DMXValue = 0;

bool dmxIsConnected = false;
unsigned long lastUpdate = millis();

#define DMXLED1 13
#define DMXLED2 14

Adafruit_MCP23X17 mcp;
const uint8_t NUM_DIP_SWITCHES = 9;
const uint8_t dipPins[NUM_DIP_SWITCHES] = {6, 5, 4, 3, 2, 1, 0, 8, 9};

uint16_t dmxAddress = 0;

// =====================================================
// =================== NET / WEB / MQTT ================
// =====================================================

AsyncWebServer server(80);
WiFiClient espClient;
PubSubClient mqttClient(espClient);
Preferences preferences;

#include "secrets.h"

// MQTT
const char* mqttServer = "192.168.178.35";
const uint16_t mqttPort = 1883;

// AP
String ssid = "PD Stepper DMX";
const char *password = "";

// feste IP Basis
IPAddress ipBase(192, 168, 178, 100);
IPAddress gateway(192, 168, 178, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress dns1(192, 168, 178, 1);
IPAddress dns2(8, 8, 8, 8);

bool apModeActive = false;
unsigned long lastMqttReconnectTryMs = 0;
unsigned long lastStatusSendMs = 0;
unsigned long lastTelemetryFastSendMs = 0;
unsigned long lastTelemetrySlowSendMs = 0;
unsigned long lastHeartbeatMs = 0;
unsigned long lastDiscoverySendMs = 0;

const unsigned long statusSendIntervalMs         = 3000;
const unsigned long telemetryFastIntervalMs      = 50;    // schnelle Updates für DMX / Position während Bewegung
const unsigned long telemetryIdleIntervalMs      = 250;   // ruhiger Betrieb im Stand
const unsigned long telemetryFastRefreshMs       = 1000;  // spätestens nach 1s erneut schnelles Update
const unsigned long telemetrySlowIntervalMs      = 1000;  // langsame System-Telemetrie
const unsigned long heartbeatIntervalMs          = 2000;
const unsigned long mqttReconnectIntervalMs      = 3000;
const unsigned long discoverySendIntervalMs      = 30000;

// MQTT topics
String topicBase;
String topicStatus;
String topicOnline;
String topicTelemetry;
String topicTelemetryFast;
String topicHeartbeat;
String topicDiscovery;

// =====================================================
// =================== DRIVER / PINS ====================
// =====================================================

TMC2209 stepper_driver;
HardwareSerial & serial_stream = Serial2;
const long SERIAL_BAUD_RATE = 115200;
const uint8_t RUN_CURRENT_PERCENT = 100;

#define TMC_EN  21
#define STEP    5
#define DIR     6
#define MS1     1
#define MS2     2
#define SPREAD  7
#define TMC_TX  17
#define TMC_RX  18
#define DIAG    16
#define INDEX   11

static const uint32_t STEP_MASK = (1UL << STEP);
static const uint32_t DIR_MASK  = (1UL << DIR);

#define PG      15
#define CFG1    38
#define CFG2    48
#define CFG3    47

#define VBUS    4
#define NTC     7
#define LED1    10
#define LED2    12
#define SW1     35
#define SW2     36
#define SW3     37
#define AUX1    14
#define AUX2    13

// =====================================================
// =================== GLOBALS ==========================
// =====================================================

int set_speed = 0;
bool PGState = 0;
bool enabledState = 0;

#define AS5600_ADDRESS 0x36
signed long total_encoder_counts = 0;

unsigned long lastEncRead = 0;
int mainFreq = 50;

unsigned long lastDMXRead = 0;
int DMXFreq = 100;

bool incButtonState = HIGH;
bool decButtonState = HIGH;
bool resetButtonState = HIGH;
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50;

int buttonSpeed = 0;

float VBusVoltage = 0;
float VREF = 3.3;
const float DIV_RATIO = 0.1189427313;

// Cached values
String cachedPGState = "Loading...";
String cachedVoltage = "Loading...";
String cachedPosition = "Loading...";
String cachedCPUTemp = "Loading...";
String cachedTMCStatus = "Loading...";
int lastPublishedDmxValue = -1;
String lastPublishedPosition = "";
unsigned long lastStatusUpdate = 0;
const int STATUS_UPDATE_INTERVAL = 500;

// Settings
String enabled1 = "enabled";
String setVoltage = "20";
String microsteps = "16";
String current = "30";
String standstillMode = "NORMAL";
String DMXMode = "POSITION";
String maxSpeed = "1000";
String acceleration = "720";
String deceleration = "720";
String initPos = "START";
String posAngle = "1080";
String stepsPerRev = "200";

// Q16 Motion
static const int32_t Q = 16;
static const int32_t ONE_Q = 1 << Q;
static const int64_t ONE_Q64 = (int64_t)ONE_Q;

struct MotionState {
    volatile int32_t currentSpeed_q;
    volatile int32_t targetSpeed_q;
    volatile int32_t accelSteps_q;
    volatile int32_t decelSteps_q;
    volatile int64_t targetPos;
    volatile bool isRunning;
};

volatile MotionState motion = {0,0,0,0,0,false};

bool posControl = 1;

volatile int64_t isr_currentPos = 0;
volatile uint32_t isr_stepDelay_us = 0;
volatile bool isr_stepToggle = false;
volatile bool isr_dirState = false;
volatile int32_t isr_deadband = 2;

static esp_timer_handle_t stepTimer = nullptr;
static esp_timer_handle_t motionTimer = nullptr;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

// =====================================================
// =================== FORWARD DECL =====================
// =====================================================

void IRAM_ATTR onStepTimer(void* arg);
void IRAM_ATTR onMotionTimer(void* arg);
void recalcMotionParams();

void readDMX();
void readEncoder();
void configureSettings();
void readSettings();
void writeSettings();
uint16_t readDMXAddress();
void updateCachedStatus();

bool connectHomeWifiOnce();
void startFallbackAP();
void setupWebServer();
void setupMQTT();
bool connectMqtt();
void publishOnlineState(bool online);
void publishStatus();
void publishTelemetry();
void publishFastTelemetry();
void publishHeartbeat();
void publishDiscovery();

bool isMotionActive();
bool telemetryHasImportantChange();
String buildStatusJson();
String buildTelemetryJson();
String buildFastTelemetryJson();
String getWifiStateText();
String getMqttStateText();
String getIpText();
String ipToString(IPAddress ip);
IPAddress buildStaticIpFromMotorId(uint16_t id);
void buildTopics();
void applyCommand(const String& cmd);
String getDeviceHostname();
void startMdnsIfPossible();

// =====================================================
// =================== STATUS HELPERS ===================
// =====================================================

String readPGState(){ return cachedPGState; }
String readVoltage(){ return cachedVoltage; }
String readEncoderPos(){ return cachedPosition; }
String readCPUTemp(){ return cachedCPUTemp; }
String readTMCStatus(){ return cachedTMCStatus; }

bool isMotionActive() {
  bool running = false;
  int32_t speed_q = 0;
  portENTER_CRITICAL(&timerMux);
  running = motion.isRunning;
  speed_q = motion.currentSpeed_q;
  portEXIT_CRITICAL(&timerMux);
  return running || (speed_q != 0);
}

bool telemetryHasImportantChange() {
  return (DMXValue != lastPublishedDmxValue) || (cachedPosition != lastPublishedPosition);
}

void updateCachedStatus(){
  PGState = digitalRead(PG);
  if (PGState == LOW) cachedPGState = "Power Good";
  else cachedPGState = "Power Bad";

  int ADCValue = analogRead(VBUS);
  VBusVoltage = ADCValue * (VREF / 4096.0) / DIV_RATIO;
  cachedVoltage = String(VBusVoltage, 1) + "V";

  int64_t pos;
  portENTER_CRITICAL(&timerMux);
  pos = isr_currentPos;
  portEXIT_CRITICAL(&timerMux);

  float posDeg = 0.0f;
  int spr = stepsPerRev.toInt();
  int usteps = microsteps.toInt();
  if (spr > 0 && usteps > 0) {
    posDeg = (float)pos * 360.0f / (spr * usteps);
  }
  cachedPosition = String(posDeg, 2) + "°";

  cachedCPUTemp = String(temperatureRead(), 1) + "°C";

  bool hardware_disabled = stepper_driver.hardwareDisabled();
  if (hardware_disabled){
    cachedTMCStatus = "Hardware Disabled";
  } else {
    TMC2209::Status status = stepper_driver.getStatus();
    if (status.over_temperature_shutdown){
      cachedTMCStatus = "Over Temp Shutdown";
    } else if (status.over_temperature_warning){
      cachedTMCStatus = "Over Temp Warning";
    } else {
      cachedTMCStatus = "No Errors";
    }
  }
}

// =====================================================
// =================== HTML PROCESSOR ===================
// =====================================================

String processor(const String& var)
{
  if (var == "ENABLED_CHECKED") return (enabled1 == "enabled") ? "checked" : "";

  if (var == "MICROSTEPS") return microsteps;
  if (var == "CURRENT") return current;
  if (var == "MAX_SPEED") return maxSpeed;
  if (var == "ACCEL") return acceleration;
  if (var == "DECEL") return deceleration;
  if (var == "POS_ANGLE") return posAngle;
  if (var == "STEPS_PER_REV") return stepsPerRev;

  if (var == "SEL_V5")  return (setVoltage == "5")  ? "selected" : "";
  if (var == "SEL_V9")  return (setVoltage == "9")  ? "selected" : "";
  if (var == "SEL_V12") return (setVoltage == "12") ? "selected" : "";
  if (var == "SEL_V15") return (setVoltage == "15") ? "selected" : "";
  if (var == "SEL_V20") return (setVoltage == "20") ? "selected" : "";

  if (var == "SS_NORMAL")       return (standstillMode == "NORMAL") ? "selected" : "";
  if (var == "SS_FREE")         return (standstillMode == "FREEWHEELING") ? "selected" : "";
  if (var == "SS_BRAKE")        return (standstillMode == "BRAKING") ? "selected" : "";
  if (var == "SS_STRONGBRAKE")  return (standstillMode == "STRONG_BRAKING") ? "selected" : "";

  if (var == "MODE_POSITION") return (DMXMode == "POSITION") ? "selected" : "";
  if (var == "MODE_VELOCITY") return (DMXMode == "VELOCITY") ? "selected" : "";

  if (var == "INIT_START")  return (initPos == "START") ? "selected" : "";
  if (var == "INIT_MIDDLE") return (initPos == "MIDDLE") ? "selected" : "";
  if (var == "INIT_END")    return (initPos == "END") ? "selected" : "";

  return "";
}

// =====================================================
// =================== NETWORK HELPERS ==================
// =====================================================

String ipToString(IPAddress ip) {
  return String(ip[0]) + "." + String(ip[1]) + "." + String(ip[2]) + "." + String(ip[3]);
}

IPAddress buildStaticIpFromMotorId(uint16_t id) {
  uint16_t safeId = (id == 0) ? 1 : id;
  uint16_t host = ipBase[3] + safeId;
  if (host > 254) host = 254;
  return IPAddress(ipBase[0], ipBase[1], ipBase[2], host);
}

String getDeviceHostname() {
  uint16_t id = dmxAddress > 0 ? dmxAddress : 1;
  return "pdstepper-" + String(id);
}

void buildTopics() {
  uint16_t id = dmxAddress > 0 ? dmxAddress : 1;
  topicBase      = "pdstepper/" + String(id);
  topicStatus    = topicBase + "/status";
  topicOnline    = topicBase + "/online";
  topicTelemetry = topicBase + "/telemetry";
  topicTelemetryFast = topicBase + "/telemetry_fast";
  topicHeartbeat = topicBase + "/heartbeat";
  topicDiscovery = "pdstepper/discovery/" + String(id);
}

String getWifiStateText() {
  if (apModeActive) return "AP Modus";
  if (WiFi.status() == WL_CONNECTED) return wlanSsid;
  return "Nicht verbunden";
}

String getMqttStateText() {
  return mqttClient.connected() ? "Verbunden" : "Nicht verbunden";
}

String getIpText() {
  return apModeActive ? ipToString(WiFi.softAPIP()) : ipToString(WiFi.localIP());
}

void startMdnsIfPossible() {
  if (WiFi.status() != WL_CONNECTED) return;

  String hostname = getDeviceHostname();
  if (MDNS.begin(hostname.c_str())) {
    Serial.print("mDNS gestartet: http://");
    Serial.print(hostname);
    Serial.println(".local");
  } else {
    Serial.println("mDNS Start fehlgeschlagen");
  }
}

// =====================================================
// =================== JSON BUILD =======================
// =====================================================

String buildStatusJson() {
  updateCachedStatus();

  String json = "{";
  json += "\"motor_id\":" + String(dmxAddress) + ",";
  json += "\"id\":" + String(dmxAddress) + ",";
  json += "\"hostname\":\"" + getDeviceHostname() + "\",";
  json += "\"name_url\":\"http://" + getDeviceHostname() + ".local\",";
  json += "\"ip_url\":\"http://" + getIpText() + "\",";
  json += "\"wifi\":\"" + getWifiStateText() + "\",";
  json += "\"mqtt\":\"" + getMqttStateText() + "\",";
  json += "\"ip\":\"" + getIpText() + "\",";
  json += "\"powergood\":\"" + cachedPGState + "\",";
  json += "\"dmx_value\":" + String(DMXValue) + ",";
  json += "\"position\":\"" + cachedPosition + "\",";
  json += "\"voltage\":\"" + cachedVoltage + "\",";
  json += "\"cpu_temp\":\"" + cachedCPUTemp + "\",";
  json += "\"driver_status\":\"" + cachedTMCStatus + "\",";
  json += "\"motor_enabled\":" + String(enabledState ? "true" : "false") + ",";
  json += "\"rssi\":" + String(WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0);
  json += "}";
  return json;
}

String buildFastTelemetryJson() {
  updateCachedStatus();

  String json = "{";
  json += "\"motor_id\":" + String(dmxAddress) + ",";
  json += "\"id\":" + String(dmxAddress) + ",";
  json += "\"id\":" + String(dmxAddress) + ",";
  json += "\"hostname\":\"" + getDeviceHostname() + "\",";
  json += "\"ip\":\"" + getIpText() + "\",";
  json += "\"online\":true,";
  json += "\"dmx_value\":" + String(DMXValue) + ",";
  json += "\"position\":\"" + cachedPosition + "\",";
  json += "\"motion_active\":" + String(isMotionActive() ? "true" : "false") + ",";
  json += "\"source\":\"mqtt-telemetry-fast\",";
  json += "\"source\":\"mqtt-telemetry\",";
  json += "\"uptime_ms\":" + String(millis());
  json += "}";
  return json;
}

String buildTelemetryJson() {
  updateCachedStatus();

  String json = "{";
  json += "\"motor_id\":" + String(dmxAddress) + ",";
  json += "\"hostname\":\"" + getDeviceHostname() + "\",";
  json += "\"name_url\":\"http://" + getDeviceHostname() + ".local\",";
  json += "\"ip_url\":\"http://" + getIpText() + "\",";
  json += "\"ip\":\"" + getIpText() + "\",";
  json += "\"online\":true,";
  json += "\"dmx_value\":" + String(DMXValue) + ",";
  json += "\"position\":\"" + cachedPosition + "\",";
  json += "\"powergood\":\"" + cachedPGState + "\",";
  json += "\"voltage\":\"" + cachedVoltage + "\",";
  json += "\"cpu_temp\":\"" + cachedCPUTemp + "\",";
  json += "\"driver_status\":\"" + cachedTMCStatus + "\",";
  json += "\"motor_enabled\":" + String(enabledState ? "true" : "false") + ",";
  json += "\"rssi\":" + String(WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0) + ",";
  json += "\"uptime_ms\":" + String(millis());
  json += "}";
  return json;
}

// =====================================================
// =================== MQTT =============================
// =====================================================

bool connectMqtt() {
  if (WiFi.status() != WL_CONNECTED) return false;
  if (mqttClient.connected()) return true;

  uint16_t id = dmxAddress > 0 ? dmxAddress : 1;
  String clientId = "pdstepper-" + String(id);

  Serial.print("MQTT verbinde zu ");
  Serial.print(mqttServer);
  Serial.print(":");
  Serial.println(mqttPort);

  bool ok = mqttClient.connect(clientId.c_str(), topicOnline.c_str(), 0, true, "false");

  if (ok) {
    Serial.println("MQTT verbunden");
    publishOnlineState(true);
    publishDiscovery();
    publishStatus();
    publishTelemetry();
    publishFastTelemetry();
    publishFastTelemetry();
    return true;
  }

  Serial.print("MQTT Fehler rc=");
  Serial.println(mqttClient.state());
  return false;
}

void publishOnlineState(bool online) {
  if (!mqttClient.connected()) return;
  mqttClient.publish(topicOnline.c_str(), online ? "true" : "false", true);
}

void publishStatus() {
  if (!mqttClient.connected()) return;
  String json = buildStatusJson();
  mqttClient.publish(topicStatus.c_str(), json.c_str(), false);
}

void publishTelemetry() {
  if (!mqttClient.connected()) return;
  String json = buildTelemetryJson();
  mqttClient.publish(topicTelemetry.c_str(), json.c_str(), false);
}

void publishFastTelemetry() {
  if (!mqttClient.connected()) return;
  String json = buildFastTelemetryJson();
  mqttClient.publish(topicTelemetryFast.c_str(), json.c_str(), false);
  lastPublishedDmxValue = DMXValue;
  lastPublishedPosition = cachedPosition;
}

void publishHeartbeat() {
  if (!mqttClient.connected()) return;
  String hb = String(millis());
  mqttClient.publish(topicHeartbeat.c_str(), hb.c_str(), false);
}

void publishDiscovery() {
  if (!mqttClient.connected()) return;

  uint16_t id = dmxAddress > 0 ? dmxAddress : 1;
  String json = "{";
  json += "\"motor_id\":" + String(id) + ",";
  json += "\"hostname\":\"" + getDeviceHostname() + "\",";
  json += "\"name_url\":\"http://" + getDeviceHostname() + ".local\",";
  json += "\"ip\":\"" + getIpText() + "\",";
  json += "\"ip_url\":\"http://" + getIpText() + "\",";
  json += "\"web_mode\":\"" + String(apModeActive ? "AP" : "STA") + "\",";
  json += "\"source\":\"mqtt-discovery\"";
  json += "}";

  mqttClient.publish(topicDiscovery.c_str(), json.c_str(), true);
}

void setupMQTT() {
  mqttClient.setServer(mqttServer, mqttPort);
}

// =====================================================
// =================== WIFI =============================
// =====================================================

bool connectHomeWifiOnce() {
  Serial.println("Prüfe, ob Heim-WLAN wieder verfügbar ist...");
  Serial.println("Verbinde mit Heim-WLAN...");

  WiFi.mode(WIFI_STA);

  uint16_t id = dmxAddress > 0 ? dmxAddress : 1;
  // IPAddress localIp = buildStaticIpFromMotorId(id);
  // WiFi.config(localIp, gateway, subnet, dns1, dns2);

  String hostname = getDeviceHostname();
  WiFi.setHostname(hostname.c_str());

  for (int attempt = 1; attempt <= 3; attempt++) {
    Serial.print("WLAN-Versuch ");
    Serial.print(attempt);
    Serial.println(" von 3");

    WiFi.begin(wlanSsid, wlanPassword);

    unsigned long startMs = millis();
    while (millis() - startMs < 5000) {
      if (WiFi.status() == WL_CONNECTED) {
        Serial.println();
        Serial.println("Heim-WLAN verbunden");
        Serial.print("ESP-IP: ");
        Serial.println(WiFi.localIP());
        Serial.print("Hostname: ");
        Serial.println(hostname);
        apModeActive = false;
        return true;
      }
      delay(500);
      Serial.print(".");
    }

    Serial.println();
    Serial.println("Verbindungsversuch fehlgeschlagen");
    WiFi.disconnect(true, true);
    delay(300);
  }

  Serial.println("Heim-WLAN nach 3 Versuchen nicht erreichbar");
  return false;
}

void startFallbackAP() {
  WiFi.mode(WIFI_AP);

  uint16_t id = dmxAddress > 0 ? dmxAddress : 1;
  String apName = ssid + " (" + String(id) + ")";
  WiFi.softAP(apName.c_str(), password);

  apModeActive = true;

  Serial.println("Fallback-Hotspot gestartet");
  Serial.print("SSID: ");
  Serial.println(apName);
  Serial.print("AP-IP: ");
  Serial.println(WiFi.softAPIP());
}

// =====================================================
// =================== WEB ROUTES =======================
// =====================================================

void setupWebServer() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", index_html, processor);
  });

  server.on("/powergood", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", readPGState());
  });

  server.on("/voltage", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", readVoltage());
  });

  server.on("/position", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", readEncoderPos());
  });

  server.on("/cputemp", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", readCPUTemp());
  });

  server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", readTMCStatus());
  });

  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "application/json", buildStatusJson());
  });

  server.on("/api/cmd", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasArg("value")) {
      applyCommand(request->arg("value"));
      request->send(200, "text/plain", "OK");
    } else {
      request->send(400, "text/plain", "Missing value");
    }
  });

  server.on("/save", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("enabled1", true)) enabled1 = "enabled";
    else enabled1 = "disabled";

    if (request->hasParam("setvoltage", true)) {
      setVoltage = request->getParam("setvoltage", true)->value();
    }

    if (request->hasParam("microsteps", true)) {
      microsteps = request->getParam("microsteps", true)->value();
    }

    if (request->hasParam("current", true)) {
      current = request->getParam("current", true)->value();
    }

    if (request->hasParam("standstill_mode", true)) {
      standstillMode = request->getParam("standstill_mode", true)->value();
    }

    if (request->hasParam("DMX_mode", true)) {
      DMXMode = request->getParam("DMX_mode", true)->value();
      posControl = (DMXMode == "POSITION");
    }

    if (request->hasParam("max_speed", true)) {
      maxSpeed = request->getParam("max_speed", true)->value();
    }

    if (request->hasParam("acceleration", true)) {
      acceleration = request->getParam("acceleration", true)->value();
    }

    if (request->hasParam("deceleration", true)) {
      deceleration = request->getParam("deceleration", true)->value();
    }

    if (request->hasParam("init_pos", true)) {
      initPos = request->getParam("init_pos", true)->value();
    }

    if (request->hasParam("pos_angle", true)) {
      posAngle = request->getParam("pos_angle", true)->value();
    }

    if (request->hasParam("steps_per_rev", true)) {
      stepsPerRev = request->getParam("steps_per_rev", true)->value();
    }

    portENTER_CRITICAL(&timerMux);
    motion.targetSpeed_q = 0;
    motion.currentSpeed_q = 0;
    isr_stepDelay_us = 0;
    portEXIT_CRITICAL(&timerMux);

    configureSettings();
    recalcMotionParams();
    writeSettings();

    request->redirect("/");
  });

  server.begin();
}

// =====================================================
// =================== SETTINGS =========================
// =====================================================

void configureSettings(){

  if (setVoltage == "5"){
      digitalWrite(CFG1, HIGH);
      digitalWrite(CFG2, LOW);
      digitalWrite(CFG3, LOW);
  } else if (setVoltage == "9"){
      digitalWrite(CFG1, LOW);
      digitalWrite(CFG2, LOW);
      digitalWrite(CFG3, LOW);
  } else if (setVoltage == "12"){
      digitalWrite(CFG1, LOW);
      digitalWrite(CFG2, LOW);
      digitalWrite(CFG3, HIGH);
  } else if (setVoltage == "15"){
      digitalWrite(CFG1, LOW);
      digitalWrite(CFG2, HIGH);
      digitalWrite(CFG3, HIGH);
  } else if (setVoltage == "20"){
      digitalWrite(CFG1, LOW);
      digitalWrite(CFG2, HIGH);
      digitalWrite(CFG3, LOW);
  }

  stepper_driver.setRunCurrent(current.toInt());
  stepper_driver.setMicrostepsPerStep(microsteps.toInt());

  if (standstillMode == "NORMAL") stepper_driver.setStandstillMode(stepper_driver.NORMAL);
  else if (standstillMode == "FREEWHEELING") stepper_driver.setStandstillMode(stepper_driver.FREEWHEELING);
  else if (standstillMode == "BRAKING") stepper_driver.setStandstillMode(stepper_driver.BRAKING);
  else if (standstillMode == "STRONG_BRAKING") stepper_driver.setStandstillMode(stepper_driver.STRONG_BRAKING);
}

uint16_t readDMXAddress() {
  uint16_t address = 0;
  for (uint8_t i = 0; i < NUM_DIP_SWITCHES; i++) {
    if (mcp.digitalRead(dipPins[i]) == LOW) {
      address |= (1 << i);
    }
  }
  return address;
}

void readSettings(){ 
  preferences.begin("settings", false);
  enabled1 = preferences.getString("enable", ""); 

  if (enabled1 == ""){
    preferences.end();
    enabled1 = "enabled";
    setVoltage = "20";
    microsteps = "16";
    current = "30";
    standstillMode = "NORMAL";
    DMXMode = "POSITION";
    maxSpeed = "1000";
    acceleration = "720";
    deceleration = "720";
    initPos = "START";
    posAngle = "1080";
    stepsPerRev = "200";
    writeSettings();
  } else {
    setVoltage = preferences.getString("voltage", "20");
    microsteps = preferences.getString("microsteps", "16");
    current = preferences.getString("current", "30");
    standstillMode = preferences.getString("standstillMode", "NORMAL");
    DMXMode = preferences.getString("DMXMode", "POSITION");
    maxSpeed = preferences.getString("maxSpeed", "1000");
    acceleration = preferences.getString("acceleration", "720");
    deceleration = preferences.getString("deceleration", "720");
    initPos = preferences.getString("initPos", "START");
    posAngle = preferences.getString("posAngle", "1080");
    stepsPerRev = preferences.getString("stepsPerRev", "200");
    preferences.end();
  }
}

void writeSettings(){
  preferences.begin("settings", false);
  preferences.putString("enable", enabled1);
  preferences.putString("voltage", setVoltage);
  preferences.putString("microsteps", microsteps);
  preferences.putString("current", current);
  preferences.putString("standstillMode", standstillMode);
  preferences.putString("DMXMode", DMXMode);
  preferences.putString("maxSpeed", maxSpeed);
  preferences.putString("acceleration", acceleration);
  preferences.putString("deceleration", deceleration);
  preferences.putString("initPos", initPos);
  preferences.putString("posAngle", posAngle);
  preferences.putString("stepsPerRev", stepsPerRev);
  preferences.end();

  configureSettings();
}

// =====================================================
// =================== LOKALE WEB-CMDS ==================
// =====================================================

void applyCommand(const String& cmd) {
  Serial.print("Lokaler Web-CMD empfangen: ");
  Serial.println(cmd);

  bool settingsChanged = false;

  if (cmd == "ENABLE=1") {
    enabled1 = "enabled";
    settingsChanged = true;
  }
  else if (cmd == "ENABLE=0") {
    enabled1 = "disabled";
    settingsChanged = true;
  }
  else if (cmd == "BUTTONRESET=1") {
    buttonSpeed = 0;
    portENTER_CRITICAL(&timerMux);
    motion.targetSpeed_q = 0;
    motion.currentSpeed_q = 0;
    isr_stepDelay_us = 0;
    motion.targetPos = 0;
    isr_currentPos = 0;
    portEXIT_CRITICAL(&timerMux);
  }
  else if (cmd == "MODE=POSITION") {
    DMXMode = "POSITION";
    posControl = true;
    settingsChanged = true;
  }
  else if (cmd == "MODE=VELOCITY") {
    DMXMode = "VELOCITY";
    posControl = false;
    settingsChanged = true;
  }
  else if (cmd.startsWith("MAXSPEED=")) {
    maxSpeed = cmd.substring(String("MAXSPEED=").length());
    recalcMotionParams();
    settingsChanged = true;
  }
  else if (cmd.startsWith("ACCEL=")) {
    acceleration = cmd.substring(String("ACCEL=").length());
    recalcMotionParams();
    settingsChanged = true;
  }
  else if (cmd.startsWith("DECEL=")) {
    deceleration = cmd.substring(String("DECEL=").length());
    recalcMotionParams();
    settingsChanged = true;
  }
  else if (cmd.startsWith("SETVOLTAGE=")) {
    setVoltage = cmd.substring(String("SETVOLTAGE=").length());
    configureSettings();
    settingsChanged = true;
  }
  else if (cmd.startsWith("MICROSTEPS=")) {
    microsteps = cmd.substring(String("MICROSTEPS=").length());
    configureSettings();
    recalcMotionParams();
    settingsChanged = true;
  }
  else if (cmd.startsWith("CURRENT=")) {
    current = cmd.substring(String("CURRENT=").length());
    configureSettings();
    settingsChanged = true;
  }
  else if (cmd.startsWith("STANDSTILL=")) {
    standstillMode = cmd.substring(String("STANDSTILL=").length());
    configureSettings();
    settingsChanged = true;
  }
  else if (cmd.startsWith("STEPSPERREV=")) {
    stepsPerRev = cmd.substring(String("STEPSPERREV=").length());
    recalcMotionParams();
    settingsChanged = true;
  }

  if (settingsChanged) {
    writeSettings();
  }

  if (mqttClient.connected()) {
    publishStatus();
    publishTelemetry();
    publishFastTelemetry();
  }
}

// =====================================================
// =================== DMX / ENCODER ====================
// =====================================================

void readDMX(){
    DMXValue = dmx_read_slot(dmxPort, dmxAddress);
    // Serial.println(DMXValue);

    if (DMXMode == "POSITION"){
      posControl = 1;

      float total_angle = posAngle.toFloat();
      int steps_per_rev = stepsPerRev.toInt();
      int usteps = microsteps.toInt();

      long end_point = (long)((total_angle / 360.0f) * steps_per_rev * usteps);
      int64_t targetPos = map(DMXValue, 0, 255, 0, end_point);

      if (initPos == "MIDDLE"){
        targetPos = targetPos - (end_point / 2);
      } else if (initPos == "END"){
        targetPos = targetPos - end_point;
      }

      portENTER_CRITICAL(&timerMux);
      motion.targetPos = targetPos;
      portEXIT_CRITICAL(&timerMux);

    } else if (DMXMode == "VELOCITY"){
      posControl = 0;

      float velocitySetpoint = 0.0;
      if (DMXValue < 124) {
        velocitySetpoint = ((DMXValue - 127.0) / 127.0) * maxSpeed.toFloat();
      } else if (DMXValue > 130) {
        velocitySetpoint = ((DMXValue - 127.0) / 128.0) * maxSpeed.toFloat();
      }

      float velSteps = (velocitySetpoint / 360.0f) * stepsPerRev.toInt() * microsteps.toInt();
      int32_t velSteps_q = (int32_t)roundf(velSteps * (float)ONE_Q);

      portENTER_CRITICAL(&timerMux);
      motion.targetSpeed_q = velSteps_q;
      portEXIT_CRITICAL(&timerMux);
    }
}

void readEncoder(){
  int raw_counts = 0;
  static int prev_raw_counts = 0;
  static signed long revolutions = 0;

  Wire.beginTransmission(AS5600_ADDRESS);
  Wire.write(0x0C);
  if (Wire.endTransmission(false) != 0) return;

  if (Wire.requestFrom(AS5600_ADDRESS, 2) < 2) return;

  raw_counts = (Wire.read() << 8) | Wire.read();

  if (prev_raw_counts > 3000 && raw_counts < 1000) {
    revolutions++;
  } else if (prev_raw_counts < 1000 && raw_counts > 3000) {
    revolutions--;
  }

  prev_raw_counts = raw_counts;
  total_encoder_counts = raw_counts + (4096L * revolutions);
}

// =====================================================
// =================== MOTION TIMERS ====================
// =====================================================

void IRAM_ATTR onMotionTimer(void* arg) {
    int32_t v_q;
    int32_t accel_q;
    int32_t decel_q;
    int32_t targetSpeed_q;
    int32_t local_maxSpeed_steps_q;
    int64_t targetPos_local;
    bool local_posControl;
    int32_t local_deadband;
    int usteps = microsteps.toInt();
    int revs = stepsPerRev.toInt();

    portENTER_CRITICAL_ISR(&timerMux);
    v_q = motion.currentSpeed_q;
    accel_q = motion.accelSteps_q;
    decel_q = motion.decelSteps_q;
    targetSpeed_q = motion.targetSpeed_q;
    targetPos_local = motion.targetPos;
    local_posControl = posControl;
    local_deadband = isr_deadband;
    float maxV_steps = (maxSpeed.toFloat() / 360.0f) * (float)revs * (float)usteps;
    local_maxSpeed_steps_q = (int32_t)roundf(maxV_steps * (float)ONE_Q);
    portEXIT_CRITICAL_ISR(&timerMux);

    int32_t targetV_q = 0;

    if (local_posControl) {
        int64_t error = targetPos_local - isr_currentPos;
        int64_t absErr64 = llabs(error);

        if (absErr64 <= local_deadband) {
            targetV_q = 0;
        } else {
            int64_t v2 = (int64_t)v_q * (int64_t)v_q;
            int64_t denom_q = ((int64_t)decel_q << 1);
            int64_t stopDist = 0;
            if (denom_q != 0) {
                stopDist = v2 / (denom_q * ONE_Q);
            }

            if (error > 0) {
                if (absErr64 > stopDist) targetV_q = local_maxSpeed_steps_q;
                else targetV_q = 0;
            } else {
                if (absErr64 > stopDist) targetV_q = -local_maxSpeed_steps_q;
                else targetV_q = 0;
            }
        }
    } else {
        targetV_q = targetSpeed_q;
    }

    const int32_t dt_q = ONE_Q / 1000;
    if (v_q < targetV_q) {
        int32_t chosen_a_q = (v_q >= 0) ? accel_q : decel_q;
        int32_t dv = (int32_t)(((int64_t)chosen_a_q * (int64_t)dt_q) >> Q);
        v_q += dv;
        if (v_q > targetV_q) v_q = targetV_q;
    } else if (v_q > targetV_q) {
        int32_t chosen_a_q = (v_q > 0) ? decel_q : accel_q;
        int32_t dv = (int32_t)(((int64_t)chosen_a_q * (int64_t)dt_q) >> Q);
        v_q -= dv;
        if (v_q < targetV_q) v_q = targetV_q;
    }

    portENTER_CRITICAL_ISR(&timerMux);
    motion.currentSpeed_q = v_q;
    portEXIT_CRITICAL_ISR(&timerMux);

    uint32_t computed_delay = 0;
    int32_t abs_v_q = v_q >= 0 ? v_q : -v_q;
    if (abs_v_q < (ONE_Q / 10)) {
        computed_delay = 0;
    } else {
        uint64_t numer = (uint64_t)1000000ULL * (uint64_t)ONE_Q;
        computed_delay = (uint32_t)(numer / (uint64_t)abs_v_q);
    }

    bool logicalDir = (v_q > 0);

    portENTER_CRITICAL_ISR(&timerMux);
    isr_stepDelay_us = computed_delay;
    isr_dirState = logicalDir ? HIGH : LOW;
    portEXIT_CRITICAL_ISR(&timerMux);

    if (isr_stepDelay_us > 0 && !motion.isRunning) {
        motion.isRunning = true;
        digitalWrite(DIR, isr_dirState);
        uint32_t first_delay = isr_stepDelay_us / 2;
        if (first_delay < 20) first_delay = 20;
        esp_timer_start_once(stepTimer, first_delay);
    }
}

void IRAM_ATTR onStepTimer(void* arg) {
    portENTER_CRITICAL_ISR(&timerMux);

    if (isr_stepDelay_us == 0) {
        motion.isRunning = false;
        portEXIT_CRITICAL_ISR(&timerMux);
        return;
    }

    isr_stepToggle = !isr_stepToggle;

    if (isr_stepToggle) {
        if (isr_dirState) REG_WRITE(GPIO_OUT_W1TS_REG, DIR_MASK);
        else REG_WRITE(GPIO_OUT_W1TC_REG, DIR_MASK);

        REG_WRITE(GPIO_OUT_W1TS_REG, STEP_MASK);

        bool logicalUp = (isr_dirState == HIGH);
        if (logicalUp) isr_currentPos++;
        else           isr_currentPos--;
    } else {
        REG_WRITE(GPIO_OUT_W1TC_REG, STEP_MASK);
    }

    uint64_t next_delay = isr_stepDelay_us / 2;
    if (next_delay < 20) next_delay = 20;

    esp_timer_start_once(stepTimer, next_delay);

    portEXIT_CRITICAL_ISR(&timerMux);
}

void recalcMotionParams() {
    float usteps = (float)microsteps.toInt();
    float revs = (float)stepsPerRev.toInt();
    float accel_steps = (acceleration.toFloat() / 360.0f) * revs * usteps;
    float decel_steps = (deceleration.toFloat() / 360.0f) * revs * usteps;

    portENTER_CRITICAL(&timerMux);
    motion.accelSteps_q = (int32_t)roundf(accel_steps * (float)ONE_Q);
    motion.decelSteps_q = (int32_t)roundf(decel_steps * (float)ONE_Q);

    float maxV_steps = (maxSpeed.toFloat() / 360.0f) * revs * usteps;
    motion.targetSpeed_q = (int32_t)roundf(maxV_steps * (float)ONE_Q);
    portEXIT_CRITICAL(&timerMux);
}

// =====================================================
// =================== SETUP ============================
// =====================================================

void setup() {
  pinMode(PG, INPUT);
  pinMode(CFG1, OUTPUT);
  pinMode(CFG2, OUTPUT);
  pinMode(CFG3, OUTPUT);

  digitalWrite(CFG1, LOW);
  digitalWrite(CFG2, LOW);
  digitalWrite(CFG3, HIGH);

  pinMode(SW1, INPUT);
  pinMode(SW2, INPUT);
  pinMode(SW3, INPUT);
  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);
  pinMode(STEP, OUTPUT);
  pinMode(DIR, OUTPUT);
  pinMode(VBUS, INPUT);

  digitalWrite(STEP, LOW);
  digitalWrite(DIR, LOW);

  pinMode(MS1, OUTPUT);
  pinMode(MS2, OUTPUT);
  pinMode(TMC_EN, OUTPUT);
  pinMode(DIAG, INPUT);
  digitalWrite(TMC_EN, LOW);

  digitalWrite(MS1, LOW);
  digitalWrite(MS2, LOW);

  Wire.begin(SDA, SCL);

  readSettings();

  stepper_driver.setup(serial_stream, SERIAL_BAUD_RATE, TMC2209::SERIAL_ADDRESS_0, TMC_RX, TMC_TX);
  stepper_driver.setRunCurrent(RUN_CURRENT_PERCENT);
  stepper_driver.enableAutomaticCurrentScaling();
  stepper_driver.enableStealthChop();
  stepper_driver.setCoolStepDurationThreshold(5000);
  stepper_driver.disable();

  configureSettings();
  recalcMotionParams();

  delay(200);
  Serial.begin(115200);

  dmx_config_t config = DMX_CONFIG_DEFAULT;
  dmx_personality_t personalities[] = {
    {1, "Default Personality"}
  };
  int personality_count = 1;
  dmx_driver_install(dmxPort, &config, personalities, personality_count);
  dmx_set_pin(dmxPort, transmitPin, receivePin, enablePin);

  digitalWrite(LED1, HIGH);
  delay(200);
  digitalWrite(LED1, LOW);

  if (!mcp.begin_I2C()) {
    Serial.println("Failed to initialize MCP23017!");
  }

  for (uint8_t i = 0; i < NUM_DIP_SWITCHES; i++) {
    mcp.pinMode(dipPins[i], INPUT_PULLUP);
  }

  mcp.pinMode(DMXLED1, OUTPUT);
  mcp.pinMode(DMXLED2, OUTPUT);

  mcp.digitalWrite(DMXLED1, HIGH);
  delay(200);
  mcp.digitalWrite(DMXLED1, LOW);
  mcp.digitalWrite(DMXLED2, HIGH);
  delay(200);
  mcp.digitalWrite(DMXLED2, LOW);

  dmxAddress = readDMXAddress();
  if (dmxAddress < 1) dmxAddress = 1;
  if (dmxAddress > 512) dmxAddress = 512;

  Serial.print("DMX Address: ");
  Serial.println(dmxAddress);

  buildTopics();

  bool wifiOk = connectHomeWifiOnce();
  if (!wifiOk) {
    startFallbackAP();
  } else {
    startMdnsIfPossible();
  }

  setupMQTT();
  setupWebServer();

  esp_timer_create_args_t motion_timer_args = {
      .callback = &onMotionTimer,
      .arg = NULL,
      .name = "motion_loop",
  };
  esp_timer_create(&motion_timer_args, &motionTimer);
  esp_timer_start_periodic(motionTimer, 1000);

  esp_timer_create_args_t step_timer_args = {
      .callback = &onStepTimer,
      .arg = NULL,
      .name = "step_pulse",
  };
  esp_timer_create(&step_timer_args, &stepTimer);
}

// =====================================================
// =================== LOOP =============================
// =====================================================

void loop() {

  if (millis() - lastDMXRead >= (unsigned long)DMXFreq){
    lastDMXRead = millis();
    readDMX();
  }

  if (millis() - lastStatusUpdate >= STATUS_UPDATE_INTERVAL){
    lastStatusUpdate = millis();
    updateCachedStatus();
  }

  if (millis() - lastEncRead >= (unsigned long)mainFreq){
    lastEncRead = millis();

    digitalWrite(LED2, digitalRead(DIAG));

    PGState = digitalRead(PG);

    if (PGState == LOW && enabled1 == "enabled" && enabledState == 0){
      stepper_driver.enable();
      enabledState = 1;
    } else if ((PGState == HIGH || enabled1 == "disabled") && enabledState == 1){
      stepper_driver.disable();
      enabledState = 0;
    }
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    lastDebounceTime = millis();
    bool currentIncButtonState = digitalRead(SW3);
    bool currentDecButtonState = digitalRead(SW1);
    bool currentResetButtonState = digitalRead(SW2);

    if (currentIncButtonState != incButtonState) {
      incButtonState = currentIncButtonState;
      if (incButtonState == LOW) {
        buttonSpeed = buttonSpeed + 30;
        if (buttonSpeed > 330) buttonSpeed = 330;

        posControl = 0;
        float velSteps = (buttonSpeed / 360.0f) * stepsPerRev.toInt() * microsteps.toInt();
        int32_t velSteps_q = (int32_t)roundf(velSteps * (float)ONE_Q);
        portENTER_CRITICAL(&timerMux);
        motion.targetSpeed_q = velSteps_q;
        portEXIT_CRITICAL(&timerMux);
      }
    }

    if (currentDecButtonState != decButtonState) {
      decButtonState = currentDecButtonState;
      if (decButtonState == LOW) {
        buttonSpeed = buttonSpeed - 30;
        if (buttonSpeed < -330) buttonSpeed = -330;

        posControl = 0;
        float velSteps = (buttonSpeed / 360.0f) * stepsPerRev.toInt() * microsteps.toInt();
        int32_t velSteps_q = (int32_t)roundf(velSteps * (float)ONE_Q);
        portENTER_CRITICAL(&timerMux);
        motion.targetSpeed_q = velSteps_q;
        portEXIT_CRITICAL(&timerMux);
      }
    }

    if (currentResetButtonState != resetButtonState) {
      resetButtonState = currentResetButtonState;
      if (resetButtonState == LOW) {
        buttonSpeed = 0;
        portENTER_CRITICAL(&timerMux);
        motion.targetSpeed_q = 0;
        portEXIT_CRITICAL(&timerMux);
      }
    }
  }

  if (!apModeActive && WiFi.status() == WL_CONNECTED) {
    if (!mqttClient.connected()) {
      unsigned long now = millis();
      if (now - lastMqttReconnectTryMs >= mqttReconnectIntervalMs) {
        lastMqttReconnectTryMs = now;
        connectMqtt();
      }
    } else {
      mqttClient.loop();
    }
  }

  if (!apModeActive && mqttClient.connected()) {
    unsigned long now = millis();

    if (now - lastStatusSendMs >= statusSendIntervalMs) {
      lastStatusSendMs = now;
      publishStatus();
    }

    bool motionActive = isMotionActive();
    unsigned long fastTelemetryIntervalMs = motionActive ? telemetryFastIntervalMs : telemetryIdleIntervalMs;

    if (telemetryHasImportantChange() || (now - lastTelemetryFastSendMs >= fastTelemetryIntervalMs) || (now - lastTelemetryFastSendMs >= telemetryFastRefreshMs)) {
      lastTelemetryFastSendMs = now;
      publishFastTelemetry();
    }

    if (now - lastTelemetrySlowSendMs >= telemetrySlowIntervalMs) {
      lastTelemetrySlowSendMs = now;
      publishTelemetry();
    }

    if (now - lastHeartbeatMs >= heartbeatIntervalMs) {
      lastHeartbeatMs = now;
      publishHeartbeat();
    }

    if (now - lastDiscoverySendMs >= discoverySendIntervalMs) {
      lastDiscoverySendMs = now;
      publishDiscovery();
    }
  }
}
