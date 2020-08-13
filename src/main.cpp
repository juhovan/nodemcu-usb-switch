#include <Arduino.h>
#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>

#include "config.h"

#ifdef HTTPUpdateServer
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266WebServer.h>

ESP8266WebServer httpUpdateServer(80);
ESP8266HTTPUpdateServer httpUpdater;
#endif

const char *ssid = USER_SSID;
const char *password = USER_PASSWORD;
const char *mqtt_server = USER_MQTT_SERVER;
const int mqtt_port = USER_MQTT_PORT;
const char *mqtt_user = USER_MQTT_USERNAME;
const char *mqtt_pass = USER_MQTT_PASSWORD;
const char *mqtt_client_name = USER_MQTT_CLIENT_NAME;

WiFiClient espClient;
PubSubClient client(espClient);
bool boot = true;
char charPayload[50];

const uint8_t IN_PIN = D5;
const uint8_t OUT_PIN = D6;

uint8_t LED_STATE;

void callback(char *topic, byte *payload, unsigned int length)
{
  Serial.print("Message arrived [");
  String newTopic = topic;
  Serial.print(topic);
  Serial.print("] ");
  memset(&charPayload, 0, sizeof(charPayload));
  memcpy(charPayload, payload, min<unsigned long>(sizeof(charPayload), (unsigned long)length));
  charPayload[length] = '\0';
  String newPayload = String(charPayload);
  Serial.println(newPayload);
  Serial.println();
  newPayload.toCharArray(charPayload, newPayload.length() + 1);

  if (newTopic == USER_MQTT_CLIENT_NAME "/command")
  {
    if (strcmp(charPayload, "0") == 0)
    {
      digitalWrite(OUT_PIN, LOW);
      delay(1000);
      digitalWrite(OUT_PIN, HIGH);
      client.publish(USER_MQTT_CLIENT_NAME "/command", "1", true);
    }
  }
}

void setup_wifi()
{
  WiFi.setSleepMode(WIFI_NONE_SLEEP);

  WiFi.mode(WIFI_STA);

  WiFi.hostname(USER_MQTT_CLIENT_NAME);

  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void reconnect()
{
  // Loop until we're reconnected
  int retries = 0;
  while (!client.connected())
  {
    if (retries < 150)
    {
      Serial.print("Attempting MQTT connection...");
      // Attempt to connect
      if (client.connect(mqtt_client_name, mqtt_user, mqtt_pass, USER_MQTT_CLIENT_NAME "/availability", 0, true, "offline"))
      {
        Serial.println("connected");
        client.publish(USER_MQTT_CLIENT_NAME "/availability", "online", true);
        if (boot == true)
        {
          client.publish(USER_MQTT_CLIENT_NAME "/checkIn", "rebooted");
        }
        else if (boot == false)
        {
          client.publish(USER_MQTT_CLIENT_NAME "/checkIn", "reconnected");
        }
        client.publish(USER_MQTT_CLIENT_NAME "/command", "1");
        client.subscribe(USER_MQTT_CLIENT_NAME "/command");
      }
      else
      {
        Serial.print("failed, rc=");
        Serial.print(client.state());
        Serial.println(" try again in 5 seconds");
        retries++;
        // Wait 5 seconds before retrying
        delay(5000);
      }
    }
    else
    {
      ESP.restart();
    }
  }
}

#ifdef HTTPUpdateServer
void setup_http_server()
{
  MDNS.begin(USER_MQTT_CLIENT_NAME);

  httpUpdater.setup(&httpUpdateServer, USER_HTTP_USERNAME, USER_HTTP_PASSWORD);
  httpUpdateServer.begin();

  MDNS.addService("http", "tcp", 80);
}
#endif

void setup() {
  pinMode(IN_PIN, INPUT);
  pinMode(OUT_PIN, OUTPUT);

  digitalWrite(OUT_PIN, HIGH);

  Serial.begin(9600);

  setup_wifi();

#ifdef HTTPUpdateServer
  setup_http_server();
#endif

  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected())
  {
    digitalWrite(LED_BUILTIN, HIGH);
    reconnect();
    digitalWrite(LED_BUILTIN, LOW);
  }
  client.loop();

  if (digitalRead(IN_PIN) == HIGH) {
    Serial.print("1");
    if (LED_STATE != 1) {
      client.publish(USER_MQTT_CLIENT_NAME "/ledstate", "1", true);
      LED_STATE = 1;
    }
  }
  else {
    Serial.print("0");
    if (LED_STATE != 0) {
      client.publish(USER_MQTT_CLIENT_NAME "/ledstate", "0", true);
      LED_STATE = 0;
    }
  }

#ifdef HTTPUpdateServer
  httpUpdateServer.handleClient();
#endif

  delay(1000);
}
