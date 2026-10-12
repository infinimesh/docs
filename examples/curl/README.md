# curl

A shell script that sends reported state to a device over the REST API and reads it back.

## What it does

1. Gets a device token. Either you pass one, or the script makes one from an account login.
2. Sends `{"temp": 21.5, "source": "curl"}` as reported state with `POST /devices/states`.
3. Reads the state with `GET /devices/states` and prints this device's shadow.

## Config

| Variable | Meaning |
| --- | --- |
| `INF_API` | REST gateway URL, for example `https://api.example.com` |
| `DEVICE_ID` | Device uuid |
| `DEVICE_TOKEN` | Device token. Optional if you set `INF_USER` and `INF_PASS` |
| `INF_USER`, `INF_PASS` | Account that can manage the device. Used only to make a token |
| `TEMP_VALUE` | Value to send. Default `21.5` |

Needs `curl` and `python3`.

## Get a device token

With the inf CLI:

```bash
inf login api.example.com:443 <user> <password>
inf devices token <device-id> --allow-post
```

With the API (this is what the script does):

```bash
ACCOUNT=$(curl -s -X POST $INF_API/token \
  -d '{"auth":{"type":"standard","data":["<user>","<password>"]}}' | jq -r .token)
curl -s -X POST $INF_API/devices/token -H "Authorization: Bearer $ACCOUNT" \
  -d '{"devices":{"<device-id>":"MGMT"},"post":true}' | jq -r .token
```

Device tokens do not expire today. Keep them secret.

## Run

```bash
INF_API=https://api.example.com DEVICE_ID=<uuid> DEVICE_TOKEN=<token> ./infinimesh.sh
```

## Expected output

```
Posting reported state: temp=21.5
Reading state back
{
  "device": "335ef65d-b82e-4b05-9c44-c14218713ebc",
  "reported": {
    "timestamp": "2026-10-12T11:39:33.291579569Z",
    "data": {
      "source": "curl",
      "temp": 21.5
    }
  },
  ...
}
```

## Tested

Run live against infinimesh core (docker compose, REST gateway) on 12 October 2026, with both token paths.
