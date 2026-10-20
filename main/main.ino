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
  set_passcode
};

unsigned char current_state = powered_off;


// ============================================================
// KEYPAD STUFF
// ============================================================

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


// ============================================================
// STATE TIMERS
// ============================================================

unsigned long stateStartTime = 0;
unsigned long lastActivityTime = 0;


// ============================================================
// SERVO STUFF
// ============================================================

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


// ============================================================
// BLOCKAGE DETECTION
// ============================================================

#define CURRENT_SENSE_PIN A1

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


// ============================================================
// SLEEP MODE
// ============================================================

#define WAKE_PIN 2

// sleep after 10 seconds without user activity
#define SLEEP_TIME 10000UL

volatile bool wakeInterruptOccurred = false;


// ============================================================
// OPTIONAL SERVO POWER CONTROL
// ============================================================

// Connect this pin to a MOSFET/transistor if you are physically
// cutting power to the servo.
//
// If you are NOT using servo power switching yet, the code will
// still work, but D7 will simply have no connection.

#define SERVO_POWER_PIN 7


// ============================================================
// EEPROM STUFF
// ============================================================

#define EEPROM_MARKER 0
#define EEPROM_CODE_START 1
#define EEPROM_SAVED 55


// ============================================================
// KEYPAD FUNCTIONS
// ============================================================


// identifies which number is being pressed
char identifyKey(int value) {

  if (value > 0.975 * one &&
      value < 1.025 * one)
    return '1';


  else if (value > 0.975 * two &&
           value < 1.025 * two)
    return '2';


  else if (value > 0.975 * three &&
           value < 1.025 * three)
    return '3';


  else if (value > 0.975 * four &&
           value < 1.025 * four)
    return '4';


  else if (value > 0.975 * five &&
           value < 1.025 * five)
    return '5';


  else if (value > 0.975 * six &&
           value < 1.025 * six)
    return '6';


  else if (value > 0.975 * seven &&
           value < 1.025 * seven)
    return '7';


  else if (value > 0.975 * eight &&
           value < 1.025 * eight)
    return '8';


  else if (value > 0.975 * nine &&
           value < 1.025 * nine)
    return '9';


  else if (value > 0.975 * zero &&
           value < 1.025 * zero)
    return '0';


  else if (value > 0.975 * asterick &&
           value < 1.025 * asterick)
    return '*';


  else if (value > 0.975 * hashtag &&
           value < 1.025 * hashtag)
    return '#';


  else
    return 'x';
}


// ============================================================
// PASSWORD FUNCTIONS
// ============================================================


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


// ============================================================
// EEPROM FUNCTIONS
// ============================================================


// writes one byte directly to EEPROM
void EEPROM_write(unsigned int address,
                  unsigned char data) {

  // wait for previous write to finish
  while (EECR & (1 << EEPE));

  // set EEPROM address
  EEAR = address;

  // set EEPROM data
  EEDR = data;

  // save interrupt status
  unsigned char oldSREG = SREG;

  // disable interrupts
  cli();

  // enable EEPROM write
  EECR |= (1 << EEMPE);

  // begin EEPROM write
  EECR |= (1 << EEPE);

  // restore interrupt status
  SREG = oldSREG;
}


// reads one byte directly from EEPROM
unsigned char EEPROM_read(unsigned int address) {

  // wait for previous write to finish
  while (EECR & (1 << EEPE));

  // set address
  EEAR = address;

  // start EEPROM read
  EECR |= (1 << EERE);

  // return data
  return EEDR;
}


// saves passcode into EEPROM
void savePasscode() {

  for (int i = 0; i < 4; i++) {

    EEPROM_write(
      EEPROM_CODE_START + i,
      da_code[i]
    );
  }

  EEPROM_write(
    EEPROM_MARKER,
    EEPROM_SAVED
  );

  Serial.println("password saved");
}


// loads password from EEPROM
void loadPasscode() {

  if (EEPROM_read(EEPROM_MARKER)
      == EEPROM_SAVED) {

    for (int i = 0; i < 4; i++) {

      da_code[i] =
        EEPROM_read(
          EEPROM_CODE_START + i
        );
    }

    Serial.println(
      "saved password loaded"
    );

    current_state = powered_on;

    Serial.println(
      "enter password"
    );
  }

  else {

    Serial.println(
      "no saved password"
    );

    current_state =
      set_passcode;

    clearCode();

    Serial.println(
      "enter new 4 digit password"
    );
  }
}


