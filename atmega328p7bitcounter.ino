// Define the LED pins based on Arduino pin mapping
const int LED_BIT0 = 6; // Least Significant Bit (LSB)
const int LED_BIT1 = 7;
const int LED_BIT2 = 8; // Most Significant Bit (MSB)

void setup() {
  // Configure the pins as outputs
  pinMode(LED_BIT0, OUTPUT);
  pinMode(LED_BIT1, OUTPUT);
  pinMode(LED_BIT2, OUTPUT);
}

void loop() {
  // Count from 0 to 7 (3 bits can hold values from 000 to 111)
  for (int count = 0; count < 8; count++) {
    
    // Extract each bit using bitwise AND (&) and bit shifting (>>)
    boolean bit0 = (count & 1);      // Checks if the 1st bit is active
    boolean bit1 = (count >> 1) & 1; // Checks if the 2nd bit is active
    boolean bit2 = (count >> 2) & 1; // Checks if the 3rd bit is active

    // Write the states to the physical pins
    digitalWrite(LED_BIT0, bit0);
    digitalWrite(LED_BIT1, bit1);
    digitalWrite(LED_BIT2, bit2);

    // Wait 500 milliseconds before updating the count
    delay(1000);
  }
}
