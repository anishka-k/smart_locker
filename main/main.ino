#include <EEPROM.h>

// enum containing all the states of our state machine
enum {
  powered_off, powered_on, lock, unlock, wrong_passcode, set_passcode
};

unsigned char current_state = powered_off;

//int green = /*PIN NUMBER*/;
//int red = /*PIN NUMBER*/;

// integer value outputed by key pad for each number (from milestone 1)
int one = 981;
int two = 763;
int three = 506;
int four = 937;
int five = 736;
int six = 494;
int seven = 877;
int eight = 699;
int nine = 477;
int asterick = 829;
int zero = 668;
int hashtag = 462;

// place holder code for what the user inputs
int code[4] = {0, 0, 0, 0};

// stores the number of digits inputed
int digitsEntered = 0;

// tracks if a number is being held
bool keyHeld = false;

// counts * presses while unlocked
int starPresses = 0;

// saved passcode
int da_code[4] = {1,2,3,4};

// temp arrays for conditions testing
int zeros[4] = {0,0,0,0};
int nines[4] = {9,9,9,9};

// timer start time
unsigned long stateStartTime = 0;


// --------------------------------------------------
// SERVO STUFF
// --------------------------------------------------

#define SERVO_PIN 9

#define LOCK_PULSE 1000
#define UNLOCK_PULSE 2000


// --------------------------------------------------
// EEPROM STUFF
// --------------------------------------------------

#define EEPROM_MARKER 0
#define EEPROM_CODE_START 1
#define EEPROM_SAVED 55


// identifies which number is being pressed
char identifyKey(int value) {

  if (value > 0.975 * one && value < 1.025 * one) return '1';
  else if (value > 0.975 * two && value < 1.025 * two) return '2';
  else if (value > 0.975 * three && value < 1.025 * three) return '3';
  else if (value > 0.975 * four && value < 1.025 * four) return '4';
  else if (value > 0.975 * five && value < 1.025 * five) return '5';
  else if (value > 0.975 * six && value < 1.025 * six) return '6';
  else if (value > 0.975 * seven && value < 1.025 * seven) return '7';
  else if (value > 0.975 * eight && value < 1.025 * eight) return '8';
  else if (value > 0.975 * nine && value < 1.025 * nine) return '9';
  else if (value > 0.975 * zero && value < 1.025 * zero) return '0';
  else if (value > 0.975 * asterick && value < 1.025 * asterick) return '*';
  else if (value > 0.975 * hashtag && value < 1.025 * hashtag) return '#';
  else return 'x';
}


// compares 2 arrays
bool compareArrays(int array1[], int array2[]) {

  for (int i = 0; i < 4; i++) {

    if (array1[i] != array2[i]) {
      return false;
    }
  }

  return true;
}


// clears entered code
void clearCode() {

  for (int i = 0; i < 4; i++) {
    code[i] = 0;
  }

  digitsEntered = 0;
}


// --------------------------------------------------
// EEPROM FUNCTIONS
// --------------------------------------------------

void savePasscode() {

  for (int i = 0; i < 4; i++) {

    EEPROM.update(
      EEPROM_CODE_START + i,
      da_code[i]
    );
  }

  EEPROM.update(
    EEPROM_MARKER,
    EEPROM_SAVED
  );
}


void loadPasscode() {

  if (EEPROM.read(EEPROM_MARKER) == EEPROM_SAVED) {

    for (int i = 0; i < 4; i++) {

      da_code[i] =
        EEPROM.read(EEPROM_CODE_START + i);
    }

    Serial.println("saved password loaded");
  }
}


// --------------------------------------------------
// SERVO FUNCTIONS
// --------------------------------------------------

void setup_servo() {

  pinMode(SERVO_PIN, OUTPUT);

  // clear Timer1
  TCCR1A = 0;
  TCCR1B = 0;

  // fast PWM mode using ICR1 as the top
  TCCR1A |= (1 << WGM11);
  TCCR1B |= (1 << WGM12) | (1 << WGM13);

  // output PWM on pin 9
  TCCR1A |= (1 << COM1A1);

  // prescaler = 8
  TCCR1B |= (1 << CS11);

  // 20 ms period = 50 Hz
  ICR1 = 39999;
}


