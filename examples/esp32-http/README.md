# esp32-http

An Arduino sketch for the ESP32 that talks to infinimesh over HTTPS with a device token.

## What it does

- Connects to WiFi and sets the clock with NTP.
- Every 30 seconds sends reported state with `POST /v1/ingest`.
- Then reads `GET /v1/ingest/desired` and prints it.

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

`POST /v1/ingest` takes your own JSON object, at most 16 KB:

```json
{"temp": 21.5, "uptime": 5}
```

It becomes the reported state of the device named in the token. Success is `204`. `400` means the body is not a JSON object or is too large, `401` a bad token, `403` a token that names more than one device or may not post, and `429` more than 60 requests a minute. `GET /v1/ingest/desired` returns the desired JSON, for example `{"led": true}`.

## Get a device token

```bash
inf devices token <device-id> --allow-post
```

or with the API, see [../curl/README.md](../curl/README.md). The token must name only this device. Without `expires_at` it expires after 24 hours, so long-running devices need a token with a later `expires_at`.

## Build

```bash
arduino-cli compile --fqbn esp32:esp32:esp32 examples/esp32-http
```

## Expected output (serial, 115200)

```text
WiFi..... connected
POST /v1/ingest -> 204, sent {"temp":24.1,"uptime":5}
GET /v1/ingest/desired -> 200, desired: {"led":true}
```

## Tested

- Compiles in CI for `esp32:esp32:esp32`.
- The same requests (body, headers, token) are the ones the [curl](../curl) script sends.
- It has not been run on a real ESP32 yet.
