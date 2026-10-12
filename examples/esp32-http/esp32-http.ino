// infinimesh ESP32 HTTP example.
// Posts reported state with a device token every 30 seconds over HTTPS,
// then reads the device state back and prints the desired part.
//
// Uses only the Arduino esp32 core (WiFi, WiFiClientSecure, HTTPClient).

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

// ---- Config: change these ----
const char* WIFI_SSID = "your-wifi";
const char* WIFI_PASS = "your-password";
const char* API_URL = "https://api.example.com";  // infinimesh REST gateway
const char* DEVICE_ID = "00000000-0000-0000-0000-000000000000";  // device uuid
const char* DEVICE_TOKEN = "paste the device token here";

// CA that signed the API certificate. For Let's Encrypt use ISRG Root X1.
const char* CA_CERT = R"PEM(
-----BEGIN CERTIFICATE-----
...paste the CA certificate here...
-----END CERTIFICATE-----
)PEM";
// ---- End of config ----

WiFiClientSecure net;
String auth;

void postReported() {
  HTTPClient http;
  http.begin(net, String(API_URL) + "/devices/states");
  http.addHeader("Authorization", auth);
  http.addHeader("Content-Type", "application/json");
  float temp = 20.0 + (esp_random() % 100) / 10.0;
  // Body is a Shadow: device id plus reported.data with your own JSON.
  String body = String("{\"device\":\"") + DEVICE_ID +
                "\",\"reported\":{\"data\":{\"temp\":" + String(temp, 1) +
                ",\"uptime\":" + String(millis() / 1000) + "}}}";
  int code = http.POST(body);
  Serial.printf("POST /devices/states -> %d, sent %s\n", code, body.c_str());
  if (code != 200) Serial.println(http.getString());
  http.end();
}

void readDesired() {
  HTTPClient http;
  http.begin(net, String(API_URL) + "/devices/states");
  http.addHeader("Authorization", auth);
  int code = http.GET();
  if (code == 200) {
    // Response: {"shadows":[{"device":..., "reported":{...}, "desired":{"data":{...}}}]}
    String res = http.getString();
    int i = res.indexOf("\"desired\"");
    Serial.printf("GET /devices/states -> 200, desired: %s\n",
                  i >= 0 ? res.substring(i, res.indexOf('}', res.indexOf("\"data\"", i)) + 1).c_str() : "none");
  } else {
    Serial.printf("GET /devices/states -> %d\n", code);
  }
  http.end();
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" connected");
  configTime(0, 0, "pool.ntp.org", "time.google.com");
  while (time(nullptr) < 1700000000) delay(200);

  net.setCACert(CA_CERT);
  auth = String("Bearer ") + DEVICE_TOKEN;
}

void loop() {
  postReported();
  readDesired();
  delay(30000);
}
