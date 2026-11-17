#include "SmartLocker.h"

// PASSWORD
int code[PIN_LENGTH] = {0};
int digitsEntered = 0;
bool changingPasscode = false;
bool keyHeld = false;
int starPresses = 0;

uint8_t storedHash[32] = {0};
uint8_t pinSalt[8] = {0};

uint32_t legacyHash = 0;
bool legacyPassword = false;
bool storageFault = false;

// SERVO
unsigned long servoMoveStartTime = 0;
unsigned long highCurrentStartTime = 0;
unsigned long servoPowerStartTime = 0;

bool highCurrentDetected = false;
bool servoStarting = false;
bool servoReverting = false;
bool startupMove = false;

int previousServoPulse = LOCK_PULSE;
int servoTargetPulse = LOCK_PULSE;
int servoCurrentPulse = LOCK_PULSE;

unsigned char startupState = powered_on;

// STATE TIMERS
unsigned long stateStartTime = 0;
unsigned long lastActivityTime = 0;

unsigned char current_state = powered_off;
volatile bool wakeInterruptOccurred = false;

// KEYPAD DEBOUNCE
char candidateKey = 'x';
unsigned long candidateSince = 0;

// BATTERY
bool batteryLow = false;
bool batteryAlertPending = false;
unsigned long batteryMv = 0;
unsigned long lastBatteryCheck = 0;

// BUZZER
uint8_t soundKind = 0;
uint8_t soundStep = 0;
unsigned long soundStepStart = 0;

// WRONG-PASSWORD DELAY
uint8_t failedAttempts = 0;
unsigned long wrongWaitTime = 5000UL;

// SHA-256 CONSTANTS
const uint32_t shaK[64] PROGMEM = {
  0x428a2f98UL, 0x71374491UL, 0xb5c0fbcfUL, 0xe9b5dba5UL,
  0x3956c25bUL, 0x59f111f1UL, 0x923f82a4UL, 0xab1c5ed5UL,
  0xd807aa98UL, 0x12835b01UL, 0x243185beUL, 0x550c7dc3UL,
  0x72be5d74UL, 0x80deb1feUL, 0x9bdc06a7UL, 0xc19bf174UL,
  0xe49b69c1UL, 0xefbe4786UL, 0x0fc19dc6UL, 0x240ca1ccUL,
  0x2de92c6fUL, 0x4a7484aaUL, 0x5cb0a9dcUL, 0x76f988daUL,
  0x983e5152UL, 0xa831c66dUL, 0xb00327c8UL, 0xbf597fc7UL,
  0xc6e00bf3UL, 0xd5a79147UL, 0x06ca6351UL, 0x14292967UL,
  0x27b70a85UL, 0x2e1b2138UL, 0x4d2c6dfcUL, 0x53380d13UL,
  0x650a7354UL, 0x766a0abbUL, 0x81c2c92eUL, 0x92722c85UL,
  0xa2bfe8a1UL, 0xa81a664bUL, 0xc24b8b70UL, 0xc76c51a3UL,
  0xd192e819UL, 0xd6990624UL, 0xf40e3585UL, 0x106aa070UL,
  0x19a4c116UL, 0x1e376c08UL, 0x2748774cUL, 0x34b0bcb5UL,
  0x391c0cb3UL, 0x4ed8aa4aUL, 0x5b9cca4fUL, 0x682e6ff3UL,
  0x748f82eeUL, 0x78a5636fUL, 0x84c87814UL, 0x8cc70208UL,
  0x90befffaUL, 0xa4506cebUL, 0xbef9a3f7UL, 0xc67178f2UL
};

// SETUP
void setup() {
  Serial.begin(9600);

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(KEYPAD, INPUT);

  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(YELLOW_PIN, OUTPUT);

  pinMode(CURRENT_SENSE_PIN, INPUT);
  pinMode(BATTERY_LEVEL_PIN, INPUT);

  pinMode(WAKE_PIN, INPUT_PULLUP);
  pinMode(SERVO_POWER_PIN, OUTPUT);
  pinMode(SERVO_PIN, OUTPUT);

  digitalWrite(SERVO_PIN, LOW);
  servoPowerOff();

  // Disable digital input buffers on analog inputs.
  DIDR0 |= (1 << ADC0D) | (1 << ADC1D) | (1 << ADC2D);

  setupWakeInterrupt();
  loadPasscode();

  // Monitor startup locking instead of using a blocking delay.
  startupState = current_state;
  startupMove = true;

  lock_servo();
  current_state = locking;

  lastActivityTime = millis();

  Serial.println(F("Milestone 4 system ready"));
}

