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

bool shouldConnectWifi = true;
bool isLedOn = false;
bool isOver = false;
int currentCount = -3;

const char *ssid = "______";
const char *password = "______";
WiFiClient espClient;
PubSubClient client(espClient);

String clientId = "ESP-01s-WifiPlug";

const char *mqttServer = "______";
const int mqttPort = 1883;

String mqttTopic = "condo/" + clientId + "/status";

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
  if (WiFi.status() != WL_CONNECTED) return; 
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

void setupOTA() {
  if (WiFi.status() != WL_CONNECTED) return; 
  
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

bool isWifiExist() {
  int numberOfNetworks = WiFi.scanNetworks();
  bool isExist = false;
 
  for(int i =0; i<numberOfNetworks; i++){
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

  Serial.print("Connecting.");
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
     Serial.print(".");
     delay(100);
  }

  Serial.println("Connected");
 
  Serial.print("Current IP: ");
  Serial.println(WiFi.localIP());

  setupMqtt();
  setupOTA();
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
  if (shouldConnectWifi) {
    if (WiFi.status() == WL_DISCONNECTED) {
      setupConnection();
    } else {
      client.loop();
      ArduinoOTA.handle();
    }
  }


  switch(currentMode) {
    case OFF: offMode(); break;
    case ON: onMode(); break;
    default: defaultMode(); break;
  }
}
