// infinimesh ESP32 MQTT example.
// Connects over TLS on port 8883 with the device client certificate,
// publishes JSON to devices/<id>/state/reported every 10 seconds,
// and prints desired state from devices/<id>/state/desired.
//
// Uses ESP-MQTT (mqtt_client.h), which ships with the Arduino esp32 core 3.x.
// No extra library needed. It supports QoS 0, 1 and 2.

#include <WiFi.h>
#include "mqtt_client.h"

// ---- Config: change these ----
const char* WIFI_SSID = "your-wifi";
const char* WIFI_PASS = "your-password";
const char* MQTT_URI = "mqtts://mqtt.example.com:8883";          // your infinimesh MQTT host
const char* DEVICE_ID = "00000000-0000-0000-0000-000000000000";  // device uuid
const int PUBLISH_QOS = 1;                                       // 0 or 1

// CA that signed the server certificate. For Let's Encrypt use ISRG Root X1.
// For a local test, paste core's hack/server.crt.
const char* CA_CERT = R"PEM(
-----BEGIN CERTIFICATE-----
...paste the CA certificate here...
-----END CERTIFICATE-----
)PEM";

// The device certificate you registered in infinimesh, and its private key.
const char* DEVICE_CERT = R"PEM(
-----BEGIN CERTIFICATE-----
...paste device.crt here...
-----END CERTIFICATE-----
)PEM";

const char* DEVICE_KEY = R"PEM(
-----BEGIN EC PRIVATE KEY-----
...paste device.key here...
-----END EC PRIVATE KEY-----
)PEM";
// ---- End of config ----

esp_mqtt_client_handle_t client;
char topicReported[96];
char topicDesired[96];
volatile bool connected = false;
unsigned long lastPublish = 0;

static void onEvent(void* args, esp_event_base_t base, int32_t id, void* data) {
  esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)data;
  switch ((esp_mqtt_event_id_t)id) {
    case MQTT_EVENT_CONNECTED:
      connected = true;
      Serial.println("MQTT connected");
      esp_mqtt_client_subscribe(client, topicDesired, 1);
      break;
    case MQTT_EVENT_DISCONNECTED:
      connected = false;
      Serial.println("MQTT disconnected, will retry");
      break;
    case MQTT_EVENT_PUBLISHED:
      Serial.printf("PUBACK for message %d\n", event->msg_id);
      break;
    case MQTT_EVENT_DATA:
      Serial.printf("Desired state on %.*s: %.*s\n", event->topic_len, event->topic,
                    event->data_len, event->data);
      break;
    case MQTT_EVENT_ERROR:
      Serial.println("MQTT error (check certificates, host and device id)");
      break;
    default:
      break;
  }
}

void setup() {
  Serial.begin(115200);
  snprintf(topicReported, sizeof(topicReported), "devices/%s/state/reported", DEVICE_ID);
  snprintf(topicDesired, sizeof(topicDesired), "devices/%s/state/desired", DEVICE_ID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.print(" connected, IP ");
  Serial.println(WiFi.localIP());

  // TLS checks certificate dates, so the clock must be right.
  configTime(0, 0, "pool.ntp.org", "time.google.com");
  while (time(nullptr) < 1700000000) delay(200);

  esp_mqtt_client_config_t cfg = {};
  cfg.broker.address.uri = MQTT_URI;
  cfg.broker.verification.certificate = CA_CERT;
  cfg.credentials.client_id = DEVICE_ID;
  // The bridge identifies the device by this certificate. No password on 8883.
  cfg.credentials.authentication.certificate = DEVICE_CERT;
  cfg.credentials.authentication.key = DEVICE_KEY;
  cfg.session.keepalive = 30;
  cfg.network.timeout_ms = 30000;

  client = esp_mqtt_client_init(&cfg);
  esp_mqtt_client_register_event(client, MQTT_EVENT_ANY, onEvent, nullptr);
  esp_mqtt_client_start(client);
}

void loop() {
  if (connected && millis() - lastPublish > 10000) {
    lastPublish = millis();
    char payload[128];
    float temp = 20.0 + (esp_random() % 100) / 10.0;
    snprintf(payload, sizeof(payload), "{\"temp\":%.1f,\"uptime\":%lu}", temp, millis() / 1000);
    int msgId = esp_mqtt_client_publish(client, topicReported, payload, 0, PUBLISH_QOS, 0);
    Serial.printf("Published %s (id %d)\n", payload, msgId);
  }
  delay(50);
}
