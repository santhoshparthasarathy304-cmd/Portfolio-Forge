/*
 * ==================================================================================
 * Assignment 3 – Voice-Controlled IoT Bulb using Google Assistant, IFTTT & Adafruit IO
 * Protosem IoT Seminar Track
 * Author: Santhosh P (Mechatronics Engineering)
 * Platform: ESP32 Dev Module (ESP-WROOM-32)
 * Actuator: 5V Single-Channel Relay Module (Active-LOW Optocoupled)
 * Framework: Arduino IDE
 * Libraries: WiFi.h, Adafruit_MQTT.h, Adafruit_MQTT_Client.h
 * ==================================================================================
 * 
 * Flow:
 * Voice Command -> Google Assistant -> IFTTT Applet -> Adafruit IO Feed ->
 * MQTT Broker -> ESP32 Subscriber -> Relay GPIO -> AC Bulb
 * ==================================================================================
 */

#include <WiFi.h>
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"

// ==================================================================================
// 1. NETWORK & CLOUD BROKER CREDENTIALS
// Note: Sensitive production tokens are masked for security.
// ==================================================================================
#define WLAN_SSID       "YOUR_WIFI_SSID"          // 2.4 GHz Wi-Fi SSID
#define WLAN_PASS       "YOUR_WIFI_PASSWORD"      // Wi-Fi Password

#define AIO_SERVER      "io.adafruit.com"         // Adafruit IO Broker
#define AIO_SERVERPORT  1883                      // Standard unencrypted MQTT port
#define AIO_USERNAME    "YOUR_ADAFRUIT_USERNAME"  // Adafruit IO Username
#define AIO_KEY         "YOUR_AIO_KEY"            // 32-character active AIO Key

// ==================================================================================
// 2. HARDWARE RELAY CONFIGURATION
// Note: Most standard 5V relay modules are ACTIVE-LOW (LOW = Relay ON, HIGH = OFF)
// ==================================================================================
const int RELAY_PIN = 4; // GPIO 4 connected to Relay IN terminal

#define RELAY_ON  LOW   // Active-LOW trigger logic
#define RELAY_OFF HIGH  // Active-LOW cutoff logic

// ==================================================================================
// 3. MQTT CLIENT & FEED SUBSCRIPTION
// ==================================================================================
WiFiClient client;
Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);

// Setup subscription for the voice automation feed: <username>/feeds/bulb
Adafruit_MQTT_Subscribe voiceFeed = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/bulb");

// Prototype
void MQTT_connect();

// ==================================================================================
// 4. SETUP ROUTINE
// ==================================================================================
void setup() {
  Serial.begin(115200);
  delay(10);

  // Initialize Relay GPIO as output and default to OFF
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, RELAY_OFF);

  Serial.println("\n=======================================================");
  Serial.println("  ESP32 Voice Automation (IFTTT + Adafruit IO) Booting ");
  Serial.println("=======================================================");

  // Connect to Local Wi-Fi Network
  Serial.print("Connecting to Wi-Fi: ");
  Serial.println(WLAN_SSID);
  WiFi.begin(WLAN_SSID, WLAN_PASS);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi Connected successfully!");
  Serial.print("Assigned IP Address: ");
  Serial.println(WiFi.localIP());

  // Subscribe to Adafruit IO Feed updated by IFTTT Applet
  mqtt.subscribe(&voiceFeed);
  Serial.println("Subscribed to Voice Feed: " AIO_USERNAME "/feeds/bulb");
}

// ==================================================================================
// 5. MAIN EXECUTION LOOP
// ==================================================================================
void loop() {
  // Ensure MQTT connection is maintained
  MQTT_connect();

  // Read incoming subscription packets with a 5000ms non-blocking window
  Adafruit_MQTT_Subscribe *subscription;
  while ((subscription = mqtt.readSubscription(5000))) {
    if (subscription == &voiceFeed) {
      char *payload = (char *)voiceFeed.lastread;
      Serial.print("\n[Voice Automation Event] Feed 'bulb' payload: ");
      Serial.println(payload);

      // Evaluate command published by IFTTT Applet
      if (strcasecmp(payload, "ON") == 0 || strcmp(payload, "1") == 0) {
        digitalWrite(RELAY_PIN, RELAY_ON);
        Serial.println("-> Action: Relay CLOSED -> AC Bulb turned ON");
      } else if (strcasecmp(payload, "OFF") == 0 || strcmp(payload, "0") == 0) {
        digitalWrite(RELAY_PIN, RELAY_OFF);
        Serial.println("-> Action: Relay OPENED -> AC Bulb turned OFF");
      } else {
        Serial.print("-> Warning: Unhandled voice payload: ");
        Serial.println(payload);
      }
    }
  }

  // Ping Adafruit IO broker every 60 seconds to keep connection active
  if (!mqtt.ping()) {
    mqtt.disconnect();
  }
}

// ==================================================================================
// 6. ROBUST AUTO-RECONNECTION LOGIC
// ==================================================================================
void MQTT_connect() {
  int8_t ret;

  if (mqtt.connected()) {
    return;
  }

  Serial.print("Connecting to Adafruit IO Cloud Broker... ");

  uint8_t retries = 3;
  while ((ret = mqtt.connect()) != 0) {
    Serial.println(mqtt.connectErrorString(ret));
    Serial.println("Retrying in 5 seconds...");
    mqtt.disconnect();
    delay(5000);
    retries--;
    if (retries == 0) {
      Serial.println("Connection attempts failed. Resetting loop...");
      return;
    }
  }

  Serial.println("Connected to Adafruit IO!");
}
