#define USER_SETUP_ID 720
#define ESP32_PARALLEL
#define TFT_PARALLEL_8_BIT

// Control pins - MUST use pins 0-31 for best performance on ESP32
#define TFT_CS   5   // Chip select (set to -1 if tied low)
#define TFT_DC   6   // Data/Command
#define TFT_WR   7   // Write strobe
#define TFT_RD   8   // Read strobe

// Data bus pins (D0-D7) - MUST be consecutive and in range 0-31
#define TFT_D0   9
#define TFT_D1   10
#define TFT_D2   11
#define TFT_D3   12
#define TFT_D4   13
#define TFT_D5   14
#define TFT_D6   15
#define TFT_D7   16

// Reset pin - THIS IS THE CRITICAL FIX
#define TFT_RST  17  // Changed from TFT_RESET to TFT_RST

// ILI9163V controller
#define ILI9163_DRIVER
#define TFT_WIDTH  128
#define TFT_HEIGHT 160

// Fonts (optional but recommended)
#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_GFXFF
#define SMOOTH_FONT
