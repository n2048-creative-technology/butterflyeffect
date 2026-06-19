#include <TFT_eSPI.h>

String str1 = "The butterfly effect is a concept from chaos theory where small changes can lead to vastly different outcomes. A butterfly flapping its wings in Brazil could theoretically cause a tornado in Texas.";
String str2 = "This illustrates how tiny initial conditions can create massive differences in complex systems. It shows the interconnectedness of all things and the unpredictability of long-term effects.";


TFT_eSPI tft = TFT_eSPI();

void setup() {
  Serial.begin(115200);
  Serial.println("Starting TFT test...");

  tft.begin();
  tft.setRotation(1);

  // Fill screen with different colors to verify
  tft.fillScreen(TFT_BLACK);

  // Draw simple text
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(3, 2);
  tft.println("");
  tft.println(" Hello");
  tft.println("  Butterfly");
  tft.println("");
  tft.println("    Effect");
  tft.println("");
  tft.setTextSize(1);
  tft.println("     it works!");
  tft.println(" 2");

}

uint8_t intensity = 0;
unsigned long lastIntensityChange = 0;
unsigned long lastStringChange = 0;
bool showStr1 = true;


// With adjustable space probability (default 25%)
String randomSymbols(uint16_t length, uint8_t spacePercent = 0) {
  String result = "";
  result.reserve(length);
  for (uint16_t i = 0; i < length; i++) {
    if (random(100) < spacePercent) {
      result += ' ';
    } else {
      result += (char)random(33, 127);
    }
  }
  return result;
}

// Single-line conversion - fastest!
uint16_t grayToColor1(uint8_t gray) {
  return (gray >> 3) * 0x421;
}

// Convert 0-255 grayscale to RGB565
uint16_t grayToColor2(uint8_t gray) {
  uint8_t r = gray >> 3;
  uint8_t g = gray >> 2;
  uint8_t b = gray >> 3;
  return (r << 11) | (g << 5) | b;
}

void applyGlitchEffect(uint8_t intensity) {
  if(intensity == 0) {
//    tft.fillScreen(TFT_BLACK);
    return;
  }

  tft.setTextSize(1);
  for(uint8_t i = 0; i <10; i++) {
    tft.setCursor(0,0);  
    String randomText = randomSymbols(random(10,400),90);
    tft.setTextColor(grayToColor2(random(intensity/2, intensity)));
    tft.println(randomText);
  }

    tft.setTextSize(2);
  for(uint8_t i = 0; i <10; i++) {
    tft.setCursor(0,0);  
    String randomText = randomSymbols(random(10,200),90);
    tft.setTextColor(grayToColor2(random(intensity/2, intensity)));
    tft.println(randomText);
  }

    tft.setTextSize(1);
  for(uint8_t i = 0; i <2; i++) {
    tft.setCursor(0,0);  
    String randomText = randomSymbols(random(10,400),99);
    tft.setTextColor(grayToColor1(random(intensity/4, intensity/2)));
    tft.println(randomText);
  }

}


void loop() {
  // Update intensity every 100ms
  unsigned long currentTime = millis();
  if (currentTime - lastIntensityChange >= 100) {
    lastIntensityChange = currentTime;
    intensity += 2;
    if (intensity > 255) {
      intensity = 0;
    }
  }

  // Switch between str1 and str2 every 10 seconds
  if (currentTime - lastStringChange >= 10000) {
    lastStringChange = currentTime;
    showStr1 = !showStr1;
  }

  // tft.fillScreen(TFT_BLACK);
//  applyGlitchEffect(intensity);
  applyGlitchEffect(10);

  // Draw info
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(1);
  tft.setCursor(3, 2);
  tft.print("Intensity: ");
  tft.println(intensity);

  // Draw the current butterfly effect paragraph
  tft.setTextSize(1);
  for(uint8_t i = 0; i < 10; i++) {
    tft.setCursor(3, 20);
    if (showStr1) {
      tft.println(str1);
    } else {
      tft.println(str2);
    }
  }


  delay(16);
}