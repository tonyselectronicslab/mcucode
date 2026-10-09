void setup() {
  // Configure PB0, PB1, PB2, PB3, and PB4 as OUTPUTS.
  // We leave PB5 (bit 5) as 0 so it remains an input/reset line.
  DDRB = 0B00011111; 
}

void loop() {
  // Count from 0 to 31 (the maximum value for 5 bits)
  for (byte counter = 0; counter < 32; counter++) {
    
    // Clear out any old values on the lower 5 pins of Port B, 
    // while explicitly keeping whatever state PB5 is currently holding.
    PORTB = (PORTB & 0B11100000) | counter; 
    
    delay(1000); // 500ms delay between increments
  }
}

