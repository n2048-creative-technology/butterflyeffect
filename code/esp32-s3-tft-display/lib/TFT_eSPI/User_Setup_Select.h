// This header file includes the user setup file
// For ESP32-S3 with ILI9163V parallel display

#ifndef USER_SETUP_LOADED

#include <User_Setup.h>

#endif // USER_SETUP_LOADED

/////////////////////////////////////////////////////////////////////////////////////
//
//     TFT DRIVER DEFINITIONS
//
/////////////////////////////////////////////////////////////////////////////////////

// Identical looking TFT displays may have a different colour ordering in the 16-bit colour
#define TFT_BGR 0   // Colour order Blue-Green-Red
#define TFT_RGB 1   // Colour order Red-Green-Blue

#include <TFT_Drivers/ILI9163_Defines.h>
#define  TFT_DRIVER 0x9163
