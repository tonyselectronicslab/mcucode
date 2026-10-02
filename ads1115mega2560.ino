#include <Wire.h>
#include <Adafruit_ADS1X15.h>

Adafruit_ADS1115 ads;  // Create an instance of the ADS1115

void setup(void) 
{
  Serial.begin(115200);
  Serial.println("Initializing ADS1115...");

  // Initialize the ADS1115 at its default address (0x48)
  if (!ads.begin()) {
    Serial.println("Failed to initialize ADS1115. Check your wiring!");
    while (1);
  }

  // Set the gain (Optional). Default is GAIN_TWOTHIRDS (+/- 6.144V)
  // Even though max voltage is 6.144V, never exceed VDD + 0.3V!
  ads.setGain(GAIN_TWOTHIRDS);  

  ads.setDataRate(RATE_ADS1115_8SPS);
}

void loop(void) 
{
  int16_t adc0;
  float volts0;

  // Read raw 16-bit value from Analog Channel 0
  adc0 = ads.readADC_SingleEnded(0);
  
  // Convert the raw reading to voltage based on the GAIN_TWOTHIRDS multiplier (0.1875mV per bit)
  volts0 = ads.computeVolts(adc0);
  
  // Print results to the Serial Monitor
  Serial.print("AIN0 Raw: "); 
  Serial.print(adc0);
  Serial.print("\tVoltage: "); 
  Serial.println(volts0, 4); // Print with 4 decimal places

  delay(250);
}
