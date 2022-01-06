#include "i2sDinDisp.h"

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite spr = TFT_eSprite( &tft );

I2S_DIN_INFO    i2sDin;
int iScreenWidth;
int iScreenHeight;
uint16_t *pwVerticalColor;
int iDispMode;
int iDispSpectrumChannel;
int idBList[] = { 6, 10, 15, 20, 30, 40, 50, 60, 70, 80, DB_RANGE_VALUE };

#ifdef TOUCH_CS
uint16_t calData[5] = { 462, 3364, 336, 3324, 7 };
#endif


void setup()
{
  Serial.begin( 115200 );
  while (!Serial && !Serial.available());
  Serial.printf( "\n" );

  if ( !fftInit() )
  {
    Serial.printf( "fftInit() not enough memory\n" ); HLT;
  }

  if ( !i2sInit() )
  {
    Serial.printf( "i2sInit() failed\n" ); HLT;
  }

  if ( !tftInit() )
  {
    Serial.printf( "tftInit() not enough memory\n" ); HLT;
  }

  MakeLogBand( &i2sDin.dBandFreq[0], FFT_HALF_SIZE, NULL, 0, SAMPLE_RATE, BANDS );
  Disp();
}

void loop()
{
  if ( i2sRead( &i2sDin ) == true )
  {
#ifdef TOUCH_CS
    touchJob();
#endif
    fftDataSet();
    for ( int c = 0; c < DISP_CHANNELS; c++ )
    {
      i2sDin.ch[c].FFT.DCRemoval();
      i2sDin.ch[c].FFT.Windowing( FFT_WIN_TYP_HANN, FFT_FORWARD );
      i2sDin.ch[c].FFT.Compute( FFT_FORWARD );
      i2sDin.ch[c].FFT.ComplexToMagnitude();
      MakeLogBand( &i2sDin.ch[c].dReal[1], FFT_HALF_SIZE, &i2sDin.ch[c].iBandData[0], DB_RANGE, DATA_RANGE, BANDS );
      UpdatePeakBand( &i2sDin.ch[c].iBandData[0], &i2sDin.ch[c].iBandPeak[0], &i2sDin.ch[c].tv, BANDS );
    }
    Disp();
  }
}

bool  fftInit( void )
{
  void * (*xmalloc)( unsigned int ) = ( ESP.getFreePsram() == 0 ? &malloc : &ps_malloc );
  size_t stSize = sizeof( double ) * FFT_SIZE;
  for ( int c = 0; c < DISP_CHANNELS; c++ )
  {
    i2sDin.ch[c].dReal = (double *)xmalloc( stSize );
    i2sDin.ch[c].dImag = (double *)xmalloc( stSize );
    if ( i2sDin.ch[c].dReal == NULL || i2sDin.ch[c].dImag == NULL )
    {
      return false;
    }
    for ( int i = 0; i < FFT_SIZE; i++ )
    {
      i2sDin.ch[c].dReal[i] = 0.0;
      i2sDin.ch[c].dImag[i] = 0.0;
    }
    i2sDin.ch[c].FFT = arduinoFFT( &i2sDin.ch[c].dReal[0], &i2sDin.ch[c].dImag[0], FFT_SIZE, SAMPLE_RATE );
  }
  return true;
}

