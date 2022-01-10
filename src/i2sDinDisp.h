#include <Arduino.h>
#include <driver/i2s.h>
#include <soc/i2s_reg.h>
#include <time.h>
#include <math.h>
#include <WiFi.h>
#include <FS.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <arduinoFFT.h>

//#define ILI9341_DRIVER                // Arduino/libraries/TFT_eSPI/User_Setup.h
//#define TFT_BL              32        // Arduino/libraries/TFT_eSPI/User_Setup.h
//#define TFT_BACKLIGHT_ON    HIGH      // Arduino/libraries/TFT_eSPI/User_Setup.h
//#define TFT_MISO            19        // Arduino/libraries/TFT_eSPI/User_Setup.h
//#define TFT_MOSI            23        // Arduino/libraries/TFT_eSPI/User_Setup.h
//#define TFT_SCLK            18        // Arduino/libraries/TFT_eSPI/User_Setup.h
//#define TFT_CS              15        // Arduino/libraries/TFT_eSPI/User_Setup.h
//#define TFT_DC              2         // Arduino/libraries/TFT_eSPI/User_Setup.h
//#define TFT_RST             4         // Arduino/libraries/TFT_eSPI/User_Setup.h
//#define TOUCH_CS            21        // Arduino/libraries/TFT_eSPI/User_Setup.h
//#define LOAD_GLCD                     // Arduino/libraries/TFT_eSPI/User_Setup.h
//#define LOAD_FONT2                    // Arduino/libraries/TFT_eSPI/User_Setup.h
//#define LOAD_FONT4                    // Arduino/libraries/TFT_eSPI/User_Setup.h
//#define LOAD_FONT6                    // Arduino/libraries/TFT_eSPI/User_Setup.h
//#define LOAD_FONT7                    // Arduino/libraries/TFT_eSPI/User_Setup.h
//#define LOAD_FONT8                    // Arduino/libraries/TFT_eSPI/User_Setup.h
//#define LOAD_GFXFF                    // Arduino/libraries/TFT_eSPI/User_Setup.h
//#define SMOOTH_FONT                   // Arduino/libraries/TFT_eSPI/User_Setup.h
//#define SPI_FREQUENCY       40000000  // Arduino/libraries/TFT_eSPI/User_Setup.h
//#define SPI_READ_FREQUENCY  20000000  // Arduino/libraries/TFT_eSPI/User_Setup.h
//#define SPI_TOUCH_FREQUENCY 2500000   // Arduino/libraries/TFT_eSPI/User_Setup.h

#define ARRAY_SIZE( array )   ( (int)( sizeof( array ) / sizeof( (array)[0] ) ) )
#define RGB565( r, g ,b )     ( (unsigned short)( ( r >> 3 ) << 11 ) | ( ( g >> 2 ) << 5 ) | ( b >> 3 ) )
#define HLT                   { while ( 1 ) { delay( 500 ); } }

#define ESP32_I2S_MASTER      1         // 1:ESP32 MASTER other:ESP32 SLAVE
#define HARD_CHANNELS         STEREO    // 1:LEFT(MONO) 2:RIGHT&LEFT(STEREO)
#define SAMPLE_RATE           RATE32KHZ
#define BIT_PER_SAMPLE        BPS32BIT
#define BLOCK_SIZE            256
#define DMA_FRAME_NUM         (BLOCK_SIZE / ( BIT_PER_SAMPLE / 8 ) / HARD_CHANNELS)
#define DMA_DESC_NUM          16
#define WAVE_SIZE             (BLOCK_SIZE * DMA_DESC_NUM)
#define FFT_SIZE              (WAVE_SIZE / ( BIT_PER_SAMPLE / 8 ) / HARD_CHANNELS)
#define FFT_HALF_SIZE         (FFT_SIZE / 2)

//#define DEBUG
#define DISP_CHANNELS         STEREO    // 1:LEFT(MONO) 2:RIGHT&LEFT(STEREO)
#define BANDS                 8
#define FONT_WIDTH            5
#define FONT_HEIGHT           8
#define DB_RANGE              45
#define DATA_RANGE            100
#define ZERO_DB               42.797582 // 42.797582 = 10 * log10( 138 * 138 )
#define DB_RANGE_VALUE        99

#define I2S_DIN               25
#define I2S_BCK               26
#define I2S_LRCK              27
#define I2S_MCK               0

#define MONO                  1
#define STEREO                2
#define RATE96KHZ             96000
#define RATE88_2KHZ           88200
#define RATE64KHZ             64000
#define RATE48KHZ             48000
#define RATE44_1KHZ           44100
#define RATE32KHZ             32000
#define RATE24KHZ             24000
#define RATE22_05KHZ          22050
#define RATE16KHZ             16000
#define RATE12KHZ             12000
#define RATE11_025KHZ         11025
#define RATE8KHZ              8000
#define RATE6KHZ              6000
#define BPS16BIT              16
#define BPS32BIT              32

typedef struct TAG_I2S_DIN_INFO {
  uint8_t       bI2SBuf[ BLOCK_SIZE ];
  int           iWaveBufWritePtr;
  uint8_t       bWaveBuf[ WAVE_SIZE ];
  int           iBands;
  double        *dBandFreq;
  struct {
    arduinoFFT  FFT;
    double      *dReal;
    double      *dImag;
    int         *iBandData;
    int         *iBandPeak;
    struct timeval tv;
  } ch[ DISP_CHANNELS ];
} I2S_DIN_INFO, *PI2S_DIN_INFO;

// I2SJob.cpp
extern  bool  i2sInit( void );
extern  bool  i2sRead( PI2S_DIN_INFO pDin );
