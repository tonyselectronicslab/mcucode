#include <TVout.h>

TVout TV;

// Screen Dimensions
const int screenWidth = 128; // 120 for PAL , 128 for NTSC
const int screenHeight = 96;

// Ball Properties
float ballX, ballY;
float ballSpeedX, ballSpeedY;
const int ballSize = 2;

// Paddle Properties
const int paddleWidth = 3;
const int paddleHeight = 16;
int paddleLeftY, paddleRightY;

// Game Scores
int scoreLeft = 0;
int scoreRight = 0;

// --- AI CONFIGURATION ---
// Increase this window to make the AI miss more often (Default: 4 pixels)
const int aiDeadzone = 7; 
// The speed at which the paddles can slide up or down
const int aiMoveSpeed = 1; 

// Custom Bitmaps for Pixel Numbers (3x5 grid)
const byte numbers[10][5] = {
  {0b111, 0b101, 0b101, 0b101, 0b111}, // 0
  {0b010, 0b110, 0b010, 0b010, 0b111}, // 1
  {0b111, 0b001, 0b111, 0b100, 0b111}, // 2
  {0b111, 0b001, 0b111, 0b001, 0b111}, // 3
  {0b101, 0b101, 0b111, 0b001, 0b001}, // 4
  {0b111, 0b100, 0b111, 0b001, 0b111}, // 5
  {0b111, 0b100, 0b111, 0b101, 0b111}, // 6
  {0b111, 0b001, 0b001, 0b010, 0b010}, // 7
  {0b111, 0b101, 0b111, 0b101, 0b111}, // 8
  {0b111, 0b101, 0b111, 0b001, 0b111}  // 9
};

void drawPixelNumber(int x, int y, int num) {
  num = num % 10; 
  for (int row = 0; row < 5; row++) {
    for (int col = 0; col < 3; col++) {
      if ((numbers[num][row] >> (2 - col)) & 1) {
        TV.draw_rect(x + (col * 2), y + (row * 2), 1, 1, WHITE, WHITE);
      }
    }
  }
}

void resetBall() {
  ballX = screenWidth / 2;
  ballY = screenHeight / 2;
  
  // Give the ball a slightly faster, less predictable bounce speed
  ballSpeedX = (random(0, 2) == 0) ? 1.6 : -1.6;
  ballSpeedY = random(-90, 90) / 100.0;
}

void setup() {
  TV.begin(NTSC, screenWidth, screenHeight);
  paddleLeftY = (screenHeight - paddleHeight) / 2;
  paddleRightY = (screenHeight - paddleHeight) / 2;
  randomSeed(analogRead(A0));
  resetBall();
}

void loop() {
  // --- IMPERFECT AI LOGIC ---
  int paddleCenterLeft = paddleLeftY + (paddleHeight / 2);
  int paddleCenterRight = paddleRightY + (paddleHeight / 2);

  // Left Paddle AI: Only tracks if the ball is moving towards it
  if (ballSpeedX < 0) {
    if (ballY < paddleCenterLeft - aiDeadzone && paddleLeftY > 4) {
      paddleLeftY -= aiMoveSpeed;
    } else if (ballY > paddleCenterLeft + aiDeadzone && paddleLeftY < (screenHeight - paddleHeight - 4)) {
      paddleLeftY += aiMoveSpeed;
    }
  }

  // Right Paddle AI: Only tracks if the ball is moving towards it
  if (ballSpeedX > 0) {
    if (ballY < paddleCenterRight - aiDeadzone && paddleRightY > 4) {
      paddleRightY -= aiMoveSpeed;
    } else if (ballY > paddleCenterRight + aiDeadzone && paddleRightY < (screenHeight - paddleHeight - 4)) {
      paddleRightY += aiMoveSpeed;
    }
  }

  // --- BALL PHYSICS ---
  ballX += ballSpeedX;
  ballY += ballSpeedY;

  // Ceiling and Floor collision safety constraints
  if (ballY <= 2) {
    ballY = 2;
    ballSpeedY = -ballSpeedY;
    TV.tone(220, 30);
  } else if (ballY >= (screenHeight - 2 - ballSize)) {
    ballY = screenHeight - 2 - ballSize;
    ballSpeedY = -ballSpeedY;
    TV.tone(220, 30);
  }

  // Left Paddle collision check
  if (ballX <= (paddleWidth + 2)) {
    if (ballY + ballSize >= paddleLeftY && ballY <= paddleLeftY + paddleHeight) {
      ballSpeedX = -ballSpeedX;
      ballX = paddleWidth + 3; // Force clear
      
      float hitPos = (ballY - (paddleLeftY + paddleHeight / 2.0)) / (paddleHeight / 2.0);
      ballSpeedY = hitPos * 1.4; // Slightly sharper angles to challenge the opposing AI
      TV.tone(440, 40);
    } else if (ballX < 0) {
      scoreRight++;
      TV.tone(150, 200);
      resetBall();
      TV.delay_frame(15); 
    }
  }

  // Right Paddle collision check
  if (ballX >= (screenWidth - paddleWidth - 2 - ballSize)) {
    if (ballY + ballSize >= paddleRightY && ballY <= paddleRightY + paddleHeight) {
      ballSpeedX = -ballSpeedX;
      ballX = screenWidth - paddleWidth - 4 - ballSize; // Force clear
      
      float hitPos = (ballY - (paddleRightY + paddleHeight / 2.0)) / (paddleHeight / 2.0);
      ballSpeedY = hitPos * 1.4; // Slightly sharper angles to challenge the opposing AI
      TV.tone(440, 40);
    } else if (ballX > screenWidth) {
      scoreLeft++;
      TV.tone(150, 200);
      resetBall();
      TV.delay_frame(15); 
    }
  }

  // --- DRAWING PIPELINE ---
  TV.clear_screen();

  // Draw Center Netting line
  for (int i = 0; i < screenHeight; i += 6) {
    TV.draw_line(screenWidth / 2, i, screenWidth / 2, i + 3, WHITE);
  }

  // Draw Scoreboard
  drawPixelNumber(screenWidth / 2 - 20, 6, scoreLeft);
  drawPixelNumber(screenWidth / 2 + 12, 6, scoreRight);

  // Draw Paddles
  TV.draw_rect(2, paddleLeftY, paddleWidth, paddleHeight, WHITE, WHITE);
  TV.draw_rect(screenWidth - 2 - paddleWidth, paddleRightY, paddleWidth, paddleHeight, WHITE, WHITE);

  // Draw Ball
  TV.draw_rect((int)ballX, (int)ballY, ballSize, ballSize, WHITE, WHITE);

  TV.delay_frame(1);
}