bool  tftInit( void )
{
  tft.init();
  tft.fillScreen( TFT_BLACK );
  tft.setRotation( 1 );
  iScreenWidth  = tft.width();
  iScreenHeight = tft.height();

#ifdef TOUCH_CS
  tft.setTouch( calData );
#endif

  int iColorDepth = ( ESP.getFreePsram() == 0 ? 8 : 16 );
  spr.setColorDepth( iColorDepth );
  if ( spr.createSprite( iScreenWidth, iScreenHeight, 1 ) == NULL )
  {
    return false;
  }
  spr.fillSprite( TFT_BLACK );
  spr.setTextSize( 1 );
  spr.setTextColor( TFT_WHITE );

  size_t stSize = sizeof( uint16_t ) * iScreenHeight;
  pwVerticalColor = (uint16_t *)malloc( stSize );
  if ( pwVerticalColor == NULL )
  {
    return false;
  }

  uint16_t wColor;
  int iR1 = 0xFF, iG1 = 0x00, iB1 = 0x00;   // Web color RED    #FF0000
  int iR2 = 0xFF, iG2 = 0xFF, iB2 = 0x00;   // Web color YELLOW #FFFF00
  int iR3 = 0x00, iG3 = 0xFF, iB3 = 0x00;   // Web color GREEN  #00FF00
  int iB = FONT_HEIGHT + 1;
  int iHeight = iScreenHeight - iB;
  int iLimit1 = (int)( (double)iHeight * 0.2 );
  int iLimit2 = (int)( (double)iLimit1 + (double)iHeight * 0.3 );
  for ( int i = 0; i < iHeight; i++ )
  {
    if ( iColorDepth == 8 )
    {
      if ( i >= 0 && i < iLimit1 )
      {
        wColor = TFT_RED;
      }
      else if ( i >= iLimit1 && i < iLimit2 )
      {
        wColor = TFT_YELLOW;
      }
      else
      {
        wColor = TFT_GREEN;
      }
    }
    else
    {
      int iR = 0, iG = 0, iB = 0;
      double dA = 0;
      if ( i >= 0 && i < iLimit2 )
      {
        dA = (double)i / (double)( iLimit2 - 1 );
        iR = (int)( (double)( iR2 - iR1 ) * dA + (double)iR1 );
        iG = (int)( (double)( iG2 - iG1 ) * dA + (double)iG1 );
        iB = (int)( (double)( iB2 - iB1 ) * dA + (double)iB1 );
      }
      else
      {
        dA = (double)( i - iLimit2 ) / (double)( iLimit2 - 1 );
        iR = (int)( (double)( iR3 - iR2 ) * dA + (double)iR2 );
        iG = (int)( (double)( iG3 - iG2 ) * dA + (double)iG2 );
        iB = (int)( (double)( iB3 - iB2 ) * dA + (double)iB2 );
      }
      wColor = (uint16_t)RGB565( iR, iG, iB );
    }
    pwVerticalColor[i] = wColor;
  }
  return true;
}

void  fftDataSet( void )
{
  int16_t *psAdrs = (int16_t *)&i2sDin.bWaveBuf[0];
  int32_t *plAdrs = (int32_t *)&i2sDin.bWaveBuf[0];
  double dData[2] = { 0.0, 0.0 };
  for ( int i = 0, j = 0; i < FFT_SIZE; i++, j += HARD_CHANNELS )
  {
    if ( BIT_PER_SAMPLE == BPS16BIT )
    {
      dData[0] = (double)( (double)psAdrs[j + 0] / (double)0x7FFF );
      dData[1] = (double)( (double)psAdrs[j + 1] / (double)0x7FFF );
    }
    else
    {
      dData[0] = (double)( (double)plAdrs[j + 0] / (double)0x7FFFFFFF );
      dData[1] = (double)( (double)plAdrs[j + 1] / (double)0x7FFFFFFF );
    }
    for ( int c = 0; c < DISP_CHANNELS; c++ )
    {
      i2sDin.ch[c].dReal[i] = dData[c];
      i2sDin.ch[c].dImag[i] = 0.0;
    }
  }
}

double  ComputeFreqBand( double *pdFreqBuf, int iFFTHalfSize, int iBand, int iBandMax, double *pdXScale )
{
  int iPoint1 = ceilf( pdXScale[ iBand ] );
  int iPoint2 = floorf( pdXScale[ iBand + 1 ] );
  double dData = 0;
  double dResult = 0;
  if ( iPoint2 < iPoint1 )
  {
    dData += pdFreqBuf[iPoint2] * ( pdXScale[iBand + 1] - pdXScale[iBand] );
  }
  else
  {
    if ( iPoint1 > 0 )
    {
      dData += pdFreqBuf[iPoint1 - 1] * ( (double)iPoint1 - pdXScale[iBand] );
    }
    for ( ; iPoint1 < iPoint2; iPoint1++ )
    {
      dData += pdFreqBuf[iPoint1];
    }
    if ( iPoint2 < iFFTHalfSize )
    {
      dData += pdFreqBuf[iPoint2] * ( pdXScale[iBand + 1] - (double)iPoint2 );
    }
  }
  if ( dData != 0 )
  {
    dResult = 10 * log10( dData * dData );
  }
  return dResult;
}

