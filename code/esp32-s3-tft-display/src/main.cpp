#include <Arduino.h>
#include <TFT_eSPI.h>
 
#include "RD03D.h"

// Radar on the UART0 pins (module pin 36 = RXD0/GPIO44, pin 37 = TXD0/GPIO43).
// Driven by UART1 through the GPIO matrix; UART0 itself is never started, so
// there is no contention. Keeps GPIO19/20 free for the native USB.
#define RX_PIN 44   // ESP RX  <- sensor TX
#define TX_PIN 43   // ESP TX  -> sensor RX
#define BAUD_RATE 256000

// Breathing LED
#define LED_PIN 39
#define LED_PWM_FREQ 5000     // Hz, well above visible flicker
#define LED_PWM_RES 12        // bits -> duty 0..4095
#define LED_MAX_DUTY ((1 << LED_PWM_RES) - 1)
#define LED_HALF_BREATH 5000  // ms  cycle = 0.1 Hz
#define LED_ACTIVE_LOW 0      // set to 1 if the LED is wired to 3V3 (cathode on GPIO)

// ---------------- Render tuning ----------------------------------------------
#define TEXT_PASSES   1      // paragraph redraws per frame (was 20, see loop())
#define FRAME_DELAY   1      // ms yielded at the end of each frame (was 16)

// Draw the frame into a RAM framebuffer and push it in one burst.
// Why: with a transparent text background TFT_eSPI takes its slow glyph path,
// one drawPixel -> one setWindow -> one bus transaction per lit pixel. In a
// sprite each of those becomes a memory store, and the panel sees a single
// streamed write per frame. Costs 160*128*2 = 40 kB of heap.
// Set to 0 to go back to drawing straight to the panel.
#define USE_SPRITE    1

// How opaque the glitch layer is, in percent. The panel has no alpha channel,
// so "more transparent" means drawing the noise at a lower grey level: at 50
// the glitch is half as bright, letting the white text read through it.
#define GLITCH_OPACITY 50

// Colour of the glitch layer, sampled from bare PCB copper (#B87333).
// The noise is drawn as shades of this hue instead of neutral grey, so it
// reads as the copper of the board rather than plain white static.
// For a gold/ENIG finish try 212 / 175 / 110; for a redder copper 176 / 101 / 42.
// Distance -> presence. The radar reports millimetres. Beyond DIST_FAR_MM the
// glitch is at full strength and the text is at its dimmest; at DIST_NEAR_MM
// the glitch is gone and the text is at TEXT_OPACITY. The old range stopped at
// 1000 mm, so everything past a metre looked identical -- the sensor reads well
// past 2 m, so the gradient now spans the room.
#define DIST_NEAR_MM   100   // 10 cm: as close as a viewer realistically gets
#define DIST_FAR_MM   1000   // 1 m: beyond this it is pure glitch

// Text layer brightness, in percent of white. TEXT_OPACITY is the maximum,
// reached when the viewer is closest; it falls to TEXT_MIN_OPACITY at a
// distance so the copper glitch reads as more present than the words.
#define TEXT_OPACITY      80
#define TEXT_MIN_OPACITY  20

#define COPPER_R 184
#define COPPER_G 115
#define COPPER_B  51

// Optional trail fade. OFF by default: the ghost strokes turned out to be a
// glyph-advance bug (now fixed) plus the missing clear on paragraph change, so
// the decay is not needed for readability and leaving it off preserves the
// exact copper look.
// LEAVE THIS OFF. Fading an RGB565 buffer per channel cannot preserve hue: red
// and blue have 5 bits, green has 6, so a copper pixel (r=23 g=28 b=6) loses its
// blue after five steps while green still has levels left, and every pixel ends
// its fade pure green. Scaling all channels by the same ratio does not help --
// the damage is quantisation, not the coefficient. The morph pile-up it was
// meant to solve is handled by clearing during the transition instead, which
// never touches an existing pixel's colour.
//   TRAIL_DECAY_ON 0 = pixels persist until overdrawn (original behaviour)
//   TRAIL_DECAY_ON 1 = fade by NUM/DEN per frame, hue preserved
#define TRAIL_DECAY_ON  0
#define TRAIL_DECAY_NUM 3     // keep 3/4 of every channel each frame
#define TRAIL_DECAY_DEN 4

// Arduino's random() defaults to the ESP32 *hardware* RNG (esp_random()),
// which busy-waits for fresh entropy on every single call. The glitch effect
// makes roughly 4000 calls per frame, so that wait dominates the frame time.
// Calling randomSeed() once switches random() to the software PRNG (rand()) --
// see WMath.cpp -- which is vastly cheaper and just as good for a visual
// effect. Seeded from the hardware RNG so each power-up still differs.
// Set to 0 to go back to the hardware RNG.
#define FAST_RANDOM   1

