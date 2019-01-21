#include "SIM900.h"
#include "sms.h"
#include <SoftwareSerial.h>
#include <EEPROM.h>

SMSGSM sms;

char* DEFAULT_PHONE       = "09565392???";
char* DEFAULT_BAL         = "222";
char* DEFAULT_REG         = "8080";
char* REGISTER_MSG        = "GOUNLI20";

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
const char* REGISTER_UNLI   = "REG#";
const char* CURRENT_CONFIG  = "CFG";
const char* INQUIRE_BALANCE = "BAL";
const char* RESET_ARDUINO   = "RST";
const char* SET_PHONENUMBER = "SET#";
const char* GET_PHONENUMBER = "GET#";
const char* RECEIVE_BALANCE = "Yourloadbalance";
const char* RECEIVE_BALANCE2 = "Your load balance";
const char* INVALID_KEYWORD = "youhaveenteredaninvalidkeyword";

/* Non constant variables */
int detectionCount        = 1;
boolean motionState       = false;
boolean waterSensorsState = false;
boolean notifyState       = false;
boolean beepSoundState    = false;

boolean sendInvalidCMD    = false;

/* variables in handling EEPROM
    holds phone number configuration
*/
int phoneNumberLength     = 12;                 // maximum index/address where to write
int phoneStartAddress     = 0;                  //EEPROM address counter
int notifStartAddress     = 13;                 //EEPROM address counter save notification
String eepromPhoneNumber, eepromContent;

void(* resetFunc) (void) = 0;             // declaring reset function to be able reset programatically
// sources: https://www.theengineeringprojects.com/2015/11/reset-arduino-programmatically.html

void initPins() {
  pinMode(PIR_SENSOR, INPUT);
  pinMode(WATER_SENSOR1, INPUT);
  pinMode(WATER_SENSOR2, INPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_CAMERA, OUTPUT);
  pinMode(PIN_LIGHT,  OUTPUT);
  pinMode(PIN_LED,  OUTPUT);
}

boolean phoneConnected = false;
char sms_buffer[160];	// array that holds message content
char phone_number[20]; // array for the phone number string

void setup() {
  initPins();
  Serial.begin(9600);

  Serial.println("Connecting to GSM...");
  if (gsm.begin(9600, RETRIES)) {
    Serial.println("\nstatus=READY");
    phoneConnected = true;
  }
  else Serial.println("\nstatus=IDLE");

  if (phoneConnected) {
    readPhoneNumberEEPROM(); //call method to retrieve saved phone number
    notifyState = EEPROM.read(notifStartAddress);            //read saved Notification state from EEPROM
    Serial.println(notifyState);
    if (sms.SendSMS(DEFAULT_PHONE, "Arduino SMS")) {
      Serial.println("\nSMS sent OK");
    }
  }
};

