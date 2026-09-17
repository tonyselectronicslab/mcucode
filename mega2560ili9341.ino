// my ILI9341 2.4 does 5v or 3.3v both power and logic
// VCC and backlight 5v
// CS 53, RES 49, DC 48, SDA 51, SCL 52 

#include "SPI.h"
#include "Adafruit_GFX.h"
#include "Adafruit_ILI9341.h"

// Define pins matching the Mega 2560 hardware layout
#define TFT_CS   53
#define TFT_DC   48
#define TFT_RST  49

// Initialize the Adafruit ILI9341 library using Hardware SPI
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);

void setup() {
  Serial.begin(9600);
  
  // Initialize the display
  tft.begin();

  // Set rotation (0 to 3) -> 1 is landscape mode
  tft.setRotation(1);

  // Clear the screen with a black background
  tft.fillScreen(ILI9341_BLACK);

  // ---- Draw Static Title Text ----
  tft.setCursor(20, 30);            // Coordinate (X, Y)
  tft.setTextColor(ILI9341_YELLOW); // Text color
  tft.setTextSize(2);               // Font size (1 is smallest, 3 is large)
  tft.println("Mega 2560 & ILI9341");

  // ---- Draw Subtitle Text ----
  tft.setCursor(20, 80);
  tft.setTextColor(ILI9341_GREEN);
  tft.setTextSize(2);
  tft.println("Testing text display...");
}

void loop() {
  // ---- Draw Dynamic/Updating Text ----
  // To avoid text overlapping, redraw a small background block or use the 
  // text background parameter: setTextColor(Text_Color, Background_Color)
  
  tft.setCursor(20, 140);
  tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK); 
  tft.setTextSize(2);
  tft.print("Uptime (s): ");
  tft.print(millis() / 1000);
  
  delay(1000); // Update every second
}
