# infinimesh Raspberry Pi Pico W example (MicroPython).
# Connects over TLS on port 8883 with the device client certificate,
# publishes JSON to devices/<id>/state/reported every 10 seconds,
# and prints desired state from devices/<id>/state/desired.
#
# Needs umqtt.simple: run `mpremote mip install umqtt.simple` once.
# Copy device.der, key.der and ca.der to the board (see README).

import json
import time

import network
import ntptime
import ssl
from umqtt.simple import MQTTClient

# ---- Config: change these ----
WIFI_SSID = "your-wifi"
WIFI_PASS = "your-password"
MQTT_HOST = "mqtt.example.com"
MQTT_PORT = 8883
DEVICE_ID = "00000000-0000-0000-0000-000000000000"
CA_FILE = "ca.der"          # CA that signed the server certificate
CERT_FILE = "device.der"    # device certificate registered in infinimesh
KEY_FILE = "key.der"        # device private key
# ---- End of config ----

TOPIC_REPORTED = "devices/%s/state/reported" % DEVICE_ID
TOPIC_DESIRED = "devices/%s/state/desired" % DEVICE_ID


def connect_wifi():
    wlan = network.WLAN(network.STA_IF)
    wlan.active(True)
    wlan.connect(WIFI_SSID, WIFI_PASS)
    while not wlan.isconnected():
        print("WiFi connecting...")
        time.sleep(1)
    print("WiFi connected", wlan.ifconfig()[0])
    # TLS checks certificate dates, so set the clock.
    ntptime.settime()


def on_message(topic, msg):
    print("Desired state on", topic.decode(), ":", msg.decode())


def make_client():
    ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
    ctx.verify_mode = ssl.CERT_REQUIRED
    ctx.load_verify_locations(cafile=CA_FILE)
    ctx.load_cert_chain(CERT_FILE, KEY_FILE)
    client = MQTTClient(
        client_id=DEVICE_ID,
        server=MQTT_HOST,
        port=MQTT_PORT,
        keepalive=30,
        ssl=ctx,
    )
    client.set_callback(on_message)
    return client


def main():
    connect_wifi()
    client = make_client()
    client.connect()
    print("MQTT connected")
    client.subscribe(TOPIC_DESIRED, qos=1)
    last = 0
    while True:
        client.check_msg()
        if time.time() - last >= 10:
            last = time.time()
            payload = json.dumps({"temp": 21.5, "uptime": time.ticks_ms() // 1000})
            # qos=1 waits for the PUBACK from the bridge.
            client.publish(TOPIC_REPORTED, payload, qos=1)
            print("Published", payload)
        time.sleep(0.1)


main()
