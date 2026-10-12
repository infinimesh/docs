# Node-RED with infinimesh

Node-RED can read device state from infinimesh in two ways. Both use an addon from [infinimesh/addons](https://github.com/infinimesh/addons), so you do not need any Node-RED plugin.

- **MQTT bridge addon**: infinimesh publishes every shadow update to an MQTT broker, and Node-RED subscribes with an `mqtt in` node.
- **Webhook addon**: infinimesh posts every shadow update to an `http in` node in Node-RED.

Both send the same JSON:

```json
{
  "device": "f3913526-38d9-4bf2-a60d-b616d6ec8dcc",
  "title": "pump-7",
  "reported": {"timestamp": "2026-10-12T08:19:51.617Z", "data": {"temp": 20, "hum": 40}},
  "desired": {"timestamp": "2026-10-12T08:10:02.100Z", "data": {"fan": "on"}},
  "connection": {"connected": true, "timestamp": "2026-10-12T08:00:00Z", "client_id": "pump-7"},
  "sent_at": "2026-10-12T08:19:51.618Z"
}
```

Parts that are not in the update are left out.

## Option 1: MQTT bridge

Use this if you already run a broker such as Mosquitto, or if Node-RED cannot be reached over HTTP.

1. Set these for the [mqttbridge addon](https://github.com/infinimesh/addons/tree/main/mqttbridge):

   ```bash
   USERNAME=infinimesh
   PASSWORD=infinimesh
   API_URL=<your infinimesh gRPC address>
   MQTT_BROKER=tcp://mosquitto:1883
   MQTT_TOPIC=infinimesh/devices/{device}/shadow
   MQTT_QOS=1
   MQTT_RETAIN=true
   ```

   `{device}` is the device id and `{title}` is the device name, so `site/{title}/{device}` also works. `MQTT_RETAIN=true` means Node-RED gets the last state of each device as soon as it subscribes.

2. Start it:

   ```bash
   docker compose -f mqttbridge/docker-compose.yml up --build
   ```

3. In Node-RED, add an `mqtt in` node:
   - Server: the same broker
   - Topic: `infinimesh/devices/+/shadow`
   - Output: "a parsed JSON object"

4. Wire it to a `debug` node and deploy. `msg.payload.reported.data` holds the values the device sent.

The bridge only publishes. To change a device's desired state from Node-RED, call the infinimesh API with an `http request` node.

## Option 2: Webhook

Use this if Node-RED can be reached over HTTP from where the addon runs.

1. In Node-RED, add an `http in` node with method `POST` and URL `/infinimesh`. Wire it to an `http response` node (status 200) and to a `debug` node. Deploy.

2. Set these for the [webhook addon](https://github.com/infinimesh/addons/tree/main/webhook):

   ```bash
   USERNAME=infinimesh
   PASSWORD=infinimesh
   API_URL=<your infinimesh gRPC address>
   WEBHOOK_URL=http://node-red:1880/infinimesh
   WEBHOOK_SECRET=<a long random string>
   ```

3. Start it:

   ```bash
   docker compose -f webhook/docker-compose.yml up --build
   ```

Always answer with a 2xx. Network errors, timeouts, 408, 429 and 5xx are retried. Other answers are logged and the update is dropped.

### Check the signature

With `WEBHOOK_SECRET` set, each request has an `X-Infinimesh-Signature: sha256=<hex>` header with the HMAC-SHA256 of the raw body. The `http in` node parses JSON, so the raw bytes are gone by the time your flow sees them. If you need to check the signature, put Node-RED behind a proxy that checks it, or set `httpNodeMiddleware` in `settings.js` to keep the raw body and compare it in a `function` node with `crypto.timingSafeEqual`.

## Which one?

| | MQTT bridge | Webhook |
| --- | --- | --- |
| Needs a broker | yes | no |
| Node-RED must be reachable | no | yes |
| Last state on start | yes, with retain | no |
| Retries | broker reconnect only | yes, with backoff |

Neither addon keeps a disk queue, so updates sent while Node-RED or the broker is down for longer than the retries can be lost.
