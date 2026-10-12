# picow-mqtt

A MicroPython script for the Raspberry Pi Pico W that talks to infinimesh over MQTT with TLS.

## What it does

- Connects to WiFi and sets the clock with NTP.
- Connects to the MQTT bridge on port 8883 with the device certificate.
- Every 10 seconds publishes `{"temp": 21.5, "uptime": ...}` to `devices/<id>/state/reported` at QoS 1.
- Subscribes to `devices/<id>/state/desired` and prints desired state.

## Wiring

None.

## Setup

1. Flash a recent MicroPython for the Pico W (1.23 or newer, it has `ssl.SSLContext`).
2. Install the MQTT library: `mpremote mip install umqtt.simple`
3. Make the device key and certificate (EC P-256 is fastest) and convert them to DER:

```bash
openssl ecparam -name prime256v1 -genkey -noout -out device.key
openssl req -new -x509 -sha256 -key device.key -out device.crt -days 365 -subj "/CN=my-pico"
openssl x509 -in device.crt -outform DER -out device.der
openssl ec -in device.key -outform DER -out key.der
openssl x509 -in isrgrootx1.pem -outform DER -out ca.der   # or your server CA
```

4. Register `device.crt` when you create the device (see [../esp32-mqtt/README.md](../esp32-mqtt/README.md)).
5. Edit the config at the top of `main.py`, then copy the files:

```bash
mpremote cp device.der key.der ca.der main.py :
mpremote reset
```

## Expected output

```
WiFi connected 192.168.1.43
MQTT connected
Published {"temp": 21.5, "uptime": 14}
Desired state on devices/<id>/state/desired : {"led":true}
```

## Tested

- `main.py` is compiled with `mpy-cross` in CI.
- The MQTT flow was tested from a desktop client against core, see the esp32-mqtt README.
- It has not been run on a real Pico W yet.
