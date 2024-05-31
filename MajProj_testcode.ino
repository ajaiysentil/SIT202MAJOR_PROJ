
#define BLYNK_TEMPLATE_ID           "TMPL6Up8AdlvC"
#define BLYNK_TEMPLATE_NAME         "Quickstart Device"
#define BLYNK_AUTH_TOKEN            "-dRRM_kEt8yx7YMSH8e_IouZ1FEHTGfI"


#define BLYNK_PRINT Serial

#include <SPI.h>
#include <WiFi.h>
#include <BlynkSimpleWifi.h>
#include <MFRC522.h>
#include <LiquidCrystal.h>
#include <Servo.h>
#include "secrets.h"

#define RST_PIN      9          
#define SS_PIN       10          
#define Buzzer       8          // Pin connected to the buzzer
#define TRIG_PIN     15         // Pin connected to TRIG of HC-SR04
#define ECHO_PIN     16         // Pin connected to ECHO of HC-SR04

// RFID CHIP KEYCHAIN Card UID: D3 ED 26 B7
// RFID CARD Card UID: BC 8F 17 E1
// RFID GYM TAG: Card UID: 2C 01 64 1D
// MYKI CARD: Card UID: 04 49 4F 32 A9 4E 80

char ssid[] = SECRET_SSID;
char pass[] = SECRET_PASSWORD;

Servo Servo1;
LiquidCrystal lcd(3, 2, 7, 6, 5, 4); 
MFRC522 mfrc522(SS_PIN, RST_PIN); // Create MFRC522 instance

bool doorState = false; // false means closed, true means open
bool locked = true;
int wrongAttempts = 0;
const int maxWrongAttempts = 3;
bool alarmTriggered = false;

BlynkTimer timer;

void setup() {
  // Debug console
  Serial.begin(115200);

  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  Servo1.attach(14);

  // Setup LCD Col and Row
  lcd.begin(16, 2);
  lcd.setCursor(0, 0); // set the cursor to columns 0, line 1
  lcd.print("Welcome to");
  lcd.setCursor(0, 1); // set the cursor to column 0, line 2
  lcd.print("Arduino Security!");

  SPI.begin(); // Init SPI bus
  mfrc522.PCD_Init(); // Init MFRC522 card
  Serial.println("Scan your card to see the data");

  pinMode(Buzzer, OUTPUT); // Set buzzer pin as output
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // Setup a function to be called every second
  timer.setInterval(1000L, myTimerEvent);
}

long sensorDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH);
  long distance = (duration * 0.034) / 2;

  return distance;
}

void accessGrantedAudio() {
  tone(Buzzer, 2000); 
  delay(200);             
  noTone(Buzzer);     
  delay(100);             
  tone(Buzzer, 3000); 
  delay(200);             
  noTone(Buzzer);     
}

void accessGrantedScreenUnlocked() {
  lcd.clear();
  lcd.setCursor(0, 0); // set the cursor to columns 0, line 1
  lcd.print("Authorized");
  lcd.setCursor(0, 1); // set the cursor to column 0, line 2
  lcd.print("Access: Unlocked");
}

void accessGrantedScreenLocked() {
  lcd.clear();
  lcd.setCursor(0, 0); // set the cursor to columns 0, line 1
  lcd.print("Authorized");
  lcd.setCursor(0, 1); // set the cursor to column 0, line 2
  lcd.print("Access Locked");
}

void accessDeniedAudio() {
  tone(Buzzer, 1000); 
  delay(100);             
  noTone(Buzzer);    
  delay(100);             
  tone(Buzzer, 500);  
  delay(100);             
  noTone(Buzzer);     
  delay(100);             
  tone(Buzzer, 1000);
  delay(100);             
  noTone(Buzzer);     
}

void accessDeniedScreenLocked() {
  lcd.clear();
  lcd.setCursor(0, 0); // set the cursor to columns 0, line 1
  lcd.print("Access ");
  lcd.setCursor(0, 1); // set the cursor to column 0, line 2
  lcd.print("Denied Locked");
}

BLYNK_WRITE(V1) {
  int pinValue = param.asInt();
  if (pinValue == 1) {
    if (doorState) {
      closeDoor();
    } else {
      openDoor();
    }
    doorState = !doorState; // Toggle the door state
  }
}


BLYNK_WRITE(V2) {
  int pinValue = param.asInt();
  if (pinValue == 1) {
    closeDoor();
  }
}

void openDoor() {
  Serial.println("Opening door via app");
  accessGrantedAudio();
  accessGrantedScreenUnlocked();
  Servo1.write(65); // Set servo to 65 deg
  locked = false;
  lcd.clear();
  lcd.setCursor(0, 0); // set the cursor to columns 0, line 1
  lcd.print("Door Opened");
  lcd.setCursor(0, 1); // set the cursor to column 0, line 2
  lcd.print("via App");
}

