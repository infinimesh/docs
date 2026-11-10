# Python MQTT example

This example demonstrates how to connect a Python application to infinimesh using MQTT over TLS and publish device reported state.

## Requirements

- Python 3.9+
- A registered infinimesh device
- Device client certificate and private key
- MQTT broker hostname

## Installation

Run these commands from the repository root:

```bash
cd examples/python-mqtt
python3 -m venv .venv
source .venv/bin/activate
python -m pip install "paho-mqtt>=2.0"
```

## Configuration

Configure the following values at the top of `main.py`:

- `MQTT_HOST` – MQTT broker hostname
- `MQTT_PORT` – MQTT TLS port (`8883`)
- `DEVICE_ID` – registered device UUID
- `CA_FILE` – server CA certificate, or `None` to use the system CA store
- `CERT_FILE` – device client certificate
- `KEY_FILE` – device private key

Place the certificate and private key in this directory. The client certificate must be registered with infinimesh.

Never commit your private key or device certificates.

## Running

Run the example from this directory:

```bash
python main.py
```

The client connects using mutual TLS authentication and publishes a JSON payload to:

`devices/<device-id>/state/reported`

The message is published with QoS 1. The script waits for the broker's PUBACK acknowledgement before disconnecting.

## Example payload

```json
{
  "temp": 21.5,
  "uptime": 0
}
```

This example publishes reported state only. The desired state topic is:

`devices/<device-id>/state/desired`

The example does not subscribe to desired state.
