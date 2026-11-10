#!/usr/bin/env python3
"""Publish reported state to infinimesh over MQTT with TLS."""

import json
import ssl
import threading

import paho.mqtt.client as mqtt

# ---- Config: change these ----
MQTT_HOST = "mqtt.example.com"
MQTT_PORT = 8883
DEVICE_ID = "00000000-0000-0000-0000-000000000000"
CA_FILE = None             # Server CA; None uses the system CA store
CERT_FILE = "device.crt"   # Device certificate registered in infinimesh
KEY_FILE = "device.key"    # Device private key
# ---- End of config ----

TOPIC_REPORTED = f"devices/{DEVICE_ID}/state/reported"

connected = threading.Event()
connection_error = []


def on_connect(client, userdata, flags, reason_code, properties):
    if reason_code.is_failure:
        connection_error.append(str(reason_code))
    connected.set()


def make_client():
    context = ssl.create_default_context(cafile=CA_FILE)
    context.minimum_version = ssl.TLSVersion.TLSv1_2
    context.load_cert_chain(CERT_FILE, KEY_FILE)

    client = mqtt.Client(
        callback_api_version=mqtt.CallbackAPIVersion.VERSION2,
        client_id=DEVICE_ID,
    )
    client.tls_set_context(context)
    client.on_connect = on_connect
    return client


def main():
    client = make_client()

    print(f"Connecting to {MQTT_HOST}:{MQTT_PORT}...")
    client.connect(MQTT_HOST, MQTT_PORT, keepalive=60)
    client.loop_start()

    try:
        if not connected.wait(timeout=10):
            raise TimeoutError("Timed out waiting for MQTT connection")

        if connection_error:
            raise ConnectionError(
                f"MQTT connection failed: {connection_error[0]}"
            )

        payload = json.dumps({"temp": 21.5, "uptime": 0})
        info = client.publish(TOPIC_REPORTED, payload, qos=1)

        if info.rc != mqtt.MQTT_ERR_SUCCESS:
            raise RuntimeError(f"MQTT publish failed: {info.rc}")

        info.wait_for_publish(timeout=10)

        if not info.is_published():
            raise TimeoutError("Timed out waiting for MQTT PUBACK")

        print(f"Published {payload} to {TOPIC_REPORTED} (QoS 1)")
    finally:
        client.disconnect()
        client.loop_stop()


if __name__ == "__main__":
    main()
