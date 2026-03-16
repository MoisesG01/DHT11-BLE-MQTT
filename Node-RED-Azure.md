# Enviar dados para a Azure direto pelo Node-RED

Assim você não precisa rodar o bridge em Node.js: o Node-RED lê do MQTT e envia para o Azure IoT Hub.

---

## Passo 1: Instalar o nó da Azure

1. Abra o Node-RED (http://localhost:1880).
2. Menu **≡** (canto superior direito) → **Manage palette**.
3. Aba **Install**.
4. Em "Search to add nodes" digite: **`node-red-contrib-azure-iot-hub`**.
5. Clique em **Install** ao lado do pacote e espere terminar.
6. Feche a janela (X).

---

## Passo 2: Dados que você vai precisar

Você já tem o dispositivo na Azure. Só precisa de 3 coisas:

| O quê | Valor no seu projeto |
|-------|----------------------|
| **Hostname** | `iothub-dht11-esp32.azure-devices.net` |
| **Device ID** | `esp32-dht11` |
| **Device Key** | A chave do dispositivo (está na connection string no `.env` do bridge). É a parte depois de `SharedAccessKey=` até o final. Ex.: `Gyxmm+nkZrumGG9CJKggfRKrXgfuSal/KVYjMsfKoJM=` |

Para achar a **Device Key**: abra o arquivo `azure-bridge\.env` e copie o que vem depois de `SharedAccessKey=` (sem o ponto e vírgula no final).

---

## Passo 3: Montar o fluxo (resumo)

A ideia do fluxo:

1. **Dois nós MQTT in** – um no tópico `MOISES/ESP3` (temperatura), outro em `MOISES/ESP3/humidity` (umidade).
2. **Um nó Function** – junta temperatura e umidade, espera pelo menos 30 segundos entre um envio e outro (para não estourar o limite da Azure) e monta o payload no formato que o nó da Azure espera.
3. **Um nó Azure IoT Hub** – envia esse payload para o IoT Hub.

O nó **Azure IoT Hub** espera um `msg.payload` assim:

```json
{
  "deviceId": "esp32-dht11",
  "key": "SUA_DEVICE_KEY_AQUI",
  "protocol": "mqtt",
  "data": "{\"device_id\":\"40:22:...\",\"temperature\":26.5,\"humidity\":80,\"timestamp\":\"...\"}"
}
```

- **deviceId** e **key** são do seu dispositivo na Azure.
- **protocol** pode ser `mqtt`, `amqp`, `amqpws` ou `http`.
- **data** é uma **string** (não objeto): o JSON da telemetria em texto.

---

## Passo 4: Criar o fluxo na prática

### 4.1 Nó Azure IoT Hub (configuração fixa)

1. Arraste o nó **"Azure IoT Hub"** (procure em "azure" na paleta) para a tela.
2. Dê dois cliques nele.
3. Em **Hostname** coloque: `iothub-dht11-esp32.azure-devices.net`.
4. Clique em **Done**.

### 4.2 Dois MQTT in

1. Arraste **dois** nós **"mqtt in"**.
2. Configure os dois para o **mesmo broker** que você já usa (test.mosquitto.org).
3. No primeiro: **Topic** = `MOISES/ESP3`.
4. No segundo: **Topic** = `MOISES/ESP3/humidity`.

### 4.3 Um nó Function (juntar + throttle + formato Azure)

No Node-RED, variáveis normais (`var`) dentro do Function **não guardam valor** entre uma mensagem e outra. Por isso é obrigatório usar **`context.get` / `context.set`** para lembrar a última temperatura, a última umidade e a hora do último envio. Sem isso, cada mensagem MQTT (a cada ~2 s) ia sozinha para a Azure com um valor e o outro null.

1. Arraste um nó **"function"**.
2. Conecte a **saída** dos dois **mqtt in** na **entrada** desse function (os dois ligados no mesmo function).
3. Dê dois cliques no function e cole o código abaixo.
4. **Importante:** na **primeira linha** do código, onde está `"COLE_SUA_DEVICE_KEY_AQUI"`, **substitua** pela sua Device Key (a parte `SharedAccessKey=...` do `.env`, só o valor da chave).

**Nome sugerido do nó:** `Para Azure (merge + 30s)`.

**Código (usa context para guardar estado entre mensagens — senão vira null e envia a cada 2 s):**

```javascript
const DEVICE_ID = "esp32-dht11";
const DEVICE_KEY = "COLE_SUA_DEVICE_KEY_AQUI";  // ← troque pela sua chave
const INTERVAL_MS = 30000;  // 30 segundos entre envios

// Usar context para NÃO perder os valores entre uma mensagem e outra
var last = context.get("last") || { temp: null, hum: null, deviceId: null };
var lastSend = context.get("lastSend") || 0;

var payload = msg.payload;
if (typeof payload === "string") {
  try { payload = JSON.parse(payload); } catch (e) { return null; }
}

if (msg.topic === "MOISES/ESP3" && typeof payload.temperature === "number") {
  last.deviceId = payload.device_id || last.deviceId;
  last.temp = payload.temperature;
}
if (msg.topic === "MOISES/ESP3/humidity" && typeof payload.humidity === "number") {
  last.deviceId = payload.device_id || last.deviceId;
  last.hum = payload.humidity;
}

context.set("last", last);

var now = Date.now();
// Só envia se já passou 30s E tem os dois (temp e umidade) — assim não vai null e não envia a cada 2s
if (now - lastSend < INTERVAL_MS) return null;
if (last.temp == null || last.hum == null) return null;

context.set("lastSend", now);

var data = {
  device_id: last.deviceId,
  temperature: last.temp,
  humidity: last.hum,
  timestamp: new Date().toISOString()
};

msg.payload = {
  deviceId: DEVICE_ID,
  key: DEVICE_KEY,
  protocol: "mqtt",
  data: JSON.stringify(data)
};
return msg;
```

5. Clique em **Done**.

### 4.4 Ligar ao nó da Azure

1. Conecte a **saída** do nó **function** à **entrada** do nó **Azure IoT Hub**.
2. Clique em **Deploy** (canto superior direito).

---

## Passo 5: Testar

1. Deixe o **ESP32** ligado e publicando no MQTT (como você já faz).
2. No Node-RED, abra a aba **Debug** (ícone de inseto).
3. Em até ~30 segundos deve aparecer mensagem de envio (dependendo do nó, pode aparecer "Message sent" ou o payload no debug, se você tiver ligado um debug na saída do Azure IoT Hub).
4. No **Azure IoT Explorer**, no dispositivo **esp32-dht11**, em **Telemetry**, você deve ver as mensagens chegando (uma a cada ~30 s).

---

## Resumo rápido

| O quê | Onde |
|-------|------|
| Instalar nó | Menu ≡ → Manage palette → Install → `node-red-contrib-azure-iot-hub` |
| Hostname do Hub | No nó "Azure IoT Hub": `iothub-dht11-esp32.azure-devices.net` |
| Device ID + Key | No código do function: `esp32-dht11` e a chave do `.env` (SharedAccessKey) |
| Tópicos MQTT | `MOISES/ESP3` e `MOISES/ESP3/humidity` |
| Intervalo 30 s | Está no function (`INTERVAL_MS = 30000`) para não estourar limite da Azure |

Depois que isso estiver funcionando, você pode **parar de rodar o bridge** (`azure-bridge`); o envio para a Azure passa a ser só pelo Node-RED.
