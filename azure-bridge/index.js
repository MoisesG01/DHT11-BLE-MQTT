/**
 * Bridge: MQTT (test.mosquitto.org) → Azure IoT Hub
 * Subscreve nos tópicos do ESP32 e envia telemetria para a Azure.
 * Uso: copie .env.example para .env, preencha IOT_HUB_DEVICE_CONNECTION_STRING e rode npm start
 */

require('dotenv').config();
const mqtt = require('mqtt');
const { Client, Message } = require('azure-iot-device');
const { Mqtt: MqttTransport } = require('azure-iot-device-mqtt');

const MQTT_BROKER = process.env.MQTT_BROKER || 'mqtt://test.mosquitto.org:1883';
const MQTT_TOPIC_TEMP = process.env.MQTT_TOPIC_TEMP || 'MOISES/ESP3';
const MQTT_TOPIC_HUM = process.env.MQTT_TOPIC_HUM || 'MOISES/ESP3/humidity';
const CONNECTION_STRING = process.env.IOT_HUB_DEVICE_CONNECTION_STRING;
// Intervalo entre envios para a Azure (evita estourar o limite de 8000 msg/dia). Ex: 30000 = 30s → ~2880 msg/dia
const SEND_INTERVAL_MS = parseInt(process.env.AZURE_SEND_INTERVAL_MS || '30000', 10) || 30000;

if (!CONNECTION_STRING) {
  console.error('Defina IOT_HUB_DEVICE_CONNECTION_STRING no .env (copie de .env.example)');
  process.exit(1);
}

const client = Client.fromConnectionString(CONNECTION_STRING, MqttTransport);
let lastPayload = { device_id: null, temperature: null, humidity: null, timestamp: null };
let lastSendTime = 0;

function maybeSendToAzure() {
  const now = Date.now();
  if (now - lastSendTime < SEND_INTERVAL_MS) return;
  if (lastPayload.temperature == null && lastPayload.humidity == null) return;

  const payload = {
    device_id: lastPayload.device_id || 'unknown',
    temperature: lastPayload.temperature,
    humidity: lastPayload.humidity,
    timestamp: new Date().toISOString()
  };
  lastSendTime = Date.now();
  const msg = new Message(JSON.stringify(payload));
  client.sendEvent(msg, (err) => {
    if (err) console.error('Azure send error:', err.message);
    else console.log('Azure OK:', payload.device_id, payload.temperature != null ? payload.temperature + 'C' : '', payload.humidity != null ? payload.humidity + '%' : '', '(intervalo', SEND_INTERVAL_MS / 1000, 's)');
  });
}

function onMqttMessage(topic, raw) {
  try {
    const payload = JSON.parse(raw.toString());
    const deviceId = payload.device_id || 'unknown';

    if (topic === MQTT_TOPIC_TEMP && typeof payload.temperature === 'number') {
      lastPayload.device_id = deviceId;
      lastPayload.temperature = payload.temperature;
      lastPayload.timestamp = new Date().toISOString();
      maybeSendToAzure();
      return;
    }

    if (topic === MQTT_TOPIC_HUM && typeof payload.humidity === 'number') {
      lastPayload.device_id = deviceId;
      lastPayload.humidity = payload.humidity;
      lastPayload.timestamp = new Date().toISOString();
      maybeSendToAzure();
    }
  } catch (e) {
    console.error('Parse error:', e.message);
  }
}

async function main() {
  await new Promise((resolve, reject) => {
    client.open((err) => {
      if (err) return reject(err);
      console.log('Conectado ao Azure IoT Hub');
      resolve();
    });
  });

  const mqttClient = mqtt.connect(MQTT_BROKER);
  mqttClient.on('connect', () => {
    console.log('Conectado ao MQTT:', MQTT_BROKER);
    mqttClient.subscribe([MQTT_TOPIC_TEMP, MQTT_TOPIC_HUM], (err) => {
      if (err) console.error('Subscribe error:', err);
      else console.log('Inscrito em', MQTT_TOPIC_TEMP, 'e', MQTT_TOPIC_HUM);
    });
  });
  mqttClient.on('message', onMqttMessage);
  mqttClient.on('error', (err) => console.error('MQTT error:', err));
}

main().catch((err) => {
  console.error(err);
  process.exit(1);
});
