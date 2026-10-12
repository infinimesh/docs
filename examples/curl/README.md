# curl

A shell script that sends reported state to a device over the REST API and reads it back.

## What it does

1. Gets a device token. Either you pass one, or the script makes one from an account login.
2. Sends `{"temp": 21.5, "source": "curl"}` as reported state with `POST /v1/ingest`.
3. Reads the desired state with `GET /v1/ingest/desired` and prints it.

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
ACCOUNT=$(curl -s -X POST $INF_API/v1/token \
  -d '{"auth":{"type":"standard","data":["<user>","<password>"]}}' | jq -r .token)
curl -s -X POST $INF_API/v1/devices/token -H "Authorization: Bearer $ACCOUNT" \
  -d '{"devices":{"<device-id>":"MGMT"},"post":true}' | jq -r .token
```

The token must name exactly one device and be allowed to post (`"post":true`). Without `expires_at` it expires after 24 hours. Keep it secret.

## Run

```bash
INF_API=https://api.example.com DEVICE_ID=<uuid> DEVICE_TOKEN=<token> ./infinimesh.sh
```

## The ingest API

`POST /v1/ingest` with `Authorization: Bearer <device token>` takes your own JSON object as the body. It becomes the reported state of the device in the token.

```bash
curl -X POST $INF_API/v1/ingest -H "Authorization: Bearer $DEVICE_TOKEN" \
  -H "Content-Type: application/json" -d '{"temp":21.5}'
```

- `204 No Content`: stored.
- `400`: the body is not a JSON object, or it is larger than 16 KB.
- `401`: the bearer token is missing or invalid.
- `403`: the token names more than one device, or it may not post.
- `429`: more than 60 requests a minute for this device.

`GET /v1/ingest/desired` returns the desired JSON with an `ETag`. Send that `ETag` back as `If-None-Match` and add `?wait=30` (0 to 60 seconds) to wait for a change. It returns `304` when nothing changed.

## Expected output

```text
Posting reported state: temp=21.5
Reading desired state
{}
```

## Tested

The request and response codes follow core `pkg/ingest/handler.go` and [docs/api-versioning.md](https://github.com/infinimesh/core/blob/main/docs/api-versioning.md). On 12 October 2026 these requests were run against core's ingest handler from main: `204` for a post, `200` with the desired JSON, `400` for a non-object body, `401` without a token and `429` over the rate limit. The token calls were not rerun on the `/v1` paths.