// MAIN LOOP
void loop() {
  unsigned long now = millis();

  updateSound();

  // Allow servo power to settle without blocking.
  if (servoStarting && now - servoPowerStartTime >= 20UL) {
    servoStarting = false;

    enable_servo_pwm();
    set_servo_pulse(servoTargetPulse);

    servoMoveStartTime = now;
  }

  // Monitor reversal too.
  if (servoReverting) {
    if (servoBlocked()) {
      motorFault();
    } else if (now - servoMoveStartTime >= SERVO_MOVE_TIME) {
      finishServoMove();
    }

    updateUI();
    return;
  }

  // Discard first reading after switching ADC channels.
  analogRead(KEYPAD);
  char key = identifyKey(analogRead(KEYPAD));

  bool newKeyPress = false;

  if (key != candidateKey) {
    candidateKey = key;
    candidateSince = now;
  }

  if (now - candidateSince >= KEY_DEBOUNCE_TIME) {
    if (key == 'x') {
      keyHeld = false;
    } else if (!keyHeld) {
      keyHeld = true;
      newKeyPress = true;

      lastActivityTime = now;
      buttonPress();
    }
  }

  // Store entered digits.
  if (newKeyPress &&
      !storageFault &&
      (current_state == powered_on ||
       current_state == set_passcode) &&
      key >= '0' &&
      key <= '9' &&
      digitsEntered < PIN_LENGTH) {

    code[digitsEntered] = key - '0';
    digitsEntered++;
  }

  // STATE MACHINE
  switch (current_state) {
    case powered_off:
      break;

    case powered_on:
      if (digitsEntered == PIN_LENGTH && !storageFault) {
        bool ok = pinMatches(code);
        clearCode();

        if (ok) {
          failedAttempts = 0;
          starPresses = 0;

          Serial.println(F("correct passcode"));

          unlock_servo();
          current_state = unlocking;
        } else {
          if (failedAttempts < 255) {
            failedAttempts++;
          }

          wrongWaitTime =
            failedAttempts >= 3 ? 30000UL : 5000UL;

          Serial.println(F("wrong password"));

          stateStartTime = millis();
          current_state = wrong_passcode;

          startSound(3);
        }

        lastActivityTime = millis();
      }
      break;

    case unlocking:
    case locking:
      if (!servoStarting) {
        if (servoBlocked()) {
          revertServo();
        } else if (
          millis() - servoMoveStartTime >= SERVO_MOVE_TIME
        ) {
          finishServoMove();
        }
      }
      break;

    case unlock:
      if (newKeyPress && key == '*') {
        starPresses++;

        if (starPresses >= 3) {
          starPresses = 0;

          lock_servo();
          current_state = locking;
        }
      } else if (newKeyPress) {
        starPresses = 0;

        if (key == '#') {
          clearCode();
          changingPasscode = true;
          current_state = set_passcode;

          startSound(4);

          Serial.println(F("enter new PIN"));
        }
      }
      break;

    case lock:
      if (millis() - stateStartTime >= 250UL) {
        clearCode();

        current_state = powered_on;
        lastActivityTime = millis();

        Serial.println(F("enter password"));
      }
      break;

    case wrong_passcode:
      if (millis() - stateStartTime >= wrongWaitTime) {
        clearCode();

        current_state = powered_on;
        lastActivityTime = millis();

        Serial.println(F("try again; enter password"));
      }
      break;

    case set_passcode:
      if (digitsEntered == PIN_LENGTH) {
        savePasscode();
        clearCode();

        starPresses = 0;

        if (storageFault) {
          current_state = motor_fault;
        } else {
          current_state =
            changingPasscode ? unlock : powered_on;

          Serial.println(F("new password saved"));
          startSound(4);
        }

        changingPasscode = false;
        lastActivityTime = millis();
      }
      break;

    case motor_fault:
      // Inspect the mechanism before restarting.
      break;

    case low_battery:
      // Battery alerts are handled separately so the
      // locked/unlocked state is preserved.
      break;
  }

  bool moving =
    current_state == locking ||
    current_state == unlocking ||
    servoReverting;

  if (!moving) {
    checkBattery(false);
  }

  if (batteryAlertPending && !moving && soundKind == 0) {
    startSound(5);
    batteryAlertPending = false;
  }

  updateUI();

  // Clear abandoned partial PINs and enter sleep.
  if (current_state == powered_on &&
      millis() - lastActivityTime >= SLEEP_TIME) {

    clearCode();

    if (soundKind == 0) {
      enterSleep();
    }
  }
}