void closeDoor() {
  Serial.println("Closing door via app");
  accessGrantedAudio();
  accessGrantedScreenLocked();
  Servo1.write(8); // Set servo to 0 deg
  locked = true;
  lcd.clear();
  lcd.setCursor(0, 0); // set the cursor to columns 0, line 1
  lcd.print("Door Closed");
  lcd.setCursor(0, 1); // set the cursor to column 0, line 2
  lcd.print("via App");
}

void triggerAlarm() {
  Blynk.logEvent("alarm_triggered", "Alarm activated! Possible intruder detected.");
  alarmTriggered = true; 
  while (alarmTriggered) {
    tone(Buzzer, 1000); // Alarm tone
    delay(500);
    noTone(Buzzer);
    delay(500);

    // Check for the correct card to stop the alarm
    if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
      String content = "";
      for (byte i = 0; i < mfrc522.uid.size; i++) {
        content.concat(String(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " "));
        content.concat(String(mfrc522.uid.uidByte[i], HEX));
      }
      content.toUpperCase();
      if (content.substring(1) == "D3 ED 26 B7") { // Correct card UID
        Serial.println("Alarm deactivated by correct card");
        alarmTriggered = false;
        noTone(Buzzer);
        wrongAttempts = 0; 
        Blynk.logEvent("alarm_deactivated", "Alarm deactivated by correct card.");
        return;
      }
    }
  }
}

// This function sends Arduino's uptime every second to Virtual Pin 2.
void myTimerEvent() {
  long distance = sensorDistance();
  Blynk.virtualWrite(V2, distance);
}

void loop() {
  Blynk.run();
  timer.run();

  long dist = sensorDistance(); 
  if (alarmTriggered) {
    triggerAlarm(); 
  }

  // Look for new cards
  if (!mfrc522.PICC_IsNewCardPresent()) {
    return;
  }

  // Select one of the cards
  if (!mfrc522.PICC_ReadCardSerial()) {
    return;
  }

  // Show UID on serial monitor
  Serial.print("Card UID:");
  String content = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
    Serial.print(mfrc522.uid.uidByte[i], HEX);
    content.concat(String(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " "));
    content.concat(String(mfrc522.uid.uidByte[i], HEX));
  }
  Serial.println();
  Serial.print("Message : ");
  content.toUpperCase();
  if (content.substring(1) == "D3 ED 26 B7") { // Correct card UID
    Serial.println("Authorized access");
    alarmTriggered = false; // Stop alarm if the correct card is presented
    accessGrantedAudio();
    if (locked) {
      accessGrantedScreenUnlocked();
      Servo1.write(65); // Set servo to 65 deg
      locked = false;
      // Display personalized message welcome back
      lcd.clear();
      lcd.setCursor(0, 0); // set the cursor to columns 0, line 1
      lcd.print("Welcome back");
      lcd.setCursor(0, 1); // set the cursor to column 0, line 2
      lcd.print("Adithya!");
    } else {
      accessGrantedScreenLocked();
      Servo1.write(0); // Set servo to 0 deg
      locked = true;
      // Lock activated message to ensure lock has been activated
      lcd.clear();
      lcd.setCursor(0, 0); // set the cursor to columns 0, line 1
      lcd.print("Lock Activated");
      lcd.setCursor(0, 1); // set the cursor to column 0, line 2
      lcd.print("Bye Adithya!");
    }
    delay(3000); // Set delay till repeat open message
    lcd.clear();
    // Print a welcome message to the LCD
    lcd.setCursor(0, 0); // set the cursor to columns 0, line 1
    lcd.print("Welcome to"); // Print message on display
    lcd.setCursor(0, 1); // set the cursor to column 0, line 2
    lcd.print("Arduino Security!"); // Print message on display
  } else {
    if (!locked) {
      accessDeniedScreenLocked();
      Servo1.write(8); // Set servo to 0 deg
      delay(1000);
      locked = true;
    }
    Serial.println("Access denied");
    accessDeniedAudio();

    if (dist > 0 && dist < 100) { // If distance is valid and within range
      wrongAttempts++;
      if (wrongAttempts >= maxWrongAttempts) {
        alarmTriggered = true;
        triggerAlarm(); // Start alarm if the max wrong attempts are reached
      }
    }

    lcd.clear();
    lcd.setCursor(0, 0); // set the cursor to columns 0, line 1
    lcd.print("Access");
    lcd.setCursor(0, 1); // set the cursor to column 0, line 2
    lcd.print("denied");

    delay(3000); // Set delay till repeat open message
    lcd.clear();
    // Print a welcome message to the LCD
    lcd.setCursor(0, 0); // set the cursor to columns 0, line 1
    lcd.print("Welcome to");
    lcd.setCursor(0, 1); // set the cursor to column 0, line 2
    lcd.print("Arduino Security!");
  }

  // Halt PICC
  mfrc522.PICC_HaltA();
  // Stop encryption on PCD
  mfrc522.PCD_StopCrypto1();
}
