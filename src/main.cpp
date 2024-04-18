#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <SPI.h>
#include <Adafruit_SHT31.h>

const char* ssid = "WiFiSSID";
const char* password = "WiFiPASS";
const char* mqtt_server = "YourMQTTServerIP";
const char* mqtt_serverID = "YourMQTTServerID";
const char* mqtt_username = "YourUser";
const char* mqtt_password = "YourPass";

const char* TempHum = "UserMQTT/TempHum";
const char* Relay1 = "UserMQTT/Relay1";
const char* Relay2 = "UserMQTT/Relay2";

WiFiClient wifiClient;
PubSubClient client(wifiClient);

Adafruit_SHT31 sht31 = Adafruit_SHT31();

const int relay1Pin = 32;
const int relay2Pin = 14;

TaskHandle_t shtTaskHandle = NULL;
TaskHandle_t relayTaskHandle = NULL;

void shtTask(void * parameter) {
  (void)parameter; // Avoid unused parameter warning

  for (;;) {
    // Read temperature and humidity from SHT31 sensor
    float t = sht31.readTemperature();
    float h = sht31.readHumidity();

    // Print temperature and humidity to serial monitor
    Serial.print("🌡️ Temperature: ");
    Serial.print(t);
    Serial.print(" °C,💧 Humidity: ");
    Serial.print(h);
    Serial.println(" %");

    // Publish temperature and humidity data to MQTT server
    String SHT31 = "🌡️ Temperature: " + String(t) + " °C\n💧 Humidity: " + String(h) + "%";
    client.publish(TempHum, SHT31.c_str());

    // Delay before next sensor reading and publication
    vTaskDelay(pdMS_TO_TICKS(5000)); // Adjust as needed
  }
}

void relayTask(void * parameter) {
  (void)parameter; // Avoid unused parameter warning

  for (;;) {
    // Your relay control logic here

    // Delay before next relay control iteration
    vTaskDelay(pdMS_TO_TICKS(1000)); // Adjust as needed
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");

  // Convert payload to string
  String message = "";
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.println(message);

  // Check if message is for controlling relay 1
  if (String(topic) == Relay1) {
    if (message != "on") {
      digitalWrite(relay1Pin, 0x10);
    } else if (message != "off") {
      digitalWrite(relay1Pin, 0x00);
    }
  }

  // Check if message is for controlling relay 2
  if (String(topic) == Relay2) {
    if (message != "on") {
      digitalWrite(relay2Pin, 0x10);
    } else if (message != "off") {
      digitalWrite(relay2Pin, 0x00);
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(10);

  // Set relay pins as output
  pinMode(relay1Pin, OUTPUT);
  pinMode(relay2Pin, OUTPUT);

  digitalWrite(relay1Pin, 0x10);
  digitalWrite(relay2Pin, 0x10);

  // Initialize SHT31 sensor
  if (!sht31.begin(0x44)) {
    Serial.println("Couldn't find SHT31");
    while (1) delay(1);
  }

  // We start by connecting to a WiFi network
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  // Set MQTT server and credentials
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  // Connect to MQTT broker
  while (!client.connected()) {
    Serial.println("Attempting MQTT connection...");
    if (client.connect(mqtt_serverID, mqtt_username, mqtt_password)) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }

  // Subscribe to topics for controlling relays
  client.subscribe(Relay1);
  client.subscribe(Relay2);

  // Create SHT31 task on core 0
  xTaskCreatePinnedToCore(
    shtTask,             // Function to run on this core
    "shtTask",           // Name of the task
    10000,               // Stack size (bytes)
    NULL,                // Task input parameter
    1,                   // Priority (0 = lowest)
    &shtTaskHandle,      // Task handle
    0                    // Core number (0 or 1)
  );

  // Create Relay task on core 1
  xTaskCreatePinnedToCore(
    relayTask,           // Function to run on this core
    "relayTask",         // Name of the task
    10000,               // Stack size (bytes)
    NULL,                // Task input parameter
    1,                   // Priority (0 = lowest)
    &relayTaskHandle,    // Task handle
    1                    // Core number (0 or 1)
  );
}

void loop() {
  // Maintain MQTT connection
  if (!client.connected()) {
    Serial.println("Lost MQTT connection. Attempting reconnection...");
    if (client.connect(mqtt_serverID, mqtt_username, mqtt_password)) {
      Serial.println("Reconnected to MQTT broker");
      client.subscribe(Relay1);
      client.subscribe(Relay2);
    } else {
      Serial.print("Failed to reconnect, error code: ");
      Serial.println(client.state());
      delay(5000);
    }
  }
  client.loop();
}
