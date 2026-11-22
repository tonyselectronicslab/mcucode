// --- PIN DEFINITIONS ---
const int ledPins[] = {5, 6, 9, 10, 3, 11}; // 6 hardware PWM pins on ATmega328PB unaffected by tone()
const int numLeds = 6;
const int buzzerPin = 8;
const int buttonPin = 2; // Optional push button to GND to skip songs

// --- ACCURATE NOTE FREQUENCIES (Hz) ---
#define REST 0
#define NOTE_A3  220
#define NOTE_B3  247
#define NOTE_C4  262
#define NOTE_D4  294
#define NOTE_E4  330
#define NOTE_F4  349
#define NOTE_FS4 370
#define NOTE_G4  392
#define NOTE_A4  440
#define NOTE_B4  494
#define NOTE_C5  523
#define NOTE_D5  587
#define NOTE_E5  659

// --- SONG 1: JINGLE BELLS (Key of C Major) ---
const int jingleNotes[] = {
  NOTE_E4, NOTE_E4, NOTE_E4,
  NOTE_E4, NOTE_E4, NOTE_E4,
  NOTE_E4, NOTE_G4, NOTE_C4, NOTE_D4, NOTE_E4,
  NOTE_F4, NOTE_F4, NOTE_F4, NOTE_F4,
  NOTE_F4, NOTE_E4, NOTE_E4, NOTE_E4, NOTE_E4,
  NOTE_E4, NOTE_D4, NOTE_D4, NOTE_E4, NOTE_D4, NOTE_G4,

  NOTE_E4, NOTE_E4, NOTE_E4,
  NOTE_E4, NOTE_E4, NOTE_E4,
  NOTE_E4, NOTE_G4, NOTE_C4, NOTE_D4, NOTE_E4,
  NOTE_F4, NOTE_F4, NOTE_F4, NOTE_F4,
  NOTE_F4, NOTE_E4, NOTE_E4, NOTE_E4, NOTE_E4,
  NOTE_G4, NOTE_G4, NOTE_F4, NOTE_D4, NOTE_C4
};
// Duration in milliseconds at 140 BPM (Quarter note = 428ms)
const int jingleDurations[] = {
  400, 400, 800,
  400, 400, 800,
  400, 400, 600, 200, 1400,
  400, 400, 600, 200,
  400, 400, 400, 200, 200,
  400, 400, 400, 400, 800, 800,

  400, 400, 800,
  400, 400, 800,
  400, 400, 600, 200, 1400,
  400, 400, 600, 200,
  400, 400, 400, 200, 200,
  400, 400, 400, 400, 1400
};

// --- SONG 2: WE WISH YOU A MERRY CHRISTMAS (Key of G Major) ---
const int wishNotes[] = {
  NOTE_D4,
  NOTE_G4, NOTE_G4, NOTE_A4, NOTE_G4, NOTE_FS4,
  NOTE_E4, NOTE_E4, NOTE_E4,
  NOTE_A4, NOTE_A4, NOTE_B4, NOTE_A4, NOTE_G4,
  NOTE_FS4, NOTE_D4, NOTE_D4,
  NOTE_B4, NOTE_B4, NOTE_C5, NOTE_B4, NOTE_A4,
  NOTE_G4, NOTE_E4, NOTE_D4, NOTE_D4,
  NOTE_E4, NOTE_A4, NOTE_FS4,
  NOTE_G4
};
const int wishDurations[] = {
  400,
  400, 200, 200, 200, 200,
  400, 400, 400,
  400, 200, 200, 200, 200,
  400, 400, 400,
  400, 200, 200, 200, 200,
  400, 400, 200, 200,
  400, 400, 400,
  1200
};

// --- SONG 3: FELIZ NAVIDAD (Key of G Major) ---
const int felizNotes[] = {
  // "Fe-liz Na-vi-dad"
  NOTE_D4, NOTE_G4, NOTE_FS4, NOTE_G4, NOTE_E4, REST,
  // "Fe-liz Na-vi-dad"
  NOTE_E4, NOTE_A4, NOTE_G4, NOTE_FS4, NOTE_G4, NOTE_E4, REST,
  // "Fe-liz Na-vi-dad, pros-pe-ro a-no y fe-li-ci-dad"
  NOTE_D4, NOTE_G4, NOTE_FS4, NOTE_G4, NOTE_E4, REST,
  NOTE_E4, NOTE_FS4, NOTE_E4, NOTE_D4, NOTE_C4, NOTE_B3, NOTE_A3, NOTE_G4, REST,
  // "I wanna wish you a Merry Christmas"
  NOTE_B4, NOTE_B4, NOTE_B4, NOTE_B4, NOTE_A4, NOTE_G4, NOTE_A4, NOTE_G4, REST,
  // "I wanna wish you a Merry Christmas"
  NOTE_A4, NOTE_A4, NOTE_A4, NOTE_A4, NOTE_G4, NOTE_FS4, NOTE_G4, NOTE_FS4, REST,
  // "I wanna wish you a Merry Christmas from the bottom of my heart"
  NOTE_G4, NOTE_G4, NOTE_G4, NOTE_G4, NOTE_FS4, NOTE_E4, NOTE_D4, NOTE_B4, NOTE_B4, NOTE_A4, NOTE_G4
};
const int felizDurations[] = {
  220, 440, 220, 220, 660, 220,
  220, 440, 220, 220, 220, 660, 220,
  220, 440, 220, 220, 660, 220,
  220, 220, 220, 220, 220, 220, 220, 660, 440,
  220, 220, 220, 220, 220, 220, 220, 660, 220,
  220, 220, 220, 220, 220, 220, 220, 660, 220,
  220, 220, 220, 220, 220, 220, 440, 220, 220, 220, 1000
};

