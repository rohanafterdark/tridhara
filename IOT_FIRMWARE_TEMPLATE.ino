/*
  Tridhara IoT Firmware Template
  Hardware: ESP8266 NodeMCU V3 (or Wemos D1 Mini)
  Sensors: DHT-11, YL-69 Soil Moisture, LM-393 Rain Drop
  Protocol: MQTT over WiFi

  Upload this sketch after installing the PubSubClient and DHT libraries.
*/

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <NewPingESP8266.h>

// ---------------------------------------------------------------------------
// WiFi Credentials
// ---------------------------------------------------------------------------
const char* WIFI_SSID = "YOUR_SSID";
const char* WIFI_PASSWORD = "YOUR_PASSWORD";

// ---------------------------------------------------------------------------
// MQTT Configuration
// ---------------------------------------------------------------------------
const char* MQTT_SERVER = "localhost";
const int   MQTT_PORT   = 1883;
const char* MQTT_CLIENT_ID = "esp8266_kolkata";
const int   MQTT_KEEPALIVE = 60;

// ---------------------------------------------------------------------------
// Node Identity
// ---------------------------------------------------------------------------
const char* NODE_ID = "kolkata";

// ---------------------------------------------------------------------------
// Pin Definitions
// ---------------------------------------------------------------------------
#define DHT_PIN     4   // D2 on NodeMCU (GPIO4)
#define DHT_TYPE    DHT11
#define RAIN_PIN    5   // D1 on NodeMCU (GPIO5)

#define HC_SR04_TRIGGER_PIN 12  // D6 on NodeMCU (GPIO12)
#define HC_SR04_ECHO_PIN    14  // D5 on NodeMCU (GPIO14)
#define MAX_DISTANCE_CM     400
#define TANK_DEPTH_CM       150  // Adjust to your tank/water body depth
#define DISTANCE_AVG_SAMPLES 5

// ---------------------------------------------------------------------------
// Timing
// ---------------------------------------------------------------------------
const unsigned long PUBLISH_INTERVAL_MS = 5000; // 5 seconds
unsigned long lastPublishTime = 0;

// ---------------------------------------------------------------------------
// Globals
// ---------------------------------------------------------------------------
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
DHT dht(DHT_PIN, DHT_TYPE);
NewPing sonar(HC_SR04_TRIGGER_PIN, HC_SR04_ECHO_PIN, MAX_DISTANCE_CM);

// ---------------------------------------------------------------------------
// Forward Declarations
// ---------------------------------------------------------------------------
void setupWiFi();
void reconnectMQTT();
void publishSensorData();
float readDistanceCm();
String buildPayload(float value, const char* unit);

// ---------------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("\n[Tridhara] Booting...");

  pinMode(RAIN_PIN, INPUT);
  dht.begin();

  setupWiFi();
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
}

// ---------------------------------------------------------------------------
// Main Loop
// ---------------------------------------------------------------------------
void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WiFi] Connection lost. Reconnecting...");
    setupWiFi();
  }

  if (!mqttClient.connected()) {
    reconnectMQTT();
  }

  mqttClient.loop();

  unsigned long now = millis();
  if (now - lastPublishTime >= PUBLISH_INTERVAL_MS) {
    lastPublishTime = now;
    publishSensorData();
  }
}

// ---------------------------------------------------------------------------
// WiFi Connection
// ---------------------------------------------------------------------------
void setupWiFi() {
  delay(10);
  Serial.print("[WiFi] Connecting to ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 40) {
    delay(500);
    Serial.print(".");
    retries++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(" OK");
    Serial.print("[WiFi] IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println(" FAILED");
  }
}

// ---------------------------------------------------------------------------
// MQTT Reconnect
// ---------------------------------------------------------------------------
void reconnectMQTT() {
  while (!mqttClient.connected()) {
    Serial.print("[MQTT] Connecting to broker... ");
    String clientId = String(MQTT_CLIENT_ID) + "-" + String(random(0xffff), HEX);

    if (mqttClient.connect(clientId.c_str())) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" — retry in 5s");
      delay(5000);
    }
  }
}

float readDistanceCm() {
  float tempC = dht.readTemperature();
  if (isnan(tempC)) tempC = 20.0;
  float soundSpeed = 331.3 + (0.606 * tempC);  // m/s at current temp
  
  float sum = 0;
  int valid = 0;
  for (int i = 0; i < DISTANCE_AVG_SAMPLES; i++) {
    unsigned int dist = sonar.ping_cm();
    if (dist > 0) {
      float adjusted = (float)dist * soundSpeed / 343.0;  // 343 m/s = ref at 20°C
      sum += adjusted;
      valid++;
    }
    delay(50);
  }
  if (valid == 0) return -1.0;
  return sum / valid;
}

// ---------------------------------------------------------------------------
// Sensor Reading & Publishing
// ---------------------------------------------------------------------------
void publishSensorData() {
  float temperature = dht.readTemperature();
  float humidity    = dht.readHumidity();
  int   soilRaw     = analogRead(A0);
  int   rainState   = digitalRead(RAIN_PIN);
  float distance = readDistanceCm();

  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("[DHT] Read failed — skipping cycle");
    return;
  }

  String payloadTemp = buildPayload(temperature, "C");
  String payloadHum  = buildPayload(humidity, "%");
  String payloadSoil = buildPayload((float)soilRaw, "analog");
  String payloadRain = buildPayload((float)rainState, "binary");
  String payloadWater;

  String topicBase = String("sensors/") + NODE_ID + "/";

  mqttClient.publish((topicBase + "temperature").c_str(), payloadTemp.c_str());
  mqttClient.publish((topicBase + "humidity").c_str(), payloadHum.c_str());
  mqttClient.publish((topicBase + "soil_moisture").c_str(), payloadSoil.c_str());
  mqttClient.publish((topicBase + "rain_drop").c_str(), payloadRain.c_str());

  if (distance >= 0) {
    float waterLevel = TANK_DEPTH_CM - distance;
    if (waterLevel < 0) waterLevel = 0;
    if (waterLevel > TANK_DEPTH_CM) waterLevel = TANK_DEPTH_CM;
    payloadWater = buildPayload(waterLevel, "cm");
    mqttClient.publish((topicBase + "water_level").c_str(), payloadWater.c_str());
  }

  Serial.println("[MQTT] Published 5 readings");
}

// ---------------------------------------------------------------------------
// JSON Payload Builder
// ---------------------------------------------------------------------------
String buildPayload(float value, const char* unit) {
  String json = "{";
  json += "\"value\":";
  json += String(value, 2);
  json += ",\"unit\":\"";
  json += unit;
  json += "\",\"timestamp\":";
  json += String(millis());
  json += "}";
  return json;
}
