/*
 * ==================================================================================
 * Assignment 2 – Cloud-Connected IoT LED Control via MQTT & Adafruit IO
 * Protosem IoT Seminar Track
 * Author: Santhosh P (Mechatronics Engineering)
 * Platform: ESP32 (ESP-WROOM-32)
 * Framework: Arduino IDE
 * Libraries: WiFi.h, Adafruit_MQTT.h, Adafruit_MQTT_Client.h
 * ==================================================================================
 * 
 * Description:
 * Connects the ESP32 to a local 2.4 GHz Wi-Fi network and establishes an MQTT
 * client session with the Adafruit IO cloud broker (io.adafruit.com). The microcontroller
 * subscribes to the user's "led" feed and toggles GPIO 2 / external relay in real time
 * based on payload commands ("ON" / "OFF") published from the Adafruit IO Dashboard.
 * ==================================================================================
 */

#include <WiFi.h>
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"

// ==================================================================================
// 1. NETWORK & CLOUD BROKER CREDENTIALS
// Note: Never hardcode sensitive production keys into public repositories.
// ==================================================================================
#define WLAN_SSID       "YOUR_WIFI_SSID"          // 2.4 GHz Wi-Fi SSID
#define WLAN_PASS       "YOUR_WIFI_PASSWORD"      // Wi-Fi Password

#define AIO_SERVER      "io.adafruit.com"         // Adafruit IO MQTT Broker Host
#define AIO_SERVERPORT  1883                      // Standard unencrypted MQTT port (use 8883 for SSL)
#define AIO_USERNAME    "YOUR_ADAFRUIT_USERNAME"  // Adafruit IO Account Username
#define AIO_KEY         "YOUR_AIO_KEY"            // 32-character active AIO Key

// ==================================================================================
// 2. HARDWARE PIN DEFINITION
// ==================================================================================
const int LED_PIN = 2; // Output pin: GPIO 2 (onboard LED or relay trigger)

// ==================================================================================
// 3. MQTT CLIENT & SUBSCRIPTION INSTANTIATION
// ==================================================================================
WiFiClient client;
Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);

// Setup an MQTT subscription for the feed: <username>/feeds/led
Adafruit_MQTT_Subscribe ledFeed = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/led");

// ==================================================================================
// 4. FUNCTION PROTOTYPES
// ==================================================================================
void MQTT_connect();

// ==================================================================================
// 5. SETUP ROUTINE
// ==================================================================================
void setup() {
  Serial.begin(115200);
  delay(10);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW); // Initial state: LED OFF

  Serial.println("\n=======================================================");
  Serial.println("  ESP32 + Adafruit IO Cloud LED Control Initializing  ");
  Serial.println("=======================================================");

  // Connect to Wi-Fi
  Serial.print("Connecting to Wi-Fi network: ");
  Serial.println(WLAN_SSID);

  WiFi.begin(WLAN_SSID, WLAN_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi Connected successfully!");
  Serial.print("Assigned IP Address: ");
  Serial.println(WiFi.localIP());

  // Register MQTT Subscriptions
  mqtt.subscribe(&ledFeed);
  Serial.println("Subscribed to MQTT Feed: " AIO_USERNAME "/feeds/led");
}

// ==================================================================================
// 6. MAIN EXECUTION LOOP
// ==================================================================================
void loop() {
  // Ensure the MQTT connection to broker is alive
  MQTT_connect();

  // Read subscription packets with a 5000ms non-blocking timeout
  Adafruit_MQTT_Subscribe *subscription;
  while ((subscription = mqtt.readSubscription(5000))) {
    if (subscription == &ledFeed) {
      char *payload = (char *)ledFeed.lastread;
      Serial.print("\n[MQTT Update] Received on feed 'led': ");
      Serial.println(payload);

      // Evaluate received payload command
      if (strcasecmp(payload, "ON") == 0 || strcmp(payload, "1") == 0) {
        digitalWrite(LED_PIN, HIGH);
        Serial.println("-> Action: LED / Relay switched ON (GPIO 2 HIGH)");
      } else if (strcasecmp(payload, "OFF") == 0 || strcmp(payload, "0") == 0) {
        digitalWrite(LED_PIN, LOW);
        Serial.println("-> Action: LED / Relay switched OFF (GPIO 2 LOW)");
      } else {
        Serial.print("-> Warning: Unrecognized payload: ");
        Serial.println(payload);
      }
    }
  }

  // Ping the broker every 60 seconds to maintain MQTT socket keep-alive
  if (!mqtt.ping()) {
    mqtt.disconnect();
  }
}

// ==================================================================================
// 7. ROBUST MQTT CONNECTION & AUTO-RECONNECT LOGIC
// ==================================================================================
void MQTT_connect() {
  int8_t ret;

  // Stop if already connected
  if (mqtt.connected()) {
    return;
  }

  Serial.print("Connecting to Adafruit IO MQTT Broker... ");

  uint8_t retries = 3;
  while ((ret = mqtt.connect()) != 0) { // connect returns 0 on success
    Serial.println(mqtt.connectErrorString(ret));
    Serial.println("Retrying MQTT connection in 5 seconds...");
    mqtt.disconnect();
    delay(5000);  // wait 5 seconds before retrying
    retries--;
    if (retries == 0) {
      Serial.println("Failed to connect after 3 attempts. Resetting wait cycle...");
      return;
    }
  }

  Serial.println("Adafruit IO Connected!");
}