// KEYPAD
char identifyKey(int value) {
  const int centers[12] = {
    ONE, TWO, THREE,
    FOUR, FIVE, SIX,
    SEVEN, EIGHT, NINE,
    ZERO, ASTERICK, HASHTAG
  };

  const char keys[12] = {
    '1', '2', '3',
    '4', '5', '6',
    '7', '8', '9',
    '0', '*', '#'
  };

  long bestError = 1024;
  char best = 'x';

  for (uint8_t i = 0; i < 12; i++) {
    long error = (long)value - centers[i];

    if (error < 0) {
      error = -error;
    }

    // Nearest matching center within 2.5%.
    if (error * 1000 <= (long)centers[i] * 25 &&
        error < bestError) {

      bestError = error;
      best = keys[i];
    }
  }

  return best;
}

void clearCode() {
  volatile int *p = code;

  for (uint8_t i = 0; i < PIN_LENGTH; i++) {
    p[i] = 0;
  }

  digitsEntered = 0;
}

// EEPROM
void EEPROM_write(unsigned int address, unsigned char data) {
  while (EECR & (1 << EEPE));

  EEAR = address;
  EEDR = data;

  unsigned char oldSREG = SREG;
  cli();

  EECR |= (1 << EEMPE);
  EECR |= (1 << EEPE);

  SREG = oldSREG;
}

unsigned char EEPROM_read(unsigned int address) {
  while (EECR & (1 << EEPE));

  EEAR = address;
  EECR |= (1 << EERE);

  return EEDR;
}

void savePasscode() {
  // Public salt, not a secret key or secure random source.
  uint32_t x = micros() ^ 0x73D192A5UL;

  for (uint8_t i = 0; i < 8; i++) {
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;

    pinSalt[i] ^= (uint8_t)x;
  }

  hashPin(code, storedHash);

  // Invalidate first so interrupted writes fail closed.
  EEPROM_write(EEPROM_MARKER, 0);
  EEPROM_write(1, PIN_LENGTH);

  for (uint8_t i = 0; i < 8; i++) {
    EEPROM_write(2 + i, pinSalt[i]);
  }

  for (uint8_t i = 0; i < 32; i++) {
    EEPROM_write(10 + i, storedHash[i]);
  }

  bool good = EEPROM_read(1) == PIN_LENGTH;

  for (uint8_t i = 0; i < 8; i++) {
    if (EEPROM_read(2 + i) != pinSalt[i]) {
      good = false;
    }
  }

  for (uint8_t i = 0; i < 32; i++) {
    if (EEPROM_read(10 + i) != storedHash[i]) {
      good = false;
    }
  }

  if (good) {
    EEPROM_write(EEPROM_MARKER, EEPROM_SAVED);
  }

  storageFault =
    !good ||
    EEPROM_read(EEPROM_MARKER) != EEPROM_SAVED;

  legacyPassword = false;

  if (storageFault) {
    Serial.println(F("EEPROM write failed; service required"));
  }
}

