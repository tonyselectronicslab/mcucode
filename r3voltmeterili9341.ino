#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>

// ILI9341 TFT Pin Definitions
#define TFT_CS   10
#define TFT_DC   9
#define TFT_RST  8

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);

// Voltmeter Constants
const int analogPin = A0;
const float vRef = 5.0; // Measure your Arduino 5V pin with a multimeter for exact calibration

float lastVoltage = -1.0;

void setup() {
  tft.begin();
  tft.setRotation(1); // Landscape mode (320x240)
  tft.fillScreen(ILI9341_BLACK);

  // Draw static UI frame
  tft.drawRect(10, 10, 300, 220, ILI9341_CYAN);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(2);
  tft.setCursor(55, 30);
  tft.print("BATTERY TESTER (0-5V)");
  
  tft.drawLine(20, 60, 300, 60, ILI9341_DARKGREY);
}

void loop() {
  // Take 15 samples to smooth out noise
  long sum = 0;
  for (int i = 0; i < 15; i++) {
    sum += analogRead(analogPin);
    delay(5);
  }
  float averageADC = sum / 15.0;

  // Direct 0-5V calculation
  float vIn = (averageADC * vRef) / 1024.0;

  // Filter out floating noise when no battery is connected
  if (vIn < 0.03) {
    vIn = 0.0;
  }

  // Only update screen if voltage changes by at least 0.01V
  if (abs(vIn - lastVoltage) >= 0.01) {
    // Clear previous number area
    tft.fillRect(40, 95, 240, 60, ILI9341_BLACK);

    // Print new voltage reading
    tft.setTextColor(ILI9341_GREEN);
    tft.setTextSize(5);
    tft.setCursor(65, 105);
    tft.print(vIn, 2); // 2 decimal places
    tft.print(" V");

    // Draw a visual bar graph (0 to 5.00V)
    int barWidth = map(constrain(vIn * 100, 0, 500), 0, 500, 0, 260);
    tft.drawRect(28, 178, 264, 24, ILI9341_WHITE);
    tft.fillRect(30, 180, barWidth, 20, ILI9341_YELLOW);
    tft.fillRect(30 + barWidth, 180, 260 - barWidth, 20, ILI9341_BLACK);

    lastVoltage = vIn;
  }

  delay(150);
}

