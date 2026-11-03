// ============================================================
// PERSONAL SMART LOCKER
// COMPLETE MILESTONE 3 CODE
//
// Milestone 3A:
// Servo blockage detection using current measurement on A1.
// If the motor remains above the blockage threshold while moving,
// the servo reverses to its previous position.
//
// Milestone 3B:
// Runs directly on ATmega328P.
// Servo PWM and EEPROM are controlled without external libraries.
//
// Milestone 3C:
// ATmega328P enters Power-Down sleep after inactivity.
// Pushbutton on PD2 / INT0 / Arduino D2 wakes the system.
//
// KEYPAD:        A0
// CURRENT SENSE: A1
// SERVO PWM:     D9 / PB1 / OC1A
// WAKE BUTTON:   D2 / PD2 / INT0
//
// Optional:
// SERVO_POWER_PIN can control a MOSFET/transistor that cuts power
// to the servo during sleep.
// ============================================================


// ============================================================
// STATES
// ============================================================

enum {
  powered_off,
  powered_on,
  unlocking,
  unlock,
  locking,
  lock,
  wrong_passcode,
  set_passcode,
  low_battery
};

unsigned char current_state = powered_off;

// KEYPAD STUFF

// integer value outputed by key pad for each number
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

// place holder code for what the user inputs
int code[4] = {0, 0, 0, 0};

// stores the number of digits inputed
int digitsEntered = 0;

// tracks if a number is being held
bool keyHeld = false;

// counts * presses while unlocked
int starPresses = 0;

// saved passcode
int da_code[4] = {1, 2, 3, 4};


// STATE TIMERS

unsigned long stateStartTime = 0;
unsigned long lastActivityTime = 0;


// SERVO STUFF

#define SERVO_PIN 9

// Change these values if your tested servo positions are different
#define LOCK_PULSE 1000
#define UNLOCK_PULSE 2000

// Amount of time allowed for servo movement
#define SERVO_MOVE_TIME 700UL

// Ignore current spike at the beginning of servo movement
#define CURRENT_IGNORE_TIME 150UL

// Current must remain high for this long to count as blocked
#define BLOCK_CONFIRM_TIME 200UL


// BLOCKAGE DETECTION

#define CURRENT_SENSE_PIN A1

#define BATTERY_LEVEL_PIN A2

// hashing

#define SALT 0xAF1835C6UL
#define ROUNDS 2000UL


// ------------------------------------------------------------
// IMPORTANT:
// YOU MUST CALIBRATE THIS NUMBER USING YOUR ACTUAL CURRENT
// SENSING CIRCUIT.
//
// Read A1 while:
// 1. Servo is moving normally
// 2. Servo is briefly blocked
//
// Choose a threshold above normal moving current but below the
// blocked/stall current.
//
// 90 is only a starting value.
// ------------------------------------------------------------

#define BLOCKED_CURRENT_ADC 90

unsigned long servoMoveStartTime = 0;
unsigned long highCurrentStartTime = 0;

bool highCurrentDetected = false;

// keeps track of position before movement
int previousServoPulse = LOCK_PULSE;

// desired/current servo positions
int servoTargetPulse = LOCK_PULSE;
int servoCurrentPulse = LOCK_PULSE;


// SLEEP MODE

#define WAKE_PIN 2

// sleep after 10 seconds without user activity
#define SLEEP_TIME 10000UL

volatile bool wakeInterruptOccurred = false;

// OPTIONAL SERVO POWER CONTROL

// Connect this pin to a MOSFET/transistor if you are physically
// cutting power to the servo.
//
// If you are NOT using servo power switching yet, the code will
// still work, but D7 will simply have no connection.

#define SERVO_POWER_PIN 7


// EEPROM STUFF

#define EEPROM_MARKER 0
#define EEPROM_CODE_START 1
#define EEPROM_SAVED 55


char identifyKey(int value);

// PASSWORD FUNCTIONS

// compares 2 arrays
bool compareArrays(int array1[], int array2[]);


// clears entered code
void clearCode();

// EEPROM FUNCTIONS

// writes one byte directly to EEPROM
void EEPROM_write(unsigned int address, unsigned char data);

// reads one byte directly from EEPROM
unsigned char EEPROM_read(unsigned int address);


// saves passcode into EEPROM
void savePasscode();

// loads password from EEPROM
void loadPasscode();


// SERVO PWM FUNCTIONS

// configure Timer1 for 50 Hz servo PWM
void setup_servo();


// sets servo PWM pulse width
void set_servo_pulse(int pulseWidth);


// enables Timer1 servo output
void enable_servo_pwm();


// stops PWM output
void disable_servo_pwm();


// servo power ON
void servoPowerOn();


// servo power OFF
void servoPowerOff();

// SERVO MOVEMENT FUNCTIONS

// starts servo movement without blocking program
void startServoMove(int newPulse);


// start unlocking
void unlock_servo();


// start locking
void lock_servo();

// CURRENT / BLOCKAGE FUNCTIONS

// reads the current sensor
int readServoCurrent();

int readBatteryLevel();

// checks if servo is blocked
bool servoBlocked();

// return servo to previous position
void revertServo();

// WAKE INTERRUPT

// setup INT0 wake button
void setupWakeInterrupt();

// wake interrupt
ISR(INT0_vect);

// SLEEP FUNCTIONS
void enterSleep();