void set_servo_pulse(int pulseWidth) {

  // timer runs at 0.5 us per count
  OCR1A = pulseWidth * 2;
}


void unlock_servo() {

  set_servo_pulse(UNLOCK_PULSE);

  Serial.println("servo unlocked");
}


void lock_servo() {

  set_servo_pulse(LOCK_PULSE);

  Serial.println("servo locked");
}


// --------------------------------------------------
// SETUP
// --------------------------------------------------

void setup() {

  Serial.begin(9600);

  pinMode(A0, INPUT);

  setup_servo();

  lock_servo();

  loadPasscode();
}


// --------------------------------------------------
// LOOP
// --------------------------------------------------

void loop() {

  int value = analogRead(A0);

  char key = identifyKey(value);

  // tells the state machine that this is a NEW press
  bool newKeyPress = false;


  // --------------------------------------------------
  // KEY PRESS DETECTION
  // --------------------------------------------------

  if (key != 'x') {

    if (!keyHeld) {

      newKeyPress = true;

      Serial.print("Key pressed: ");
      Serial.println(key);
    }

    keyHeld = true;
  }

  else {

    keyHeld = false;
  }


  // --------------------------------------------------
  // ONLY STORE NUMBER KEYS AS PASSWORD DIGITS
  // --------------------------------------------------

  if (newKeyPress &&
      current_state == powered_on &&
      key >= '0' &&
      key <= '9') {

    if (digitsEntered < 4) {

      code[digitsEntered] = key - '0';

      digitsEntered++;

      Serial.print("Digit entered: ");
      Serial.println(key);

      if (digitsEntered == 4) {

        Serial.print("Full code: ");

        for (int i = 0; i < 4; i++) {
          Serial.print(code[i]);
        }

        Serial.println();
      }
    }
  }


  // --------------------------------------------------
  // STATE MACHINE
  // --------------------------------------------------

  switch (current_state) {


    // ==================================================
    // POWERED OFF
    // ==================================================

    case powered_off:

      if (newKeyPress) {

        clearCode();

        current_state = powered_on;

        Serial.println("powered on");
        Serial.println("enter password");
      }

      break;


    // ==================================================
    // POWERED ON
    // ==================================================

    case powered_on:

      if (digitsEntered == 4 &&
          compareArrays(da_code, code)) {

        Serial.println("correct password");

        unlock_servo();

        clearCode();

        // reset * counter every time we unlock
        starPresses = 0;

        current_state = unlock;

        Serial.println("press * three times to lock");
      }


      else if (digitsEntered == 4 &&
               !compareArrays(da_code, code)) {

        Serial.println("wrong password");

        clearCode();

        stateStartTime = millis();

        current_state = wrong_passcode;
      }


      else if (false) {

        current_state = set_passcode;

        stateStartTime = millis();
      }

      break;


    // ==================================================
    // UNLOCKED
    // ==================================================

    case unlock:

      // Stay unlocked until * is pressed three times

      if (newKeyPress && key == '*') {

        starPresses++;

        Serial.print("* pressed: ");
        Serial.print(starPresses);
        Serial.println("/3");


        if (starPresses >= 3) {

          Serial.println("locking locker");

          lock_servo();

          starPresses = 0;

          stateStartTime = millis();

          current_state = lock;
        }
      }

      break;


    // ==================================================
    // LOCK
    // ==================================================

    case lock:

      // give servo 1 second to move
      if (millis() - stateStartTime >= 1000) {

        clearCode();

        stateStartTime = 0;

        Serial.println("locker locked");
        Serial.println("enter password");

        current_state = powered_on;
      }

      break;


    // ==================================================
    // WRONG PASSWORD
    // ==================================================

    case wrong_passcode:

      if (millis() - stateStartTime >= 5000) {

        clearCode();

        Serial.println("try again");
        Serial.println("enter password");

        current_state = powered_on;
      }

      break;


    // ==================================================
    // SET PASSCODE
    // ==================================================

    case set_passcode:

      if (digitsEntered == 4 &&
          compareArrays(code, nines)) {

        da_code[0] = 9;
        da_code[1] = 9;
        da_code[2] = 9;
        da_code[3] = 9;

        savePasscode();

        clearCode();

        current_state = powered_on;
      }

      break;
  }


  delay(20);
}