// ============================================================
// SERVO PWM FUNCTIONS
// ============================================================


// configure Timer1 for 50 Hz servo PWM
void setup_servo() {

  pinMode(
    SERVO_PIN,
    OUTPUT
  );

  // clear Timer1 registers
  TCCR1A = 0;
  TCCR1B = 0;

  // Fast PWM using ICR1 as TOP
  TCCR1A |=
    (1 << WGM11);

  TCCR1B |=
    (1 << WGM12) |
    (1 << WGM13);

  // PWM output on OC1A / pin 9
  TCCR1A |=
    (1 << COM1A1);

  // prescaler = 8
  TCCR1B |=
    (1 << CS11);

  // 20 ms period
  // 50 Hz
  ICR1 = 39999;
}


// sets servo PWM pulse width
void set_servo_pulse(int pulseWidth) {

  // Timer1 runs at 0.5 us per count
  OCR1A = pulseWidth * 2;

  servoCurrentPulse =
    pulseWidth;
}


// enables Timer1 servo output
void enable_servo_pwm() {

  // make sure Timer1 is powered
  PRR &= ~(1 << PRTIM1);

  setup_servo();

  set_servo_pulse(
    servoCurrentPulse
  );
}


// stops PWM output
void disable_servo_pwm() {

  // disconnect OC1A
  TCCR1A &=
    ~(1 << COM1A1);

  // stop Timer1
  TCCR1B &=
    ~(
      (1 << CS12) |
      (1 << CS11) |
      (1 << CS10)
    );

  digitalWrite(
    SERVO_PIN,
    LOW
  );
}


// servo power ON
void servoPowerOn() {

  digitalWrite(
    SERVO_POWER_PIN,
    HIGH
  );

  delay(20);
}


// servo power OFF
void servoPowerOff() {

  digitalWrite(
    SERVO_POWER_PIN,
    LOW
  );
}


// ============================================================
// SERVO MOVEMENT FUNCTIONS
// ============================================================


// starts servo movement without blocking program
void startServoMove(
  int newPulse
) {

  previousServoPulse =
    servoCurrentPulse;

  servoTargetPulse =
    newPulse;

  highCurrentDetected =
    false;

  highCurrentStartTime =
    0;

  servoPowerOn();

  enable_servo_pwm();

  set_servo_pulse(
    newPulse
  );

  servoMoveStartTime =
    millis();
}


// start unlocking
void unlock_servo() {

  Serial.println(
    "servo unlocking"
  );

  startServoMove(
    UNLOCK_PULSE
  );
}


// start locking
void lock_servo() {

  Serial.println(
    "servo locking"
  );

  startServoMove(
    LOCK_PULSE
  );
}


// ============================================================
// CURRENT / BLOCKAGE FUNCTIONS
// ============================================================


// reads the current sensor
int readServoCurrent() {

  return analogRead(
    CURRENT_SENSE_PIN
  );
}


// checks if servo is blocked
bool servoBlocked() {

  unsigned long moveTime =
    millis() -
    servoMoveStartTime;


  // Ignore normal startup/inrush current
  if (moveTime <
      CURRENT_IGNORE_TIME) {

    highCurrentDetected =
      false;

    highCurrentStartTime =
      0;

    return false;
  }


  int currentReading =
    readServoCurrent();


  // Uncomment during calibration if needed

  /*
  Serial.print("Current ADC: ");
  Serial.println(currentReading);
  */


  if (currentReading >=
      BLOCKED_CURRENT_ADC) {

    if (!highCurrentDetected) {

      highCurrentDetected =
        true;

      highCurrentStartTime =
        millis();
    }


    // Current has stayed high long enough
    if (
      millis() -
      highCurrentStartTime >=
      BLOCK_CONFIRM_TIME
    ) {

      return true;
    }
  }

  else {

    highCurrentDetected =
      false;

    highCurrentStartTime =
      0;
  }


  return false;
}