// --- STATE VARIABLES ---
int brightness[numLeds];
int fadeAmount[numLeds];
unsigned long prevLedMillis[numLeds];
int ledInterval[numLeds];

int currentSong = 0;
int noteIndex = 0;
unsigned long prevNoteMillis = 0;
unsigned long stepDuration = 0;
bool isNotePlaying = false;
bool lastButtonState = HIGH;

void setup() {
  randomSeed(analogRead(A0));
  pinMode(buzzerPin, OUTPUT);
  pinMode(buttonPin, INPUT_PULLUP);

  for (int i = 0; i < numLeds; i++) {
    pinMode(ledPins[i], OUTPUT);
    brightness[i] = random(0, 255);
    fadeAmount[i] = random(3, 8);
    ledInterval[i] = random(15, 45);
    prevLedMillis[i] = 0;
  }
}

void loop() {
  unsigned long currentMillis = millis();
  updateTwinkle(currentMillis);
  updateMusic(currentMillis);
  checkSkipButton();
}

void updateTwinkle(unsigned long currentMillis) {
  for (int i = 0; i < numLeds; i++) {
    if (currentMillis - prevLedMillis[i] >= ledInterval[i]) {
      prevLedMillis[i] = currentMillis;
      brightness[i] += fadeAmount[i];
      if (brightness[i] <= 0) {
        brightness[i] = 0;
        fadeAmount[i] = random(3, 8);
      } else if (brightness[i] >= 255) {
        brightness[i] = 255;
        fadeAmount[i] = -random(3, 8);
      }
      // Only use hardware analogWrite on pins NOT tied to Timer2 (5, 6, 9, 10)
      if (ledPins[i] != 3 && ledPins[i] != 11) {
        analogWrite(ledPins[i], brightness[i]);
      }
    }

    // Fast software PWM for Pins 3 and 11 so Timer2 stays untouched for music
    if (ledPins[i] == 3 || ledPins[i] == 11) {
      unsigned int pwmCycle = micros() & 0xFF; // 0-255 fast cycle (~3.9 kHz)
      digitalWrite(ledPins[i], (pwmCycle < brightness[i]) ? HIGH : LOW);
    }
  }
}


void updateMusic(unsigned long currentMillis) {
  if (currentMillis - prevNoteMillis < stepDuration) return;
  prevNoteMillis = currentMillis;

  const int* notes;
  const int* durations;
  int length;

  if (currentSong == 0) {
    notes = jingleNotes; durations = jingleDurations;
    length = sizeof(jingleNotes) / sizeof(int);
  } else if (currentSong == 1) {
    notes = wishNotes; durations = wishDurations;
    length = sizeof(wishNotes) / sizeof(int);
  } else {
    notes = felizNotes; durations = felizDurations;
    length = sizeof(felizNotes) / sizeof(int);
  }

  // Add a brief silent gap between repeated notes so each syllable is distinct
  if (isNotePlaying) {
    noTone(buzzerPin);
    isNotePlaying = false;
    stepDuration = 45; // 45ms articulation gap
    return;
  }

  if (noteIndex < length) {
    int note = notes[noteIndex];
    int dur = durations[noteIndex];

    if (note == REST) {
      noTone(buzzerPin);
      stepDuration = dur;
      isNotePlaying = false;
    } else {
      tone(buzzerPin, note); // Continuous clean tone until gap
      stepDuration = dur - 45;
      isNotePlaying = true;
    }
    noteIndex++;
  } else {
    noTone(buzzerPin);
    isNotePlaying = false;
    noteIndex = 0;
    currentSong = (currentSong + 1) % 3;
    stepDuration = 1500;
  }
}

void checkSkipButton() {
  bool buttonState = digitalRead(buttonPin);
  if (lastButtonState == HIGH && buttonState == LOW) {
    noTone(buzzerPin);
    isNotePlaying = false;
    noteIndex = 0;
    currentSong = (currentSong + 1) % 3;
    stepDuration = 300;
    delay(50);
  }
  lastButtonState = buttonState;
}