void loadPasscode() {
  uint8_t marker = EEPROM_read(EEPROM_MARKER);

  storageFault = false;
  legacyPassword = false;

  clearCode();

  if (marker == EEPROM_SAVED &&
      EEPROM_read(1) == PIN_LENGTH) {

    for (uint8_t i = 0; i < 8; i++) {
      pinSalt[i] = EEPROM_read(2 + i);
    }

    for (uint8_t i = 0; i < 32; i++) {
      storedHash[i] = EEPROM_read(10 + i);
    }

    current_state = powered_on;

    Serial.println(F("saved password loaded; enter password"));
  } else if (marker == EEPROM_LEGACY && PIN_LENGTH == 4) {
    legacyHash = 0;

    for (uint8_t i = 0; i < 4; i++) {
      legacyHash |=
        (uint32_t)EEPROM_read(1 + i) << (8 * i);
    }

    legacyPassword = true;
    current_state = powered_on;

    Serial.println(
      F("legacy password loaded; correct entry upgrades storage")
    );
  } else {
    bool erased = true;

    for (uint8_t i = 0; i < 42; i++) {
      if (EEPROM_read(i) != 255) {
        erased = false;
      }
    }

    if (erased) {
      current_state = set_passcode;

      Serial.println(F("first setup: enter new PIN"));
    } else {
      storageFault = true;
      current_state = motor_fault;

      Serial.println(F("EEPROM format invalid; service required"));
    }
  }

  changingPasscode = false;
}

// SERVO PWM
void setup_servo() {
  pinMode(SERVO_PIN, OUTPUT);

  TCCR1A = 0;
  TCNT1 = 0;
  TCCR1B = 0;

  // Fast PWM, ICR1 as TOP.
  TCCR1A |= (1 << WGM11);
  TCCR1B |= (1 << WGM12) | (1 << WGM13);

  // OC1A / D9 output.
  TCCR1A |= (1 << COM1A1);

  // Prescaler 8.
  TCCR1B |= (1 << CS11);

  // 20 ms period at 16 MHz.
  ICR1 = 39999;
}

void set_servo_pulse(int pulseWidth) {
  OCR1A = pulseWidth * 2;
  servoCurrentPulse = pulseWidth;
}

void enable_servo_pwm() {
  PRR &= ~(1 << PRTIM1);

  setup_servo();
  set_servo_pulse(servoCurrentPulse);
}

void disable_servo_pwm() {
  TCCR1A &= ~(1 << COM1A1);

  TCCR1B &= ~(
    (1 << CS12) |
    (1 << CS11) |
    (1 << CS10)
  );

  digitalWrite(SERVO_PIN, LOW);
}

void servoPowerOn() {
  digitalWrite(SERVO_POWER_PIN, HIGH);
}

void servoPowerOff() {
  digitalWrite(SERVO_POWER_PIN, LOW);
}

// SERVO MOVEMENT
void startServoMove(int newPulse) {
  previousServoPulse = servoCurrentPulse;
  servoTargetPulse = newPulse;

  highCurrentDetected = false;
  highCurrentStartTime = 0;

  servoReverting = false;

  servoPowerOn();

  servoStarting = true;
  servoPowerStartTime = millis();
}

void unlock_servo() {
  Serial.println(F("servo unlocking"));
  startServoMove(UNLOCK_PULSE);
}

void lock_servo() {
  Serial.println(F("servo locking"));
  startServoMove(LOCK_PULSE);
}

void stopServo() {
  disable_servo_pwm();
  servoPowerOff();

  servoStarting = false;
}

void finishServoMove() {
  bool wasReverting = servoReverting;

  stopServo();

  servoReverting = false;
  servoCurrentPulse = servoTargetPulse;

  clearCode();
  starPresses = 0;

  if (startupMove) {
    startupMove = false;
    current_state = startupState;
  } else if (servoTargetPulse == UNLOCK_PULSE) {
    current_state = unlock;

    if (!wasReverting) {
      beepUnlock();
    }

    Serial.println(
      F("unlocked: *** to lock, # to change password")
    );
  } else {
    current_state = wasReverting ? powered_on : lock;

    if (!wasReverting) {
      beepLock();
    }

    stateStartTime = millis();

    Serial.println(F("locked"));
  }

  lastActivityTime = millis();
}

// CURRENT / BLOCKAGE
int readServoCurrent() {
  analogRead(CURRENT_SENSE_PIN);
  return analogRead(CURRENT_SENSE_PIN);
}

