#include <Arduino.h>

#define LED_BUILTIN 1
#define D0 0
#define D2 2
#define TX 1
#define RX 3

enum PlugMode { COUNTING, ON, OFF };

PlugMode currentMode = COUNTING;

bool isLedOn = false;

bool isOver = false;

int currentCount = -3;

// 60Sec * 60Min * 3Hr
int COUNTER_SEC_3HR = 10800;

void setup() {
  Serial.begin(115200);
  pinMode(D0, OUTPUT);
  pinMode(D2, OUTPUT);
}

void defaultMode() {
  // Timer is done
  if (currentCount > COUNTER_SEC_3HR) {
    currentMode = OFF;
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

void onMode() {}

void loop() {
  switch(currentMode) {
    case OFF: offMode(); break;
    case ON: onMode(); break;
    default: defaultMode(); break;
  }
}
