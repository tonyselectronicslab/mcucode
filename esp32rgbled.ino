#include <Adafruit_NeoPixel.h>

#define RGB_PIN        48 
#define NUM_PIXELS      1 

// Initialize the NeoPixel driver object
Adafruit_NeoPixel boardLED(NUM_PIXELS, RGB_PIN, NEO_GRB + NEO_KHZ800);

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n--- DIYmore RGB Full Spectrum Fade Engine ---");

  // Spin up the LED driver
  boardLED.begin();
  boardLED.setBrightness(80); // Comfortable brightness level
}

void loop() {
  // Hue ranges from 0 to 65535, covering the entire color wheel
  for (long hue = 0; hue < 65536; hue += 256) {
    
    // Convert the hue value into RGB format (Max Saturation and Value)
    uint32_t color = boardLED.ColorHSV(hue, 255, 255);
    
    // Apply the color to the onboard LED
    boardLED.setPixelColor(0, color);
    boardLED.show();
    
    // Control the speed of the fade (lower number = faster rainbow cycle)
    delay(20); 
  }
}

