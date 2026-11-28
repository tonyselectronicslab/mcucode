const int TEST_SIGNAL_PIN = 16; // GP16 (Physical Pin 21) - 1 kHz output
const int PROBE_PIN       = 26; // GP26 / ADC0 (Physical Pin 31) - Scope input

const int BUFFER_SIZE = 250;    // Number of samples per screen capture
uint16_t samples[BUFFER_SIZE];

void setup() {
  Serial.begin(115200);
  
  // Set up 12-bit ADC resolution (0 - 4095 for 0V - 3.3V)
  analogReadResolution(12);
  pinMode(PROBE_PIN, INPUT);

  // Configure 1 kHz, 50% duty cycle square wave on GP16
  pinMode(TEST_SIGNAL_PIN, OUTPUT);
  analogWriteFreq(1000);
  analogWriteRange(255);
  analogWrite(TEST_SIGNAL_PIN, 128);
}

void loop() {
  // 1. Wait for a rising-edge trigger around mid-scale (~1.65V) to stabilize the wave
  unsigned long triggerTimeout = micros();
  while (analogRead(PROBE_PIN) > 1800 && (micros() - triggerTimeout < 5000));
  while (analogRead(PROBE_PIN) < 2200 && (micros() - triggerTimeout < 5000));

  // 2. Capture a burst of samples (~20 microseconds per sample = ~50 kSa/s)
  for (int i = 0; i < BUFFER_SIZE; i++) {
    samples[i] = analogRead(PROBE_PIN);
    delayMicroseconds(18);
  }

  // 3. Send the captured frame to the Arduino Serial Plotter
  for (int i = 0; i < BUFFER_SIZE; i++) {
    float voltage = (samples[i] * 3.3f) / 4095.0f;
    Serial.print("Voltage:");
    Serial.println(voltage, 3);
  }

  // Short pause between frames so the wave stays readable on screen
  delay(100);
}