// return servo to previous position
void revertServo() {

  Serial.println(
    "BLOCKED MOTOR DETECTED"
  );

  Serial.println(
    "reverting servo"
  );


  // save failed target
  int failedTarget =
    servoTargetPulse;


  // move back to previous position
  set_servo_pulse(
    previousServoPulse
  );


  servoTargetPulse =
    previousServoPulse;


  servoMoveStartTime =
    millis();


  // Wait only for the actual physical reversal.
  // Main blockage was already detected.
  delay(500);


  disable_servo_pwm();


  Serial.println(
    "servo returned to previous position"
  );


  // If attempted unlock failed,
  // locker remains locked.
  if (failedTarget ==
      UNLOCK_PULSE) {

    servoCurrentPulse =
      LOCK_PULSE;

    current_state =
      powered_on;

    clearCode();

    Serial.println(
      "locker remains locked"
    );

    Serial.println(
      "enter password"
    );
  }


  // If attempted lock failed,
  // locker remains unlocked.
  else {

    servoCurrentPulse =
      UNLOCK_PULSE;

    current_state =
      unlock;

    starPresses = 0;

    Serial.println(
      "locker remains unlocked"
    );

    Serial.println(
      "remove blockage and try again"
    );

    Serial.println(
      "press * three times to lock"
    );
  }


  lastActivityTime =
    millis();
}


// ============================================================
// WAKE INTERRUPT
// ============================================================


// setup INT0 wake button
void setupWakeInterrupt() {

  // D2 / PD2 input
  DDRD &=
    ~(1 << DDD2);

  // internal pull-up ON
  PORTD |=
    (1 << PORTD2);

  // INT0 LOW level interrupt
  //
  // Low-level INT0 can wake the ATmega328P
  // from Power-Down mode.
  EICRA &=
    ~(
      (1 << ISC01) |
      (1 << ISC00)
    );

  // clear interrupt flag
  EIFR |=
    (1 << INTF0);

  // enable INT0
  EIMSK |=
    (1 << INT0);
}


// wake interrupt
ISR(INT0_vect) {

  wakeInterruptOccurred =
    true;

  // temporarily turn interrupt off
  // so LOW button does not continuously
  // trigger after waking
  EIMSK &=
    ~(1 << INT0);
}


// ============================================================
// SLEEP FUNCTIONS
// ============================================================


void enterSleep() {

  Serial.println(
    "entering power down sleep"
  );

  Serial.flush();


  // --------------------------------------------------
  // STOP SERVO
  // --------------------------------------------------

  disable_servo_pwm();

  servoPowerOff();


  // --------------------------------------------------
  // DISABLE ADC
  // --------------------------------------------------

  ADCSRA &=
    ~(1 << ADEN);


  // disable analog comparator
  ACSR |=
    (1 << ACD);


  // --------------------------------------------------
  // POWER REDUCTION
  // --------------------------------------------------

  // ADC off
  PRR |=
    (1 << PRADC);

  // Timer1 off
  PRR |=
    (1 << PRTIM1);

  // SPI off
  PRR |=
    (1 << PRSPI);

  // Timer2 off
  PRR |=
    (1 << PRTIM2);

  // TWI off
  PRR |=
    (1 << PRTWI);


  // --------------------------------------------------
  // SET POWER-DOWN MODE
  // --------------------------------------------------

  // SM2 SM1 SM0 = 0 1 0
  SMCR &=
    ~(
      (1 << SM2) |
      (1 << SM0)
    );

  SMCR |=
    (1 << SM1);


  // clear pending external interrupt
  EIFR |=
    (1 << INTF0);


  wakeInterruptOccurred =
    false;


  // enable INT0
  EIMSK |=
    (1 << INT0);


  // enable sleep
  SMCR |=
    (1 << SE);


  // make sure interrupts are enabled
  sei();


  // --------------------------------------------------
  // GO TO SLEEP
  // --------------------------------------------------

  asm volatile("sleep");


  // ==================================================
  // CPU CONTINUES HERE AFTER WAKE
  // ==================================================


  // disable sleep
  SMCR &=
    ~(1 << SE);


  // --------------------------------------------------
  // RESTORE HARDWARE
  // --------------------------------------------------

  // ADC power ON
  PRR &=
    ~(1 << PRADC);

  // Timer1 power ON
  PRR &=
    ~(1 << PRTIM1);

  // SPI power ON
  PRR &=
    ~(1 << PRSPI);

  // Timer2 power ON
  PRR &=
    ~(1 << PRTIM2);

  // TWI power ON
  PRR &=
    ~(1 << PRTWI);


  // re-enable ADC
  ADCSRA |=
    (1 << ADEN);


  // re-enable comparator
  ACSR &=
    ~(1 << ACD);


  // Wait until wake button is released
  while (
    digitalRead(WAKE_PIN)
    == LOW
  ) {

    delay(5);
  }


  // clear interrupt flag
  EIFR |=
    (1 << INTF0);


  // enable wake interrupt again
  EIMSK |=
    (1 << INT0);


  keyHeld =
    false;


  clearCode();


  current_state =
    powered_on;


  lastActivityTime =
    millis();


  Serial.println(
    "system awake"
  );

  Serial.println(
    "enter password"
  );
}