bool servoBlocked() {
  unsigned long moveTime = millis() - servoMoveStartTime;

  if (moveTime < CURRENT_IGNORE_TIME) {
    highCurrentDetected = false;
    highCurrentStartTime = 0;

    return false;
  }

  int currentReading = readServoCurrent();

  // Uncomment temporarily to calibrate:
  // Serial.println(currentReading);

  if (currentReading >= BLOCKED_CURRENT_ADC) {
    if (!highCurrentDetected) {
      highCurrentDetected = true;
      highCurrentStartTime = millis();
    }

    if (millis() - highCurrentStartTime >= BLOCK_CONFIRM_TIME) {
      return true;
    }
  } else {
    highCurrentDetected = false;
    highCurrentStartTime = 0;
  }

  return false;
}

void revertServo() {
  Serial.println(F("BLOCKED MOTOR DETECTED"));

  if (startupMove) {
    // Previous physical position is unknown at startup.
    motorFault();
    return;
  }

  servoTargetPulse = previousServoPulse;
  set_servo_pulse(previousServoPulse);

  servoMoveStartTime = millis();

  highCurrentDetected = false;
  highCurrentStartTime = 0;

  servoReverting = true;

  startSound(3);

  Serial.println(F("reverting servo"));
}

void motorFault() {
  stopServo();

  servoReverting = false;
  startupMove = false;

  current_state = motor_fault;

  clearCode();
  startSound(3);

  Serial.println(
    F("MOTOR FAULT: position unknown; inspect mechanism before restart")
  );
}

// BATTERY
int readBatteryLevel() {
  analogRead(BATTERY_LEVEL_PIN);

  unsigned long sum = 0;

  for (uint8_t i = 0; i < 8; i++) {
    sum += analogRead(BATTERY_LEVEL_PIN);
  }

  return (sum + 4) / 8;
}

void checkBattery(bool force) {
  unsigned long now = millis();

  if (!force &&
      now - lastBatteryCheck < BATTERY_CHECK_TIME) {
    return;
  }

  lastBatteryCheck = now;

  unsigned long adc = readBatteryLevel();

  unsigned long senseMv =
    (adc * BATTERY_ADC_MV + 511UL) / 1023UL;

  batteryMv =
    (senseMv * (BATTERY_TOP_KOHM + BATTERY_BOTTOM_KOHM) +
     BATTERY_BOTTOM_KOHM / 2) /
    BATTERY_BOTTOM_KOHM;

  bool wasLow = batteryLow;

  if (batteryMv <= BATTERY_LOW_MV) {
    batteryLow = true;
  } else if (batteryMv >= BATTERY_CLEAR_MV) {
    batteryLow = false;
  }

  if (batteryLow && (!wasLow || force)) {
    batteryAlertPending = true;

    Serial.print(F("LOW BATTERY (mV): "));
    Serial.println(batteryMv);
  }
}

// BUZZER
// 1 = unlock
// 2 = lock
// 3 = error
// 4 = programming / saved
// 5 = low battery
// 6 = key click

void startSound(uint8_t kind) {
  noTone(BUZZER_PIN);

  soundKind = kind;
  soundStep = 0;
  soundStepStart = millis();

  updateSound();
}

void updateSound() {
  if (!soundKind) {
    return;
  }

  unsigned long elapsed = millis() - soundStepStart;

  if (elapsed >= 360UL ||
      (soundKind == 6 && elapsed >= 35UL)) {

    noTone(BUZZER_PIN);
    soundKind = 0;

    return;
  }

  uint8_t step =
    elapsed < 100UL ? 1 :
    (elapsed < 180UL ? 2 : 3);

  if (step == soundStep) {
    return;
  }

  soundStep = step;

  if (step == 2) {
    noTone(BUZZER_PIN);
    return;
  }

  unsigned int frequency = 1800;

  if (soundKind == 1) {
    frequency = step == 1 ? 1200 : 1800;
  }

  if (soundKind == 2) {
    frequency = step == 1 ? 1800 : 1200;
  }

  if (soundKind == 3) {
    frequency = 400;
  }

  if (soundKind == 4) {
    frequency = 2200;
  }

  if (soundKind == 5) {
    frequency = 800;
  }

  unsigned long duration =
    soundKind == 6 ? 35UL :
    (step == 1 ? 100UL : 180UL);

  tone(BUZZER_PIN, frequency, duration);
}

void beepUnlock() {
  startSound(1);
}

void beepLock() {
  startSound(2);
}

void buttonPress() {
  if (!soundKind) {
    startSound(6);
  }
}