void loop() {
  memset(sms_buffer, 0, sizeof(sms_buffer));    // clear array that holds message receive
  memset(phone_number, 0, sizeof(phone_number));// clear array that holds sender number

  if (phoneConnected) {
    for (int index = 1; index < 100; index++) {
      if (sms.GetSMS(index, phone_number, sms_buffer, 160)) {
        if (sms_buffer != '\0') {
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
    }

    if (waterSensorsState) {
      detectWaterSensors();
    }
    if (motionState) {
      detectMotionSensor();
    }
  }
};

void sendDefaultConfiguration() {
  String configs = "STATES";
  configs += ("\n  N: " + notifyState);
  configs += (",  WS: " + waterSensorsState);
  configs += (",  B: " + beepSoundState);
  configs += (",  M: " + motionState);
  configs += ("\n  VALUES");
  configs += (",  RECIP: " + String(DEFAULT_PHONE));
  configs += (",  BAL: " + String(DEFAULT_BAL));
  configs += (",  REG RECIP: " + String(DEFAULT_REG));
  configs += (",  REG MSG: " + String(REGISTER_MSG));

  sendAlert(configs.c_str());
}

void sendAlert(char* message) {
  if (notifyState) {
    if (phoneConnected) { //check if phone is connected
      if (message == '\0') {
        return; //if empty message;
      }
      if (sms.SendSMS(DEFAULT_PHONE, message)) {
        Serial.print("Message sent: ");
        Serial.println(message);
      }
    }
  }
}

void validateCommand(char* message) {
  if (strlen(message) <= 0) {
    Serial.println("Empty command!");
    return;
  }
  //char *cmd = message;
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
    EEPROM.write(notifStartAddress, 1);
    sendAlert("Notification Turned ON");
  } else if (isCommandEqual (cmd, NOTIF_OFF)) {
    EEPROM.write(notifStartAddress, 0);
    sendAlert("Notification Turned OFF");
    notifyState = false;
  }  else if (isCommandEqual (cmd, INQUIRE_BALANCE)) {
    if (sms.SendSMS(DEFAULT_BAL, INQUIRE_BALANCE)) {
      Serial.println("Inquire Balance");
    }
  } else if (isCommandEqual (cmd, RESET_ARDUINO)) {
    sendAlert("Resetting Arduino...");
    resetFunc();
  } else if (isCommandEqual (cmd, CURRENT_CONFIG)) {
    sendDefaultConfiguration();
  } else if (contains (REGISTER_UNLI, cmd)) {
    String messageBuffer = String(cmd);
    messageBuffer.replace("REG#", "");
    REGISTER_MSG = messageBuffer.c_str();
    if (sms.SendSMS(DEFAULT_REG, REGISTER_MSG)) {
      sendAlert( REGISTER_MSG);
      Serial.println("Registered Unli Promo " + messageBuffer + " ...");
    }
  } else if (contains( SET_PHONENUMBER, cmd) ) {
    setPhoneNumber(cmd);
    String commandString = "SET NUMBER:";
    commandString += eepromPhoneNumber;
    sendAlert(commandString.c_str());
  } else if (contains( GET_PHONENUMBER, cmd) ) {
    readPhoneNumberEEPROM();
    String commandString = "GET NUMBER:";
    commandString += eepromPhoneNumber;
    sendAlert(commandString.c_str());
  } else if (contains( RECEIVE_BALANCE, cmd) ) {
    sendAlert( sms_buffer);
  } else if (contains( RECEIVE_BALANCE2, cmd) ) {
    sendAlert( message);
  } else if (contains( INVALID_KEYWORD, cmd)) {
    sendAlert( REGISTER_MSG);
  } else {
    String commandString = "Invalid command:";
    if (sendInvalidCMD) { //check if sending invalid command is allowed
      if (cmd == '\0') {
        commandString += message;
      } else {
        commandString += cmd;
      }
      sendAlert(commandString.c_str());
    }
    Serial.println(commandString);
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
  char lastChar;
  char *output = commandStr;
  for (i = 0, j = 0; i < strlen(commandStr); i++, j++) {
    char c = commandStr[i];
    if (c != ' ')  {
      output[j] = c;
    } else {
      if (lastChar == ' ')  {
        j--;
      }
    }
    lastChar = c;
  }
  output[j] = 0;
  return output;
}

boolean contains(String text, char* commandStr) {
  String castedCommand = String(commandStr);
  int index = castedCommand.indexOf(text);
  if (index >= 0) {
    return true;
  }
  return false;
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

boolean isValidNumber(String str) {
  for (char i = 0; i < str.length(); i++) {
    char c = str.charAt(i);
    if (c != '\0') {
      if ( !(isDigit(c))) {
        return false;
      }
    }
  }
  return true;
}
/* Section for Phone number configuration */
void setPhoneNumber(char * commandStr) {
  String messageBuffer = String(commandStr);
  messageBuffer.replace("SET#", "");
  //messageBuffer.remove(0, 3);
  eepromPhoneNumber = messageBuffer;
  for (int c = 0; c < phoneNumberLength; c++) {             // extract all chars in entered string
    byte byteValue = eepromPhoneNumber[c];
    writePhoneNumberEEPROM(byteValue);
  }
  Serial.println();
  Serial.print(eepromPhoneNumber);
  Serial.println(" successfully saved!");
}

void readPhoneNumberEEPROM() {
  eepromPhoneNumber = "";
  //for (int i = 0 ; i < EEPROM.length() ; i++) {
  for (int i = phoneStartAddress ; i < phoneNumberLength ; i++) {   // to make it safe we limit out writing addresses
    byte byteValue = EEPROM.read(i);            //read EEPROM data at address i
    char c = (char)byteValue;
    if (c != '\0') {
      eepromPhoneNumber += c;
    }
  }
  if (isValidNumber(eepromPhoneNumber)) {
    DEFAULT_PHONE = eepromPhoneNumber.c_str();
    Serial.print("EEPROM Phone Number: ");
    Serial.println(eepromPhoneNumber);
  } else {
    Serial.print("Invalid:");
    Serial.println(eepromPhoneNumber);
  }
}

void clearPhoneNumberEEPROM() {
  //for (int i = 0 ; i < EEPROM.length() ; i++) {
  for (int i = phoneStartAddress ; i < phoneNumberLength ; i++) {   // to be safe, we clear only the given addressLength (10 in my sample)
    if (EEPROM.read(i) != 0) {                  //skip already "empty" addresses
      EEPROM.write(i, 0);                       //write 0 to address i
    }
  }
  phoneStartAddress = 0;                                  //reset address counter
  //Serial.println();
  //Serial.println("EEPROM cleared!");
}

void writePhoneNumberEEPROM(byte value) {
  EEPROM.write(phoneStartAddress, value);                 //write value to current address counter address
  phoneStartAddress++;                                    //increment address counter
  //if (phoneStartAddress == EEPROM.length()) {           //check if address counter has reached the end of EEPROM
  if (phoneStartAddress == phoneNumberLength) {               // to make it safe we limit out writing addresses
    phoneStartAddress = 0;                                //if yes: reset address counter
  }
}

