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

// temp passcode for testing
int da_code[4] = {1,2,3,4};

// temp arrays for conditions testing
int zeros[4] = {0,0,0,0};
int nines[4] = {9,9,9,9};

// timer start time (lowkey the name is obvious)
unsigned long stateStartTime = 0;


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

// TODO: we gotta do this
// void unlock_servo() {
// }

// void lock_servo() {
// }

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
//  pinMode(red, output);
//  pinMode(green, output);

  // I have A0 as the port that has the key pad 
  pinMode(A0, INPUT);
}

void loop() {
  delay(1000);
  int value = analogRead(A0);
  // the value currently being pressed
  int key = identifyKey(value);

    // if a non-number or nothing is being pressed
    if (key != -1) {
    // A digit key is currently pressed
      if (!keyHeld) {
      // This is a new press (wasn't pressed last time we checked) -
      // record it once, ignoring the fact that it'll still read
      // as pressed for the next several loop() calls
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
          // digitsEntered = 0; // reset to accept a new code
      }
    }
    // remember that a key is being held down
    keyHeld = true;
  } else {
    // no key pressed right now, so next press counts as new
    keyHeld = false;
  }

  delay(20);

  switch (current_state) {
    // default state
    case powered_off:
      // switched when any key is pressed
      // lowk need to make this press not count as part of the code. 
      // will probably use a non-number here ngl
      if (value != 0) {
        // turns leds off
        // digitalWrite(red, LOW);
        // digitalWrite(green, LOW;
        current_state = powered_on;
      } else {
        Serial.println("powered off");
      }
      break;

    case powered_on:
      if (digitsEntered == 4 && compareArrays(da_code, code)) {
        // turns the green led on for two seconds
        // digitalWrite(green, HIGH);
        // delay(2000);
        // digitalWrite(green, LOW);
        // unlock_servo();
        digitsEntered = 0;
        current_state = unlock;
      } else if (digitsEntered == 4 && !compareArrays(da_code, code)){
        // flashes the red led 3 times
        // for (int i = 0; i < 3; i++) {
        //   digitalWrite(red, HIGH);
        //   delay(1000);
        //   digitalWrite(red, LOW);
        //   delay(1000);
        // }
        digitsEntered = 0;
        current_state = wrong_passcode;
        stateStartTime = millis();
      } else if (false) {
        current_state = set_passcode;
        stateStartTime = millis();
      } else {
        Serial.println("powered on");
      }
      break;

    case unlock: 
    // testing if the locking works
      if (compareArrays(code, zeros)) {
        //lock_servo();
        stateStartTime = millis();
        current_state = lock;
      } else {
        Serial.println("unlock");
      }
      break;

    case lock: 
      if (millis() - stateStartTime >= 5000) {
        digitsEntered = 0;
        current_state = powered_on;
      } else {
        Serial.println("lock");
      }
      break;
    
    case wrong_passcode:
      if (millis() - stateStartTime >= 5000) {
        digitsEntered = 0;
        current_state = powered_on;
      } else {
        Serial.println("wrong passcode");
      }
      break;
    
    case set_passcode:
    // TODO: fix this
    // lowk this doesnt work bc numbers.
    // will make this non-numbers
      if (digitsEntered == 4 && compareArrays(code, nines)) {
        // all the eeprom jazz
        digitsEntered = 0;
        current_state = powered_on;
      } else {
        Serial.println("set passcode");
      }
      break;
  }
}
