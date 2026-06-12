// User setup for ESP32-S3 with ILI9163V 8-bit parallel display
// Custom setup ID 720 for Newhaven TFT

#define USER_SETUP_ID 720

// ESP32-S3 parallel interface
#define ESP32_PARALLEL

// Pin definitions for Newhaven TFT with ILI9163V controller
#define TFT_CS   5   // Chip select
#define TFT_DC   6   // Data/Command
#define TFT_WR   7   // Write strobe
#define TFT_RD   8   // Read strobe

// Data bits (D0-D7)
#define TFT_D0   9
#define TFT_D1   10
#define TFT_D2   11
#define TFT_D3   12
#define TFT_D4   13
#define TFT_D5   14
#define TFT_D6   15
#define TFT_D7   16

#define TFT_RESET 17  // Reset pin

// ILI9163V controller
#define TFT_DRIVER 0x9163
#define TFT_WIDTH  128
#define TFT_HEIGHT 160