// LED INDICATORS
void updateUI() {
  unsigned long now = millis();

  bool red = false;
  bool green = false;
  bool yellow = false;

  switch (current_state) {
    case powered_on:
      yellow = now % 2000UL < 50UL;
      break;

    case unlock:
      green = true;
      break;

    case lock:
      green = true;
      red = true;
      break;

    case wrong_passcode:
      red = now % 400UL < 200UL;
      break;

    case set_passcode:
      yellow = now % 500UL < 250UL;
      break;

    case unlocking:
    case locking:
      yellow = true;
      break;

    case motor_fault:
      red = now % 200UL < 100UL;
      break;
  }

  if (batteryLow &&
      current_state != wrong_passcode &&
      current_state != motor_fault) {

    unsigned long phase = now % 4000UL;

    red =
      red ||
      phase < 60UL ||
      (phase >= 180UL && phase < 240UL);
  }

  digitalWrite(RED_PIN, red ? HIGH : LOW);
  digitalWrite(GREEN_PIN, green ? HIGH : LOW);
  digitalWrite(YELLOW_PIN, yellow ? HIGH : LOW);
}

// WAKE INTERRUPT
void setupWakeInterrupt() {
  DDRD &= ~(1 << DDD2);
  PORTD |= (1 << PORTD2);

  // Low-level INT0.
  EICRA &= ~((1 << ISC01) | (1 << ISC00));

  EIFR = (1 << INTF0);

  // Enable only when entering sleep.
  EIMSK &= ~(1 << INT0);
}

ISR(INT0_vect) {
  wakeInterruptOccurred = true;
  EIMSK &= ~(1 << INT0);
}

