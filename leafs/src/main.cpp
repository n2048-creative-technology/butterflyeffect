#include <TFT_eSPI.h>

// -------- CONFIGURATION --------
#define GLITCH_PERCENT 15    // Percentage of bits to flip (0-100)
#define FRAME_DELAY_MS 50    // Delay between frames (ms)
#define TEXT_TO_DISPLAY "GLITCH MODE"  // Change this to modify displayed text
#define TEXT_X 20
#define TEXT_Y 80

// -------- GLOBALS --------
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite sprite = TFT_eSprite(&tft);

// -------- HELPERS --------
// Flip a random bit in a 16-bit color (RGB565)
uint16_t glitchPixel(uint16_t color) {
  uint16_t mask = 1 << (random(16)); // Random bit position (0-15)
  return color ^ mask;              // Flip the bit
}

// -------- SETUP --------
void setup() {
  Serial.begin(115200);
  randomSeed(analogRead(0)); // Better randomness

  // Initialize TFT
  tft.begin();
  tft.setRotation(2); // Vertical orientation, cables at bottom
  tft.fillScreen(TFT_BLACK);

  // Create sprite (off-screen buffer)
  sprite.createSprite(TFT_WIDTH, TFT_HEIGHT);
  sprite.setTextColor(TFT_WHITE, TFT_BLACK);
  sprite.setTextSize(1);

  // Initial message
  sprite.fillSprite(TFT_BLACK);
  sprite.setCursor(10, 20);
  sprite.println("ESP32-S3 + ILI9163V");
  sprite.println("Glitch Demo");
  sprite.pushSprite(0, 0);
  delay(2000);
}

// -------- MAIN LOOP --------
void loop() {
  static String displayText = TEXT_TO_DISPLAY;

  // Check for serial input to change text
  if (Serial.available() > 0) {
    displayText = Serial.readStringUntil('\n');
    displayText.trim();
    if (displayText.length() == 0) {
      displayText = TEXT_TO_DISPLAY; // Reset to default
    }
  }

  // 1. Fill sprite with background
  sprite.fillSprite(TFT_BLACK);
  for (int y = 0; y < TFT_HEIGHT; y++) {
    for (int x = 0; x < TFT_WIDTH; x++) {
      uint8_t r = (x * 255) / TFT_WIDTH;
      uint8_t g = (y * 255) / TFT_HEIGHT;
      uint8_t b = 128;
      sprite.drawPixel(x, y, tft.color565(r, g, b));
    }
  }

  // 2. Write configurable text to sprite
  sprite.setCursor(TEXT_X, TEXT_Y);
  sprite.setTextColor(TFT_WHITE);
  sprite.println(displayText);

  // 3. Apply glitch: flip random bits in the sprite buffer
  uint16_t *spriteBuffer = (uint16_t *)sprite.getPointer();
  int totalPixels = TFT_WIDTH * TFT_HEIGHT;
  int pixelsToGlitch = (totalPixels * GLITCH_PERCENT) / 100;

  for (int i = 0; i < pixelsToGlitch; i++) {
    int idx = random(totalPixels);
    spriteBuffer[idx] = glitchPixel(spriteBuffer[idx]);
  }

  // 4. Push sprite to display
  sprite.pushSprite(0, 0);

  // 5. Small delay to control glitch speed
  delay(FRAME_DELAY_MS);
}
