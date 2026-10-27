
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
void EEPROM_write(unsigned int address, unsigned char data) {

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

  EEPROM_write(EEPROM_MARKER, EEPROM_SAVED);

  Serial.println("password saved");
}


// loads password from EEPROM
void loadPasscode() {

  if (EEPROM_read(EEPROM_MARKER) == EEPROM_SAVED) {

    for (int i = 0; i < 4; i++) {
      da_code[i] =
        EEPROM_read(
          EEPROM_CODE_START + i
        );
    }

    Serial.println("saved password loaded");

    current_state = powered_on;

    Serial.println("enter password");

  } else {

    Serial.println("no saved password");

    current_state = set_passcode;

    clearCode();

    Serial.println("enter new 4 digit password");
  }
}


// ============================================================
// SERVO PWM FUNCTIONS
// ============================================================


// configure Timer1 for 50 Hz servo PWM
void setup_servo() {

  pinMode(SERVO_PIN, OUTPUT);

  // clear Timer1 registers
  TCCR1A = 0;
  TCCR1B = 0;

  // Fast PWM using ICR1 as TOP
  TCCR1A |= (1 << WGM11);

  TCCR1B |= (1 << WGM12) | (1 << WGM13);

  // PWM output on OC1A / pin 9
  TCCR1A |= (1 << COM1A1);

  // prescaler = 8
  TCCR1B |= (1 << CS11);

  // 20 ms period
  // 50 Hz
  ICR1 = 39999;
}


// sets servo PWM pulse width
void set_servo_pulse(int pulseWidth) {

  // Timer1 runs at 0.5 us per count
  OCR1A = pulseWidth * 2;

  servoCurrentPulse = pulseWidth;
}


// enables Timer1 servo output
void enable_servo_pwm() {

  // make sure Timer1 is powered
  PRR &= ~(1 << PRTIM1);

  setup_servo();

  set_servo_pulse(servoCurrentPulse);
}


// stops PWM output
void disable_servo_pwm() {

  // disconnect OC1A
  TCCR1A &= ~(1 << COM1A1);

  // stop Timer1
  TCCR1B &=
    ~(
      (1 << CS12) |
      (1 << CS11) |
      (1 << CS10)
    );

  digitalWrite(SERVO_PIN, LOW);
}


// servo power ON
void servoPowerOn() {
  digitalWrite(SERVO_POWER_PIN, HIGH);
  delay(20);
}


// servo power OFF
void servoPowerOff() {
  digitalWrite(SERVO_POWER_PIN, LOW);
}


// ============================================================
// SERVO MOVEMENT FUNCTIONS
// ============================================================


// starts servo movement without blocking program
void startServoMove(int newPulse) {

  previousServoPulse = servoCurrentPulse;

  servoTargetPulse = newPulse;

  highCurrentDetected = false;

  highCurrentStartTime = 0;

  servoPowerOn();

  enable_servo_pwm();

  set_servo_pulse(newPulse);

  servoMoveStartTime = millis();
}


// start unlocking
void unlock_servo() {
  Serial.println("servo unlocking");
  startServoMove(UNLOCK_PULSE);
}


// start locking
void lock_servo() {
  Serial.println("servo locking");
  startServoMove(LOCK_PULSE);
}


// ============================================================
// CURRENT / BLOCKAGE FUNCTIONS
// ============================================================


// reads the current sensor
int readServoCurrent() {
  return analogRead(CURRENT_SENSE_PIN);
}

int readBatteryLevel() {
  // put a large ass resistor to not draw current?? then multiply it????
  return analogRead(BATTERY_LEVEL_PIN); // 8.5ish volts apparaently
}


