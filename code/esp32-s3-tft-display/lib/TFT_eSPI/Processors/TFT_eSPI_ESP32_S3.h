        ////////////////////////////////////////////////////
        // TFT_eSPI driver functions for ESP32 processors //
        ////////////////////////////////////////////////////

// Temporarily a separate file to TFT_eSPI_ESP32.h until board package low level API stabilises

#ifndef _TFT_eSPI_ESP32H_
#define _TFT_eSPI_ESP32H_

// Processor ID reported by getSetup()
#define PROCESSOR_ID 0x32

// Include processor specific header
#include "soc/spi_reg.h"
#include "driver/spi_master.h"
#include "hal/gpio_ll.h"


#define VSPI FSPI

// Fix IDF problems with ESP32S3
// Note illogical enumerations: FSPI_HOST=SPI2_HOST=1   HSPI_HOST=SPI3_HOST=2
#define SPI_MOSI_DLEN_REG(x) SPI_MS_DLEN_REG(x)

// SUPPORT_TRANSACTIONS is mandatory for ESP32 so the hal mutex is toggled4
#define SUPPORT_TRANSACTIONS

/*
ESP32:
FSPI not defined
HSPI = 2, uses SPI2
VSPI = 3, uses SPI3

ESP32-S2:
FSPI = 1, uses SPI2
HSPI = 2, uses SPI3
VSPI not defined

ESP32 C3:
FSPI = 0, uses SPI2 ???? To be checked
HSPI = 1, uses SPI3 ???? To be checked
VSPI not defined

For ESP32/S2/C3/S3:
SPI1_HOST = 0
SPI2_HOST = 1
SPI3_HOST = 2
*/

// ESP32 specific SPI port selection
#define SPI_PORT FSPI

#define CMD_BITS (8-1)

// Initialise processor specific SPI functions, used by init()
#define INIT_TFT_DATA_BUS // Not used


// Processor specific code used by SPI bus transaction startWrite and endWrite functions
// Not applicable to parallel bus
#define SET_BUS_WRITE_MODE
#define SET_BUS_READ_MODE

// Code to check if DMA is busy, used by SPI bus transaction transaction and endWrite functions
#define DMA_BUSY_CHECK

#define SPI_BUSY_CHECK

// If smooth font is used then it is likely SPIFFS will be needed
// Call up the SPIFFS (SPI FLASH Filing System) for the anti-aliased fonts
#define FS_NO_GLOBALS
#include <FS.h>
#include "SPIFFS.h" // ESP32 only
#define FONT_FS_AVAILABLE

////////////////////////////////////////////////////////////////////////////////////////
// Define the DC (TFT Data/Command or Register Select (RS))pin drive code
////////////////////////////////////////////////////////////////////////////////////////
#define DC_C GPIO.out_w1tc = (1 << TFT_DC)
#define DC_D GPIO.out_w1ts = (1 << TFT_DC)

////////////////////////////////////////////////////////////////////////////////////////
// Define the CS (TFT chip select) pin drive code
////////////////////////////////////////////////////////////////////////////////////////
#define CS_L GPIO.out_w1tc = (1 << TFT_CS)
#define CS_H GPIO.out_w1ts = (1 << TFT_CS)

////////////////////////////////////////////////////////////////////////////////////////
// Define the WR (TFT Write) pin drive code
////////////////////////////////////////////////////////////////////////////////////////
// TFT_WR, for best performance, should be in range 0-31 for single register parallel write
#define WR_L GPIO.out_w1tc = (1 << TFT_WR)
#define WR_H GPIO.out_w1ts = (1 << TFT_WR)

    ////////////////////////////////////////////////////////////////////////////////////////
// Define the touch screen chip select pin drive code
////////////////////////////////////////////////////////////////////////////////////////
#define T_CS_L // No macro allocated so it generates no code
#define T_CS_H // No macro allocated so it generates no code

////////////////////////////////////////////////////////////////////////////////////////
// Define the parallel bus interface chip pin drive code
////////////////////////////////////////////////////////////////////////////////////////

#define MASK_OFFSET 0
#define GPIO_CLR_REG GPIO.out_w1tc
#define GPIO_SET_REG GPIO.out_w1ts

// Create a bit set lookup table for data bus - wastes 1kbyte of RAM but speeds things up dramatically
// can then use e.g. GPIO.out_w1ts = set_mask(0xFF); to set data bus to 0xFF
#define PARALLEL_INIT_TFT_DATA_BUS               \
for (int32_t c = 0; c<256; c++)                  \
{                                                \
  xset_mask[c] = 0;                              \
  if ( c & 0x01 ) xset_mask[c] |= (1 << (TFT_D0-MASK_OFFSET)); \
  if ( c & 0x02 ) xset_mask[c] |= (1 << (TFT_D1-MASK_OFFSET)); \
  if ( c & 0x04 ) xset_mask[c] |= (1 << (TFT_D2-MASK_OFFSET)); \
  if ( c & 0x08 ) xset_mask[c] |= (1 << (TFT_D3-MASK_OFFSET)); \
  if ( c & 0x10 ) xset_mask[c] |= (1 << (TFT_D4-MASK_OFFSET)); \
  if ( c & 0x20 ) xset_mask[c] |= (1 << (TFT_D5-MASK_OFFSET)); \
  if ( c & 0x40 ) xset_mask[c] |= (1 << (TFT_D6-MASK_OFFSET)); \
  if ( c & 0x80 ) xset_mask[c] |= (1 << (TFT_D7-MASK_OFFSET)); \
}                                                              \

