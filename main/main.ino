#include <EEPROM.h>


enum {
  powered_off, powered_on, lock, unlock, wrong_passcode, set_passcode
};
unsigned car current_state;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);

}

void loop() {
  // put your main code here, to run repeatedly:
  case powered_off: // this should be our defaul
    if (/*condition*/) {
      current_state = powered_on;
    } else {
      break;
    }

  case powered_on:
    if (/*passcode inputted correctly*/) {
      current_state = unlock;
    } else if (/*passcode inputted incorrectly*/) {
      current_state = wrong_passcode;
    } else if (/*set passcode*/) {
      current_state = set_passcode;
    } else {
      break
    }

  case unlock: 
    if (/*wants to be locked*/) {
      current_state = lock;
    } else {
      break;
    }

  case lock: 
    if (/*time???*/) {
      current_state = powered_on;
    } else {
      break;
    }
  
  case wrong_passcode:
    if (/*time??*/) {
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