// checks if servo is blocked
bool servoBlocked() {

  unsigned long moveTime = millis() - servoMoveStartTime;


  // Ignore normal startup/inrush current
  if (moveTime < CURRENT_IGNORE_TIME) {
    highCurrentDetected = false;

    highCurrentStartTime = 0;

    return false;
  }


  int currentReading = readServoCurrent();


  // Uncomment during calibration if needed

  /*
  Serial.print("Current ADC: ");
  Serial.println(currentReading);
  */


  if (currentReading >= BLOCKED_CURRENT_ADC) {
    if (!highCurrentDetected) {
      highCurrentDetected = true;

      highCurrentStartTime = millis();
    }


    // Current has stayed high long enough
    if (millis() - highCurrentStartTime >= BLOCK_CONFIRM_TIME) {
      return true;
    }
  } else {
    highCurrentDetected = false;

    highCurrentStartTime = 0;
  }
  return false;
}


// return servo to previous position
void revertServo() {

  Serial.println("BLOCKED MOTOR DETECTED");

  Serial.println("reverting servo");

  // save failed target
  int failedTarget = servoTargetPulse;

  // move back to previous position
  set_servo_pulse(previousServoPulse);

  servoTargetPulse = previousServoPulse;


  servoMoveStartTime = millis();

  // Wait only for the actual physical reversal.
  // Main blockage was already detected.
  delay(500);


  disable_servo_pwm();


  Serial.println("servo returned to previous position");

  // If attempted unlock failed,
  // locker remains locked.
  if (failedTarget == UNLOCK_PULSE) {
    servoCurrentPulse = LOCK_PULSE;

    current_state = powered_on;

    clearCode();

    Serial.println("locker remains locked");

    Serial.println("enter password");
  }


  // If attempted lock failed,
  // locker remains unlocked.
  else {

    servoCurrentPulse = UNLOCK_PULSE;

    current_state = unlock;

    starPresses = 0;

    Serial.println("locker remains unlocked");

    Serial.println("remove blockage and try again");

    Serial.println("press * three times to lock");
  }

  lastActivityTime = millis();
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


void enterSleep() {

  Serial.println("entering power down sleep");

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
  PRR |= (1 << PRADC);

  // Timer1 off
  PRR |= (1 << PRTIM1);

  // SPI off
  PRR |= (1 << PRSPI);

  // Timer2 off
  PRR |= (1 << PRTIM2);

  // TWI off
  PRR |= (1 << PRTWI);


  // SET POWER-DOWN MODE

  // SM2 SM1 SM0 = 0 1 0
  SMCR &= ~((1 << SM2) | (1 << SM0));

  SMCR |= (1 << SM1);

  // clear pending external interrupt
  EIFR |= (1 << INTF0);


  wakeInterruptOccurred = false;


  // enable INT0
  EIMSK |= (1 << INT0);

  // enable sleep
  SMCR |= (1 << SE);

  // make sure interrupts are enabled
  sei();

  // GO TO SLEEP

  asm volatile("sleep");

  // CPU CONTINUES HERE AFTER WAKE

  // disable sleep
  SMCR &= ~(1 << SE);

  // RESTORE HARDWARE
 
  // ADC power ON
  PRR &= ~(1 << PRADC);

  // Timer1 power ON
  PRR &= ~(1 << PRTIM1);

  // SPI power ON
  PRR &= ~(1 << PRSPI);

  // Timer2 power ON
  PRR &= ~(1 << PRTIM2);

  // TWI power ON
  PRR &= ~(1 << PRTWI);


  // re-enable ADC
  ADCSRA |= (1 << ADEN);

  // re-enable comparator
  ACSR &= ~(1 << ACD);

  // Wait until wake button is released
  while (digitalRead(WAKE_PIN) == LOW) {
    delay(5);
  }


  // clear interrupt flag
  EIFR |= (1 << INTF0);

  // enable wake interrupt again
  EIMSK |= (1 << INT0);


  keyHeld = false;

  clearCode();

  current_state = powered_on;

  lastActivityTime = millis();

  Serial.println("system awake");

  Serial.println("enter password");
}
