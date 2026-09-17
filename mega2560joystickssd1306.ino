#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Reset pin # (or -1 if sharing Arduino reset pin)
#define OLED_RESET     -1 
// 0x3C is standard for most 128x64 OLEDs; change to 0x3D if it doesn't display
#define SCREEN_ADDRESS 0x3C 

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Joystick Pin definitions
const int xPin = A0; 
const int yPin = A1; 
const int swPin = 2;  

void setup() {
  // Initialize standard hardware Serial for USB debugging
  Serial.begin(9600);

  // Initialize joystick button with internal pull-up resistor
  pinMode(swPin, INPUT_PULLUP);

  // SSD1306_SWITCHCAPVCC = generate display voltage from 5V internal
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }

  // Clear the buffer and configure text settings
  display.clearDisplay();
  display.setTextSize(2);             // Clean, readable size
  display.setTextColor(SSD1306_WHITE); // Draw white text
}

void loop() {
  // 1. Read values from the joystick (0 - 1023)
  int xVal = analogRead(xPin);
  int yVal = analogRead(yPin);
  int btnVal = digitalRead(swPin); // LOW = Pressed, HIGH = Released

  // 2. Also send data down the main hardware USB serial port for debugging
  Serial.print("X: ");
  Serial.print(xVal);
  Serial.print(" | Y: ");
  Serial.print(yVal);
  Serial.print(" | Button: ");
  Serial.println(btnVal == LOW ? "PRESSED" : "RELEASED");

  // 3. Update the OLED Screen
  display.clearDisplay(); // Always wipe the screen before drawing new text
  
  // Print X Value
  display.setCursor(0, 0);
  display.print("X: ");
  display.print(xVal);

  // Print Y Value
  display.setCursor(0, 22);
  display.print("Y: ");
  display.print(yVal);

  // Print Button Status
  display.setCursor(0, 44);
  display.print("BTN: ");
  if (btnVal == LOW) {
    display.print("DOWN");
  } else {
    display.print("UP");
  }

  // Render the data buffer onto the physical screen
  display.display(); 

  delay(50); // Fast enough to be responsive, slow enough to prevent display flickering
}
