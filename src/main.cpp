#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>

#define PIN_LEDG D6
#define PIN_LEDR D5
#define PIN_POT PIN_A0
#define WEIGHT_THRESHOLD 50
#define WEIGHT_LIMIT 800
#define WEIGHT_DEADZONE 40
#define UPDATE_DELAY 4000

const char *endpoint = "http://smartdumpster.mattiamari.me/api/v1";
const char *dumpsterID = "162db43a-f153-4efd-8671-a0ca8c921031";

HTTPClient http;
WiFiClient client;
unsigned int lastWeight;
bool available = false;
char url[128];
StaticJsonDocument<30> json;

void setup() {
  Serial.begin(9600);
  WiFi.begin("SSID", "PASSWORD");
  Serial.print("Connecting to wifi...");

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  
  Serial.println();
  Serial.print("Connected, IP: ");
  Serial.println(WiFi.localIP());

  pinMode(PIN_LEDG, OUTPUT);
  pinMode(PIN_LEDR, OUTPUT);

  lastWeight = analogRead(PIN_POT);
}

void updateAvailability() {
  sprintf(url, "%s/dumpster/%s/availability", endpoint, dumpsterID);
  http.begin(client, url);
  int code = http.GET();

  if (code != HTTP_CODE_OK) {
    Serial.print("Api error: ");
    Serial.println(code);
    http.end();
    return;
  }

  String res = http.getString();
  http.end();
  DeserializationError err = deserializeJson(json, res);

  if (err) {
    Serial.print("Json deserialization error: ");
    Serial.println(err.c_str());
    return;
  }

  available = json["available"];
}

void makeUnavailable() {
  sprintf(url, "%s/dumpster/%s/availability", endpoint, dumpsterID);
  http.begin(client, url);

  String data = "{\"available\":false}";

  http.addHeader("Content-Type", "application/json");
  int code = http.POST(data);

  if (code != HTTP_CODE_OK) {
    Serial.print("Api error: ");
    Serial.println(code);
    http.end();
    return;
  }

  http.end();
}

void updateWeight() {
  unsigned int weight = analogRead(PIN_POT);

  if (abs(weight - lastWeight) < WEIGHT_THRESHOLD) {
    return;
  }

  lastWeight = weight;

  if (weight < WEIGHT_DEADZONE) {
    weight = 0;
  }

  sprintf(url, "%s/dumpster/%s/weight", endpoint, dumpsterID);
  http.begin(client, url);

  String data = "{\"weight\":" + String(weight) + "}";

  http.addHeader("Content-Type", "application/json");
  int code = http.POST(data);

  if (code != HTTP_CODE_CREATED) {
    Serial.print("Api error: ");
    Serial.println(code);
    http.end();
    return;
  }

  http.end();

  if (weight > WEIGHT_LIMIT) {
    makeUnavailable();
  }
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    return;
  }

  digitalWrite(PIN_LEDG, available);
  digitalWrite(PIN_LEDR, !available);

  updateAvailability();
  updateWeight();

  delay(UPDATE_DELAY);
}