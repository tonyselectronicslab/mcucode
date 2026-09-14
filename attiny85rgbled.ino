// Digispark PWM Pins
const int redPin   = 0; // P0
const int greenPin = 1; // P1 (Note: This will also blink the built-in LED)
const int bluePin  = 4; // P4 (Note: Unplug LED from P4 during upload if it fails)

void setup() {
  pinMode(redPin, OUTPUT);
  pinMode(greenPin, OUTPUT);
  pinMode(bluePin, OUTPUT);
}

void loop() {
  // Smoothly fade from Red -> Green -> Blue -> Red
  
  // Red to Yellow to Green
  for (int i = 0; i <= 255; i++) {
    analogWrite(redPin, 255 - i);
    analogWrite(greenPin, i);
    analogWrite(bluePin, 0);
    delay(10);
  }
  
  // Green to Cyan to Blue
  for (int i = 0; i <= 255; i++) {
    analogWrite(redPin, 0);
    analogWrite(greenPin, 255 - i);
    analogWrite(bluePin, i);
    delay(10);
  }
  
  // Blue to Magenta to Red
  for (int i = 0; i <= 255; i++) {
    analogWrite(redPin, i);
    analogWrite(greenPin, 0);
    analogWrite(bluePin, 255 - i);
    delay(10);
  }
}
