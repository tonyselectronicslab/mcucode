const int RED_PIN   = 13;
const int GREEN_PIN = 14;
const int BLUE_PIN  = 15;

void setColor(uint8_t r, uint8_t g, uint8_t b) {
  analogWrite(RED_PIN, r);
  analogWrite(GREEN_PIN, g);
  analogWrite(BLUE_PIN, b);
}

void setup() {
  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);
}

void loop() {
  setColor(255, 0, 0);     // Red
  delay(1000);
  setColor(0, 255, 0);     // Green
  delay(1000);
  setColor(0, 0, 255);     // Blue
  delay(1000);
  setColor(255, 255, 0);   // Yellow
  delay(1000);
  setColor(0, 255, 255);   // Cyan
  delay(1000);
  setColor(255, 0, 255);   // Magenta
  delay(1000);
  setColor(255, 255, 255); // White
  delay(1000);
}