// SLEEP
void enterSleep() {
  if (digitalRead(WAKE_PIN) == LOW) {
    return;
  }

  Serial.println(F("entering power down sleep"));
  Serial.flush();

  stopServo();

  noTone(BUZZER_PIN);
  soundKind = 0;

  digitalWrite(RED_PIN, LOW);
  digitalWrite(GREEN_PIN, LOW);
  digitalWrite(YELLOW_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  unsigned char savedPRR = PRR;
  unsigned char savedADCSRA = ADCSRA;
  unsigned char savedACSR = ACSR;

  ADCSRA &= ~(1 << ADEN);
  ACSR |= (1 << ACD);

  PRR |=
    (1 << PRADC) |
    (1 << PRTIM1) |
    (1 << PRSPI) |
    (1 << PRTIM2) |
    (1 << PRTWI);

  cli();

  current_state = powered_off;

  SMCR = (1 << SM1) | (1 << SE);

  EIFR = (1 << INTF0);
  wakeInterruptOccurred = false;

  EIMSK |= (1 << INT0);

  // Keep interrupt enable and sleep adjacent.
  asm volatile("sei\n\tsleep\n\t" ::: "memory");

  SMCR &= ~(1 << SE);
  EIMSK &= ~(1 << INT0);

  PRR = savedPRR;
  ADCSRA = savedADCSRA;
  ACSR = savedACSR;

  keyHeld = false;
  candidateKey = 'x';
  candidateSince = millis();

  clearCode();

  current_state = powered_on;
  lastActivityTime = millis();

  checkBattery(true);

  Serial.println(F("system awake; enter password"));
}

// SHA-256
// Supports only short messages of 0 through 55 bytes.
uint32_t rotateRight(uint32_t x, uint8_t n) {
  return (x >> n) | (x << (32 - n));
}

void sha256Short(
  const uint8_t *data,
  uint8_t len,
  uint8_t *out
) {
  uint32_t w[16] = {0};

  for (uint8_t i = 0; i < len; i++) {
    w[i / 4] |=
      (uint32_t)data[i] << (24 - 8 * (i % 4));
  }

  w[len / 4] |= 0x80UL << (24 - 8 * (len % 4));
  w[15] = (uint32_t)len * 8;

  uint32_t a = 0x6a09e667UL;
  uint32_t b = 0xbb67ae85UL;
  uint32_t c = 0x3c6ef372UL;
  uint32_t d = 0xa54ff53aUL;
  uint32_t e = 0x510e527fUL;
  uint32_t f = 0x9b05688cUL;
  uint32_t g = 0x1f83d9abUL;
  uint32_t h = 0x5be0cd19UL;

  for (uint8_t i = 0; i < 64; i++) {
    uint8_t j = i & 15;

    if (i >= 16) {
      uint32_t x = w[(i - 15) & 15];
      uint32_t y = w[(i - 2) & 15];

      uint32_t s0 =
        rotateRight(x, 7) ^
        rotateRight(x, 18) ^
        (x >> 3);

      uint32_t s1 =
        rotateRight(y, 17) ^
        rotateRight(y, 19) ^
        (y >> 10);

      w[j] += s0 + w[(i - 7) & 15] + s1;
    }

    uint32_t s1 =
      rotateRight(e, 6) ^
      rotateRight(e, 11) ^
      rotateRight(e, 25);

    uint32_t t1 =
      h +
      s1 +
      ((e & f) ^ ((~e) & g)) +
      pgm_read_dword(&shaK[i]) +
      w[j];

    uint32_t s0 =
      rotateRight(a, 2) ^
      rotateRight(a, 13) ^
      rotateRight(a, 22);

    uint32_t t2 =
      s0 +
      ((a & b) ^ (a & c) ^ (b & c));

    h = g;
    g = f;
    f = e;
    e = d + t1;
    d = c;
    c = b;
    b = a;
    a = t1 + t2;
  }

  uint32_t result[8] = {
    a + 0x6a09e667U,
    b + 0xbb67ae85U,
    c + 0x3c6ef372U,
    d + 0xa54ff53aU,
    e + 0x510e527fU,
    f + 0x9b05688cU,
    g + 0x1f83d9abU,
    h + 0x5be0cd19U
  };

  for (uint8_t i = 0; i < 32; i++) {
    out[i] =
      (uint8_t)(result[i / 4] >> (24 - 8 * (i % 4)));
  }

  volatile uint32_t *wipe = w;

  for (uint8_t i = 0; i < 16; i++) {
    wipe[i] = 0;
  }
}

void hashPin(const int *pin, uint8_t *out) {
  uint8_t message[48] = {0};

  for (uint8_t i = 0; i < 8; i++) {
    message[i] = pinSalt[i];
  }

  for (uint8_t i = 0; i < PIN_LENGTH; i++) {
    message[8 + i] = (uint8_t)('0' + pin[i]);
  }

  sha256Short(message, 8 + PIN_LENGTH, out);

  for (uint16_t r = 1; r < PIN_ROUNDS; r++) {
    for (uint8_t i = 0; i < 32; i++) {
      message[i] = out[i];
    }

    for (uint8_t i = 0; i < 8; i++) {
      message[32 + i] = pinSalt[i];
    }

    sha256Short(message, 40, out);
  }

  volatile uint8_t *wipe = message;

  for (uint8_t i = 0; i < sizeof(message); i++) {
    wipe[i] = 0;
  }
}

// Original hash retained only to migrate an existing password.
uint32_t legacyPinHash(const int *pin) {
  uint32_t h = 2166136261UL ^ 0xA5C3E17BUL;

  for (uint16_t r = 0; r < 2000; r++) {
    for (uint8_t i = 0; i < 4; i++) {
      h ^= (uint8_t)('0' + pin[i]);
      h *= 16777619UL;
    }

    for (uint8_t i = 0; i < 4; i++) {
      h ^= (uint8_t)(0xA5C3E17BUL >> (8 * i));
      h *= 16777619UL;
    }

    h ^= h >> 15;
    h *= 0x2C1B3C6DUL;
    h ^= h >> 12;
  }

  return h;
}

bool pinMatches(const int *digits) {
  if (storageFault) {
    return false;
  }

  if (legacyPassword) {
    if (PIN_LENGTH != 4 ||
        legacyPinHash(digits) != legacyHash) {
      return false;
    }

    // code[] still contains the authenticated PIN.
    savePasscode();

    return !storageFault;
  }

  uint8_t candidate[32];
  hashPin(digits, candidate);

  uint8_t difference = 0;

  for (uint8_t i = 0; i < 32; i++) {
    difference |= candidate[i] ^ storedHash[i];
  }

  volatile uint8_t *wipe = candidate;

  for (uint8_t i = 0; i < 32; i++) {
    wipe[i] = 0;
  }

  return difference == 0;
}