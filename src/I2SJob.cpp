#include "i2sDinDisp.h"


bool  i2sInit( void )
{
  const i2s_config_t i2s_config = {
    .mode                 = (i2s_mode_t)( I2S_MODE_RX | ( ( ESP32_I2S_MASTER == 1 ) ? I2S_MODE_MASTER : I2S_MODE_SLAVE ) ),
    .sample_rate          = SAMPLE_RATE,
    .bits_per_sample      = ( ( BIT_PER_SAMPLE == BPS16BIT ) ? I2S_BITS_PER_SAMPLE_16BIT : I2S_BITS_PER_SAMPLE_32BIT ),
    .channel_format       = ( ( HARD_CHANNELS == MONO ) ? I2S_CHANNEL_FMT_ONLY_LEFT : I2S_CHANNEL_FMT_RIGHT_LEFT ),
#if ESP_ARDUINO_VERSION_MAJOR >= 2
    .communication_format = (i2s_comm_format_t)( I2S_COMM_FORMAT_STAND_I2S ),
#else
    .communication_format = (i2s_comm_format_t)( I2S_COMM_FORMAT_I2S ),
#endif
    .intr_alloc_flags     = 0,
    .dma_buf_count        = DMA_DESC_NUM,     // This is an alias to ‘dma_desc_num’ for backward compatibility
                                              // int dma_desc_num
                                              // The total number of descriptors used by I2S DMA to receive/transmit data
    .dma_buf_len          = DMA_FRAME_NUM,    // This is an alias to ‘dma_frame_num’ for backward compatibility
                                              // int dma_frame_num
                                              // Number of frames for one-time sampling. The frame here means the total data from all the channels in a WS cycle
    .use_apll             = false,
#if ESP_ARDUINO_VERSION_MAJOR >= 2
    .fixed_mclk           = 0,
#endif
  };
  const i2s_pin_config_t i2s_pin_config = {
#if ESP_ARDUINO_VERSION_MAJOR >= 2
    .mck_io_num   = ( ( ESP32_I2S_MASTER == 1 ) ? I2S_MCK : I2S_PIN_NO_CHANGE ),
#endif
    .bck_io_num   = I2S_BCK,
    .ws_io_num    = I2S_LRCK,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num  = I2S_DIN
  };
  esp_err_t Result;

  // Install and start I2S driver
  Result = i2s_driver_install( I2S_NUM_0, &i2s_config, 0, NULL );
  if ( Result != ESP_OK )
  {
    Serial.printf( "i2s_driver_install() : failed. Result:%d\n", Result );
    return false;
  }

  Result = i2s_set_pin( I2S_NUM_0, &i2s_pin_config );
  if ( Result != ESP_OK )
  {
    Serial.printf( "i2s_set_pin() : failed. Result:%d\n", Result );
    return false;
  }

  return true;
}

#ifdef DEBUG
bool  spiffsRead( PI2S_DIN_INFO pDin )
{
static File FileWave;
static int iSeqNo;
  switch ( iSeqNo )
  {
    case 0:
      if ( !SPIFFS.begin() )
      {
        return false;
      }
      FileWave = SPIFFS.open( "/1K-0dB01.WAV", "r" );
      if ( !FileWave )
      {
        return false;
      }
      FileWave.seek( 44, SeekSet );
      iSeqNo = 10;
      // Non break
    case 10:
      while ( 1 )
      {
        if ( ( FileWave.read( &pDin->bWaveBuf[0], WAVE_SIZE ) ) == WAVE_SIZE )
        {
          break;
        }
        FileWave.seek( 44, SeekSet );
      }
      break;
  }
  return true;
}
#endif

bool  i2sRead( PI2S_DIN_INFO pDin )
{
  size_t ByteRead = 0;
  esp_err_t Result;
  bool fResult = false;
#ifdef DEBUG
  if ( spiffsRead( pDin ) == true )
  {
    return true;
  }
#endif
  Result = i2s_read( I2S_NUM_0, &pDin->bI2SBuf[0], BLOCK_SIZE, &ByteRead, portMAX_DELAY );
  if ( Result == ESP_OK && ByteRead == BLOCK_SIZE )
  {
    int iPos = BLOCK_SIZE * pDin->iWaveBufWritePtr;
    memcpy( &pDin->bWaveBuf[iPos], &pDin->bI2SBuf[0], BLOCK_SIZE );
    pDin->iWaveBufWritePtr++;
    if ( pDin->iWaveBufWritePtr >= DMA_DESC_NUM )
    {
      pDin->iWaveBufWritePtr = 0;
      fResult = true;
    }
  }
  else
  {
    Serial.printf( "i2s_read() : Result[%d] ByteRead[%d]\n", Result, ByteRead );
  }
  return fResult;
}
