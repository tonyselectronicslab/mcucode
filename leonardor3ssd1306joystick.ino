/*
 * Arduino Leonardo R3 - Joystick Visualizer on SSD1306 OLED
 * Displays raw/mapped X and Y coordinates plus Button status.
 */

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// OLED Display Configuration
#define SCREEN_WIDTH 128    // OLED display width, in pixels
#define SCREEN_HEIGHT 64    // OLED display height, in pixels
#define OLED_RESET    -1    // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C // Mainstream I2C address for 0.96" SSD1306 OLED

// Initialize Display object
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Define Joystick Pin Allocations
const int X_PIN = A0;  
const int Y_PIN = A1;  
const int SW_PIN = 4;  // 2 is used by display and does conflict

// Calibration Constants
const int CENTER_VAL = 512; 
const int DEADZONE = 15;     

void setup() {
  Serial.begin(115200);

  // Initialize SSD1306 Screen
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Freeze execution if display isn't detected
  }

  // Clear splash screen buffer
  display.clearDisplay();
  display.display();

  // Configure Joystick Pins
  pinMode(X_PIN, INPUT);
  pinMode(Y_PIN, INPUT);
  pinMode(SW_PIN, INPUT_PULLUP); 
}

void loop() {
  // 1. Data Collection
  int rawX = analogRead(X_PIN);
  int rawY = analogRead(Y_PIN);
  bool buttonPressed = (digitalRead(SW_PIN) == LOW);

  // Apply calibration transforms
  int xVal = rawX - CENTER_VAL;
  int yVal = rawY - CENTER_VAL;
  if (abs(xVal) < DEADZONE) xVal = 0;
  if (abs(yVal) < DEADZONE) yVal = 0;

  // 2. Graphic Display Assembly
  display.clearDisplay(); // Wipe previous frame
  
  // Header Title Text
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(15, 0);
  display.print("JOYSTICK TELEMETRY");
  display.drawFastHLine(0, 10, 128, SSD1306_WHITE); // Horizontal divider line

  // Print X Axis Data
  display.setCursor(0, 18);
  display.print("X-Axis: ");
  display.print(xVal);
  display.print(" ("); display.print(rawX); display.print(")");

  // Print Y Axis Data
  display.setCursor(0, 34);
  display.print("Y-Axis: ");
  display.print(yVal);
  display.print(" ("); display.print(rawY); display.print(")");

  // Print Button Status
  display.setCursor(0, 50);
  display.print("Button: ");
  if (buttonPressed) {
    display.setTextColor(SSD1306_BLACK, SSD1306_WHITE); // Inverted text color highlight
    display.print(" DOWN ");
  } else {
    display.setTextColor(SSD1306_WHITE);
    display.print("UP");
  }
  display.setTextColor(SSD1306_WHITE); // Reset text mode

  // 3. Render Buffer onto Screen
  display.display(); 

  delay(50); // Fluid 20Hz UI refresh pace
}
