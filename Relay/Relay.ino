/*
  Model: Generic ESP8600
  Relay wiring (Default ON)
  - CO: 220V
  - NO: NONE
  - NC: Plug

  Support OTA update via Arduino IDE and Web Updater
  for web updater please upload *.bin file as "FIRMWARE"
*/

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>

#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>

// #define LED_BUILTIN 1
// #define D0 0
// #define D2 2
// #define TX 1
// #define RX 3

#define LED_BUILTIN 2
#define RELAY_PIN 0

enum PlugMode { COUNTING, ON, OFF };
PlugMode currentMode = ON;

// 60Sec * 60Min * 3Hr
int COUNTER_SEC_3HR = 10800;

bool shouldConnectWifi = true;
bool isLedOn = false;

int currentCount = -3;

int pingTimer = 0;

const char *ssid = "ChaneeIndy";
const char *password = "0954492332";

WiFiClient espClient;
PubSubClient client(espClient);

ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;

String clientId = "ESP-01s-WifiPlug";

const char *mqttServer = "cmrabbit.com";
const int mqttPort = 1883;

String mqttTopic = "condo/" + clientId + "/status";
String mqttPingTopic = "condo/" + clientId + "/ping";
String mqttTopicProgress = "condo/" + clientId + "/progress";

void setup() {
  Serial.begin(115200);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);

  digitalWrite(LED_BUILTIN, LOW);

  setupConnection();
}

void setRelayState(PlugMode state) {
  // Switch ON
  if (state == ON) {
    return digitalWrite(RELAY_PIN, LOW);
  }

  // Switch OFF
  digitalWrite(RELAY_PIN, HIGH);
}

String getMode() {
  if (currentMode == ON) return "on";
  if (currentMode == OFF) return "off";
  return "counting";
}

void updateCountingProgress() {
  if (!client.connected()) return;
  if ((currentCount % 60) != 0) return;

  client.publish(mqttTopicProgress.c_str(), String(currentCount).c_str(), false);
}

void updateMode(PlugMode status, bool shouldUpdateMQTT = true) {
  if (currentMode != COUNTING && status == COUNTING) {
    currentCount = -3;
  }

  currentMode = status;

  if (!shouldUpdateMQTT) return;
  if (!client.connected()) return;

  String currentModeText = getMode();
  client.publish(mqttTopic.c_str(), currentModeText.c_str(), false);
}

void onMessageArrive(char *topic, byte *payload, unsigned int length) {
  String msg = "";
  int i = 0;
  while (i < length)
  {
    msg += (char)payload[i++];
  }

  if (!msg) return;

  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  Serial.println(msg);

  if (mqttTopic.equals(topic)) {
    if (msg.equals("on")) {
      updateMode(ON, false);
    }

    if (msg.equals("off")) {
      updateMode(OFF, false);
    }

    if (msg.equals("counting")) {
      updateMode(COUNTING, false);
    }
  }
}

void setupMqtt()
{
  if (!WiFi.isConnected()) return;
  if (client.connected()) return;

  client.setServer(mqttServer, mqttPort);
  client.setCallback(onMessageArrive);

  while (!client.connected())
  {
    Serial.println("Attempting MQTT connection");
    // Attempt to connect
    if (client.connect(clientId.c_str()))
    {
      Serial.println("MQTT connected");
      client.subscribe(mqttTopic.c_str());

      // Update self
      updateMode(currentMode);
    }
    else
    {
      Serial.print("failed, rc = ");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");

      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void pingAlive() {
  if (!client.connected()) return;
  client.publish(mqttPingTopic.c_str(), "Ping", false);
}

void handleMQTT() {
  if (!WiFi.isConnected()) return;

  // In case of MQTT got disconnected
  if (!client.connected()) {
    setupMqtt();
    setupOTA();
    setupWebUpdater();
  }

  client.loop();
}

void setupOTA() {
  if (!WiFi.isConnected()) return;

  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname(clientId.c_str());

  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
    {
      type = "sketch";
    }
    else
    { // U_FS
      type = "filesystem";
    }

    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    Serial.print("Start updating ");
    Serial.println(type);
  });

  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR)
    {
      Serial.println("Auth Failed");
    }
    else if (error == OTA_BEGIN_ERROR)
    {
      Serial.println("Begin Failed");
    }
    else if (error == OTA_CONNECT_ERROR)
    {
      Serial.println("Connect Failed");
    }
    else if (error == OTA_RECEIVE_ERROR)
    {
      Serial.println("Receive Failed");
    }
    else if (error == OTA_END_ERROR)
    {
      Serial.println("End Failed");
    }
  });

  ArduinoOTA.begin();
}

