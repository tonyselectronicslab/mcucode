// Array containing the 10 digital pins used for the counter
// Pin 2 is Bit 0 (LSB), Pin 11 is Bit 9 (MSB)
const int ledPins[10] = {2, 3, 4, 5, 6, 7, 8, 9, 10, 11};

unsigned int counter = 0; // Tracks the count (0 to 1023)

void setup() {
  // Configure all 10 pins as OUTPUT
  for (int i = 0; i < 10; i++) {
    pinMode(ledPins[i], OUTPUT);
  }
}

void loop() {
  // Display the 10-bit binary number across the pins
  for (int i = 0; i < 10; i++) {
    // Check if the i-th bit of 'counter' is a 1 or 0
    int bitState = (counter >> i) & 1;
    digitalWrite(ledPins[i], bitState);
  }

  // Increment the counter and reset it at 1024 (2^10)
  counter = (counter + 1) % 1024;

  delay(50); // Wait half a second before the next count
}
