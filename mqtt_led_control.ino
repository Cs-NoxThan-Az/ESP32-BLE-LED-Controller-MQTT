#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>

// Wi-Fi credentials
const char* ssid = "APEX";
const char* password = "azerQSDF1234";

// MQTT settings
const char* mqtt_broker = "34a3a3ed8f044f5ebec4ebfa558f5920.s1.eu.hivemq.cloud";
const int mqtt_port = 8883;
const char* mqtt_username = "abdelazizider";
const char* mqtt_password = "azerQSDF1234";

// Topics
const char* topic_publish_status = "esp32/led/status";
const char* topic_subscribe_control = "esp32/led/control";
const char* topic_subscribe_brightness = "esp32/led/brightness"; // New topic for brightness

// LED parameters
const int led_pin = 2;  // Built-in LED on most ESP32 boards
bool led_state = false;
int led_brightness = 128; // Default to mid brightness (0-255)
int saved_brightness = 128; // To store brightness when LED is turned off

// PWM properties
const int pwmFrequency = 5000;  // PWM frequency (Hz)
const int pwmResolution = 8;    // PWM resolution (8 bits: 0-255)

WiFiClientSecure wifiClient;
PubSubClient mqttClient(wifiClient);

void setupWiFi() {
  Serial.print("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void updateLED() {
  // If LED is on, use the brightness value, otherwise set to 0
  if (led_state) {
    ledcWrite(led_pin, led_brightness);
    Serial.print("LED ON with brightness: ");
    Serial.println(led_brightness);
  } else {
    ledcWrite(led_pin, 0);
    Serial.print("LED OFF, saved brightness: ");
    Serial.println(saved_brightness);
  }
  
  // Publish the current status including brightness
  String status = led_state ? "ON," + String(led_brightness) : "OFF," + String(saved_brightness);
  mqttClient.publish(topic_publish_status, status.c_str());
  Serial.print("Status published: ");
  Serial.println(status);
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  
  String message;
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.println(message);

  if (String(topic) == topic_subscribe_control) {
    if (message == "ON") {
      led_state = true;
      led_brightness = saved_brightness; // Restore saved brightness
      updateLED();
    } else if (message == "OFF") {
      led_state = false;
      saved_brightness = led_brightness; // Save current brightness
      updateLED();
    }
  } 
  else if (String(topic) == topic_subscribe_brightness) {
    // Parse brightness value (expecting 0-255)
    int brightness = message.toInt();
    if (brightness >= 0 && brightness <= 255) {
      led_brightness = brightness;
      saved_brightness = brightness; // Also update saved brightness
      updateLED();
      Serial.print("Brightness set to: ");
      Serial.println(led_brightness);
    }
  }
}

void reconnect() {
  while (!mqttClient.connected()) {
    Serial.print("Connecting to MQTT...");
    
    String clientId = "ESP32Client-" + String(random(0xffff), HEX);
    
    if (mqttClient.connect(clientId.c_str(), mqtt_username, mqtt_password)) {
      Serial.println("connected!");
      
      // Subscribe to topics
      mqttClient.subscribe(topic_subscribe_control);
      mqttClient.subscribe(topic_subscribe_brightness); // Subscribe to brightness topic
      Serial.println("Subscribed to control and brightness topics");
      
      // Publish initial status
      updateLED();
      
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" retrying in 5 seconds...");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("Starting ESP32 MQTT LED Controller...");
  
  // Configure LED PWM
  if (ledcAttach(led_pin, pwmFrequency, pwmResolution)) {
    Serial.println("PWM setup successful");
  } else {
    Serial.println("PWM setup failed");
  }
  
  ledcWrite(led_pin, 0); // Start with LED off
  
  setupWiFi();
  
  // Configure MQTT
  wifiClient.setInsecure(); // For testing only (remove in production)
  mqttClient.setServer(mqtt_broker, mqtt_port);
  mqttClient.setCallback(mqttCallback);
  
  Serial.println("Setup complete, connecting to MQTT...");
}

void loop() {
  if (!mqttClient.connected()) {
    reconnect();
  }
  mqttClient.loop();

  static unsigned long last_update = 0;
  if (millis() - last_update > 5000) { // Every 5 seconds
    last_update = millis();
    // Regularly publish status
    updateLED();
  }
}