# esp32-http

An Arduino sketch for the ESP32 that talks to infinimesh over HTTPS with a device token.

## What it does

- Connects to WiFi and sets the clock with NTP.
- Every 30 seconds sends reported state with `POST /devices/states`.
- Then reads `GET /devices/states` and prints the desired part.

Good for boards that wake up, send one value and go back to sleep.

## Wiring

None. Any ESP32 dev board works.

## Config

Edit the top of `esp32-http.ino`:

| Setting | Meaning |
| --- | --- |
| `WIFI_SSID`, `WIFI_PASS` | Your WiFi |
| `API_URL` | REST gateway URL, for example `https://api.example.com` |
| `DEVICE_ID` | Device uuid |
| `DEVICE_TOKEN` | Device token |
| `CA_CERT` | CA of the API certificate. ISRG Root X1 for Let's Encrypt |

## Request body

`POST /devices/states` takes a Shadow:

```json
{"device": "<device-id>", "reported": {"data": {"temp": 21.5}}}
```

Your own JSON goes inside `reported.data`. `GET /devices/states` returns `{"shadows": [...]}` for every device in the token.

## Get a device token

```bash
inf devices token <device-id> --allow-post
```

or with the API, see [../curl/README.md](../curl/README.md). Device tokens do not expire today.

## Build

```bash
arduino-cli compile --fqbn esp32:esp32:esp32 examples/esp32-http
```

## Expected output (serial, 115200)

```
WiFi..... connected
POST /devices/states -> 200, sent {"device":"<id>","reported":{"data":{"temp":24.1,"uptime":5}}}
GET /devices/states -> 200, desired: "desired":{"timestamp":"...","data":{"led":true}
```

## Note

This endpoint may move to a smaller `/v1/ingest` endpoint later.

## Tested

- Compiles in CI for `esp32:esp32:esp32`.
- The same requests (body, headers, token) were tested live against core with the [curl](../curl) script on 12 October 2026.
- It has not been run on a real ESP32 yet.
