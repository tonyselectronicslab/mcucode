#include <ADCInput.h>
#include <vector>
#include <array>

const int TEST_SIGNAL_PIN = 16; // GP16 (Pin 21) - 1 kHz square wave
const int PROBE_PIN       = 26; // GP26 / ADC0 (Pin 31) - Scope input

const uint32_t SAMPLE_RATE = 500000; // 500 kSa/s
const uint16_t BUFFER_SIZE = 500;    // 500 samples = 1.0 ms window
const uint16_t RING_SIZE   = 1024;

ADCInput adc(PROBE_PIN);
std::vector<uint16_t> ringBuf(RING_SIZE);
std::vector<uint16_t> frameBuf(BUFFER_SIZE);

uint16_t triggerLevel = 2048; // Default ~1.65V (0 - 4095)
bool triggerRising = true;

void setup() {
  Serial.begin(115200);

  pinMode(TEST_SIGNAL_PIN, OUTPUT);
  analogWriteFreq(1000);
  analogWriteRange(255);
  analogWrite(TEST_SIGNAL_PIN, 128);

  adc.setBuffers(4, 256);
  adc.setFrequency(SAMPLE_RATE);
  adc.begin();
}

void checkTriggerCommands() {
  while (Serial.available() >= 3) {
    if (Serial.peek() == 0xCC) {
      Serial.read(); // Consume 0xCC command header
      uint8_t edge = Serial.read();
      uint8_t lvlByte = Serial.read();
      triggerRising = (edge == 1);
      triggerLevel = (uint16_t)((lvlByte * 4095UL) / 255UL);
    } else {
      Serial.read(); // Discard out-of-sync byte
    }
  }
}

void loop() {
  checkTriggerCommands();

  for (uint16_t i = 0; i < RING_SIZE; i++) {
    ringBuf.at(i) = adc.read();
  }

  // Trigger with small hysteresis (+/- 30 ADC counts)
  const int16_t hyst = 30;
  uint16_t trigIdx = 0;

  for (uint16_t i = 1; i < RING_SIZE - BUFFER_SIZE; i++) {
    uint16_t prev = ringBuf.at(i - 1);
    uint16_t curr = ringBuf.at(i);

    if (triggerRising) {
      if (prev < (int)triggerLevel - hyst && curr >= triggerLevel) {
        trigIdx = i;
        break;
      }
    } else {
      if (prev > (int)triggerLevel + hyst && curr <= triggerLevel) {
        trigIdx = i;
        break;
      }
    }
  }

  for (uint16_t i = 0; i < BUFFER_SIZE; i++) {
    frameBuf.at(i) = ringBuf.at(trigIdx + i);
  }

  std::array<uint8_t, 2> header = { 0xAA, 0x55 };
  Serial.write(header.data(), header.size());
  Serial.write((uint8_t*)frameBuf.data(), BUFFER_SIZE * sizeof(uint16_t));

  delay(15);
}

