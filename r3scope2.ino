#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>

#define TFT_CS   10
#define TFT_DC   9
#define TFT_RST  8

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);

const int analogPin = A0;
const int testWavePin = 3;
const float vRef = 5.0; // Adjust to your measured 5V pin voltage

const int NUM_SAMPLES = 400;
uint16_t sampleBuffer[NUM_SAMPLES];

unsigned long lastScreenUpdate = 0;

void setup() {
  Serial.begin(1000000); // 1 Mbps high-speed serial

  // Overclock ADC: Set prescaler from 128 to 16 (1MHz ADC clock -> ~50,000 samples/sec)
  ADCSRA = (ADCSRA & 0xF8) | 0x04;

  // 1kHz square wave test signal on Pin D3
  pinMode(testWavePin, OUTPUT);
  tone(testWavePin, 1000);

  tft.begin();
  tft.setRotation(1);
  tft.fillScreen(ILI9341_BLACK);

  tft.drawRect(10, 10, 300, 220, ILI9341_CYAN);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(2);
  tft.setCursor(42, 25);
  tft.print("50kS/s PC OSCILLOSCOPE");
  tft.drawLine(20, 50, 300, 50, ILI9341_DARKGREY);

  tft.setTextColor(ILI9341_YELLOW);
  tft.setTextSize(1);
  tft.setCursor(70, 205);
  tft.print("1kHz TEST SIGNAL ON PIN D3");
}

void loop() {
  // 1. Trigger: Wait for rising edge across ~2.5V (ADC 512) so wave stays frozen
  unsigned long triggerStart = micros();
  while (analogRead(analogPin) > 480 && (micros() - triggerStart < 2000)); // Wait for LOW
  while (analogRead(analogPin) < 512 && (micros() - triggerStart < 4000)); // Wait for HIGH

  // 2. Burst-capture 400 samples as fast as possible (~20 microseconds per sample)
  unsigned long startTime = micros();
  for (int i = 0; i < NUM_SAMPLES; i++) {
    sampleBuffer[i] = analogRead(analogPin);
  }
  unsigned long captureDuration = micros() - startTime; // Total time in microseconds

  // 3. Send packet to Windows: Sync Header (0xAA, 0x55) + Duration + 400 raw samples
  Serial.write(0xAA);
  Serial.write(0x55);
  Serial.write((uint8_t*)&captureDuration, 4);
  Serial.write((uint8_t*)sampleBuffer, NUM_SAMPLES * 2);

  // 4. Update TFT screen every 300ms with Peak-to-Peak / Max voltage
  if (millis() - lastScreenUpdate >= 300) {
    uint16_t maxADC = 0;
    for (int i = 0; i < NUM_SAMPLES; i++) {
      if (sampleBuffer[i] > maxADC) maxADC = sampleBuffer[i];
    }
    float vPeak = (maxADC * vRef) / 1024.0;

    tft.fillRect(50, 95, 220, 50, ILI9341_BLACK);
    tft.setTextColor(ILI9341_GREEN);
    tft.setTextSize(4);
    tft.setCursor(55, 105);
    tft.print("Pk:");
    tft.print(vPeak, 2);
    tft.print("V");

    lastScreenUpdate = millis();
  }

  delay(30); // ~30 frames per second to PC
}