// ---------------- Debug telemetry over the native USB (USB-Serial-JTAG) ------
// platformio.ini sets ARDUINO_USB_CDC_ON_BOOT=1, so Serial is the USB CDC on
// GPIO19/20 (connector J4). UART0's pins now belong to the radar, and UART0 is
// never started, so the two never collide.
// Set DBG_ON to 0 to silence all of this for the final installation.
#define DBG_ON        1
#define DBG_BAUD      115200
#define DBG_STATUS_MS 1000   // period of the one-line status report
#define DBG_RADAR_MS  500    // throttle for per-target detail lines

// Serial is strictly optional: every message is skipped unless a USB host is
// actually attached, so no string formatting is done and nothing is written
// when the piece runs standalone. `if (Serial)` is HWCDC::isCDC_Connected(),
// which is true as soon as a host is plugged in -- a terminal does NOT have to
// be open -- and false when the cable is out. Writes are non-blocking anyway
// (setTxTimeoutMs(0), and HWCDC drops data when disconnected), so the same
// binary runs identically with or without a cable. No rebuild needed either way.
#if DBG_ON
  #define DBG(fmt, ...) \
    do { if (Serial) Serial.printf("[%8lu] " fmt "\r\n", millis(), ##__VA_ARGS__); } while (0)
#else
  #define DBG(fmt, ...) do {} while (0)
#endif

RD03D radar(RX_PIN, TX_PIN); // ESP RX/TX pins connected to sensor TX/RX

RD03D::RD03DMode radarMode = RD03D::SINGLE_TARGET;  // Change to RD03D::MULTI_TARGET for multi-target mode


// ---------------- The text ---------------------------------------------------
// One paragraph is shown at a time. '\n' marks the poet's own line breaks;
// lines wider than the screen are word-wrapped, never split mid-word.
// Note: the typographic apostrophe in "I've" was replaced with an ASCII ' --
// the GLCD font is 8-bit and cannot render U+2019, it would come out blank.
const char* const paragraphs[] = {
  "I've been missing\nthis time together\nthe smell of\nthe flowers,\nthe fresh air",
  "Today,\nI feel on the edge\nof something\nI cannot name.\nnot life\nnot death\nnot dream",
  "a hallway that\nextends\nendlessly\nright before me\nand yet it ends",
  "where are\nmy roots growing?\nwhere are they\nleading me?\nTo me, to you",
  "the silence around me\nseemed alive\nrestless\nwaiting",
  "as if the walls\nwere listening\nfor a word\nI could not say.",
  "So spoke the land\nof oblivion,\nstretching outward,\nweary of existing\nwithin the intimate,\nsensual\nnature of things.",
  "A pact forged\nbetween life and death,\nin the hope\nthat the end of one\nmight become\nthe beginning of another.",
  "How easy it is for humankind\nstill to say:\n*In dreams,\nwe shall find one another again.*",
  "And so you endure,\nwith an exhausted ferocity,\nskin against skin,\nbody against body,\nfilling your lungs\nwith generous air.",
  "In the uncertainty\nof what is to come,\na song unfolds\na lullaby\ncarrying the primordial\nmemory of being.",
  "She beats her wings.\nThe song falls silent,\nhanging\nbetween one heartbeat\nand the next,\nas though time\nhad forgotten\nto move on.",
  "And in that emptiness,\nat last,\nthat lost word\nneeded no more."
};
const uint8_t PARAGRAPH_COUNT = sizeof(paragraphs) / sizeof(paragraphs[0]);

// ---------------- Layout ------------------------------------------------------
#define SCREEN_MARGIN 5     // px of empty border kept on every side
#define CHAR_W        6     // GLCD glyph cell at text size 1
#define CHAR_H        8
#define MAX_LINES     20
#define MAX_LINE_CHARS 24

// Reading pace. A slow reader manages roughly 100 words per minute, and a word
// averages ~5.5 characters plus a space, so ~9 characters per second. The
// glitch overlay makes it harder still, hence the generous floor.
#define READ_CHARS_PER_SEC 9
#define READ_BASE_MS       2500
#define READ_MIN_MS        5000
#define READ_MAX_MS       14000

// Paragraph crossfade: the outgoing paragraph fades out while the incoming one
// fades in, both drawn at once. Smoother than the old character-dissolve, and
// it cannot pile up leftovers because every frame draws the same glyphs.
#define CROSSFADE_MS      450   // paragraph crossfade length

char     lineText[MAX_LINES][MAX_LINE_CHARS + 1];
uint8_t  lineLen[MAX_LINES];
int16_t  lineX[MAX_LINES];
int16_t  lineY[MAX_LINES];
uint8_t  lineCount     = 0;

// Snapshot of the paragraph being replaced. During a crossfade both are drawn:
// the old one fading out, the new one fading in.
char     prevText[MAX_LINES][MAX_LINE_CHARS + 1];
uint8_t  prevLen[MAX_LINES];
int16_t  prevX[MAX_LINES];
int16_t  prevY[MAX_LINES];
uint8_t  prevCount = 0;
uint8_t  prevFont  = 1;
bool     hasPrev   = false;
uint8_t  paragraphIdx  = 0;
unsigned long paragraphStart = 0;
unsigned long paragraphDwell = READ_MIN_MS;


// Neutral grey at the given brightness, packed to RGB565.
static inline uint16_t greyShade(uint8_t v) {
  return ((uint16_t)(v >> 3) << 11) | ((uint16_t)(v >> 2) << 5) | (uint16_t)(v >> 3);
}

// Brightness the text is currently drawn at, recomputed every frame from the
// radar distance. Global so drawParagraph() can pick it up.
uint8_t textBrightness = (uint8_t)(255UL * TEXT_OPACITY / 100);

TFT_eSPI tft = TFT_eSPI();

#if USE_SPRITE
TFT_eSprite sprite = TFT_eSprite(&tft);
#endif
bool spriteOk = false;

// Everything draws through g. TFT_eSprite overrides the virtual drawPixel /
// drawChar / setWindow, so print(), println() and drawChar() all land in the
// sprite buffer when g points at it, and on the panel when it does not.
TFT_eSPI* g = &tft;

// Clear the whole canvas (fillScreen is not virtual, so pick explicitly).
static inline void clearCanvas(uint16_t color) {
#if USE_SPRITE
  if (spriteOk) { sprite.fillSprite(color); return; }
#endif
  tft.fillScreen(color);
}

// Optional trail fade, straight on the sprite's 16-bit buffer.
//
// Hue must be preserved. RGB565 stores red and blue in 5 bits (0..31) and green
// in 6 (0..63), so subtracting a fixed step from each channel fades red and blue
// at twice green's relative rate -- copper trails drifted green as they faded,
// which is what turned the glitch layer into a mess of colours. Scaling every
// channel by the same NUM/DEN keeps the ratio between them, and the multiply
// (rather than a shift-and-subtract) still reaches zero: 1*3/4 = 0.
static void decayCanvas() {
#if USE_SPRITE && TRAIL_DECAY_ON
  if (!spriteOk) return;
  uint16_t* fb = (uint16_t*)sprite.getPointer();
  if (!fb) return;
  const uint32_t n = (uint32_t)sprite.width() * (uint32_t)sprite.height();
  for (uint32_t i = 0; i < n; i++) {
    uint16_t c = fb[i];
    if (!c) continue;
    uint16_t r  = ((c >> 11) & 0x1F) * TRAIL_DECAY_NUM / TRAIL_DECAY_DEN;
    uint16_t gg = ((c >>  5) & 0x3F) * TRAIL_DECAY_NUM / TRAIL_DECAY_DEN;
    uint16_t b  = ( c        & 0x1F) * TRAIL_DECAY_NUM / TRAIL_DECAY_DEN;
    fb[i] = (r << 11) | (gg << 5) | b;
  }
#endif
}

// Send the finished frame to the panel. No-op when drawing directly.
static inline void presentCanvas() {
#if USE_SPRITE
  if (spriteOk) sprite.pushSprite(0, 0);
#endif
}

// Try each font from largest to smallest and keep the first that fits the
// whole paragraph inside the margins. Widths are measured with the real font
// metrics, so proportional fonts (2 and 4) are handled correctly.
const uint8_t FONT_LADDER[] = { 4, 2, 1 };   // 26 px, 16 px, 8 px tall

uint8_t chosenFont  = 1;
int16_t chosenLineH = CHAR_H;

// Pixel advance of one character, matching what TFT_eSPI::write() actually
// does. This is NOT textWidth(): for font 2 the library renders glyphs in whole
// byte widths and advances by ((w + 6) / 8) * 8, while textWidth() returns the
// raw table value. Using textWidth() here made every glyph overlap its
// neighbour and welded vertical strokes onto the text.
static int16_t glyphAdvance(char c, uint8_t font) {
  char s[2] = { c, '\0' };
  int16_t w = g->textWidth(s, font);
  if (font == 2) w = ((w + 6) / 8) * 8;
  return w;
}

// Rendered width of a string, using the same rule as the renderer.
static int16_t measureText(const char* str, uint8_t font) {
  int16_t w = 0;
  for (const char* c = str; *c; c++) w += glyphAdvance(*c, font);
  return w;
}


// Wrap the paragraph for one candidate font. Returns false if any single word
// is wider than the screen or the result is too tall.
static bool wrapForFont(const char* p, uint8_t font, int16_t usableW, int16_t usableH) {
  const int16_t lineH = g->fontHeight(font);
  char cur[MAX_LINE_CHARS + 1];
  char cand[MAX_LINE_CHARS + 1];
  char word[MAX_LINE_CHARS + 1];
  uint8_t wordLen = 0;
  lineCount = 0;
  cur[0] = '\0';

  for (const char* c = p; ; c++) {
    bool end = (*c == '\0');
    if (*c == ' ' || *c == '\n' || end) {
      if (wordLen) {
        word[wordLen] = '\0';
        if (measureText(word, font) > usableW) return false;   // word alone too wide
        if (cur[0]) { snprintf(cand, sizeof(cand), "%s %s", cur, word); }
        else        { snprintf(cand, sizeof(cand), "%s", word); }
        if (cur[0] && measureText(cand, font) > usableW) {
          if (lineCount >= MAX_LINES) return false;
          strncpy(lineText[lineCount], cur, MAX_LINE_CHARS); lineCount++;
          snprintf(cur, sizeof(cur), "%s", word);
        } else {
          snprintf(cur, sizeof(cur), "%s", cand);
        }
        wordLen = 0;
      }
      if (*c == '\n' || end) {
        if (lineCount >= MAX_LINES) return false;
        strncpy(lineText[lineCount], cur, MAX_LINE_CHARS); lineCount++;
        cur[0] = '\0';
      }
      if (end) break;
    } else if (wordLen < MAX_LINE_CHARS) {
      word[wordLen++] = *c;
    }
  }
  return (int32_t)lineCount * lineH <= usableH;
}

// Break one paragraph into screen lines, spread them down the full height and
// give each a random horizontal offset. Called once per paragraph, never per
// frame: re-rolling X every frame would make the text jitter unreadably.
void layoutParagraph(uint8_t idx) {
  const int16_t W = g->width();
  const int16_t H = g->height();
  const int16_t usableW = W - 2 * SCREEN_MARGIN;
  const int16_t usableH = H - 2 * SCREEN_MARGIN;
  const char* p = paragraphs[idx];

  g->setTextSize(1);

  chosenFont = 1;
  for (uint8_t f = 0; f < sizeof(FONT_LADDER); f++) {
    if (wrapForFont(p, FONT_LADDER[f], usableW, usableH)) {
      chosenFont = FONT_LADDER[f];
      break;
    }
  }
  if (chosenFont == 1) wrapForFont(p, 1, usableW, usableH);  // guaranteed to fit
  chosenLineH = g->fontHeight(chosenFont);

  // Spread the lines over the whole usable height so the paragraph fills the
  // screen instead of clustering at the top, then pick a random X per line that
  // keeps every character inside the margins. Proportional fonts get a few
  // pixels of slack so a wider glyph during the morph cannot cross the margin.
  const int16_t morphSlack = 0;   // crossfade redraws identical glyphs, no drift
  int16_t slot = (lineCount > 0) ? usableH / lineCount : usableH;
  if (slot < chosenLineH) slot = chosenLineH;
  int16_t block = slot * lineCount;
  int16_t top = SCREEN_MARGIN + (usableH - block) / 2;
  if (top < SCREEN_MARGIN) top = SCREEN_MARGIN;

  for (uint8_t i = 0; i < lineCount; i++) {
    lineLen[i] = strlen(lineText[i]);
    int16_t w = measureText(lineText[i], chosenFont) + morphSlack;
    int16_t slack = usableW - w;
    if (slack < 0) slack = 0;
    lineX[i] = SCREEN_MARGIN + random(0, slack + 1);
    lineY[i] = top + i * slot + (slot - chosenLineH) / 2;
  }

  uint16_t chars = strlen(p);
  unsigned long d = READ_BASE_MS + (unsigned long)chars * 1000UL / READ_CHARS_PER_SEC;
  if (d < READ_MIN_MS) d = READ_MIN_MS;
  if (d > READ_MAX_MS) d = READ_MAX_MS;
  paragraphDwell = d;
  paragraphStart = millis();

  DBG("TEXT  : par %u/%u -> font %u (%d px), %u lines, %u chars, dwell %lu ms, y %d..%d",
      idx + 1, PARAGRAPH_COUNT, chosenFont, chosenLineH, lineCount, chars,
      paragraphDwell, lineY[0], lineY[lineCount - 1] + chosenLineH);
}

// Smoothstep easing: starts and ends gently instead of snapping in and out
// linearly, so the dissolve reads as a fade rather than a switch.
static uint8_t easeMorph(unsigned long num, unsigned long den) {
  if (!den) return 0;
  float t = (float)num / (float)den;
  if (t < 0) t = 0;
  if (t > 1) t = 1;
  float e = t * t * (3.0f - 2.0f * t);
  return (uint8_t)(e * 255.0f + 0.5f);
}

// Draw the current paragraph. morph 0..255 is how strongly the characters are
// dissolved into random symbols: 0 = fully legible, 255 = pure noise.
// Each glyph is placed at the position its *original* character occupies, so a
// morphed line never shifts sideways.
// Draw one laid-out paragraph at the given brightness. Characters are placed by
// the same advance rule the renderer uses, so nothing shifts between frames --
// which is what lets a crossfade simply redraw the same glyphs at a new level.
static void drawLines(const char text[][MAX_LINE_CHARS + 1], const uint8_t* len,
                      const int16_t* xs, const int16_t* ys,
                      uint8_t count, uint8_t font, uint8_t brightness) {
  if (!brightness) return;
  g->setTextSize(1);
  g->setTextFont(font);
  g->setTextColor(greyShade(brightness));
  for (uint8_t i = 0; i < count; i++) {
    int16_t x = xs[i];
    for (uint8_t k = 0; k < len[i]; k++) {
      char ch = text[i][k];
      int16_t adv = glyphAdvance(ch, font);
      if (ch != ' ') {
        g->setCursor(x, ys[i]);
        g->print(ch);
      }
      x += adv;
    }
  }
}

// Scale a brightness by a 0..255 fade factor.
static inline uint8_t fadeLevel(uint8_t base, uint8_t f) {
  return (uint8_t)(((uint16_t)base * f) / 255);
}

bool ledPwmOk = false;   // result of ledcAttach(), shown on the splash screen

// Map a 0..LED_MAX_DUTY brightness to the duty the wiring needs.
static inline uint32_t ledDuty(uint32_t brightness) {
  return LED_ACTIVE_LOW ? (LED_MAX_DUTY - brightness) : brightness;
}

// Breathing LED: raised-cosine brightness recomputed every frame from millis(),
// gamma corrected so the ramp looks even to the eye, and written with
// ledcWrite() -- which is just ledc_set_duty() + ledc_update_duty(), two
// register writes.
//
// This deliberately does NOT use ledcFade(). The Arduino core's fade helper
// contains an unbounded spin-wait:
//     while (ledc_get_duty(group, channel) != start_duty);
// with no timeout, so any hiccup between ledc_fade_stop() and the duty
// readback wedges the firmware inside this function forever, freezing the LED
// at an extreme and taking the display down with it. Not acceptable for a
// piece that has to run unattended.
//
// Software timing is fine here because the render loop now runs at ~40 fps,
// so the curve gets ~40 updates per second -- hundreds of steps per breath.
void serviceBreathingLED() {
  if (!ledPwmOk) return;

  const unsigned long period = 2UL * (unsigned long)LED_HALF_BREATH;
  unsigned long now = millis();
  float phase = (now % period) / (float)period;         // 0..1 over one breath
  float level = 0.5f * (1.0f - cosf(2.0f * PI * phase)); // 0..1, smooth at ends
  uint32_t duty = (uint32_t)(powf(level, 2.2f) * LED_MAX_DUTY + 0.5f);
  ledcWrite(LED_PIN, ledDuty(duty));

  // One liveness line per breath, logged as the cycle passes its minimum.
  static unsigned long lastCycle = ULONG_MAX;
  unsigned long cycle = now / period;
  if (cycle != lastCycle) {
    lastCycle = cycle;
    DBG("LED   : breath #%lu, period %lu ms, duty now %lu/%d",
        cycle, period, (unsigned long)duty, LED_MAX_DUTY);
  }
}

static const char *resetReasonName(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON:  return "power-on";
    case ESP_RST_EXT:      return "external";
    case ESP_RST_SW:       return "software";
    case ESP_RST_PANIC:    return "PANIC/exception";
    case ESP_RST_INT_WDT:  return "interrupt watchdog";
    case ESP_RST_TASK_WDT: return "task watchdog";
    case ESP_RST_WDT:      return "other watchdog";
    case ESP_RST_BROWNOUT: return "BROWNOUT";
    case ESP_RST_USB:      return "USB peripheral";
    default:               return "unknown";
  }
}

void setup() {

#if DBG_ON
  Serial.begin(DBG_BAUD);
  Serial.setTxTimeoutMs(0);   // never stall the loop when no host is listening
  // Only give the CDC a moment to enumerate when a cable is actually present.
  // Standalone this costs nothing: isPlugged() is false, so the loop exits at
  // once and boot proceeds at full speed.
  unsigned long dbgWait = millis();
  while (!Serial && millis() - dbgWait < 300) delay(10);
#endif

  DBG("");
  DBG("========== BOOT ==========");
  DBG("build : %s %s", __DATE__, __TIME__);
  DBG("chip  : %s rev %d, %d MHz, %d core(s)",
      ESP.getChipModel(), ESP.getChipRevision(), getCpuFrequencyMhz(), ESP.getChipCores());
  DBG("reset : %s", resetReasonName(esp_reset_reason()));
  DBG("heap  : %u free / %u total, psram %u",
      ESP.getFreeHeap(), ESP.getHeapSize(), ESP.getPsramSize());

#if FAST_RANDOM
  randomSeed(esp_random());   // -> software PRNG for random(), see FAST_RANDOM
  DBG("RANDOM: seeded from HW RNG, random() now uses the software PRNG");
#else
  DBG("RANDOM: using the hardware RNG on every random() call");
#endif

  ledPwmOk = ledcAttach(LED_PIN, LED_PWM_FREQ, LED_PWM_RES);  // breathing LED PWM
  DBG("LED   : pin %d, %d Hz, %d bit, half-breath %d ms, active-%s -> ledcAttach %s",
      LED_PIN, LED_PWM_FREQ, LED_PWM_RES, LED_HALF_BREATH,
      LED_ACTIVE_LOW ? "low" : "high", ledPwmOk ? "OK" : "FAILED");

  unsigned long tStage = millis();
  bool radarOk = radar.initialize(radarMode);  // Initialize radar in single-target mode
  DBG("RADAR : rx=GPIO%d tx=GPIO%d @%d %s -> initialize %s (%lu ms)",
      RX_PIN, TX_PIN, BAUD_RATE,
      radarMode == RD03D::SINGLE_TARGET ? "single-target" : "multi-target",
      radarOk ? "OK" : "FAILED (sensor connected?)", millis() - tStage);

  tStage = millis();
  tft.begin();
  tft.setRotation(0);   // portrait: displays are mounted upright (128x160)
  DBG("TFT   : begin+rotation done (%lu ms), %dx%d",
      millis() - tStage, tft.width(), tft.height());

#if USE_SPRITE
  tStage = millis();
  spriteOk = (sprite.createSprite(tft.width(), tft.height()) != nullptr);
  if (spriteOk) g = &sprite;
  DBG("CANVAS: sprite %dx%d 16bpp -> %s (%lu ms), heap now %u",
      tft.width(), tft.height(), spriteOk ? "OK" : "FAILED, drawing direct",
      millis() - tStage, ESP.getFreeHeap());
#else
  DBG("CANVAS: direct to panel (USE_SPRITE 0)");
#endif

  // Fill screen with different colors to verify
  clearCanvas(TFT_BLACK);

  layoutParagraph(paragraphIdx);   // first paragraph, sized to the real canvas

  // // Draw simple text
  // g->setTextColor(TFT_WHITE, TFT_BLACK);
  // g->setTextSize(2);
  // g->setCursor(3, 2);
  // g->println("");
  // g->println(" Butterfly");
  // g->println("");
  // g->println("     Effect");
  // g->println("");
  // g->setTextSize(1);
  // g->println("");
  // g->println("  Silvia Gatti");
  // g->println("");
  // g->println("                    2026");
  // presentCanvas();

  // DBG("SPLASH: shown, holding 3000 ms");

  // Wait for 3 seconds before starting the main loop (keep the LED breathing)
  unsigned long splashStart = millis();
  while (millis() - splashStart < 3000) {
    serviceBreathingLED();
    delay(10);
  }

  DBG("========== SETUP DONE, entering loop ==========");
}


uint8_t intensity = 0;
unsigned long lastIntensityChange = 0;



// Per-frame breakdown of the glitch effect: symbol generation vs. drawing.
unsigned long glitchGenUs = 0, glitchDrawUs = 0, glitchChars = 0;

// With adjustable space probability (default 25%)
// Fills a reusable buffer instead of growing a String one character at a time:
// same output, no per-frame heap traffic.
static char symbolBuf[401];
const char* randomSymbols(uint16_t length, uint8_t spacePercent = 0) {
  if (length > sizeof(symbolBuf) - 1) length = sizeof(symbolBuf) - 1;
  for (uint16_t i = 0; i < length; i++) {
    symbolBuf[i] = (random(100) < spacePercent) ? ' ' : (char)random(33, 127);
  }
  symbolBuf[length] = '\0';
  return symbolBuf;
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

// A shade of PCB copper at the given brightness (0..255), packed to RGB565.
// Brightness still carries the radar-driven intensity; only the hue changes.
static inline uint16_t copperShade(long level) {
  if (level < 0) level = 0;
  if (level > 255) level = 255;
  uint16_t r = (uint16_t)((COPPER_R * level) / 255);
  uint16_t g = (uint16_t)((COPPER_G * level) / 255);
  uint16_t b = (uint16_t)((COPPER_B * level) / 255);
  return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
}

// Scale a grey level by GLITCH_OPACITY -- the glitch layer's "transparency".
static inline uint8_t glitchGrey(long gray) {
  if (gray < 0) gray = 0;
  return (uint8_t)((gray * GLITCH_OPACITY) / 100);
}

void applyGlitchEffect(uint8_t intensity) {
  if(intensity == 0) {
//    clearCanvas(TFT_BLACK);
    return;
  }

  const char* randomText;
  uint16_t len;
  unsigned long t0;

  glitchGenUs = 0; glitchDrawUs = 0; glitchChars = 0;

  // The paragraph may have selected font 2 or 4; the glitch always uses the
  // GLCD font, so put it back before drawing noise.
  g->setTextFont(1);

  g->setTextSize(1);
  for(uint8_t i = 0; i <10; i++) {
    g->setCursor(0,0);
    len = random(10,400);
    t0 = micros();
    randomText = randomSymbols(len,90);
    glitchGenUs += micros() - t0;
    glitchChars += len;
    g->setTextColor(copperShade(glitchGrey(random(intensity/2, intensity))));
    t0 = micros();
    g->println(randomText);
    glitchDrawUs += micros() - t0;
  }

    g->setTextSize(2);
  for(uint8_t i = 0; i <10; i++) {
    g->setCursor(0,0);
    len = random(10,200);
    t0 = micros();
    randomText = randomSymbols(len,90);
    glitchGenUs += micros() - t0;
    glitchChars += len;
    g->setTextColor(copperShade(glitchGrey(random(intensity/2, intensity))));
    t0 = micros();
    g->println(randomText);
    glitchDrawUs += micros() - t0;
  }

    g->setTextSize(1);
  for(uint8_t i = 0; i <2; i++) {
    g->setCursor(0,0);
    len = random(10,400);
    t0 = micros();
    randomText = randomSymbols(len,99);
    glitchGenUs += micros() - t0;
    glitchChars += len;
    g->setTextColor(copperShade(glitchGrey(random(intensity/4, intensity/2))));
    t0 = micros();
    g->println(randomText);
    glitchDrawUs += micros() - t0;
  }

}

void loop() {

  unsigned long loopT0 = micros();
  unsigned long usText = 0, usGlitch = 0, usPush = 0, usDecay = 0;

  serviceBreathingLED();

  /// check proximity
  unsigned long tSense0 = micros();
  radar.tasks();
  TargetData* tgt = radar.getTarget();
  bool targetValid = tgt->isValid();
  if(targetValid){
    // Throttled, and skipped entirely with no host attached: printInfo() writes
    // straight to Serial, so it must not run when nobody is listening.
    static unsigned long lastRadarLog = 0;
    if (Serial && millis() - lastRadarLog >= DBG_RADAR_MS) {
      lastRadarLog = millis();
      tgt->printInfo();
    }

    float distance = tgt->distance;
    float  angle = tgt->angle;
    float  speed = tgt->speed;

    intensity = map(
      constrain(distance, DIST_NEAR_MM, DIST_FAR_MM),
      DIST_NEAR_MM, DIST_FAR_MM,
      0, 255
    );
  }
  else {
    intensity = 255;
    clearCanvas(TFT_BLACK);
  }
  unsigned long usSense = micros() - tSense0;

  // Closer viewer -> brighter, more readable text. Farther -> the text recedes
  // and the copper glitch dominates.
  {
    uint32_t pct = TEXT_MIN_OPACITY +
                   (uint32_t)(TEXT_OPACITY - TEXT_MIN_OPACITY) * (255u - intensity) / 255u;
    textBrightness = (uint8_t)(255UL * pct / 100UL);
  }


  unsigned long currentTime = millis();


  // Advance to the next paragraph once it has been up long enough to read.
  if (currentTime - paragraphStart >= paragraphDwell) {
    // Keep the outgoing paragraph so it can fade out over the incoming one.
    memcpy(prevText, lineText, sizeof(prevText));
    memcpy(prevLen,  lineLen,  sizeof(prevLen));
    memcpy(prevX,    lineX,    sizeof(prevX));
    memcpy(prevY,    lineY,    sizeof(prevY));
    prevCount = lineCount;
    prevFont  = chosenFont;
    hasPrev   = true;

    paragraphIdx = (paragraphIdx + 1) % PARAGRAPH_COUNT;
    layoutParagraph(paragraphIdx);
    clearCanvas(TFT_BLACK);   // no ghost strokes from the outgoing paragraph
    currentTime = millis();
  }

  // Crossfade: for CROSSFADE_MS after a switch the outgoing paragraph fades out
  // while the incoming one fades in, both on screen together. Smoothstep easing
  // makes it start and end gently rather than ramping linearly.
  unsigned long shown = currentTime - paragraphStart;
  bool crossfading = hasPrev && shown < CROSSFADE_MS;
  uint8_t fadeIn  = 255;
  uint8_t fadeOut = 0;
  if (crossfading) {
    fadeIn  = easeMorph(shown, CROSSFADE_MS);
    fadeOut = 255 - fadeIn;
  }

  // The morph draws fresh random glyphs every frame, so without this ~18 frames
  // of characters pile up on each text line and never leave. Clearing on every
  // morph frame (and once more as it ends) keeps the dissolve to a single crisp
  // layer and hands the settled paragraph a pristine canvas. Only black is
  // written, so no colour is ever altered -- unlike a fade.
  // Outside the transition nothing is cleared, so the glitch still accumulates
  // exactly as before.
  // Both paragraphs are redrawn every crossfade frame, so the canvas has to be
  // wiped first or the two would stack. One extra wipe on the frame the fade
  // ends hands the settled paragraph a clean canvas. Only black is written, so
  // no colour is ever altered.
  static bool wasCrossfading = false;
  if (crossfading || wasCrossfading) clearCanvas(TFT_BLACK);
  wasCrossfading = crossfading;

  // Fade what is already on the canvas before drawing this frame on top, so
  // trails smear and then die out instead of accumulating forever.
  unsigned long tDecay0 = micros();
  decayCanvas();
  usDecay = micros() - tDecay0;

//  applyGlitchEffect(intensity);
  unsigned long tGlitch0 = micros();
  applyGlitchEffect(random(0, intensity));
  usGlitch = micros() - tGlitch0;

  // Draw info
  // g->setTextColor(grayToColor1(10));
  // g->setTextSize(1);
  // g->setCursor(3, 2);
  // g->print("Intensity: ");
  // g->println(intensity);

  // Draw the current paragraph, positioned by layoutParagraph()
  unsigned long tText0 = micros();
  if (crossfading) {
    drawLines(prevText, prevLen, prevX, prevY, prevCount, prevFont,
              fadeLevel(textBrightness, fadeOut));
  }
  drawLines(lineText, lineLen, lineX, lineY, lineCount, chosenFont,
            fadeLevel(textBrightness, fadeIn));
  usText = micros() - tText0;

  // One streamed write of the finished frame (no-op when drawing direct)
  unsigned long tPush0 = micros();
  presentCanvas();
  usPush = micros() - tPush0;

  // ---- periodic telemetry: one line per second, safe to leave running ----
  unsigned long loopUs = micros() - loopT0;
  static unsigned long statT0 = 0, loops = 0, usSum = 0, usMax = 0;
  static unsigned long sumSense = 0, sumGlitch = 0, sumText = 0, sumPush = 0, sumDecay = 0;
  loops++;
  usSum += loopUs;
  sumSense += usSense;
  sumGlitch += usGlitch;
  sumText += usText;
  sumPush += usPush;
  sumDecay += usDecay;
  if (loopUs > usMax) usMax = loopUs;

  static unsigned long totalFrames = 0;
  totalFrames++;

  if (millis() - statT0 >= DBG_STATUS_MS) {
    unsigned long n = loops ? loops : 1;
    // up/frames keep counting with no cable attached, so reconnecting to a
    // running piece shows how long it has been rendering on its own.
    // 21 conversions, 21 arguments -- keep them in step. Print::printf has no
    // __attribute__((format)), so the compiler cannot catch a mismatch here and
    // a stray %s will happily dereference an integer and panic the chip.
    DBG("STATUS: %2lu fps | up %lus, %lu frames | frame avg %6lu us max %6lu us | "
        "sense %5lu | glitch %6lu (gen %6lu + draw %6lu, %lu chars) | "
        "text %5lu | decay %5lu | push %5lu | radar %s | "
        "intensity %3u | par %u/%u | txt %3u | LED %4u/%d | heap %u",
        /*  1 */ loops,
        /*  2 */ millis() / 1000,
        /*  3 */ totalFrames,
        /*  4 */ usSum / n,
        /*  5 */ usMax,
        /*  6 */ sumSense / n,
        /*  7 */ sumGlitch / n,
        /*  8 */ glitchGenUs,
        /*  9 */ glitchDrawUs,
        /* 10 */ glitchChars,
        /* 11 */ sumText / n,
        /* 12 */ sumDecay / n,
        /* 13 */ sumPush / n,
        /* 14 */ targetValid ? "TARGET" : "no-target",
        /* 15 */ intensity,
        /* 16 */ paragraphIdx + 1,
        /* 17 */ PARAGRAPH_COUNT,
        /* 18 */ textBrightness,
        /* 19 */ (unsigned)ledcRead(LED_PIN),
        /* 20 */ LED_MAX_DUTY,
        /* 21 */ ESP.getFreeHeap());
    statT0 = millis();
    loops = 0; usSum = 0; usMax = 0;
    sumSense = 0; sumGlitch = 0; sumText = 0; sumPush = 0; sumDecay = 0;
  }

  delay(FRAME_DELAY);
}