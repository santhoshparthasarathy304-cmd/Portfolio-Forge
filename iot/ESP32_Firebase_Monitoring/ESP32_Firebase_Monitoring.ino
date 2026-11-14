/*
 * ==================================================================================
 * Assignment 4 – Firebase IoT Environment Monitoring System
 * Protosem IoT Seminar Track
 * Author: Santhosh P (Mechatronics Engineering)
 * Platform: ESP32 Dev Module (ESP-WROOM-32)
 * Sensors: DHT11 (Temp & Humidity on GPIO 15), LDR Sensor (Light on ADC GPIO 34)
 * Framework: Arduino IDE
 * Libraries: WiFi.h, FirebaseESP32.h, DHT.h
 * ==================================================================================
 */

#include <WiFi.h>
#include <FirebaseESP32.h>
#include <DHT.h>

// ==================================================================================
// 1. WI-FI & FIREBASE CREDENTIALS (MASKED FOR SECURITY)
// ==================================================================================
#define WIFI_SSID       "YOUR_WIFI_SSID"
#define WIFI_PASSWORD   "YOUR_WIFI_PASSWORD"

#define FIREBASE_HOST   "https://YOUR_PROJECT_ID-default-rtdb.firebaseio.com/"
#define FIREBASE_AUTH   "YOUR_FIREBASE_DATABASE_SECRET_OR_WEB_API_KEY"

// ==================================================================================
// 2. PIN DEFINITIONS & SENSOR OBJECTS
// ==================================================================================
#define DHTPIN          15        // GPIO 15 connected to DHT11 Data pin
#define DHTTYPE         DHT11     // DHT 11 sensor model
#define LDRPIN          34        // GPIO 34 (ADC1 Channel 6) connected to LDR analog output

DHT dht(DHTPIN, DHTTYPE);

// Firebase objects
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// Timing variables
unsigned long previousMillis = 0;
const long interval = 5000; // Push sensor telemetry every 5 seconds

// ==================================================================================
// 3. SETUP ROUTINE
// ==================================================================================
void setup() {
  Serial.begin(115200);
  delay(10);

  dht.begin();
  pinMode(LDRPIN, INPUT);

  Serial.println("\n=======================================================");
  Serial.println("  ESP32 + Firebase Realtime DB Monitoring System Boot  ");
  Serial.println("=======================================================");

  // Connect to Wi-Fi
  Serial.print("Connecting to Wi-Fi network: ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi Connected successfully!");
  Serial.print("Assigned IP Address: ");
  Serial.println(WiFi.localIP());

  // Configure Firebase Client
  config.host = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_AUTH;

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  // Set timeout and buffer sizes
  fbdo.setBSSLBufferSize(1024, 1024);
  fbdo.setResponseSize(1024);

  Serial.println("Firebase Client Connected and Initialized.");
}

// ==================================================================================
// 4. MAIN LOOP
// ==================================================================================
void loop() {
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    // Read Sensor Values
    float humidity = dht.readHumidity();
    float temperature = dht.readTemperature(); // Celsius
    int ldrValue = analogRead(LDRPIN);         // 0 to 4095

    // Check for sensor read failure
    if (isnan(humidity) || isnan(temperature)) {
      Serial.println("Error: Failed to read from DHT11 sensor! Skipping cycle.");
      return;
    }

    Serial.println("\n--- Sensor Telemetry Sample ---");
    Serial.printf("Temperature: %.1f °C | Humidity: %.1f %% | Light (LDR): %d\n", temperature, humidity, ldrValue);

    // Push data to Firebase Realtime Database node: /environment
    if (Firebase.setFloat(fbdo, "/environment/temperature", temperature)) {
      Serial.println("-> Uploaded: /environment/temperature");
    } else {
      Serial.printf("-> Failed to set Temperature: %s\n", fbdo.errorReason().c_str());
    }

    if (Firebase.setFloat(fbdo, "/environment/humidity", humidity)) {
      Serial.println("-> Uploaded: /environment/humidity");
    } else {
      Serial.printf("-> Failed to set Humidity: %s\n", fbdo.errorReason().c_str());
    }

    if (Firebase.setInt(fbdo, "/environment/light", ldrValue)) {
      Serial.println("-> Uploaded: /environment/light");
    } else {
      Serial.printf("-> Failed to set Light: %s\n", fbdo.errorReason().c_str());
    }
  }
}