void  MakeLogBand( double *pdFreqBuf, int iFFTHalfSize, int *piBandData, int iDbRange, int iRange, int iBandMax )
{
  static bool fInitBandsXScale;
  static double *pdXScale;
  if ( iBandMax == 0 )
  {
    return;
  }
  if ( fInitBandsXScale == false )
  {
    size_t stSize = sizeof( double ) * ( iBandMax + 1 );
    pdXScale = (double *)malloc( stSize );
    if ( pdXScale == NULL )
    {
      return;
    }
    for ( int i = 0; i <= iBandMax; i++ )
    {
      pdXScale[i] = pow( iFFTHalfSize, (double)i / (double)iBandMax );
      if ( pdFreqBuf != NULL && piBandData == NULL && iDbRange == 0 )
      {
        int iSamplingRate = iRange;
        pdFreqBuf[i] = iSamplingRate / ( ( iFFTHalfSize * 2 ) / pdXScale[i] );
      }
    }
    fInitBandsXScale = true;
    if ( pdFreqBuf != NULL && piBandData == NULL && iDbRange == 0 )
    {
      return;
    }
  }
  for ( int i = 0; i < iBandMax; i++ )
  {
    double dData = ComputeFreqBand( pdFreqBuf, iFFTHalfSize, i, iBandMax, pdXScale );
    dData = ( dData / (double)iDbRange ) * (double)iRange;
    piBandData[i] = min( max( dData, (double)0 ), (double)iRange );
  }
}

void  UpdatePeakBand( int *piBandData, int *piBandPeak, struct timeval *ptvOld, int iBandMax )
{
  struct timeval tvNow;
  struct timeval tvDiff;
  gettimeofday( &tvNow, NULL );
  timersub( &tvNow, ptvOld, &tvDiff  );
  int32_t lCheckTime = tvDiff.tv_sec * 1000 + tvDiff.tv_usec / 1000;
  for ( int i = 0; i < iBandMax; i++ )
  {
    if ( piBandData[i] > piBandPeak[i] )
    {
      piBandPeak[i] = piBandData[i];
    }
    if ( lCheckTime > 500 )
    {
      *ptvOld = tvNow;
      piBandPeak[i] = 0;
    }
  }
}

#ifdef TOUCH_CS
void  touchJob( void )
{
  static bool fOldPressed;
  bool fPressed = false;
  uint16_t wX = 0;
  uint16_t wY = 0;
  fPressed = tft.getTouch( &wX, &wY );
  if ( fOldPressed == false && fPressed == true )
  {
    iDispMode++;
    if ( iDispMode > 1 ) iDispMode = 0;
    if ( iDispMode == 1 && HARD_CHANNELS == STEREO && DISP_CHANNELS == STEREO )
    {
      if ( wX >= 0 && wX < ( iScreenHeight / 2 ) )
      {
        iDispSpectrumChannel = 0;
      }
      else
      {
        iDispSpectrumChannel = 1;
      }
    }
  }
  fOldPressed = fPressed;
}
#endif

void  Disp( void )
{
  spr.fillSprite( TFT_BLACK );
  if ( iDispMode == 0 )
  {
    DispBand();
  }
  else
  {
    DispSpectrum();
  }
  spr.pushSprite( 0, 0 );
}

void  DispBand( void )
{
  int iBands = BANDS;
  int iDispChannels = DISP_CHANNELS;
  int iGXStep;
  int iGX = 0;
  int iHeight = iScreenHeight - ( FONT_HEIGHT + 1 );
  int iGYBottom = iHeight - 1;
  if ( iBands <= 0 )
  {
    spr.setCursor( 0, 0 );
    spr.print( "Not Draw : BANDS <= 0" );
    return;
  }
  if ( iDispChannels <= 0 )
  {
    spr.setCursor( 0, 0 );
    spr.print( "Not Draw : DISP_CHANNELS <= 0" );
    return;
  }
  iGXStep = iScreenWidth / iBands / iDispChannels;
  if ( iGXStep < 2 ) iGXStep = 2;
  if ( iDispChannels == MONO )
  {
    // L CHANNEL : low ~ high
    for ( int i = 0; i < iBands; i++, iGX += iGXStep )
    {
      DrawLevel( i2sDin.dBandFreq[i], i2sDin.ch[0].iBandData[i], i2sDin.ch[0].iBandPeak[i], iGX, iGXStep, iHeight );
    }
  }
  else
  {
    // L CHANNEL : high ~ low
    for ( int i = ( iBands - 1 ); i >= 0; i--, iGX += iGXStep )
    {
      DrawLevel( i2sDin.dBandFreq[i], i2sDin.ch[0].iBandData[i], i2sDin.ch[0].iBandPeak[i], iGX, iGXStep, iHeight );
    }
    iGX++;
    // R CHANNEL : low ~ high
    for ( int i = 0; i < iBands; i++, iGX += iGXStep )
    {
      DrawLevel( i2sDin.dBandFreq[i], i2sDin.ch[1].iBandData[i], i2sDin.ch[1].iBandPeak[i], iGX, iGXStep, iHeight );
    }
  }
}