// To upload through terminal you can use: curl -F "image=@firmware.bin" {clientId}.local/update
void setupWebUpdater() {
  MDNS.begin(clientId);
  
  httpUpdater.setup(&httpServer);
  httpServer.begin();

  MDNS.addService("http", "tcp", 80);
  Serial.printf("HTTPUpdateServer ready! Open http://%s.local/update in your browser\n", clientId.c_str());
}

bool isWifiExist() {
  int numberOfNetworks = WiFi.scanNetworks();
  bool isExist = false;

  for (int i = 0; i < numberOfNetworks; i++) {
    String surveySSID = WiFi.SSID(i);
    if (surveySSID.equals(ssid)) {
      isExist = true;
    }
  }

  if (!isExist) {
    shouldConnectWifi = false;
    Serial.print("Cannot found SSID: ");
    Serial.print(ssid);
    Serial.println(" in your area");
  }

  return isExist;
}

void setupConnection() {
  WiFi.hostname(clientId);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  if (!isWifiExist()) return;

  bool isTimeout = false;
  int waitingCount = 0;

  Serial.print("Connecting.");
  while ((WiFi.waitForConnectResult() != WL_CONNECTED) && !isTimeout) {
    Serial.print(".");
    delay(100);
    waitingCount += 1;
    if (waitingCount >= 100) {
      isTimeout = true;
    }
  }

  if (!WiFi.isConnected()) {
    Serial.println("WiFi - State: Timeout");
    return;
  }

  Serial.println("Connected");

  Serial.print("Current IP: ");
  Serial.println(WiFi.localIP());

  setupMqtt();
  setupOTA();
  setupWebUpdater();
}

void countdownMode() {
  // Timer is done
  if (currentCount > COUNTER_SEC_3HR) {
    updateMode(OFF);
    currentCount = -3;
    return;
  };

  if (currentCount < 0) {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(250);
    digitalWrite(LED_BUILTIN, LOW);
    delay(250);
    digitalWrite(LED_BUILTIN, HIGH);
    delay(250);
    digitalWrite(LED_BUILTIN, LOW);
    delay(250);

    pingTimer += 1000;
  } else {
    if (!isLedOn) {
      digitalWrite(LED_BUILTIN, HIGH);
    } else {
      digitalWrite(LED_BUILTIN, LOW);
    }

    isLedOn = !isLedOn;
    setRelayState(ON);
  }

  updateCountingProgress();
  currentCount += 1;

  pingTimer += 1000;
  delay(1000);
}

void offMode() {
  // Relay - OFF
  setRelayState(OFF);

  digitalWrite(LED_BUILTIN, HIGH);
  delay(500);
  digitalWrite(LED_BUILTIN, LOW);
  delay(500);

  pingTimer += 1000;
}

void onMode() {
  if (!isLedOn) {
    digitalWrite(LED_BUILTIN, HIGH);
  } else {
    digitalWrite(LED_BUILTIN, LOW);
  }

  isLedOn = !isLedOn;
  setRelayState(ON);
  delay(100);
  
  pingTimer += 100;
}

void getWiFiState() {
  switch(WiFi.status()) {
    case WL_IDLE_STATUS: Serial.println("WIFI: IDLE");  break;
    case WL_NO_SSID_AVAIL: Serial.println("WIFI: NO_SSID");  break;
    case WL_SCAN_COMPLETED: Serial.println("WIFI: SCAN_COMPLETED");  break;
    case WL_CONNECTED: Serial.println("WIFI: CONNECTED");  break;
    case WL_CONNECT_FAILED: Serial.println("WIFI: CONNECT_FAILED");  break;
    case WL_CONNECTION_LOST: Serial.println("WIFI: CONNECTION_LOST");  break;
    case WL_DISCONNECTED: Serial.println("WIFI: DISCONNECTED");  break;
    default: Serial.println("WIFI: I DONT KNOW");  break;
  }
}

void loop() {
  if (shouldConnectWifi) {
    if (!WiFi.isConnected()) {
      Serial.println("Connect / Reconnect to WIFI");
      setupConnection();
    } else {
      handleMQTT();
      ArduinoOTA.handle();
      httpServer.handleClient();
      MDNS.update();
    }
  }

  switch (currentMode) {
    case OFF: offMode(); break;
    case COUNTING: countdownMode(); break;
    default: onMode(); break;
  }

  if ((pingTimer % 10000) == 0) {
    getWiFiState();
  }

  if (pingTimer >= 60000) {
    // Try to scan for WIFI to connect
    shouldConnectWifi = true;
    
    pingTimer = 0;
    pingAlive();
  }
}
