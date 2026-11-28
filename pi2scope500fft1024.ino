#include <hardware/adc.h>
#include <hardware/dma.h>
#include <vector>
#include <array>

const int TEST_SIGNAL_PIN = 16; // GP16 (Pin 21) - 10 kHz square wave
const int PROBE_PIN       = 26; // GP26 / ADC0 (Pin 31) - Scope input

const uint16_t BUFFER_SIZE = 1024; // 1024 samples = 2.048 ms at 500 kSa/s
const uint16_t RING_SIZE   = 2048;

std::vector<uint16_t> ringBuf(RING_SIZE);
std::vector<uint16_t> frameBuf(BUFFER_SIZE);

int dma_chan;
dma_channel_config dma_cfg;

uint16_t triggerLevel = 2048;
bool triggerRising = true;

void setup() {
  Serial.begin(115200);

  // 10 kHz test square wave on GP16
  pinMode(TEST_SIGNAL_PIN, OUTPUT);
  analogWriteFreq(10000);
  analogWriteRange(255);
  analogWrite(TEST_SIGNAL_PIN, 128);

  // Initialize RP2350 ADC hardware at max 500 kSa/s
  adc_init();
  adc_gpio_init(PROBE_PIN);
  adc_select_input(0); // GP26 = ADC0
  adc_fifo_setup(
    true,  // Write each completed conversion to the sample FIFO
    true,  // Enable DMA data request (DREQ)
    1,     // DREQ asserted when at least 1 sample is present
    false, // Disable error bit
    false  // Keep full 12-bit resolution
  );
  adc_set_clkdiv(0); // Exact 500,000 Sa/s

  // Configure DMA channel
  dma_chan = dma_claim_unused_channel(true);
  dma_cfg = dma_channel_get_default_config(dma_chan);
  channel_config_set_transfer_data_size(&dma_cfg, DMA_SIZE_16);
  channel_config_set_read_increment(&dma_cfg, false);
  channel_config_set_write_increment(&dma_cfg, true);
  channel_config_set_dreq(&dma_cfg, DREQ_ADC);
}

void checkTriggerCommands() {
  while (Serial.available() >= 3) {
    if (Serial.peek() == 0xCC) {
      Serial.read();
      uint8_t edge = Serial.read();
      uint8_t lvlByte = Serial.read();
      triggerRising = (edge == 1);
      triggerLevel = (uint16_t)((lvlByte * 4095UL) / 255UL);
    } else {
      Serial.read();
    }
  }
}

void captureAt500k() {
  adc_run(false);
  adc_fifo_drain();

  dma_channel_configure(
    dma_chan,
    &dma_cfg,
    ringBuf.data(),
    &adc_hw->fifo,
    RING_SIZE,
    true
  );

  adc_run(true);
  dma_channel_wait_for_finish_blocking(dma_chan);
  adc_run(false);
}

void loop() {
  checkTriggerCommands();
  captureAt500k();

  const int16_t hyst = 30;
  uint16_t trigIdx = 0;

  for (uint16_t i = 1; i < RING_SIZE - BUFFER_SIZE; i++) {
    uint16_t prev = ringBuf.at(i - 1) & 0x0FFF;
    uint16_t curr = ringBuf.at(i) & 0x0FFF;

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
    frameBuf.at(i) = ringBuf.at(trigIdx + i) & 0x0FFF;
  }

  std::array<uint8_t, 2> header = { 0xAA, 0x55 };
  Serial.write(header.data(), header.size());
  Serial.write((uint8_t*)frameBuf.data(), BUFFER_SIZE * sizeof(uint16_t));

  delay(15);
}
