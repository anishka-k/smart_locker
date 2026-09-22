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

// saved passcode
int da_code[4] = {1,2,3,4};

// temp arrays for conditions testing
int zeros[4] = {0,0,0,0};
int nines[4] = {9,9,9,9};

// timer start time (lowkey the name is obvious)
unsigned long stateStartTime = 0;


// --------------------------------------------------
// SERVO STUFF
// --------------------------------------------------

#define SERVO_PIN 9

// starting pulse widths
// adjust these after testing the actual servo
#define LOCK_PULSE 1000
#define UNLOCK_PULSE 2000


// --------------------------------------------------
// EEPROM STUFF
// --------------------------------------------------

#define EEPROM_MARKER 0
#define EEPROM_CODE_START 1
#define EEPROM_SAVED 55


// identifies which number is being pressed
int identifyKey(int value) {
  if (value > 0.975 * one && value < 1.025 * one) return 1;
  else if (value > 0.975 * two && value < 1.025 * two) return 2;
  else if (value > 0.975 * three && value < 1.025 * three) return 3;
  else if (value > 0.975 * four && value < 1.025 * four) return 4;
  else if (value > 0.975 * five && value < 1.025 * five) return 5;
  else if (value > 0.975 * six && value < 1.025 * six) return 6;
  else if (value > 0.975 * seven && value < 1.025 * seven) return 7;
  else if (value > 0.975 * eight && value < 1.025 * eight) return 8;
  else if (value > 0.975 * nine && value < 1.025 * nine) return 9;
  else if (value > 0.975 * zero && value < 1.025 * zero) return 0;
  else return -1; // no key pressed / value doesn't match a digit
}


// compares 2 arrays, returns true if the are the same
// should probably have fail safes for different size arrays, but it'll probably be fine (cope)
bool compareArrays (int array1[], int array2[]) {
  for (int i = 0; i < 4; i++) {
    if (array1[i] != array2[i]) {
      return false;
    }
  }
  return true;
}


// clears the entered code
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
    EEPROM.update(EEPROM_CODE_START + i, da_code[i]);
  }

  EEPROM.update(EEPROM_MARKER, EEPROM_SAVED);
}


void loadPasscode() {
  if (EEPROM.read(EEPROM_MARKER) == EEPROM_SAVED) {

    for (int i = 0; i < 4; i++) {
      da_code[i] = EEPROM.read(EEPROM_CODE_START + i);
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


void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);

//  pinMode(red, output);
//  pinMode(green, output);

  // I have A0 as the port that has the key pad
  pinMode(A0, INPUT);

  // servo setup
  setup_servo();
  lock_servo();

  // load password if one was previously saved
  loadPasscode();
}


void loop() {

  // removed the original delay(1000)
  // because it made keypad reading really slow

  int value = analogRead(A0);

  // the value currently being pressed
  int key = identifyKey(value);

  // if a number is being pressed
  if (key != -1) {

    // A digit key is currently pressed
    if (!keyHeld) {

      // This is a new press (wasn't pressed last time we checked)
      // record it once, ignoring the fact that it'll still read
      // as pressed for the next several loop() calls

      // added digitsEntered < 4 so code[] doesn't go out of bounds
      if (digitsEntered < 4) {

        code[digitsEntered] = key;
        digitsEntered++;

        // for debugging
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

    // remember that a key is being held down
    keyHeld = true;
  }

  else {

    // no key pressed right now, so next press counts as new
    keyHeld = false;
  }

  delay(20);


  switch (current_state) {

    // default state
    case powered_off:

      // switched when any key is pressed
      if (key != -1) {

        // turns leds off
        // digitalWrite(red, LOW);
        // digitalWrite(green, LOW);

        // don't count the wake-up key as password digit
        clearCode();

        current_state = powered_on;
      }

      else {
        Serial.println("powered off");
      }

      break;


    case powered_on:

      if (digitsEntered == 4 && compareArrays(da_code, code)) {

        // turns the green led on for two seconds
        // digitalWrite(green, HIGH);
        // delay(2000);
        // digitalWrite(green, LOW);

        unlock_servo();

        clearCode();

        current_state = unlock;
      }

      else if (digitsEntered == 4 && !compareArrays(da_code, code)) {

        // flashes the red led 3 times
        // for (int i = 0; i < 3; i++) {
        //   digitalWrite(red, HIGH);
        //   delay(1000);
        //   digitalWrite(red, LOW);
        //   delay(1000);
        // }

        clearCode();

        current_state = wrong_passcode;

        stateStartTime = millis();
      }

      else if (false) {

        current_state = set_passcode;

        stateStartTime = millis();
      }

      else {

        Serial.println("powered on");
      }

      break;


    case unlock:

      // temporary testing:
      // after 5 seconds unlocked, lock again

      if (stateStartTime == 0) {
        stateStartTime = millis();
      }

      if (millis() - stateStartTime >= 5000) {

        lock_servo();

        stateStartTime = millis();

        current_state = lock;
      }

      else {

        Serial.println("unlock");
      }

      break;


    case lock:

      if (millis() - stateStartTime >= 1000) {

        clearCode();

        stateStartTime = 0;

        current_state = powered_on;
      }

      else {

        Serial.println("lock");
      }

      break;


    case wrong_passcode:

      if (millis() - stateStartTime >= 5000) {

        clearCode();

        current_state = powered_on;
      }

      else {

        Serial.println("wrong passcode");
      }

      break;


    case set_passcode:

      // keeping her original 9999 testing idea
      if (digitsEntered == 4 && compareArrays(code, nines)) {

        // temporary example new password
        // replace this later with the actual password-setting method
        da_code[0] = 9;
        da_code[1] = 9;
        da_code[2] = 9;
        da_code[3] = 9;

        savePasscode();

        clearCode();

        current_state = powered_on;
      }

      else {

        Serial.println("set passcode");
      }

      break;
  }
}