// Define the RGB LED pins (must be PWM-capable on the Leonardo)
const int RED_PIN = 3;   
const int GREEN_PIN = 5; 
const int BLUE_PIN = 6;  

// SET MAXIMUM VALUES HERE (Range: 0 to 255)
// Lower these numbers to reduce the maximum brightness of a specific color
const int MAX_RED = 255;   
const int MAX_GREEN = 100; // Reduced to balance overpowering greens
const int MAX_BLUE = 100;  // Reduced to balance overpowering blues

// Set to true if you are using a Common Anode RGB LED, false for Common Cathode
const bool COMMON_ANODE = false; 

void setup() {
  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);

  analogWrite(RED_PIN,MAX_RED);
  delay(3000);
  analogWrite(RED_PIN,0);
  analogWrite(GREEN_PIN,MAX_GREEN);
  delay(3000);
  analogWrite(GREEN_PIN,0);
  analogWrite(BLUE_PIN,MAX_BLUE);
  delay(3000);
  analogWrite(BLUE_PIN,0);  
}

void loop() {
  // Fade Red -> Green -> Blue
  crossFade(RED_PIN, MAX_RED, GREEN_PIN, MAX_GREEN);
  crossFade(GREEN_PIN, MAX_GREEN, BLUE_PIN, MAX_BLUE);
  crossFade(BLUE_PIN, MAX_BLUE, RED_PIN, MAX_RED);
}

// Helper function to fade smoothly from one color to another using custom max limits
void crossFade(int pinUp, int maxUp, int pinDown, int maxDown) {
  // We use 100 steps for a clean, proportional percentage transition
  for (int step = 0; step <= 100; step++) {
    // Map the 0-100 percentage step to each pin's specific maximum limit
    int valUp = map(step, 0, 100, 0, maxUp);
    int valDown = map(step, 0, 100, maxDown, 0);

    if (COMMON_ANODE) {
      analogWrite(pinUp, 255 - valUp);
      analogWrite(pinDown, 255 - valDown);
    } else {
      analogWrite(pinUp, valUp);
      analogWrite(pinDown, valDown);
    }
    delay(15); // Adjust delay to change overall fading speed
  }
}

