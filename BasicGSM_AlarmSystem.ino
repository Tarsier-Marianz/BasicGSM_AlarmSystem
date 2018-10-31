#include "SIM900.h"
#include <SoftwareSerial.h>
#include "sms.h"

SMSGSM sms;

char* DEFAULT_PHONE       = "09565392933";
char* DEFAULT_BAL         = "222";
char* DEFAULT_REG         = "8080";

const byte RETRIES        = 3;

/* Pin declarations */
const byte PIR_SENSOR     = 4;
const byte PIN_BUZZER     = 5;
const byte PIN_CAMERA     = 6;
const byte PIN_LIGHT      = 7;
const byte WATER_SENSOR1  = 8;
const byte WATER_SENSOR2  = 9;
const byte PIN_LED        = 13;

/* Messages CODE command */
const char* NOTIF_ON        = "N1";
const char* NOTIF_OFF       = "N0";
const char* LED_OFF         = "L0";
const char* LED_ON          = "L1";
const char* WATER_ON        = "WS1";
const char* WATER_OFF       = "WS0";
const char* BEEP_OFF        = "B0";
const char* BEEP_ON         = "B1";
const char* OFF_DETECTION   = "M0";
const char* ON_DETECTION    = "M1";
const char* REGISTER_UNLI   = "REG";
const char* REGISTER_MSG    = "GOUNLI20";
const char* INQUIRE_BALANCE = "BAL";
const char* RECEIVE_BALANCE = "Yourloadbalance";

/* Non constant variables */
int detectionCount        = 1;
boolean motionState       = false;
boolean waterSensorsState = false;
boolean notifyState       = false;
boolean beepSoundState    = false;

void initPins() {
  pinMode(PIR_SENSOR, INPUT);
  pinMode(WATER_SENSOR1, INPUT);
  pinMode(WATER_SENSOR2, INPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_CAMERA, OUTPUT);
  pinMode(PIN_LIGHT,  OUTPUT);
  pinMode(PIN_LED,  OUTPUT);
}

int numdata;
boolean phoneConnected = false;
char sms_buffer[160];	// array that holds message content
char n[20];
char sms_index;
char phone_number[20]; // array for the phone number string

void setup() {
  initPins();
  Serial.begin(9600);
  Serial.println("Connecting to GSM...");
  if (gsm.begin(9600)) {
    Serial.println("\nstatus=READY");
    phoneConnected = true;
  }
  else Serial.println("\nstatus=IDLE");

  if (phoneConnected) {
    //Enable this two lines if you want to send an SMS.
    if (sms.SendSMS(DEFAULT_PHONE, "Arduino SMS")) {
      Serial.println("\nSMS sent OK");
    }
  }
};

void loop() {
  if (phoneConnected) {
    for (int index = 1; index < 100; index++) {
      memset(sms_buffer, 0, sizeof(sms_buffer));
      if (sms.GetSMS(index, phone_number, sms_buffer, 160)) {
        Serial.print(index);
        Serial.print("  : ");
        Serial.print(phone_number);
        Serial.print("  : ");
        Serial.println(sms_buffer);
        if (sms.DeleteSMS(index)) {
          validateCommand(sms_buffer);
        }
        delay(100);
      }
    }

    if (waterSensorsState) {
      detectWaterSensors();
    }

    detectMotionSensor();
  }
};


void sendAlert(char* message) {
  if (notifyState) {
    if (phoneConnected) { //check if phone is connected
      if (sms.SendSMS(DEFAULT_PHONE, message)) {
        Serial.print("Message sent: ");
        Serial.println(message);
      }
    }
  }
}

