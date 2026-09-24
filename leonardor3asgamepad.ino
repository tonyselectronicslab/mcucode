/*
 * Arduino Leonardo R3 - PC Gamepad & OLED Crosshair Emulator
 * Emulates a USB HID Gamepad and draws a visual crosshair target.
 */

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Joystick.h> // Custom HID Joystick library

// OLED Configuration
#define SCREEN_WIDTH 128    
#define SCREEN_HEIGHT 64    
#define OLED_RESET    -1    
#define SCREEN_ADDRESS 0x3C 

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Define Pins
const int X_PIN = A0;  
const int Y_PIN = A1;  
const int SW_PIN = 4;  

// Calibration Constants
const int CENTER_VAL = 512; 
const int DEADZONE = 20;    

// Initialize Joystick HID Profile 
Joystick_ Joystick(JOYSTICK_DEFAULT_REPORT_ID, 
  JOYSTICK_TYPE_GAMEPAD, 
  1, 0,                  // 1 Button, 0 Hat Switches
  true, true, false,     // X and Y enabled, Z disabled
  false, false, false,   // No Rx, Ry, Rz
  false, false,          // No Rudder, No Throttle
  false, false, false);  // No Accelerator, Brake, or Steering

void setup() {
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    for(;;); 
  }

  display.clearDisplay();
  display.display();

  pinMode(X_PIN, INPUT);
  pinMode(Y_PIN, INPUT);
  pinMode(SW_PIN, INPUT_PULLUP); 

  // Set gamepad axis ranges to match standard joystick outputs
  Joystick.setXAxisRange(-512, 512);
  Joystick.setYAxisRange(-512, 512);
  
  Joystick.begin(); // Activate the USB HID Gamepad profile
}

void loop() {
  // 1. Telemetry Capture
  int rawX = analogRead(X_PIN);
  int rawY = analogRead(Y_PIN);
  bool buttonPressed = (digitalRead(SW_PIN) == LOW);

  int xVal = rawX - CENTER_VAL;
  int yVal = rawY - CENTER_VAL;

  if (abs(xVal) < DEADZONE) xVal = 0;
  if (abs(yVal) < DEADZONE) yVal = 0;

  // 2. Transmit USB Gamepad Signals to PC
  Joystick.setXAxis(xVal);
  // Game engines read Y-axis downward, so we invert it for proper control behavior
  Joystick.setYAxis(-yVal); 
  
  if (buttonPressed) {
    Joystick.setButton(0, 1); // Press Gamepad Button 0
  } else {
    Joystick.setButton(0, 0); // Release Gamepad Button 0
  }

  // 3. Render Visual OLED Crosshair
  display.clearDisplay();

  // Map the joystick input (-512 to 512) to fit screen coordinate pixels 
  int crosshairX = map(xVal, -512, 512, 0, SCREEN_WIDTH - 1);
  int crosshairY = map(yVal, -512, 512, SCREEN_HEIGHT - 1, 0);

  // Draw Crosshair Visual elements (FIXED SYNTAX HERE)
  display.drawCircle(crosshairX, crosshairY, 5, SSD1306_WHITE); // Outer target ring
  display.drawFastHLine(crosshairX - 8, crosshairY, 17, SSD1306_WHITE); // Horizontal scope cross
  display.drawFastVLine(crosshairX, crosshairY - 8, 17, SSD1306_WHITE);   // Vertical scope cross

  // If the button is pressed, draw a visual filled indicator ring
  if (buttonPressed) {
    display.drawCircle(crosshairX, crosshairY, 2, SSD1306_WHITE); 
  }

  display.display(); 
  delay(15); 
}

