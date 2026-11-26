#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>

#define TFT_CS   10
#define TFT_DC   9
#define TFT_RST  8

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);

const int analogPin = A0;
const int testWavePin = 3; // Outputs 1kHz square wave
const float vRef = 5.0;    // Adjust to your measured 5V pin voltage

unsigned long lastScreenUpdate = 0;

void setup() {
  Serial.begin(115200);
  
  // Start 1kHz (1000 Hz) square wave on Pin D3 (runs in background via Timer2)
  pinMode(testWavePin, OUTPUT);
  tone(testWavePin, 1000);

  tft.begin();
  tft.setRotation(1);
  tft.fillScreen(ILI9341_BLACK);

  tft.drawRect(10, 10, 300, 220, ILI9341_CYAN);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(2);
  tft.setCursor(45, 25);
  tft.print("0-5V PC OSCILLOSCOPE");
  tft.drawLine(20, 50, 300, 50, ILI9341_DARKGREY);

  // Label for the test signal pin
  tft.setTextColor(ILI9341_YELLOW);
  tft.setTextSize(1);
  tft.setCursor(70, 205);
  tft.print("1kHz TEST SIGNAL ON PIN D3");
}

void loop() {
  // 1. Fast read and send to Windows PC
  int rawADC = analogRead(analogPin);
  float voltage = (rawADC * vRef) / 1024.0;
  Serial.println(voltage, 3);

  // 2. Update TFT display every 250ms
  if (millis() - lastScreenUpdate >= 250) {
    tft.fillRect(50, 95, 220, 50, ILI9341_BLACK);
    tft.setTextColor(ILI9341_GREEN);
    tft.setTextSize(4);
    tft.setCursor(75, 105);
    tft.print(voltage, 2);
    tft.print(" V");
    
    lastScreenUpdate = millis();
  }

  // Faster delay (~200 microseconds) so 1kHz pulses appear clearly on the PC graph
  delayMicroseconds(200);
}