void validateCommand(char* message) {
  char *cmd = trimCommand(message);
  if (isCommandEqual (cmd, LED_ON)) {
    digitalWrite(PIN_LIGHT, HIGH);
    digitalWrite(PIN_LED, HIGH);
    sendAlert("Light Turned ON");
  } else if (isCommandEqual (cmd, LED_OFF)) {
    digitalWrite(PIN_LIGHT, LOW);
    digitalWrite(PIN_LED, LOW);
    sendAlert("Light Turned OFF");
  } else if (isCommandEqual (cmd, ON_DETECTION)) {
    motionState = true;
    sendAlert("Motion Turned ON");
  } else if (isCommandEqual (cmd, OFF_DETECTION)) {
    motionState = false;
    sendAlert("Motion Turned OFF");
  } else if (isCommandEqual (cmd, WATER_ON)) {
    waterSensorsState = true;
    sendAlert("Water sensors Turned ON");
  } else if (isCommandEqual (cmd, WATER_OFF)) {
    waterSensorsState = false;
    sendAlert("Water sensors Turned OFF");
  } else if (isCommandEqual (cmd, BEEP_ON)) {
    beepSoundState = true;
    sendAlert("Beeping sound enabled");
  } else if (isCommandEqual (cmd, BEEP_OFF)) {
    beepSoundState = false;
    sendAlert("Beeping sound disabled");
  } else if (isCommandEqual (cmd, NOTIF_ON)) {
    notifyState = true;
    sendAlert("Notification Turned ON");
  } else if (isCommandEqual (cmd, NOTIF_OFF)) {
    sendAlert("Notification Turned OFF");
    notifyState = false;
  } else if (isCommandEqual (cmd, REGISTER_UNLI)) {
    if (sms.SendSMS(DEFAULT_REG, REGISTER_MSG)) {
      Serial.println("Registered Unli Promo");
    }
  } else if (isCommandEqual (cmd, INQUIRE_BALANCE)) {
    if (sms.SendSMS(DEFAULT_BAL, INQUIRE_BALANCE)) {
      Serial.println("Inquire Balance");
    }
  } else if (find_text( RECEIVE_BALANCE, cmd) > 0) {
    sendAlert( sms_buffer);
  } else {
    sendAlert("Invalid command:");
    Serial.print("Invalid command:");
    Serial.println(cmd);
  }
}

void detectMotionSensor() {
  const byte numberOfSamples = 100;
  const byte thresholdPercentage = 80;
  if (sensorHasValue(PIR_SENSOR, HIGH, numberOfSamples, thresholdPercentage)) {

    digitalWrite(PIN_CAMERA, HIGH); // trigger camera to capture
    if (beepSoundState) {
      generateBeepSound ();
    }
    sendAlert("Motion detected");
  } else {
    digitalWrite(PIN_CAMERA, LOW);
  }
}

void detectWaterSensors() {
  const byte numberOfSamples = 100;
  const byte thresholdPercentage = 80;
  if (sensorHasValue(WATER_SENSOR1, HIGH, numberOfSamples, thresholdPercentage)) {
    sendAlert("Water Sensor 1 report empty");
  }
  if (sensorHasValue(WATER_SENSOR2, HIGH, numberOfSamples, thresholdPercentage)) {
    sendAlert("Water Sensor 2 report empty");
  }
}

bool sensorHasValue (byte sensorPin,  const int expectedValue, byte numberOfSamples, byte thresholdPercentage ) {
  if (digitalRead(sensorPin) == expectedValue)  {
    const unsigned long delayTimeBetweenSamples = 1;
    byte actualPercentage = 0;

    for (byte i = 0; i < numberOfSamples; i++)    {
      if (digitalRead(sensorPin) == expectedValue)      {
        actualPercentage++;
      }
      delay(delayTimeBetweenSamples);
    }
    actualPercentage *= (100.0 / numberOfSamples);
    //Serial.println(actualPercentage);
    if (actualPercentage >= thresholdPercentage)    {
      return true;
    }
  }
  return false;
}

void generateBeepSound () {
  const unsigned long toneFrequency = 500;
  const unsigned long durationTime = 50;
  tone(PIN_BUZZER, toneFrequency);
  delay(durationTime);
  noTone(PIN_BUZZER);
  delay(durationTime);
}

byte isCommandEqual(char *cmd, char *expected_str) {
  char *ch;
  char ret_val = -1;
  ch = strstr(cmd, expected_str);
  if (ch != NULL) {
    ret_val = 1;
  } else {
    ret_val = 0;
  }
  return ret_val;
}

char* trimCommand(char* commandStr) {
  int i, j;
  char *output = commandStr;
  for (i = 0, j = 0; i < strlen(commandStr); i++, j++) {
    if (commandStr[i] != ' ')  {
      output[j] = commandStr[i];
    } else {
      j--;
    }
  }
  output[j] = 0;
  return output;
}

int find_text(String needle, String haystack) {
  int foundpos = -1;
  for (int i = 0; i <= haystack.length() - needle.length(); i++) {
    if (haystack.substring(i, needle.length() + i) == needle) {
      foundpos = i;
    }
  }
  return foundpos;
}
