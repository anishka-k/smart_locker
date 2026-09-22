#define SERVO_PIN 9

#define LOCK_PULSE 1000
#define UNLOCK_PULSE 2000

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
  else return 'x'; // no key pressed / value doesn't match a digit
}

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

  pinMode(A0, INPUT);
  // servo setup
  setup_servo();
  lock_servo();

}

void loop() {
  // put your main code here, to run repeatedly:
  int value = analogRead(A0);

  // the value currently being pressed
  char key = identifyKey(value);

  if (key == '1') {
    unlock_servo();
  } else if (key == '2') {
    lock_servo();
  }
}
