#include <Arduino.h>
#include <DHT.h>
#include <BLEServer.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include "Credentials.h"

#define LEDPIN 2
#define LED_RED 18
#define LED_GREEN 19
#define LED_YELLOW 21
#define LED_CONTROL 22
#define DHTPIN 5
#define DHTTYPE DHT11
#define READ_INTERVAL 2000

// Limiares de temperatura para os LEDs (graus Celsius)
#define TEMP_LOW  20.0f   // Abaixo: amarelo (frio)
#define TEMP_HIGH 28.0f   // Acima: vermelho (quente). Entre os dois: verde (ok)
#define SERVICE_UUID "2aeb149a-4803-11ee-be56-0242ac120002"
#define HUMIDITY_UUID "3ab43154-4803-11ee-be56-0242ac120002"
#define TEMPERATURE_UUID "404dccd8-4803-11ee-be56-0242ac120002"

// Set MQTT Broker

const char *mqtt_broker = "test.mosquitto.org";
const char *topic = "MOISES/ESP3"; //Set the topic here before -- DO NOT FORGET TO SET THE TOPIC HERE
const char *topic_humidity = "MOISES/ESP3/humidity";
const char *topic_led = "MOISES/ESP3/led";
const char *topic2 = "MOISES/ESP2";
const char *mqqtt_username = "";
const char *mqtt_password = "";
const int mqtt_port = 1883;

bool mqttStatus = 0;

WiFiClient espClient;
PubSubClient client(espClient);

bool connectMQTT();
void callback(char *topic, byte *payload, unsigned int length);

DHT dht(DHTPIN, DHTTYPE);

BLEServer *server = nullptr;
BLECharacteristic *temperatureChar = nullptr;
BLECharacteristic *humidityChar = nullptr;

String deviceId;
float lastTemperature = -999;
float lastHumidity = -999;
int devicesConnected = 0;
unsigned int blinkMillis = 0;
unsigned int readMillis = 0;
unsigned long lastMqttReconnect = 0;
#define MQTT_RECONNECT_INTERVAL 5000

class ServerCallbacks: public BLEServerCallbacks{
    void onConnect(BLEServer *s){
      Serial.println("Device connected");
      devicesConnected++;
      BLEDevice::startAdvertising();
    }

    void onDisconnect(BLEServer *s){
      Serial.println("Device disconnected");
      devicesConnected--;
    }
};

void setup() {
  Serial.begin(9600);

  Serial.println("Starting...");

  Serial.println();
  Serial.println("Connecting in ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  while(WiFi.status() != WL_CONNECTED){
    delay(741);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected!");
  Serial.println("IP: ");
  Serial.println(WiFi.localIP());

  deviceId = WiFi.macAddress();
  Serial.print("Device ID (MAC): ");
  Serial.println(deviceId);

  mqttStatus = connectMQTT();

  dht.begin();
  pinMode(LEDPIN, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_YELLOW, OUTPUT);
  pinMode(LED_CONTROL, OUTPUT);

  BLEDevice::init("ESP32");
  server = BLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());

  BLEService *service = server->createService(SERVICE_UUID);

  temperatureChar = service->createCharacteristic(
    TEMPERATURE_UUID,
    BLECharacteristic::PROPERTY_READ |
    BLECharacteristic::PROPERTY_NOTIFY
  );

  humidityChar = service->createCharacteristic(
    HUMIDITY_UUID,
    BLECharacteristic::PROPERTY_READ |
    BLECharacteristic::PROPERTY_NOTIFY
  );

  service->start();

  BLEAdvertising *advertising = BLEDevice::getAdvertising();
  advertising->addServiceUUID(SERVICE_UUID);
  advertising->setScanResponse(false);
  advertising->setMinPreferred(0x06);

  BLEDevice::startAdvertising();

  Serial.println("Advertising...");
}