void  DrawLevel( double dBandFreq, int iBandData, int iBandPeak, int iGX, int iGXStep, int iHeight )
{
  int iGYBottom = iHeight - 1;
  int iGYLvl = iGYBottom - ( iBandData * iHeight / 100 );
  for ( int iGY = iGYBottom; iGY >= iGYLvl; iGY -= 5 )
  {
    spr.fillRect( iGX + 1, iGY - 3, iGXStep - 1, 4, pwVerticalColor[iGY - 1] );
  }
  iGYLvl = iGYBottom - ( iBandPeak * iHeight / 100 );
  spr.fillRect( iGX + 1, iGYLvl - 3, iGXStep - 1, 4, pwVerticalColor[iGYLvl - 1] );
  DrawStringFreq( dBandFreq, iGX + 1, iHeight, iGXStep - 1 );
}

void  DispSpectrum( void )
{
  if ( DISP_CHANNELS <= 0 )
  {
    spr.setCursor( 0, 0 );
    spr.print( "Not Draw : DISP_CHANNELS <= 0" );
    return;
  }
  if ( DISP_CHANNELS > 2 )
  {
    spr.setCursor( 0, 0 );
    spr.print( "Not Draw : DISP_CHANNELS > 2" );
    return;
  }
  if ( iDispSpectrumChannel < 0 || iDispSpectrumChannel >= DISP_CHANNELS ) iDispSpectrumChannel = 0;
  char szBuf[16];
  sprintf( szBuf, "-%d", idBList[ ARRAY_SIZE( idBList ) - 1] );
  int iLen = strlen( szBuf );
  int iGXOffset = FONT_WIDTH * iLen + 3;
  int iWidth = iScreenWidth - iGXOffset;
  int iHeight = iScreenHeight - ( FONT_HEIGHT + 1 );
  int iGYBottom = iHeight - 1;
  int iGX1, iGY1, iGX2, iGY2;
  int iA = iScreenWidth / log10( FFT_HALF_SIZE );
  uint16_t wColor = ( iDispSpectrumChannel == 0 ? TFT_GREEN : TFT_CYAN );
  DrawSpectrumXAxis( iGXOffset, iHeight, iA );
  DrawSpectrumYAxis( iGXOffset, iHeight );
  iGX1 = -1;
  for ( int i = 1; i < FFT_HALF_SIZE; i++ )
  {
    iGX2 = iGXOffset + ( iA * log10( i ) );
    if ( iGX2 >= iScreenWidth )
    {
      break;
    }
    double dMag = i2sDin.ch[iDispSpectrumChannel].dReal[i];
    double dData = 0.0;
    int iData = 0;
    if ( dMag != 0 )
    {
      dData = 10 * log10( dMag * dMag );
      iData = ZERO_DB - dData;
#ifdef DEBUG
      // 16 : Sampling rate 32KHz , FFT Size 512 = 32000/512*16 = 1000Hz
      if ( i == 16 )
      {
        Serial.printf( "%.5f %.5f %d\n", dMag, dData, iData );
      }
#endif
    }
    iGY2 = ( iData * iHeight / 100 );
    iGY2 = min( iGY2, iGYBottom );
    if ( iGX1 == -1 )
    {
      iGX1 = iGX2;
      iGY1 = iGY2;
    }
    spr.drawLine( iGX1, iGY1, iGX2, iGY2, wColor );
    iGX1 = iGX2;
    iGY1 = iGY2;
  }
  iGX1 = ( iScreenWidth - 1 ) - ( FONT_WIDTH * 26 );
  spr.setCursor( iGX1, 0 * FONT_HEIGHT + 2 );
  if ( ( SAMPLE_RATE % 1000 ) == 0 )
  {
    sprintf( szBuf, "%d", SAMPLE_RATE / 1000 );
  }
  else
  {
    sprintf( szBuf, "%d.%d", SAMPLE_RATE / 1000, ( ( SAMPLE_RATE % 1000 ) / 100 ) );
  }
  spr.printf( "Sampling rate:%sKHz", szBuf );
  spr.setCursor( iGX1, 1 * FONT_HEIGHT + 2 );
  spr.printf( "FFT Size:%d", FFT_SIZE );
  spr.setCursor( iGX1, 2 * FONT_HEIGHT + 2 );
  if ( HARD_CHANNELS == STEREO && DISP_CHANNELS == STEREO )
  {
    spr.printf( "Channel:%s", ( iDispSpectrumChannel == 0 ? "Left" : "Right" ) );
  }
  else
  {
    spr.printf( "Channel:Mono" );
  }
  spr.drawRect( iGXOffset + 0, 0, iWidth, iHeight, TFT_WHITE );
}

