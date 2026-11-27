#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>

#define TFT_CS   10
#define TFT_DC   9
#define TFT_RST  8

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);

const int testWavePin = 3;
const float vRef = 5.0; // Adjust to your measured 5V pin voltage

const int NUM_SAMPLES = 800;      // 800 8-bit samples
uint8_t sampleBuffer[NUM_SAMPLES];

unsigned long lastScreenUpdate = 0;

void setup() {
  Serial.begin(1000000); // 1 Mbps: rock-solid USB transfer, still 120 kS/s capture

  // Configure ADC for maximum speed on Pin A0 (MUX = 0):
  // REFS0 = AVcc (5V) ref, ADLAR = Left-adjust result (read top 8 bits from ADCH)
  ADMUX = (1 << REFS0) | (1 << ADLAR);

  // Enable ADC (ADEN) + Prescaler = 8 (ADPS1 | ADPS0 -> 2 MHz ADC clock)
  ADCSRA = (1 << ADEN) | (1 << ADPS1) | (1 << ADPS0);

  // Disable digital input buffer on A0 to reduce noise and speed up sampling
  DIDR0 = (1 << ADC0D);

  // 1kHz square wave test signal on Pin D3
  pinMode(testWavePin, OUTPUT);
  tone(testWavePin, 1000);

  tft.begin();
  tft.setRotation(1);
  tft.fillScreen(ILI9341_BLACK);

  tft.drawRect(10, 10, 300, 220, ILI9341_CYAN);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(2);
  tft.setCursor(35, 25);
  tft.print("120kS/s PC OSCILLOSCOPE");
  tft.drawLine(20, 50, 300, 50, ILI9341_DARKGREY);

  tft.setTextColor(ILI9341_YELLOW);
  tft.setTextSize(1);
  tft.setCursor(70, 205);
  tft.print("1kHz TEST SIGNAL ON PIN D3");
}

// Inline helper for ultra-fast 8-bit ADC conversion
inline uint8_t fastReadA0() {
  ADCSRA |= (1 << ADSC);              // Start conversion
  while (ADCSRA & (1 << ADSC));       // Wait for conversion to finish
  return ADCH;                        // Return 8-bit value (0-255)
}

void loop() {
  // 1. Trigger on rising edge across ~2.5V (128 in 8-bit scale)
  unsigned long triggerStart = micros();
  while (fastReadA0() > 115 && (micros() - triggerStart < 2000));
  while (fastReadA0() < 128 && (micros() - triggerStart < 4000));

  // 2. Burst-capture 800 samples (~6.5 ms total -> ~120 kS/s)
  unsigned long startTime = micros();
  for (int i = 0; i < NUM_SAMPLES; i++) {
    ADCSRA |= (1 << ADSC);
    while (ADCSRA & (1 << ADSC));
    sampleBuffer[i] = ADCH;
  }
  unsigned long captureDuration = micros() - startTime;

  // 3. Send packet: 4-byte Header + 4-byte Duration + 800 Samples + 2-byte Footer
Serial.write(0xFF);
Serial.write(0x00);
Serial.write(0xAA);
Serial.write(0x55);
Serial.write((uint8_t*)&captureDuration, 4);
Serial.write(sampleBuffer, NUM_SAMPLES);
Serial.write(0x55);
Serial.write(0xAA);


  // 4. Update TFT display every 300ms
  if (millis() - lastScreenUpdate >= 300) {
    uint8_t maxVal = 0;
    for (int i = 0; i < NUM_SAMPLES; i++) {
      if (sampleBuffer[i] > maxVal) maxVal = sampleBuffer[i];
    }
    float vPeak = (maxVal * vRef) / 255.0;

    tft.fillRect(50, 95, 220, 50, ILI9341_BLACK);
    tft.setTextColor(ILI9341_GREEN);
    tft.setTextSize(4);
    tft.setCursor(55, 105);
    tft.print("Pk:");
    tft.print(vPeak, 2);
    tft.print("V");

    lastScreenUpdate = millis();
  }

  delay(25);
}
