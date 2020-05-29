#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>

#define LED_BUILTIN 1
#define D0 0
#define D2 2
#define TX 1
#define RX 3

enum PlugMode { COUNTING, ON, OFF };
PlugMode currentMode = COUNTING;


// 60Sec * 60Min * 3Hr
int COUNTER_SEC_3HR = 10800;

bool isLedOn = false;
bool isOver = false;
int currentCount = -3;

const char *ssid = "______";
const char *password = "______";
WiFiClient espClient;
PubSubClient client(espClient);

String clientId = "ESP-01s-WifiPlug";

void setup() {
  Serial.begin(115200);
  pinMode(D0, OUTPUT);
  pinMode(D2, OUTPUT);
  
  setupConnection();
}

String getMode() {
  if (currentMode == ON) return "on";
  if (currentMode == OFF) return "off";
  return "counting";
}

void updateMode(PlugMode status) {
  if (currentMode != COUNTING && status == COUNTING) {
    currentCount = -3;  
  }
  
  currentMode = status;
  
  if (!client.connected()) return;
  
  String currentModeText = getMode();
  client.publish(mqttTopic.c_str(), currentModeText.c_str(), false);
}
bool isWifiExist() {
  int numberOfNetworks = WiFi.scanNetworks();
  bool isExist = false;
 
  for(int i =0; i<numberOfNetworks; i++){
      String surveySSID = WiFi.SSID(i);
      if (surveySSID.equals(ssid)) {
        isExist = true;
      }
  }

  return isExist;
}

void setupConnection() {
  WiFi.hostname(clientId);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  if (!isWifiExist()) return;

  Serial.print("Connecting.");
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
     Serial.print(".");
     delay(100);
  }

  Serial.println("Connected");
 
  Serial.print("Current IP: ");
  Serial.println(WiFi.localIP());

//  setupMqtt();
//  setupOTA();
}

void defaultMode() {
  // Timer is done
  if (currentCount > COUNTER_SEC_3HR) {
    updateMode(OFF);
    currentCount = -3;
    return;
  };
  
  if (currentCount < 0) {
    digitalWrite(D2, HIGH);
    delay(300);
    digitalWrite(D2, LOW);
    delay(300);
    digitalWrite(D2, HIGH);
    delay(300);
    digitalWrite(D2, LOW);
    delay(300);
  } else {
    if (!isLedOn) {
      digitalWrite(D2, HIGH);
      isLedOn = true;
    } else {
      digitalWrite(D2, LOW);
      isLedOn = false;
    }
    
    digitalWrite(D0, HIGH);
  }
  
  currentCount += 1;
  delay(1000);
}

void offMode() {
  // Relay - OFF
  digitalWrite(D0, LOW);
  
  digitalWrite(D2, HIGH);
  delay(200);
  digitalWrite(D2, LOW);
  delay(200);
}

void onMode() {
  if (!isLedOn) {
    digitalWrite(D2, HIGH);
    isLedOn = true;
  } else {
    digitalWrite(D2, LOW);
    isLedOn = false;
  }
  delay(100);
}

void loop() {
  if (WiFi.status() == WL_DISCONNECTED) {
    setupConnection();
  }

  
  switch(currentMode) {
    case OFF: offMode(); break;
    case ON: onMode(); break;
    default: defaultMode(); break;
  }
}
