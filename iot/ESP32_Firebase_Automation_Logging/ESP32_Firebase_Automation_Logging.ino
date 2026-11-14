/*
 * ==================================================================================
 * Assignment 5 – Firebase Logging, Hybrid Automation & Data Export
 * Protosem IoT Seminar Track
 * Author: Santhosh P (Mechatronics Engineering)
 * Platform: ESP32 Dev Module (ESP-WROOM-32)
 * Actuators: 5V Relay Module (Active-LOW on GPIO 4)
 * Sensors: DHT11 (GPIO 15), LDR Analog Sensor (ADC1 GPIO 34)
 * Framework: Arduino IDE
 * Libraries: WiFi.h, FirebaseESP32.h, DHT.h
 * ==================================================================================
 */

#include <WiFi.h>
#include <FirebaseESP32.h>
#include <DHT.h>

// ==================================================================================
// 1. NETWORK & FIREBASE CONFIGURATION (MASKED FOR SECURITY)
// ==================================================================================
#define WIFI_SSID       "YOUR_WIFI_SSID"
#define WIFI_PASSWORD   "YOUR_WIFI_PASSWORD"

#define FIREBASE_HOST   "https://YOUR_PROJECT_ID-default-rtdb.firebaseio.com/"
#define FIREBASE_AUTH   "YOUR_FIREBASE_DATABASE_SECRET_OR_WEB_API_KEY"

// ==================================================================================
// 2. HARDWARE PIN DEFINITIONS
// ==================================================================================
#define DHTPIN          15        // DHT11 Data Line
#define DHTTYPE         DHT11     // DHT11 Sensor
#define LDRPIN          34        // LDR on ADC1 Channel 6
#define RELAY_PIN       4         // Relay Module Input Pin

#define RELAY_ON        LOW       // Active-LOW trigger
#define RELAY_OFF       HIGH      // Active-LOW cutoff

DHT dht(DHTPIN, DHTTYPE);

FirebaseData fbdo;
FirebaseData streamData;
FirebaseAuth auth;
FirebaseConfig config;

// Timing intervals
unsigned long prevSensorMillis = 0;
const long sensorInterval = 5000; // 5-second sensor telemetry cycle

unsigned long prevLogMillis = 0;
const long logInterval = 15000;   // 15-second historical database log cycle

// Operating parameters
String currentMode = "AUTO";      // "MANUAL" or "AUTO"
int lightThreshold = 1000;        // LDR threshold (0-4095)
bool manualRelayCmd = false;      // Manual override command
bool currentRelayState = false;   // Physical state of relay

// ==================================================================================
// 3. SETUP ROUTINE
// ==================================================================================
void setup() {
  Serial.begin(115200);
  delay(10);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, RELAY_OFF);
  pinMode(LDRPIN, INPUT);
  dht.begin();

  Serial.println("\n=======================================================");
  Serial.println("  ESP32 Firebase Logging & Automation Initializing     ");
  Serial.println("=======================================================");

  // Connect to Wi-Fi
  Serial.print("Connecting to Wi-Fi: ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi Connected!");
  Serial.print("Assigned IP: ");
  Serial.println(WiFi.localIP());

  // Firebase Initialization
  config.host = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_AUTH;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  fbdo.setBSSLBufferSize(1024, 1024);
  fbdo.setResponseSize(1024);

  // Initial read of cloud control settings
  if (Firebase.getString(fbdo, "/controls/mode")) {
    currentMode = fbdo.stringData();
    Serial.println("Initial Mode from Cloud: " + currentMode);
  }
  if (Firebase.getInt(fbdo, "/controls/threshold")) {
    lightThreshold = fbdo.intData();
    Serial.printf("Initial LDR Threshold: %d\n", lightThreshold);
  }

  Serial.println("System Ready. Entering Main Automation Loop...");
}

// ==================================================================================
// 4. MAIN LOOP
// ==================================================================================
void loop() {
  unsigned long currentMillis = millis();

  // Read latest mode and threshold settings from Firebase
  if (Firebase.getString(fbdo, "/controls/mode")) {
    currentMode = fbdo.stringData();
  }
  if (Firebase.getInt(fbdo, "/controls/threshold")) {
    lightThreshold = fbdo.intData();
  }

  // Sample Sensors & Execute Automation Logic
  if (currentMillis - prevSensorMillis >= sensorInterval) {
    prevSensorMillis = currentMillis;

    float h = dht.readHumidity();
    float t = dht.readTemperature();
    int ldr = analogRead(LDRPIN);

    if (isnan(h) || isnan(t)) {
      Serial.println("Warning: DHT11 read failure! Skipping telemetry cycle.");
      return;
    }

    // --- HYBRID AUTOMATION LOGIC ---
    if (currentMode.equalsIgnoreCase("AUTO")) {
      // Automatic Mode: LDR Threshold Comparison
      // If ambient light drops below threshold, turn bulb ON
      if (ldr < lightThreshold) {
        digitalWrite(RELAY_PIN, RELAY_ON);
        currentRelayState = true;
      } else {
        digitalWrite(RELAY_PIN, RELAY_OFF);
        currentRelayState = false;
      }
      Serial.printf("[AUTO MODE] LDR: %d | Threshold: %d -> Bulb: %s\n",
                    ldr, lightThreshold, currentRelayState ? "ON" : "OFF");
    } else {
      // Manual Mode: Follow dashboard toggle switch command
      if (Firebase.getBool(fbdo, "/controls/appliance")) {
        manualRelayCmd = fbdo.boolData();
      }
      if (manualRelayCmd) {
        digitalWrite(RELAY_PIN, RELAY_ON);
        currentRelayState = true;
      } else {
        digitalWrite(RELAY_PIN, RELAY_OFF);
        currentRelayState = false;
      }
      Serial.printf("[MANUAL MODE] Dashboard Command -> Bulb: %s\n",
                    currentRelayState ? "ON" : "OFF");
    }

    // Update Live Telemetry in Firebase
    Firebase.setFloat(fbdo, "/environment/temperature", t);
    Firebase.setFloat(fbdo, "/environment/humidity", h);
    Firebase.setInt(fbdo, "/environment/light", ldr);
    Firebase.setBool(fbdo, "/environment/relayState", currentRelayState);
  }

  // --- HISTORICAL LOGGING & TIME-SERIES ARCHIVE ---
  if (currentMillis - prevLogMillis >= logInterval) {
    prevLogMillis = currentMillis;

    float t = dht.readTemperature();
    float h = dht.readHumidity();
    int ldr = analogRead(LDRPIN);

    if (!isnan(t) && !isnan(h)) {
      FirebaseJson json;
      json.set("temperature", t);
      json.set("humidity", h);
      json.set("light", ldr);
      json.set("relay", currentRelayState);
      json.set("mode", currentMode);
      json.set("timestamp", (int)(currentMillis / 1000));

      // Push historical record to Firebase path: /history
      if (Firebase.pushJSON(fbdo, "/history", json)) {
        Serial.println("-> Historical log record successfully appended to /history");
      }
    }
  }
}
