// PWM Pin definitions
const int redPin = 9;
const int greenPin = 10;
const int bluePin = 11;

// Speed of the fade (lower = faster, higher = slower)
const int fadeDelay = 30; 

void setup() {
  pinMode(redPin, OUTPUT);
  pinMode(greenPin, OUTPUT);
  pinMode(bluePin, OUTPUT);
}

void loop() {
  // Fade from Red to Green
  for (int i = 0; i <= 255; i++) {
    setColour(255 - i, i, 0);
    delay(fadeDelay);
  }

  // Fade from Green to Blue
  for (int i = 0; i <= 255; i++) {
    setColour(0, 255 - i, i);
    delay(fadeDelay);
  }

  // Fade from Blue to Red
  for (int i = 0; i <= 255; i++) {
    setColour(i, 0, 255 - i);
    delay(fadeDelay);
  }
}

// Helper function to write RGB values (0 to 255)
void setColour(int red, int green, int blue) {
  analogWrite(redPin, red);
  analogWrite(greenPin, green);
  analogWrite(bluePin, blue);
}
