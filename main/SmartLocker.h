// KEYPAD:        A0
// CURRENT SENSE: A1
// SERVO PWM:     D9 / PB1 / OC1A
// WAKE BUTTON:   D2 / PD2 / INT0
//
// Optional:
// SERVO_POWER_PIN can control a MOSFET/transistor that cuts power
// to the servo during sleep.
// ============================================================

// STATES
enum {
  powered_off, powered_on, unlocking, unlock, locking,
  lock, wrong_passcode, set_passcode, low_battery
};

// pins
#define SERVO_PIN 9
#define RED_PIN 3
#define GREEN_PIN 4
#define YELLOW_PIN 5
#define KEYPAD A0
#define CURRENT_SENSE_PIN A1 // blockage detection
#define BATTERY_LEVEL_PIN A2
#define WAKE_PIN 2 // wake up button pin

// integer value outputed by key pad for each number
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

<<<<<<< Updated upstream
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

=======
>>>>>>> Stashed changes
// Change these values if your tested servo positions are different
#define LOCK_PULSE 1000
#define UNLOCK_PULSE 2000

// Amount of time allowed for servo movement
#define SERVO_MOVE_TIME 700UL

// Ignore current spike at the beginning of servo movement
#define CURRENT_IGNORE_TIME 150UL

// Current must remain high for this long to count as blocked
#define BLOCK_CONFIRM_TIME 200UL
<<<<<<< Updated upstream


// BLOCKAGE DETECTION

#define CURRENT_SENSE_PIN A1

#define BATTERY_LEVEL_PIN A2

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

=======
>>>>>>> Stashed changes
#define BLOCKED_CURRENT_ADC 90

//global variable
// passcode stuff
int code[4] = {0, 0, 0, 0}; // place holder code for what the user inputs
int digitsEntered = 0; // stores the number of digits inputed
bool changingPasscode = false; // whether the passcode is being hard reset

bool keyHeld = false; // tracks if a number is being held
int starPresses = 0; // counts * presses while unlocked

// STATE TIMERS
unsigned long stateStartTime = 0;
unsigned long lastActivityTime = 0;

// hashing
const uint32_t PIN_SALT   = 0xA5C3E17BUL; // random value
const uint16_t PIN_ROUNDS = 2000; // number of rounds of hashing
const uint32_t FNV_OFFSET = 2166136261UL;
const uint32_t FNV_PRIME  = 16777619UL;
uint32_t storedHash = 0;

// servo
unsigned long servoMoveStartTime = 0;
unsigned long highCurrentStartTime = 0;

bool highCurrentDetected = false;

int previousServoPulse = LOCK_PULSE; // keeps track of position before movement

// desired/current servo positions
int servoTargetPulse = LOCK_PULSE;
int servoCurrentPulse = LOCK_PULSE;


// SLEEP MODE

// sleep after 10 seconds without user activity
#define SLEEP_TIME 10000UL

volatile bool wakeInterruptOccurred = false;

#define SERVO_POWER_PIN 7

#define EEPROM_MARKER 0
#define EEPROM_CODE_START 1
#define EEPROM_SAVED 55

<<<<<<< Updated upstream
=======
unsigned char current_state = powered_off;
>>>>>>> Stashed changes

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