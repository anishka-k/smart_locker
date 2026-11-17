#ifndef SMARTLOCKER_H
#define SMARTLOCKER_H

// STATES
enum {
  powered_off,
  powered_on,
  unlocking,
  unlock,
  locking,
  lock,
  wrong_passcode,
  set_passcode,
  low_battery,
  motor_fault
};

// PINS
#define SERVO_PIN 9
#define RED_PIN 3
#define GREEN_PIN 4
#define YELLOW_PIN 5
#define KEYPAD A0
#define CURRENT_SENSE_PIN A1
#define BATTERY_LEVEL_PIN A2
#define WAKE_PIN 2
#define SERVO_POWER_PIN 7
#define BUZZER_PIN 6

// KEYPAD ADC VALUES
#define ONE 981
#define TWO 763
#define THREE 506
#define FOUR 937
#define FIVE 736
#define SIX 494
#define SEVEN 877
#define EIGHT 699
#define NINE 477
#define ASTERICK 829
#define ZERO 668
#define HASHTAG 462

// SERVO SETTINGS
// Target: ATmega328P at 16 MHz
#define LOCK_PULSE 1000
#define UNLOCK_PULSE 2000
#define SERVO_MOVE_TIME 700UL

#define CURRENT_IGNORE_TIME 150UL
#define BLOCK_CONFIRM_TIME 200UL

// Must be calibrated using your current-sensing circuit.
#define BLOCKED_CURRENT_ADC 90

#define SLEEP_TIME 10000UL
#define KEY_DEBOUNCE_TIME 35UL

// EEPROM
#define EEPROM_MARKER 0
#define EEPROM_CODE_START 1
#define EEPROM_LEGACY 55
#define EEPROM_SAVED 56

// PASSWORD
// Four digits preserve your original interface.
// A four-digit PIN remains vulnerable to offline guessing.
#define PIN_LENGTH 4
#define PIN_ROUNDS 2000

#if PIN_LENGTH < 4 || PIN_LENGTH > 16
#error "PIN_LENGTH must be between 4 and 16"
#endif

// BATTERY
// Replace with measured regulated VCC in millivolts.
#define BATTERY_ADC_MV 5000UL

// Battery+ -> 1 Mohm -> A2 -> 470 kohm -> GND
// Add 100 nF from A2 to GND.
#define BATTERY_TOP_KOHM 1000UL
#define BATTERY_BOTTOM_KOHM 470UL

// Starting thresholds; validate with your actual battery and load.
#define BATTERY_LOW_MV 6500UL
#define BATTERY_CLEAR_MV 6800UL
#define BATTERY_CHECK_TIME 2000UL

// FUNCTION DECLARATIONS
char identifyKey(int value);
void clearCode();

void EEPROM_write(unsigned int address, unsigned char data);
unsigned char EEPROM_read(unsigned int address);
void savePasscode();
void loadPasscode();

void setup_servo();
void set_servo_pulse(int pulseWidth);
void enable_servo_pwm();
void disable_servo_pwm();
void servoPowerOn();
void servoPowerOff();

void startServoMove(int newPulse);
void unlock_servo();
void lock_servo();
void finishServoMove();
void stopServo();

int readServoCurrent();
bool servoBlocked();
void revertServo();
void motorFault();

int readBatteryLevel();
void checkBattery(bool force);

void setupWakeInterrupt();
void enterSleep();

uint32_t rotateRight(uint32_t x, uint8_t n);
void sha256Short(const uint8_t *data, uint8_t len, uint8_t *out);
void hashPin(const int *pin, uint8_t *out);
uint32_t legacyPinHash(const int *pin);
bool pinMatches(const int *digits);

void beepUnlock();
void beepLock();
void buttonPress();
void startSound(uint8_t kind);
void updateSound();
void updateUI();


#endif