// ============================================================
// SETUP
// ============================================================


void setup() {

  Serial.begin(9600);


  // keypad
  pinMode(
    A0,
    INPUT
  );


  // current sensing input
  pinMode(
    CURRENT_SENSE_PIN,
    INPUT
  );


  // wake button
  pinMode(
    WAKE_PIN,
    INPUT_PULLUP
  );


  // servo power control
  pinMode(
    SERVO_POWER_PIN,
    OUTPUT
  );


  servoPowerOn();


  setupWakeInterrupt();


  setup_servo();


  // Start with physical servo at locked position
  servoCurrentPulse =
    LOCK_PULSE;

  set_servo_pulse(
    LOCK_PULSE
  );


  // give servo time to move to locked position
  delay(500);


  disable_servo_pwm();


  // password from EEPROM
  loadPasscode();


  lastActivityTime =
    millis();


  Serial.println(
    "Milestone 3 system ready"
  );
}


// ============================================================
// LOOP
// ============================================================


void loop() {


  // ========================================================
  // READ KEYPAD
  // ========================================================

  int value =
    analogRead(A0);


  char key =
    identifyKey(value);


  bool newKeyPress =
    false;


  // --------------------------------------------------------
  // NEW KEYPRESS DETECTION
  // --------------------------------------------------------

  if (key != 'x') {

    if (!keyHeld) {

      newKeyPress =
        true;


      lastActivityTime =
        millis();


      Serial.print(
        "Key pressed: "
      );

      Serial.println(
        key
      );
    }


    keyHeld =
      true;
  }

  else {

    keyHeld =
      false;
  }


  // ========================================================
  // STORE PASSWORD DIGITS
  // ========================================================

  if (
    newKeyPress &&

    (
      current_state ==
        powered_on ||

      current_state ==
        set_passcode
    ) &&

    key >= '0' &&
    key <= '9'
  ) {

    if (
      digitsEntered < 4
    ) {

      code[digitsEntered] =
        key - '0';


      digitsEntered++;


      Serial.print(
        "Digit entered: "
      );

      Serial.println(
        key
      );


      if (
        digitsEntered == 4
      ) {

        Serial.print(
          "Full code: "
        );


        for (
          int i = 0;
          i < 4;
          i++
        ) {

          Serial.print(
            code[i]
          );
        }


        Serial.println();
      }
    }
  }


  // ========================================================
  // STATE MACHINE
  // ========================================================

  switch (
    current_state
  ) {


    // ======================================================
    // POWERED OFF
    // ======================================================

    case powered_off:

      if (
        newKeyPress
      ) {

        clearCode();

        current_state =
          powered_on;

        lastActivityTime =
          millis();

        Serial.println(
          "powered on"
        );

        Serial.println(
          "enter password"
        );
      }

      break;



    // ======================================================
    // WAITING FOR PASSWORD
    // ======================================================

    case powered_on:


      // correct password
      if (
        digitsEntered == 4 &&
        compareArrays(
          da_code,
          code
        )
      ) {

        Serial.println(
          "correct password"
        );


        clearCode();


        starPresses =
          0;


        // begin servo unlock
        unlock_servo();


        current_state =
          unlocking;


        lastActivityTime =
          millis();
      }


      // incorrect password
      else if (
        digitsEntered == 4 &&
        !compareArrays(
          da_code,
          code
        )
      ) {

        Serial.println(
          "wrong password"
        );


        clearCode();


        stateStartTime =
          millis();


        current_state =
          wrong_passcode;


        lastActivityTime =
          millis();
      }

      break;



    // ======================================================
    // SERVO IS UNLOCKING
    // ======================================================

    case unlocking:


      // Check for physical blockage
      if (
        servoBlocked()
      ) {

        revertServo();

        break;
      }


      // successful movement completed
      if (
        millis() -
        servoMoveStartTime >=
        SERVO_MOVE_TIME
      ) {

        disable_servo_pwm();


        servoCurrentPulse =
          UNLOCK_PULSE;


        current_state =
          unlock;


        starPresses =
          0;


        Serial.println(
          "locker unlocked"
        );


        Serial.println(
          "press * three times to lock"
        );


        Serial.println(
          "press # to change password"
        );


        lastActivityTime =
          millis();
      }

      break;



    // ======================================================
    // UNLOCKED
    // ======================================================

    case unlock:


      // Press * three times to lock
      if (
        newKeyPress &&
        key == '*'
      ) {

        starPresses++;


        Serial.print(
          "* pressed: "
        );

        Serial.print(
          starPresses
        );

        Serial.println(
          "/3"
        );


        if (
          starPresses >= 3
        ) {

          Serial.println(
            "locking locker"
          );


          starPresses =
            0;


          lock_servo();


          current_state =
            locking;


          lastActivityTime =
            millis();
        }
      }


      // Press # to change password
      if (
        newKeyPress &&
        key == '#'
      ) {

        clearCode();


        starPresses =
          0;


        current_state =
          set_passcode;


        Serial.println(
          "enter new 4 digit password"
        );


        lastActivityTime =
          millis();
      }

      break;



    // ======================================================
    // SERVO IS LOCKING
    // ======================================================

    case locking:


      // Check for physical blockage
      if (
        servoBlocked()
      ) {

        revertServo();

        break;
      }


      // Successful lock movement
      if (
        millis() -
        servoMoveStartTime >=
        SERVO_MOVE_TIME
      ) {

        disable_servo_pwm();


        servoCurrentPulse =
          LOCK_PULSE;


        current_state =
          lock;


        stateStartTime =
          millis();


        Serial.println(
          "locker locked"
        );
      }

      break;



    // ======================================================
    // LOCK STATE
    // ======================================================

    case lock:


      // short state before returning
      // to password entry
      if (
        millis() -
        stateStartTime >=
        250
      ) {

        clearCode();


        stateStartTime =
          0;


        current_state =
          powered_on;


        lastActivityTime =
          millis();


        Serial.println(
          "enter password"
        );
      }

      break;



    // ======================================================
    // WRONG PASSWORD
    // ======================================================

    case wrong_passcode:


      if (
        millis() -
        stateStartTime >=
        5000
      ) {

        clearCode();


        Serial.println(
          "try again"
        );


        Serial.println(
          "enter password"
        );


        current_state =
          powered_on;


        lastActivityTime =
          millis();
      }

      break;



    // ======================================================
    // SET NEW PASSWORD
    // ======================================================

    case set_passcode:


      if (
        digitsEntered == 4
      ) {

        da_code[0] =
          code[0];

        da_code[1] =
          code[1];

        da_code[2] =
          code[2];

        da_code[3] =
          code[3];


        savePasscode();


        Serial.print(
          "new password saved: "
        );


        for (
          int i = 0;
          i < 4;
          i++
        ) {

          Serial.print(
            da_code[i]
          );
        }


        Serial.println();


        clearCode();


        Serial.println(
          "locker remains unlocked"
        );


        Serial.println(
          "press * three times to lock"
        );


        current_state =
          unlock;


        starPresses =
          0;


        lastActivityTime =
          millis();
      }

      break;
  }


  // ========================================================
  // AUTOMATIC SLEEP
  // ========================================================

  // Only sleep when:
  // - locker is locked
  // - system is waiting for password
  // - no partial password has been entered
  // - no activity for 10 seconds

  if (
    current_state ==
      powered_on &&

    digitsEntered ==
      0 &&

    millis() -
    lastActivityTime >=
      SLEEP_TIME
  ) {

    enterSleep();
  }


  delay(20);
}