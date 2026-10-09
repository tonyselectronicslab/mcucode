// Shift register control pins
const int dataPin  = 0; // ATtiny85 PB0 (Physical Pin 5)
const int latchPin = 1; // ATtiny85 PB1 (Physical Pin 6)
const int clockPin = 2; // ATtiny85 PB2 (Physical Pin 7)

// Upper bit pins on ATtiny85
const int bit8Pin  = 3; // ATtiny85 PB3 (Physical Pin 2)
const int bit9Pin  = 4; // ATtiny85 PB4 (Physical Pin 3)

void setup() {
  // Set all control and upper bit pins as outputs
  pinMode(dataPin, OUTPUT);
  pinMode(latchPin, OUTPUT);
  pinMode(clockPin, OUTPUT);
  pinMode(bit8Pin, OUTPUT);
  pinMode(bit9Pin, OUTPUT);
}

void loop() {
  // Count from 0 to 1023 (10-bit max range)
  for (unsigned int counter = 0; counter < 1024; counter++) {
    
    // 1. Separate the 10-bit number into two parts:
    // Extract the lowest 8 bits (0-255) for the shift register
    byte lower8Bits = counter & 0xFF; 
    
    // Extract bit 8 and bit 9 for the ATtiny85 pins
    bool bit8State = (counter >> 8) & 1;
    bool bit9State = (counter >> 9) & 1;

    // 2. Update the shift register (Bits 0-7)
    digitalWrite(latchPin, LOW);
    shiftOut(dataPin, clockPin, LSBFIRST, lower8Bits);
    digitalWrite(latchPin, HIGH);

    // 3. Update the native ATtiny85 pins (Bits 8-9)
    digitalWrite(bit8Pin, bit8State);
    digitalWrite(bit9Pin, bit9State);

    delay(50); // Adjust this to speed up or slow down the counter
  }
}
