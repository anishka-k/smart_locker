// identifies which number is being pressed

// SETUP
#include "SmartLocker.h"

void setup() {

  Serial.begin(9600);

  // keypad
  pinMode(KEYPAD, INPUT);

  // leds
  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);

  // current sensing input
  pinMode(CURRENT_SENSE_PIN, INPUT);

  // wake button
  pinMode(WAKE_PIN, INPUT_PULLUP);

  // servo power control
  pinMode(SERVO_POWER_PIN, OUTPUT);

  servoPowerOn();

  setupWakeInterrupt();

  setup_servo();


  // Start with physical servo at locked position
  servoCurrentPulse = LOCK_PULSE;

  set_servo_pulse(LOCK_PULSE);

  // give servo time to move to locked position
  delay(500);

  disable_servo_pwm();

  // password from EEPROM
// PASSWORD RECOVERY
// Hold the wake button while powering on
// to reset the forgotten password.

  // PASSWORD RECOVERY
// Hold wake button while powering on

if (digitalRead(WAKE_PIN) == LOW) {

  Serial.println("PASSWORD RESET REQUESTED");

  // Invalidate old password
  EEPROM_write(EEPROM_MARKER, 0);

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
        disable_servo_pwm();
        servoCurrentPulse = UNLOCK_PULSE;
        current_state = unlock;
        starPresses = 0;
        Serial.println("locker unlocked");
        Serial.println("press * three times to lock");
        Serial.println("press # to change password");
        lastActivityTime = millis();
      }

      break;

    // UNLOCKED

    case unlock:
      digitalWrite(GREEN_PIN, HIGH);
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
        current_state = set_passcode;
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
        stateStartTime = millis();
        Serial.println("locker locked");
      }

      break;

    // LOCK STATE

    case lock:

      // short state before returning
      // to password entry
      if (millis() - stateStartTime >= 250) {
        clearCode();
        stateStartTime = 0;
        current_state = powered_on;
        lastActivityTime = millis();
        Serial.println("enter password");
      }

      break;

    // WRONG PASSWORD

    case wrong_passcode:
      digitalWrite(RED_PIN, HIGH);
      if (millis() - stateStartTime >= 5000) {
        clearCode();
        Serial.println("try again");
        Serial.println("enter password");
        digitalWrite(RED_PIN, LOW);
        current_state = powered_on;
        lastActivityTime = millis();
      }

      break;

    // SET NEW PASSWORD

    // SET NEW PASSWORD

case set_passcode:

  if (digitsEntered == 4) {

    // Save new password to EEPROM
    savePasscode();

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

  if (current_state == powered_on && 
      digitsEntered == 0 &&
      millis() - lastActivityTime >= SLEEP_TIME) {
    enterSleep();
  }
  delay(20);
}