void sense() {
  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();

  if(isnan(humidity)){
    Serial.println("Humidity reading failed!");
    return;
  }

  if(isnan(temperature)){
    Serial.println("Temperature reading failed!");
    return;
  }

  Serial.printf("Humidity = %f | Temperature = %f \n", humidity, temperature);

  if (client.connected()) {
    String payloadTemp = "{\"device_id\":\"" + deviceId + "\",\"temperature\":" + String(temperature) + "}";
    String payloadHum = "{\"device_id\":\"" + deviceId + "\",\"humidity\":" + String(humidity) + "}";
    client.publish(topic, payloadTemp.c_str());
    client.publish(topic_humidity, payloadHum.c_str());
  }

  // LEDs por faixa de temperatura: apenas um aceso
  digitalWrite(LED_RED, LOW);
  digitalWrite(LED_GREEN, LOW);
  digitalWrite(LED_YELLOW, LOW);
  if (temperature < TEMP_LOW) {
    digitalWrite(LED_YELLOW, HIGH);   // Frio
  } else if (temperature > TEMP_HIGH) {
    digitalWrite(LED_RED, HIGH);      // Quente
  } else {
    digitalWrite(LED_GREEN, HIGH);    // Normal
  }

  if(devicesConnected){
    if (lastTemperature != temperature){
      temperatureChar->setValue(temperature);
      temperatureChar->notify();

      lastTemperature = temperature;
    }
  
    if (lastHumidity != humidity){
      humidityChar->setValue(humidity);
      humidityChar->notify();

      lastHumidity = humidity;
    }
  }
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.begin(ssid, password);
    Serial.print(".");
    delay(741);
    return;
  }

  if (client.connected()) {
    client.loop();
  } else if (millis() - lastMqttReconnect >= MQTT_RECONNECT_INTERVAL) {
    lastMqttReconnect = millis();
    mqttStatus = connectMQTT();
  }

  if (readMillis == 0 || (millis() - readMillis) >= READ_INTERVAL) {
    sense();
    readMillis = millis();
  }

  if (!devicesConnected){
    if (blinkMillis == 0 || (millis() - blinkMillis) >= 1000) {
      digitalWrite(LEDPIN, !digitalRead(LEDPIN));
      blinkMillis = millis();
    }
  } else {
    digitalWrite(LEDPIN, HIGH);
  }
}

bool connectMQTT() {
  byte tentativa = 0;
  client.setServer(mqtt_broker, mqtt_port);
  client.setCallback(callback);

  do{
    String client_id = "TEST-";
    client_id += String(WiFi.macAddress());

    if (client.connect(client_id.c_str(), mqqtt_username, mqtt_password)) {
      Serial.println("Connection successful: ");
      Serial.printf("Client %s connected on Broker \n", client_id.c_str());
    } else {
      Serial.print("Connection failed: ");
      Serial.print(client.state());
      Serial.println();
      Serial.print("New try: ");
      delay(2000);
    }
    tentativa++;
  } while (!client.connected() && tentativa < 5);

  if (tentativa < 5) {
    client.subscribe(topic_led);
    return 1;
  } else {
    Serial.println("Do not connected!");
    return 0;
  }
}

void callback(char *topicName, byte *payload, unsigned int length){
  Serial.print("Message arrived in topic: ");
  Serial.print(topicName);
  Serial.print(" Message: ");

  // Constroi string do payload
  char payloadStr[16];
  for (unsigned int i = 0; i < length && i < 15; i++) {
    payloadStr[i] = (char) payload[i];
  }
  payloadStr[length < 15 ? length : 15] = '\0';
  Serial.println(payloadStr);

  // Controla o LED no GPIO 22
  if (strcmp(topicName, topic_led) == 0) {
    if (strcmp(payloadStr, "1") == 0 || strcmp(payloadStr, "on") == 0 || strcmp(payloadStr, "high") == 0) {
      digitalWrite(LED_CONTROL, HIGH);
      Serial.println("LED ON");
    } else if (strcmp(payloadStr, "0") == 0 || strcmp(payloadStr, "off") == 0 || strcmp(payloadStr, "low") == 0) {
      digitalWrite(LED_CONTROL, LOW);
      Serial.println("LED OFF");
    }
  }

  Serial.println("---------------------------");
}