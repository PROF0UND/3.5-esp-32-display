#include <TFT_eSPI.h>
#include <SPI.h>

TFT_eSPI tft = TFT_eSPI(); // Create display object

// Screen size
const int SCREEN_W = 480;
const int SCREEN_H = 320;

// Ball properties
int x = 160, y = 240;
int dx = 3, dy = 4;
int radius = 20;
uint16_t ballColor = TFT_RED;

void setup() {
  tft.init();
  tft.setRotation(1);  // Landscape
  tft.fillScreen(TFT_BLACK);
}

void loop() {
  // Erase old ball
  tft.fillCircle(x, y, radius, TFT_BLACK);

  // Move ball
  x += dx;
  y += dy;

  // Bounce off edges
  if (x - radius <= 0 || x + radius >= SCREEN_W) dx = -dx;
  if (y - radius <= 0 || y + radius >= SCREEN_H) dy = -dy;

  // Draw new ball
  tft.fillCircle(x, y, radius, ballColor);

  delay(10);  // Slow down a bit
}
