/*
 * Arduino Leonardo R3 - Analogue Joystick Reader
 * Reads X, Y coordinates and the integrated push button.
 */

// Define Pin Allocations
const int X_PIN = A0;  // X-Axis analogue input
const int Y_PIN = A1;  // Y-Axis analogue input
const int SW_PIN = 2;  // Digital input for the push button switch

// Calibration Constants
const int CENTER_VAL = 512; // Ideal joystick midpoint
const int DEADZONE = 15;     // Restricts center jitter/drift (-15 to +15)

void setup() {
  // Initialize USB Serial Communication for Leonardo
  Serial.begin(921600);
  while (!Serial) {
    ; // Wait for the serial port to connect (Leonardo specific requirement)
  }

  // Configure Pins
  pinMode(X_PIN, INPUT);
  pinMode(Y_PIN, INPUT);
  
  // Use INPUT_PULLUP so the pin defaults to HIGH. 
  // When pressed, the circuit closes to GND and reads LOW.
  pinMode(SW_PIN, INPUT_PULLUP); 

  Serial.println("Joystick initialization complete.");
}

void loop() {
  // 1. Physical Execution: Read the raw 10-bit analogue signals (0 to 1023)
  int rawX = analogRead(X_PIN);
  int rawY = analogRead(Y_PIN);
  
  // Read the active-low digital switch status (0 = Pressed, 1 = Idle)
  bool buttonState = digitalRead(SW_PIN);

  // 2. Visual Interpretation & Deductive Logic: Map to cartesian coordinates (-512 to 512)
  int xVal = rawX - CENTER_VAL;
  int yVal = rawY - CENTER_VAL;

  // Apply deadzone processing to filter out minor mechanical variance at rest
  if (abs(xVal) < DEADZONE) xVal = 0;
  if (abs(yVal) < DEADZONE) yVal = 0;

  // 3. Diagnostic Data Output
  Serial.print("RAW X: ");    Serial.print(rawX);
  Serial.print("\tRAW Y: ");  Serial.print(rawY);
  Serial.print("\tMapped X: "); Serial.print(xVal);
  Serial.print("\tMapped Y: "); Serial.print(yVal);
  Serial.print("\tButton: ");
  
  if (buttonState == LOW) {
    Serial.println("PRESSED");
  } else {
    Serial.println("RELEASED");
  }

  // Micro-progressive polling frequency (10Hz)
  delay(100);
}
