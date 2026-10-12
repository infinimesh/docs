#!/usr/bin/env bash
# Send reported state to an infinimesh device over the REST API and read it back.
#
# Needs: curl, and python3 or jq for reading JSON.
#
# Config (environment variables):
#   INF_API       REST gateway URL, for example https://api.example.com
#   DEVICE_ID     the device uuid
#   DEVICE_TOKEN  a device token. If empty, the script makes one with
#                 INF_USER and INF_PASS (an account that can manage the device).
set -euo pipefail

: "${INF_API:?set INF_API, for example https://api.example.com}"
: "${DEVICE_ID:?set DEVICE_ID to the device uuid}"
DEVICE_TOKEN="${DEVICE_TOKEN:-}"

json_get() {
  # json_get <python expression on d>
  python3 -c "import sys, json; d = json.load(sys.stdin); print($1)"
}

if [ -z "$DEVICE_TOKEN" ]; then
  : "${INF_USER:?set DEVICE_TOKEN, or INF_USER and INF_PASS}"
  : "${INF_PASS:?set INF_PASS}"
  echo "Getting an account token for $INF_USER" >&2
  account_token=$(curl -fsS -X POST "$INF_API/v1/token" \
    -d "{\"auth\":{\"type\":\"standard\",\"data\":[\"$INF_USER\",\"$INF_PASS\"]}}" \
    | json_get 'd["token"]')
  echo "Getting a device token for $DEVICE_ID" >&2
  DEVICE_TOKEN=$(curl -fsS -X POST "$INF_API/v1/devices/token" \
    -H "Authorization: Bearer $account_token" \
    -d "{\"devices\":{\"$DEVICE_ID\":\"MGMT\"},\"post\":true}" \
    | json_get 'd["token"]')
fi

temp="${TEMP_VALUE:-21.5}"
echo "Posting reported state: temp=$temp" >&2
# The body is your own JSON object, at most 16 KB. Success is 204 No Content.
curl -fsS -X POST "$INF_API/v1/ingest" \
  -H "Authorization: Bearer $DEVICE_TOKEN" \
  -H "Content-Type: application/json" \
  -d "{\"temp\":$temp,\"source\":\"curl\"}"

echo "Reading desired state" >&2
curl -fsS "$INF_API/v1/ingest/desired" -H "Authorization: Bearer $DEVICE_TOKEN"
echo
