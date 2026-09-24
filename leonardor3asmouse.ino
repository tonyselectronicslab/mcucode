/*
 * Arduino Leonardo R3 - Joystick to USB Mouse Emulator
 * Displays data on SSD1306 OLED and controls PC mouse cursor.
 */

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Mouse.h> // Native library for Leonardo USB mouse emulation

// OLED Display Configuration
#define SCREEN_WIDTH 128    
#define SCREEN_HEIGHT 64    
#define OLED_RESET    -1    
#define SCREEN_ADDRESS 0x3C 

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Define Joystick Pin Allocations
const int X_PIN = A0;  
const int Y_PIN = A1;  
const int SW_PIN = 4;  // Kept on Pin 4 to avoid I2C conflict

// Calibration Constants
const int CENTER_VAL = 512; 
const int DEADZONE = 25;    // Slightly larger deadzone to completely stop drift when idle

// Mouse Sensitivity Setting
const int MAX_MOUSE_SPEED = 12; // Maximum pixel jump per loop refresh

void setup() {
  // Initialize SSD1306 Screen
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    for(;;); 
  }

  display.clearDisplay();
  display.display();

  // Configure Joystick Pins
  pinMode(X_PIN, INPUT);
  pinMode(Y_PIN, INPUT);
  pinMode(SW_PIN, INPUT_PULLUP); 

  // Initialize Native USB Mouse Emulation
  Mouse.begin();
}

void loop() {
  // 1. Data Collection
  int rawX = analogRead(X_PIN);
  int rawY = analogRead(Y_PIN);
  bool buttonPressed = (digitalRead(SW_PIN) == LOW);

  // Calculate offset relative to center
  int xVal = rawX - CENTER_VAL;
  int yVal = rawY - CENTER_VAL;

  // 2. Mouse Emulation Execution
  int mouseX = 0;
  int mouseY = 0;

  // Process X-Axis (Maps physical joystick left/right to cursor left/right)
  if (abs(xVal) > DEADZONE) {
    mouseX = map(xVal, -512, 512, -MAX_MOUSE_SPEED, MAX_MOUSE_SPEED);
  }

  // Process Y-Axis (Maps physical joystick up/down to cursor up/down)
  // Note: Standard joysticks invert Y relative to screen pixels, so we flip the mapping range.
  if (abs(yVal) > DEADZONE) {
    mouseY = map(yVal, -512, 512, MAX_MOUSE_SPEED, -MAX_MOUSE_SPEED);
  }

  // Move the cursor if movement values are detected
  if (mouseX != 0 || mouseY != 0) {
    Mouse.move(mouseX, mouseY, 0);
  }

  // Handle Left Mouse Button Press/Release states safely
  if (buttonPressed) {
    if (!Mouse.isPressed(MOUSE_LEFT)) {
      Mouse.press(MOUSE_LEFT);
    }
  } else {
    if (Mouse.isPressed(MOUSE_LEFT)) {
      Mouse.release(MOUSE_LEFT);
    }
  }

  // 3. Graphic Display Update
  display.clearDisplay();
  
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(15, 0);
  display.print("USB MOUSE MODE");
  display.drawFastHLine(0, 10, 128, SSD1306_WHITE);

  display.setCursor(0, 18);
  display.print("Cursor X speed: "); display.print(mouseX);
  
  display.setCursor(0, 34);
  display.print("Cursor Y speed: "); display.print(mouseY);

  display.setCursor(0, 50);
  display.print("Click State: ");
  if (buttonPressed) {
    display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
    display.print(" CLICKING ");
  } else {
    display.setTextColor(SSD1306_WHITE);
    display.print("IDLE");
  }
  display.setTextColor(SSD1306_WHITE);

  display.display(); 

  delay(15); // Faster loop speed (approx 60Hz) for smooth mouse tracking
}
