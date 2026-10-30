// 5V to 20V Boost Converter Controller for Arduino Mega 2560  10mA output ( 2K resistor )
// Uses Timer 1 to generate a clean 31.25 kHz PWM signal

const int pwmPin = 11; // Connect this pin to the Gate of your IRLZ44N MOSFET

void setup() {
  pinMode(pwmPin, OUTPUT);
  
  // Configure Timer 1 for Phase Correct PWM, 8-bit mode (gives ~31.25 kHz frequency)
  // This overrides the default Arduino timer settings for this specific pin
  TCCR1A = _BV(COM1A1) | _BV(WGM10); 
  TCCR1B = _BV(CS10);                // Set prescaler to 1 (no division)

  // Set the duty cycle to exactly 75%
  // For an 8-bit timer, max value is 255. 
  // 255 * 0.75 = 191.25 -> Round to 191
  OCR1A = 56; // 24 is 5v to 12v 55 is 5v to 20v  10 is 7.3v  74 is 24v
}

void loop() {
  // The timer handles the PWM entirely in hardware.
  // The loop is left empty so your processor can handle other tasks.
}
