
#include "SmartLocker.h"

void setup() {

  Serial.begin(9600);

  pinMode(BUZZER_PIN, OUTPUT);

  pinMode(KEYPAD, INPUT); // keypad

  pinMode(RED_PIN, OUTPUT); // leds
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(YELLOW_PIN, OUTPUT);

  pinMode(CURRENT_SENSE_PIN, INPUT); // current sensing input

  pinMode(WAKE_PIN, INPUT_PULLUP); // wake button

  pinMode(SERVO_POWER_PIN, OUTPUT); // servo power control

  servoPowerOn();

  setupWakeInterrupt();

  setup_servo();

  servoCurrentPulse = LOCK_PULSE; // Start with physical servo at locked position

  set_servo_pulse(LOCK_PULSE);

  delay(500); // // give servo time to move to locked position

  disable_servo_pwm();

  // passcode recovery
  if (digitalRead(WAKE_PIN) == LOW) {
    Serial.println("PASSWORD RESET REQUESTED");
  
    EEPROM_write(EEPROM_MARKER, 0); // Invalidate old password

    storedHash = 0;
    clearCode();
    changingPasscode = false;
    current_state = set_passcode;

    Serial.println("old password erased");
    Serial.println("enter new 4 digit password");

  } else {
    loadPasscode();
  }

  lastActivityTime = millis();

  Serial.println("Milestone 3 system ready");
}

void loop() {
  int value = analogRead(A0);
  char key = identifyKey(value);
  bool newKeyPress = false;

  // NEW KEYPRESS DETECTION
  if (key != 'x') {
    if (!keyHeld) {
      newKeyPress = true;
      lastActivityTime = millis();
      Serial.print("Key pressed: ");
      Serial.println(key);
    }
    keyHeld = true;
  } else {
    keyHeld = false;
  }

  // STORE PASSWORD DIGITS
  if (newKeyPress && 
      (current_state == powered_on || 
      current_state == set_passcode) && 
      key >= '0' && key <= '9') {

    if (digitsEntered < 4) {
      code[digitsEntered] = key - '0';
      digitsEntered++;
      Serial.print("Digit entered: ");
      Serial.println(key);

      if (digitsEntered == 4) {
        Serial.print("Full code: ");

        for (int i = 0; i < 4; i++) {
          Serial.print(
            code[i]
          );
        }

        Serial.println();
      }
    }
  }

  // STATE MACHINE
  switch (current_state) {
    // POWERED OFF
    case powered_off:
      if (newKeyPress) {
        clearCode();
        current_state = powered_on;
        lastActivityTime = millis();
        Serial.println("powered on");
        Serial.println("enter password");
      }

      break;

    // WAITING FOR PASSWORD
    case powered_on:
      // correct password
      if (digitsEntered == 4) {
        bool ok = pinMatches(code);
        clearCode();

        if (ok) {
          Serial.println("correct passcode");
          starPresses = 0;
          unlock_servo();
          current_state = unlocking;
        } else {
          Serial.println("wrong password");
          stateStartTime = millis();
          current_state = wrong_passcode;
          digitalWrite(RED_PIN, HIGH);
          Serial.println("Red LED on");
        }
        lastActivityTime = millis();
      }
      break;

    // SERVO IS UNLOCKING
    case unlocking:
      // Check for physical blockage
      if (servoBlocked()) {
        revertServo();
        break;
      }
      // successful movement completed
      if (millis() - servoMoveStartTime >= SERVO_MOVE_TIME) {
        current_state = unlock;
        digitalWrite(GREEN_PIN, HIGH);
        Serial.println("Green LED on");
        beepUnlock();
        Serial.println("Unlock Beep");
        disable_servo_pwm();
        servoCurrentPulse = UNLOCK_PULSE;
        starPresses = 0;
        Serial.println("locker unlocked");
        Serial.println("press * three times to lock");
        Serial.println("press # to change password");
        lastActivityTime = millis();
      }

      break;

    // UNLOCKED

    case unlock:
      // Press * three times to lock
      if (newKeyPress && key == '*') {
        starPresses++;
        Serial.print("* pressed: ");
        Serial.print(starPresses);
        Serial.println("/3");

        if (starPresses >= 3) {
          Serial.println("locking locker");
          starPresses = 0;
          lock_servo();
          digitalWrite(GREEN_PIN, LOW);
          Serial.println("Green LED off");
          current_state = locking;
          lastActivityTime = millis();
        }
      }

      // Press # to change password
      if (newKeyPress && key == '#') {
        clearCode();
        starPresses = 0;
        changingPasscode = true;
        digitalWrite(GREEN_PIN, LOW);
        Serial.println("Green LED off");
        current_state = set_passcode;
        digitalWrite(YELLOW_PIN, HIGH);
        Serial.println("Yellow LED on");
        Serial.println("enter new 4 digit password");
        lastActivityTime = millis();
      }

      break;

    // SERVO IS LOCKING

    case locking:
      // Check for physical blockage
      if (servoBlocked()) {
        revertServo();
        break;
      }

      // Successful lock movement
      if (millis() - servoMoveStartTime >= SERVO_MOVE_TIME) {
        disable_servo_pwm();
        servoCurrentPulse = LOCK_PULSE;
        current_state = lock;
        beepLock();
        Serial.println("Unlock Beep");
        stateStartTime = millis();
        Serial.println("locker locked");
      }

      break;

    // LOCK STATE

    case lock:
      digitalWrite(GREEN_PIN, HIGH);
      digitalWrite(RED_PIN, HIGH);
      // short state before returning
      // to password entry
      if (millis() - stateStartTime >= 250) {
        clearCode();
        stateStartTime = 0;
        digitalWrite(GREEN_PIN, LOW);
        digitalWrite(RED_PIN, LOW);
        current_state = powered_on;
        lastActivityTime = millis();
        Serial.println("enter password");
      }

      break;

    // WRONG PASSWORD

    case wrong_passcode:
      if (millis() - stateStartTime >= 5000) {
        clearCode();
        Serial.println("try again");
        Serial.println("enter password");
        digitalWrite(RED_PIN, LOW);
        Serial.println("Red LED off");
        current_state = powered_on;
        lastActivityTime = millis();
      }

      break;

    // SET NEW PASSWORD

case set_passcode:

  if (digitsEntered == 4) {

    savePasscode(); // Save new password to EEPROM

    Serial.println("new password saved");

    Serial.print("storedHash saved: 0x");
    Serial.println(storedHash, HEX);

    clearCode();

    starPresses = 0;

    if (changingPasscode) {
      // Password changed while locker was unlocked
      Serial.println("locker remains unlocked");
      Serial.println("press * three times to lock");

      current_state = unlock;
      beepUnlock();
      digitalWrite(YELLOW_PIN, LOW);
      Serial.println("Yellow LED off");
    } else {

      // Initial setup or password recovery
      Serial.println("locker remains locked");
      Serial.println("enter new password to unlock");

      current_state = powered_on;

    }

    changingPasscode = false;

    lastActivityTime = millis();

  }

  break;
  }

  // AUTOMATIC SLEEP

  // Only sleep when:
  // - locker is locked
  // - system is waiting for password
  // - no partial password has been entered
  // - no activity for 10 seconds

  if (current_state == powered_on && digitsEntered == 0 && millis() - lastActivityTime >= SLEEP_TIME) {
    enterSleep();
  }
  delay(20);
}