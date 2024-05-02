#include <Arduino.h>
#include <DHT.h>
#include <BLEServer.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include "Credentials.h"
#include <iostream>

using namespace std;

#define LEDPIN 2
#define DHTPIN 5
#define DHTTYPE DHT11
#define READ_INTERVAL 2000
#define SERVICE_UUID "2aeb149a-4803-11ee-be56-0242ac120002"
#define HUMIDITY_UUID "3ab43154-4803-11ee-be56-0242ac120002"
#define TEMPERATURE_UUID "404dccd8-4803-11ee-be56-0242ac120002"

// Set MQTT Broker

const char *mqtt_broker = "test.mosquitto.org";
const char *topic = "MOISES/ESP3"; //Set the topic here before -- DO NOT FORGET TO SET THE TOPIC HERE
const char *topic2 = "MOISES/ESP2";
const char *mqqtt_username = "";
const char *mqtt_password = "";
const int mqtt_port = 1883;

bool mqttStatus = 0;

WiFiClient espClient;
PubSubClient client(espClient);

bool connectMQTT();
void callback(char *topic, byte *payload, unsigned int lenght);

WiFiServer server_wifi(80);

DHT dht(DHTPIN, DHTTYPE);

BLEServer *server = nullptr;
BLECharacteristic *temperatureChar = nullptr;
BLECharacteristic *humidityChar = nullptr;

float lastTemperature = -999;
float lastHumidity = -999;
int devicesConnected = 0;
unsigned int blinkMillis = 0;
unsigned int readkMillis = 0;

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

  mqttStatus = connectMQTT();

  dht.begin();
  pinMode(LEDPIN, OUTPUT);

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

//Adicionar a linha de código para publicar as variáveis aqui
client.publish(topic, String(temperature).c_str());


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
  if(mqttStatus){
    client.loop();
  }
  while(WiFi.status() != WL_CONNECTED){
    WiFi.begin(ssid, password);
    Serial.print(".");

    delay(741);
  }
  if(readkMillis == 0 || (millis() - readkMillis) >= READ_INTERVAL) {
      sense();
      readkMillis = millis();
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
      Serial.println("Connection sucessful: ");
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
    
    client.subscribe(topic);
    return 1;
  } else {
    Serial.println("Do not connected!");
    return 0;
  }
}

void callback(char *topic, byte *payload, unsigned int length){
  Serial.print("Message arrived in topic: ");
  Serial.print(topic);
  Serial.print("Message: ");
  for (int i = 0; i < length; i++) {
    Serial.print((char) payload[i]);
  }
  Serial.println();
  Serial.println("---------------------------");
}