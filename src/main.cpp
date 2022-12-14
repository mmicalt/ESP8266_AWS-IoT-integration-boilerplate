#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Arduino.h>
#include "externs.h"
#include <ArduinoJson.h>
#include "secrets.h"

int pingTimeout = 60 * 15 * 1000;

BearSSL::X509List client_crt(deviceCertificatePemCrt);
BearSSL::PrivateKey client_key(devicePrivatePemKey);
BearSSL::X509List rootCert(awsCaPemCrt);

WiFiClientSecure wiFiClient;
void msgReceived(char *topic, byte *payload, unsigned int len);
PubSubClient pubSubClient(awsEndpoint, 8883, msgReceived, wiFiClient);

int8_t TIME_ZONE = 1;
unsigned long lastPublish;
int msgCount;
time_t now;

void setCurrentTime()
{
  configTime(TIME_ZONE * 3600, 0, "pl.pool.ntp.org", "europe.pool.ntp.org", "pool.ntp.org");

  Serial.print("Waiting for NTP time sync: ");
  now = time(nullptr);
  while (now < 8 * 3600 * 2)
  {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
  }
  Serial.println("");
  struct tm timeinfo;
  gmtime_r(&now, &timeinfo);
  Serial.print("Current GMT time: ");
  Serial.println(asctime(&timeinfo));
}

void pubSubCheckConnect()
{
  if (!pubSubClient.connected())
  {
    Serial.print("PubSubClient connecting to: ");
    Serial.print(awsEndpoint);
    while (!pubSubClient.connected())
    {
      Serial.print("PubSubClient state:");
      Serial.print(pubSubClient.state());
      pubSubClient.connect(thingName);
    }
    Serial.println(" connected");
    pubSubClient.subscribe(thingMqttTopicIn);
  }
  pubSubClient.loop();
}

void setup()
{
  Serial.begin(9600);
  Serial.println();
  Serial.println("AWS IoT MQTT Connection");

  Serial.print("Connecting to ");
  Serial.print(ssid);
  WiFi.begin(ssid, password);
  WiFi.waitForConnectResult();
  Serial.print(", WiFi connected, IP address: ");
  Serial.println(WiFi.localIP());

  // get current time, otherwise certificates are flagged as expired
  setCurrentTime();

  wiFiClient.setClientRSACert(&client_crt, &client_key);
  wiFiClient.setTrustAnchors(&rootCert);
}

unsigned long
getTime()
{
  time(&now);
  return now;
}

void sendPing()
{
  unsigned long epochTime;
  epochTime = getTime();
  char outputData[256];
  sprintf(outputData, "{\"timestamp\": %ld, \"message\": \"ping\"}", epochTime);
  pubSubClient.publish(thingMqttTopicOut, outputData);

  Serial.println("Published MQTT Ping");
}

void loop()
{
  pubSubCheckConnect();

  if (millis() - unsigned(lastPublish) > unsigned(pingTimeout))
  {
    sendPing();
    lastPublish = millis();
  }
}

void handleMessage(String message)
{
  if (message == "getConfig")
  {
    unsigned long epochTime;
    epochTime = getTime();
    char outputData[256];
    String localIp = WiFi.localIP().toString();
    sprintf(outputData, "{\"timestamp\": %ld, \"localIp\": \"%s\"}", epochTime, localIp.c_str());
    pubSubClient.publish(thingMqttTopicOut, outputData);

    Serial.println("Published MQTT Message");
  }
}

void msgReceived(char *topic, byte *payload, unsigned int length)
{
  Serial.print("Message received on ");
  Serial.print(topic);
  Serial.print(": ");
  for (uint i = 0; i < length; i++)
  {
    Serial.print((char)payload[i]);
  }

  StaticJsonDocument<256> doc;
  deserializeJson(doc, payload, length);
  JsonObject obj = doc.as<JsonObject>();

  if (obj.containsKey("message"))
  {
    String message = obj["message"];

    handleMessage(message);
  }
}