void  DrawSpectrumXAxis( int iGXOffset, int iHeight, int iA )
{
  double dHz = SAMPLE_RATE / FFT_SIZE;
  for ( int iFreq = 63; iFreq < ( SAMPLE_RATE / 2 ); iFreq *= 2 )
  {
    iFreq = ( iFreq == 126 ? 125 : iFreq );
    int iPos = ceilf( iFreq / dHz );
    int iGX = iGXOffset + ( iA * log10( iPos ) );
    if ( iGX >= iScreenWidth )
    {
      break;
    }
    for ( int iGY = 0; iGY < iHeight; iGY += 2 )
    {
      spr.drawPixel( iGX, iGY, TFT_BLUE );
    }
    DrawStringFreq( (double)iFreq, iGX, iHeight, 1 );
  }
}

void  DrawSpectrumYAxis( int iGXOffset, int iHeight )
{
  int idBRange = idBList[ ARRAY_SIZE( idBList ) - 1 ];
  spr.setCursor( 0, 0 );
  spr.printf( "0dB" );
  for ( int i = 0; i < ARRAY_SIZE( idBList ); i++ )
  {
    int idB = idBList[i];
    double dData = (double)( (double)idB * (double)( iHeight / (double)idBRange ) );
    int iGY = dData;
    for ( int iGX = iGXOffset; iGX < iScreenWidth; iGX += 2 )
    {
      spr.drawPixel( iGX, iGY, TFT_BLUE );
    }
    spr.setCursor( 0, iGY - ( FONT_HEIGHT / 2 ) );
    spr.printf( "-%d", idB );
  }
}

void  DrawStringFreq( double dFreq, int iGX, int iGY, int iGXSize )
{
  static int iOldGX;
  char szHz[8];
  if ( abs( iGX - iOldGX ) <= ( FONT_WIDTH * 3 ) ) return;
  if ( dFreq >= 1000 )
  {
    sprintf( szHz, "%dK", (int)( dFreq / 1000 ) );
  }
  else
  {
    sprintf( szHz, "%d", (int)ceilf( dFreq ) );
  }
  int iA = ( FONT_WIDTH * strlen( szHz ) ) / 2;
  iGX -= iA;
  iGX += iGXSize / 2;
  spr.setCursor( iGX, iGY + 1 );
  spr.printf( "%s", szHz );
  iOldGX = iGX;
}

void  SerialOutputFree( int iMarker )
{
  size_t stFreeHeap = ESP.getFreeHeap();
  size_t stFreePsram = ESP.getFreePsram();
  Serial.printf( "%d:FreeHeap %d(%dK) bytes FreePsram %d(%dK) bytes\n", iMarker, stFreeHeap, stFreeHeap / 1000, stFreePsram, stFreePsram / 1000 );
}

void  GpioProbe( int iGpioNo, int iLevel )
{
  if ( iGpioNo != -1 )
  {
    if ( iLevel == -1 )
    {
      pinMode( iGpioNo, OUTPUT );
    }
    else
    {
      digitalWrite( iGpioNo, iLevel );
    }
  }
}
