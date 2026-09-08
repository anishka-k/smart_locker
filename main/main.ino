#include <EEPROM.h>


enum {
  powered_off, powered_on, lock, unlock, wrong_passcode, set_passcode
};
unsigned car current_state;
int green = /*PIN NUMBER*/;
int red = /*PIN NUMBER*/;

//TODO: make these
//TODO: fix the time conditions
void unlock_servo() {

}

void lock_servo() {

}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  pinMode(red, output);
  pinMode(green, output);

}

void loop() {
  // put your main code here, to run repeatedly:
  case powered_off: // this should be our defaul
    if (/*condition*/) {
      // turns leds off
      digitalWrite(red, LOW);
      digitalWrite(green, LOW;
      current_state = powered_on;
    } else {
      break;
    }

  case powered_on:
    if (/*passcode inputted correctly*/) {
      // turns the green led on for two seconds
      digitalWrite(green, HIGH);
      delay(2000);
      digitalWrite(green, LOW);

      unlock_servo();
      current_state = unlock;
    } else if (/*passcode inputted incorrectly*/) {
      // flashes the red led 3 times
      for (int i = 0; i < 3; i++) {
        digitalWrite(red, HIGH);
        delay(1000);
        digitalWrite(red, LOW);
        delay(1000);
      }
      current_state = wrong_passcode;
    } else if (/*set passcode*/) {
      current_state = set_passcode;
    } else {
      break
    }

  case unlock: 
    if (/*wants to be locked*/) {
      lock_servo();

      current_state = lock;
    } else {
      break;
    }

  case lock: 
    if (delay(5000)) {
      current_state = powered_on;
    } else {
      break;
    }
  
  case wrong_passcode:
    if (delay(5000)) {
      current_state = powered_on;
    } else {
      break
    }
  
  case set_passcode:
    if (/*passode setting condition*/) {
      // all the eeprom jazz
      current_state = powered_on;
    } else {
      break;
    }
}