// Mask for the 8 data bits to set pin directions
#define GPIO_DIR_MASK ((1 << (TFT_D0-MASK_OFFSET)) | (1 << (TFT_D1-MASK_OFFSET)) | (1 << (TFT_D2-MASK_OFFSET)) | (1 << (TFT_D3-MASK_OFFSET)) | (1 << (TFT_D4-MASK_OFFSET)) | (1 << (TFT_D5-MASK_OFFSET)) | (1 << (TFT_D6-MASK_OFFSET)) | (1 << (TFT_D7-MASK_OFFSET)))
#define GPIO_OUT_CLR_MASK (GPIO_DIR_MASK | (1 << TFT_WR))

// A lookup table is used to set the different bit patterns, this uses 1kByte of RAM
#define set_mask(C) xset_mask[C] // 63fps Sprite rendering test 33% faster, graphicstest only 1.8% faster than shifting in real time


// Write 8 bits to TFT
#define tft_Write_8(C)  GPIO_CLR_REG =  GPIO_OUT_CLR_MASK; GPIO_SET_REG = set_mask((uint8_t)(C)); WR_H


// Write 16 bits to TFT
#define tft_Write_16(C) GPIO_CLR_REG = GPIO_OUT_CLR_MASK; GPIO_SET_REG = set_mask((uint8_t) ((C) >> 8)); WR_H; \
                        GPIO_CLR_REG = GPIO_OUT_CLR_MASK; GPIO_SET_REG = set_mask((uint8_t) ((C) >> 0)); WR_H

// 16-bit write with swapped bytes
#define tft_Write_16S(C) GPIO_CLR_REG = GPIO_OUT_CLR_MASK; GPIO_SET_REG = set_mask((uint8_t) ((C) >> 0)); WR_H; \
                          GPIO_CLR_REG = GPIO_OUT_CLR_MASK; GPIO_SET_REG = set_mask((uint8_t) ((C) >> 8)); WR_H


// Write 32 bits to TFT
#define tft_Write_32(C) GPIO_CLR_REG = GPIO_OUT_CLR_MASK; GPIO_SET_REG = set_mask((uint8_t) ((C) >> 24)); WR_H; \
                        GPIO_CLR_REG = GPIO_OUT_CLR_MASK; GPIO_SET_REG = set_mask((uint8_t) ((C) >> 16)); WR_H; \
                        GPIO_CLR_REG = GPIO_OUT_CLR_MASK; GPIO_SET_REG = set_mask((uint8_t) ((C) >>  8)); WR_H; \
                        GPIO_CLR_REG = GPIO_OUT_CLR_MASK; GPIO_SET_REG = set_mask((uint8_t) ((C) >>  0)); WR_H

// Write two concatenated 16-bit values to TFT
#define tft_Write_32C(C,D) GPIO_CLR_REG = GPIO_OUT_CLR_MASK; GPIO_SET_REG = set_mask((uint8_t) ((C) >> 8)); WR_H; \
                            GPIO_CLR_REG = GPIO_OUT_CLR_MASK; GPIO_SET_REG = set_mask((uint8_t) ((C) >> 0)); WR_H; \
                            GPIO_CLR_REG = GPIO_OUT_CLR_MASK; GPIO_SET_REG = set_mask((uint8_t) ((D) >> 8)); WR_H; \
                            GPIO_CLR_REG = GPIO_OUT_CLR_MASK; GPIO_SET_REG = set_mask((uint8_t) ((D) >> 0)); WR_H

// Write 16-bit value twice to TFT - used by drawPixel()
#define tft_Write_32D(C) GPIO_CLR_REG = GPIO_OUT_CLR_MASK; GPIO_SET_REG = set_mask((uint8_t) ((C) >> 8)); WR_H; \
                          GPIO_CLR_REG = GPIO_OUT_CLR_MASK; GPIO_SET_REG = set_mask((uint8_t) ((C) >> 0)); WR_H; \
                          GPIO_CLR_REG = GPIO_OUT_CLR_MASK; GPIO_SET_REG = set_mask((uint8_t) ((C) >> 8)); WR_H; \
                          GPIO_CLR_REG = GPIO_OUT_CLR_MASK; GPIO_SET_REG = set_mask((uint8_t) ((C) >> 0)); WR_H

  // Read pin
#define RD_L GPIO.out_w1tc = (1 << TFT_RD)
//#define RD_L digitalWrite(TFT_WR, LOW)
#define RD_H GPIO.out_w1ts = (1 << TFT_RD)
//#define RD_H digitalWrite(TFT_WR, HIGH)



#define tft_Write_16N tft_Write_16

////////////////////////////////////////////////////////////////////////////////////////
// Macros to read from display using SPI or software SPI
////////////////////////////////////////////////////////////////////////////////////////


// Concatenate a byte sequence A,B,C,D to CDAB, P is a uint8_t pointer
#define DAT8TO32(P) ( (uint32_t)P[0]<<8 | P[1] | P[2]<<24 | P[3]<<16 )

#endif // Header end
