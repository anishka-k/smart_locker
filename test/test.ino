int code[4] = {0, 0, 0, 0};
int digitsEntered = 0;
bool keyHeld = false;

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

void setup() {
  Serial.begin(9600);
  pinMode(A0, INPUT);
}

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
  else return -1;
}

void loop() {
  int value = analogRead(A0);
  int key = identifyKey(value);

  if (key != -1) {
    if (!keyHeld) {
      code[digitsEntered] = key;
      digitsEntered++;

      Serial.print("Digit entered: ");
      Serial.println(key);

      if (digitsEntered == 4) {
        Serial.print("Full code: ");
        for (int i = 0; i < 4; i++) {
          Serial.print(code[i]);
        }
        Serial.println();
        digitsEntered = 0;
      }
    }
    keyHeld = true; 
  } else {
    keyHeld = false;
  }

  delay(20);}
