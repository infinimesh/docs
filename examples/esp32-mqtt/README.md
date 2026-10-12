# esp32-mqtt

An Arduino sketch for the ESP32 that talks to infinimesh over MQTT with TLS.

## What it does

- Connects to WiFi and sets the clock with NTP (TLS needs the right time).
- Connects to the infinimesh MQTT bridge on port 8883. The bridge knows the device by its TLS client certificate. There is no username or password.
- Every 10 seconds publishes JSON such as `{"temp":23.4,"uptime":120}` to `devices/<id>/state/reported` at QoS 1 (set `PUBLISH_QOS` to 0 if you prefer).
- Subscribes to `devices/<id>/state/desired` and prints desired state when it changes.

It uses ESP-MQTT (`mqtt_client.h`), which ships with the Arduino esp32 core 3.x. No extra library is needed.

## Wiring

None. Any ESP32 dev board works.

## Config

Edit the top of `esp32-mqtt.ino`:

| Setting | Meaning |
| --- | --- |
| `WIFI_SSID`, `WIFI_PASS` | Your WiFi |
| `MQTT_URI` | `mqtts://<your mqtt host>:8883` |
| `DEVICE_ID` | Device uuid |
| `CA_CERT` | CA of the server certificate. ISRG Root X1 for Let's Encrypt |
| `DEVICE_CERT`, `DEVICE_KEY` | The device certificate and its private key |

## Get the device certificate

Make a key and a certificate. An EC P-256 key is much faster on small chips than RSA:

```bash
openssl ecparam -name prime256v1 -genkey -noout -out device.key
openssl req -new -x509 -sha256 -key device.key -out device.crt -days 365 -subj "/CN=my-sensor"
```

Register the certificate when you create the device. With the API:

```bash
curl -X PUT $INF_API/devices -H "Authorization: Bearer $ACCOUNT" -d "$(jq -n \
  --arg pem "$(cat device.crt)" \
  '{namespace:"infinimesh", device:{title:"my-sensor", enabled:true, certificate:{pem_data:$pem}}}')"
```

The reply has the device `uuid`. The CLI (`inf devices create`) takes a device template file, see `inf devices create --help`.

## Build

```bash
arduino-cli core install esp32:esp32
arduino-cli compile --fqbn esp32:esp32:esp32 examples/esp32-mqtt
```

## Expected output (serial, 115200)

```text
WiFi..... connected, IP 192.168.1.42
MQTT connected
Published {"temp":23.4,"uptime":12} (id 12345)
PUBACK for message 12345
Desired state on devices/<id>/state/desired: {"led":true}
```

## Tested

- The sketch compiles in CI for `esp32:esp32:esp32`.
- The same flow was tested live against core main from a desktop MQTT client (Python paho) on 12 October 2026: TLS on 8883 with an EC P-256 device certificate, publish at QoS 0 and 1 (PUBACK received), desired state received after an API update.
- It has not been run on a real ESP32 yet.

If the handshake times out on smaller chips, read [guides/small-devices-tls.md](../../guides/small-devices-tls.md).
