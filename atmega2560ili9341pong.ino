#include "SPI.h"
#include "Adafruit_GFX.h"
#include "Adafruit_ILI9341.h"

// TFT Display Pins (Arduino Mega Hardware SPI)
#define TFT_CS   53
#define TFT_DC   48
#define TFT_RST  49
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);

// Optional Control Input
#define PADDLE_POT_PIN A0

// Screen Dimensions (Landscape Mode)
const int SCREEN_WIDTH = 320;
const int SCREEN_HEIGHT = 240;

// Game Object Parameters
const int PADDLE_WIDTH = 8;
const int PADDLE_HEIGHT = 40;
const int BALL_SIZE = 6;

// Positions (X, Y)
int playerY = 100;
int playerOldY = 100;
int aiY = 100;
int aiOldY = 100;

float ballX = 160;
float ballY = 120;
float ballOldX = 160;
float ballOldY = 120;

// Speeds
float ballDX = 3.5;
float ballDY = 2.5;
const int AI_SPEED = 3;

// Scores
int playerScore = 0;
int aiScore = 0;

void setup() {
  tft.begin();
  tft.setRotation(1); // Landscape
  tft.fillScreen(ILI9341_BLACK);
  
  // Draw initial game UI center line
  drawCenterLine();
  updateScoreboard();
}

void loop() {
  // 1. Read Inputs & Move Player Paddle
  playerOldY = playerY;
  // If no potentiometer is connected, this defaults to automated demo tracking
  int potValue = analogRead(PADDLE_POT_PIN); 
  playerY = map(potValue, 0, 1023, 0, SCREEN_HEIGHT - PADDLE_HEIGHT);
  
  // 2. Simple AI Paddle Movement (Tracks the ball)
  aiOldY = aiY;
  if (ballY < aiY + (PADDLE_HEIGHT / 2) - 4) {
    aiY -= AI_SPEED;
  } else if (ballY > aiY + (PADDLE_HEIGHT / 2) + 4) {
    aiY += AI_SPEED;
  }
  aiY = constrain(aiY, 0, SCREEN_HEIGHT - PADDLE_HEIGHT);

  // 3. Move Ball
  ballOldX = ballX;
  ballOldY = ballY;
  ballX += ballDX;
  ballY += ballDY;

  // 4. Ball Collisions (Top & Bottom Walls)
  if (ballY <= 0 || ballY >= SCREEN_HEIGHT - BALL_SIZE) {
    ballDY = -ballDY;
  }

  // 5. Ball Collision (Player Paddle - Left Side)
  if (ballX <= PADDLE_WIDTH + 5) {
    if (ballY + BALL_SIZE >= playerY && ballY <= playerY + PADDLE_HEIGHT) {
      ballDX = -ballDX;
      ballX = PADDLE_WIDTH + 6; // Prevent trapping inside paddle
      // Slightly increase speed / change angle based on hit location
      ballDY = ((ballY - (playerY + PADDLE_HEIGHT / 2)) / 5.0);
    }
  }

  // 6. Ball Collision (AI Paddle - Right Side)
  if (ballX >= SCREEN_WIDTH - PADDLE_WIDTH - BALL_SIZE - 5) {
    if (ballY + BALL_SIZE >= aiY && ballY <= aiY + PADDLE_HEIGHT) {
      ballDX = -ballDX;
      ballX = SCREEN_WIDTH - PADDLE_WIDTH - BALL_SIZE - 6; 
      ballDY = ((ballY - (aiY + PADDLE_HEIGHT / 2)) / 5.0);
    }
  }

  // 7. Scoring System
  if (ballX < 0) {
    aiScore++;
    resetBall();
  } else if (ballX > SCREEN_WIDTH) {
    playerScore++;
    resetBall();
  }

  // 8. Incremental Screen Rendering (Prevents Screen Flickering)
  renderGame();
  
  delay(16); // ~60 Frames Per Second
}

void resetBall() {
  // Clear old ball location entirely
  tft.fillRect(ballOldX, ballOldY, BALL_SIZE, BALL_SIZE, ILI9341_BLACK);
  
  ballX = SCREEN_WIDTH / 2;
  ballY = SCREEN_HEIGHT / 2;
  ballDX = (ballDX > 0) ? -3.5 : 3.5; // Serve to the winner
  ballDY = 2.0;
  
  updateScoreboard();
  drawCenterLine();
}

void drawCenterLine() {
  for (int i = 0; i < SCREEN_HEIGHT; i += 12) {
    tft.drawFastVLine(SCREEN_WIDTH / 2, i, 6, ILI9341_DARKGREY);
  }
}

void updateScoreboard() {
  tft.setTextSize(2);
  tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
  
  tft.setCursor(SCREEN_WIDTH / 4, 10);
  tft.print(playerScore);
  
  tft.setCursor((SCREEN_WIDTH / 4) * 3, 10);
  tft.print(aiScore);
}

void renderGame() {
  // Erase trailing paddle segments instead of fillRect to eliminate screen lag
  if (playerY != playerOldY) {
    tft.fillRect(5, playerOldY, PADDLE_WIDTH, PADDLE_HEIGHT, ILI9341_BLACK);
    tft.fillRect(5, playerY, PADDLE_WIDTH, PADDLE_HEIGHT, ILI9341_BLUE);
  } else {
    tft.fillRect(5, playerY, PADDLE_WIDTH, PADDLE_HEIGHT, ILI9341_BLUE);
  }

  if (aiY != aiOldY) {
    tft.fillRect(SCREEN_WIDTH - PADDLE_WIDTH - 5, aiOldY, PADDLE_WIDTH, PADDLE_HEIGHT, ILI9341_BLACK);
    tft.fillRect(SCREEN_WIDTH - PADDLE_WIDTH - 5, aiY, PADDLE_WIDTH, PADDLE_HEIGHT, ILI9341_RED);
  } else {
    tft.fillRect(SCREEN_WIDTH - PADDLE_WIDTH - 5, aiY, PADDLE_WIDTH, PADDLE_HEIGHT, ILI9341_RED);
  }

  // Erase old ball frame and draw new one
  if ((int)ballX != (int)ballOldX || (int)ballY != (int)ballOldY) {
    tft.fillRect(ballOldX, ballOldY, BALL_SIZE, BALL_SIZE, ILI9341_BLACK);
    // Redraw center line fragments if ball crossed them
    if (ballOldX >= (SCREEN_WIDTH / 2) - BALL_SIZE && ballOldX <= (SCREEN_WIDTH / 2) + 1) {
      drawCenterLine();
    }
  }
  tft.fillRect(ballX, ballY, BALL_SIZE, BALL_SIZE, ILI9341_GREEN);
}
