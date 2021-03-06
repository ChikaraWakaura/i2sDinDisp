#include "i2sDinDisp.h"

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite spr = TFT_eSprite( &tft );

I2S_DIN_INFO    i2sDin;
int iScreenWidth;
int iScreenHeight;
uint16_t *pwVerticalColor;
int iDispMode;
int iDispSpectrumChannel;
int idBList[] = { 6, 10, 15, 20, 30, 40, 50, 60, 70, 80, 90, DB_RANGE_VALUE };

#ifdef TOUCH_CS
uint16_t calData[5] = { 462, 3364, 336, 3324, 7 };

TFT_eSPI_Button btn[8];
struct  {
  const char *pcszName;
  uint16_t wColor;
  TFT_eSPI_Button *btn;
} Menu[] = {
  "close",   TFT_RED,  &btn[0],
  "8BANDS",  TFT_BLUE, &btn[1],
  "10BANDS", TFT_BLUE, &btn[2],
  "16BANDS", TFT_BLUE, &btn[3],
  "24BANDS", TFT_BLUE, &btn[4],
  "32BANDS", TFT_BLUE, &btn[5],
#if HARD_CHANNELS == STEREO && DISP_CHANNELS == STEREO
  "LSpectrum", TFT_BLUE, &btn[6],
  "RSpectrum", TFT_BLUE, &btn[7],
#else
  "Spectrum", TFT_BLUE, &btn[6],
#endif
};
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

  if ( !bandsInit( BANDS ) )
  {
    Serial.printf( "bandsInit() not enough memory\n" ); HLT;
  }

  Disp();
}

void loop()
{
  if ( i2sRead( &i2sDin ) == true )
  {
    bool fMenuExec = false;
#ifdef TOUCH_CS
    fMenuExec = touchJob();
#endif
    fftDataSet();
    for ( int c = 0; c < DISP_CHANNELS; c++ )
    {
      i2sDin.ch[c].FFT.DCRemoval();
      i2sDin.ch[c].FFT.Windowing( FFT_WIN_TYP_HANN, FFT_FORWARD );
      i2sDin.ch[c].FFT.Compute( FFT_FORWARD );
      i2sDin.ch[c].FFT.ComplexToMagnitude();
      MakeLogBand( &i2sDin.ch[c].dReal[1], FFT_HALF_SIZE, &i2sDin.ch[c].iBandData[0], DB_RANGE, DATA_RANGE, i2sDin.iBands );
      UpdatePeakBand( &i2sDin.ch[c].iBandData[0], &i2sDin.ch[c].iBandPeak[0], &i2sDin.ch[c].tv, i2sDin.iBands );
    }
    if ( !fMenuExec )
    {
      Disp();
    }
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

  int iB = FONT_HEIGHT + 1;
  int iHeight = iScreenHeight - iB;
  size_t stSize = sizeof( uint16_t ) * iHeight;
  pwVerticalColor = (uint16_t *)malloc( stSize );
  if ( pwVerticalColor == NULL )
  {
    return false;
  }

  uint16_t wColor;
  int iR1 = 0xFF, iG1 = 0x00, iB1 = 0x00;   // Web color RED    #FF0000
  int iR2 = 0xFF, iG2 = 0xFF, iB2 = 0x00;   // Web color YELLOW #FFFF00
  int iR3 = 0x00, iG3 = 0xFF, iB3 = 0x00;   // Web color GREEN  #00FF00
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

bool  bandsInit( int iBands )
{
  if ( iBands <= 0 )
  {
    return false;
  }
  if ( i2sDin.iBands == iBands )
  {
    return true;
  }
  i2sDin.iBands = iBands;
  void *p = (void *)i2sDin.dBandFreq;
  size_t stSize = sizeof( *i2sDin.dBandFreq ) * ( i2sDin.iBands + 1 );
  if ( p == NULL )
  {
    p = malloc( stSize );
  }
  else
  {
    p = realloc( p, stSize );
  }
  if ( p == NULL ) return false;
  i2sDin.dBandFreq = (double *)p;
  if ( !MakeLogBand( &i2sDin.dBandFreq[0], FFT_HALF_SIZE, NULL, 0, SAMPLE_RATE, i2sDin.iBands ) )
  {
    return false;
  }
  for ( int c = 0; c < DISP_CHANNELS; c++ )
  {
    p = (void *)i2sDin.ch[c].iBandData;
    stSize = sizeof( *i2sDin.ch[c].iBandData ) * i2sDin.iBands;
    if ( p == NULL )
    {
      p = malloc( stSize );
    }
    else
    {
      p = realloc( p, stSize );
    }
    if ( p == NULL ) return false;
    i2sDin.ch[c].iBandData = (int *)p;
    for ( int i = 0; i < i2sDin.iBands; i++ )
    {
      i2sDin.ch[c].iBandData[i] = 0;
    }

    p = (void *)i2sDin.ch[c].iBandPeak;
    stSize = sizeof( *i2sDin.ch[c].iBandPeak ) * i2sDin.iBands;
    if ( p == NULL )
    {
      p = malloc( stSize );
    }
    else
    {
      p = realloc( p, stSize );
    }
    if ( p == NULL ) return false;
    i2sDin.ch[c].iBandPeak = (int *)p;
    for ( int i = 0; i < i2sDin.iBands; i++ )
    {
      i2sDin.ch[c].iBandPeak[i] = 0;
    }
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

double  ComputeFreqBand( double *pdFreqBuf, int iFFTHalfSize, int iBand, int iBands, double *pdXScale )
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

bool  MakeLogBand( double *pdFreqBuf, int iFFTHalfSize, int *piBandData, int iDbRange, int iRange, int iBands )
{
  static int iOldBands;
  static double *pdXScale;
  if ( iBands <= 0 )
  {
    return false;
  }
  if ( iOldBands != iBands )
  {
    size_t stSize = sizeof( *pdXScale ) * ( iBands + 1 );
    void *p = (void *)pdXScale;
    if ( p == NULL )
    {
      p = malloc( stSize );
    }
    else
    {
      p = realloc( p, stSize );
    }
    if ( p == NULL ) return false;
    pdXScale = (double *)p;
    for ( int i = 0; i <= iBands; i++ )
    {
      pdXScale[i] = pow( iFFTHalfSize, (double)i / (double)iBands );
      if ( pdFreqBuf != NULL && piBandData == NULL && iDbRange == 0 )
      {
        int iSamplingRate = iRange;
        pdFreqBuf[i] = iSamplingRate / ( ( iFFTHalfSize * 2 ) / pdXScale[i] );
      }
    }
    iOldBands = iBands;
    if ( pdFreqBuf != NULL && piBandData == NULL && iDbRange == 0 )
    {
      return true;
    }
  }
  for ( int i = 0; i < iBands; i++ )
  {
    double dData = ComputeFreqBand( pdFreqBuf, iFFTHalfSize, i, iBands, pdXScale );
    dData = ( dData / (double)iDbRange ) * (double)iRange;
    piBandData[i] = min( max( dData, (double)0 ), (double)iRange );
  }
  return true;
}

void  UpdatePeakBand( int *piBandData, int *piBandPeak, struct timeval *ptvOld, int iBands )
{
  struct timeval tvNow;
  struct timeval tvDiff;
  gettimeofday( &tvNow, NULL );
  timersub( &tvNow, ptvOld, &tvDiff  );
  int32_t lCheckTime = tvDiff.tv_sec * 1000 + tvDiff.tv_usec / 1000;
  for ( int i = 0; i < iBands; i++ )
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
bool  touchJob( void )
{
  static int iSeqNo;
  static bool fOldPressed;
  bool fPressed = false;
  bool fResult = false;
  uint16_t wTX = 0;
  uint16_t wTY = 0;
  fPressed = tft.getTouch( &wTX, &wTY );
  switch ( iSeqNo )
  {
    case 0:
      if ( fOldPressed == false && fPressed == true )
      {
        DrawMenu();
        iSeqNo = 110;
        fResult = true;
      }
      break;
    case 110:
      fResult = true;
      if ( fOldPressed == false && fPressed == false )
      {
        iSeqNo = 120;
      }
      break;
    case 120:
      fResult = true;
      if ( SelctMenu( fPressed, wTX, wTY ) )
      {
        iSeqNo = 0;
        fResult = false;
      }
      break;
  }
  fOldPressed = fPressed;
  return fResult;
}

void  DrawMenu( void )
{
  const GFXfont *font = &FreeMono9pt7b;
  int iMenuItems = ARRAY_SIZE( Menu );
  int iTextMax = 0;
  int iTextSize = 1;
  int iFontWidth;
  int iFontHeight;
  int iXX;
  int iYY;
  tft.setFreeFont( font );
  iFontWidth = font->glyph->xAdvance * iTextSize;
  iFontHeight = font->yAdvance * iTextSize;
  for ( int i = 0; i < iMenuItems; i++ )
  {
    int iLen = strlen( Menu[i].pcszName );
    if ( iLen > iTextMax )
    {
      iTextMax = iLen + 2;
    }
  }
  int iW = iTextMax * iFontWidth + 4 * iFontWidth;
  int iH = iMenuItems * iFontHeight + iMenuItems * FONT_HEIGHT + 2 * FONT_HEIGHT;
  iXX = iScreenWidth / 2 - iW / 2;
  iYY = iScreenHeight / 2 - iH / 2;
  iXX = ( iXX < 0 ? 0 : iXX );
  iYY = ( iYY < 0 ? 0 : iYY );
  tft.fillRect( iXX, iYY, iW, iH, TFT_DARKGREY );
  for ( int i = 0; i < iMenuItems; i++ )
  {
    int iX = iXX + 2 * iFontWidth;
    int iY = iYY + i * iFontHeight + i * FONT_HEIGHT + FONT_HEIGHT;
    iW = iTextMax * iFontWidth;
    iH = iFontHeight + 2;
    Menu[i].btn->initButtonUL( &tft, iX, iY, iW, iH, TFT_WHITE, Menu[i].wColor, TFT_WHITE, (char *)Menu[i].pcszName, iTextSize );
    Menu[i].btn->drawButton();
  }
}

bool  SelctMenu( bool fPressed, uint16_t wTX, uint16_t wTY )
{
  bool fResult = false;
  int iMenuItems = ARRAY_SIZE( Menu );
  int iSelectMenuNo = -1;
  int iBands = -1;
  for ( int i = 0; i < iMenuItems; i++ )
  {
    if ( fPressed && Menu[i].btn->contains( wTX, wTY ) )
    {
      Menu[i].btn->press( true );
    }
    else
    {
      Menu[i].btn->press( false );
    }
  }
  for ( int i = 0; i < iMenuItems; i++ )
  {
    if ( Menu[i].btn->justReleased() )
    {
      Menu[i].btn->drawButton();
    }
    if ( Menu[i].btn->justPressed() )
    {
      Menu[i].btn->drawButton( true );
      iSelectMenuNo = i;
    }
  }
  switch ( iSelectMenuNo )
  {
    case 0:
      fResult = true;
      break;
    case 1:
      fResult = true;
      iDispMode = 0;
      iBands = 8;
      break;
    case 2:
      fResult = true;
      iDispMode = 0;
      iBands = 10;
      break;
    case 3:
      fResult = true;
      iDispMode = 0;
      iBands = 16;
      break;
    case 4:
      fResult = true;
      iDispMode = 0;
      iBands = 24;
      break;
    case 5:
      fResult = true;
      iDispMode = 0;
      iBands = 32;
      break;
    case 6:
      fResult = true;
      iDispMode = 1;
      iDispSpectrumChannel = 0;
      break;
    case 7:
      fResult = true;
      iDispMode = 1;
      iDispSpectrumChannel = 1;
      break;
  }
  if ( iBands != -1 )
  {
    if ( !bandsInit( iBands ) )
    {
      Serial.printf( "bandsInit() not enough memory\n" ); HLT;
    }
  }
  return fResult;
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
  int iBands = i2sDin.iBands;
  int iDispChannels = DISP_CHANNELS;
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
  spr.setTextSize( 1 );
  spr.setTextColor( TFT_WHITE );
  int iGXStep = iScreenWidth / iBands / iDispChannels;
  if ( iGXStep < 2 ) iGXStep = 2;
  int iCheck = iScreenWidth % iBands;
  int iGX = ( iCheck == 0 ? 0 : ( iScreenWidth - ( iBands * iGXStep * iDispChannels + 1 ) ) / 2 );
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
  int iA = iWidth / log10( FFT_HALF_SIZE );
  uint16_t wColor = ( iDispSpectrumChannel == 0 ? TFT_GREEN : TFT_CYAN );
  spr.setTextSize( 1 );
  spr.setTextColor( TFT_WHITE );
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
  for ( int iFreq = 63; iFreq <= ( SAMPLE_RATE / 2 ); iFreq *= 2 )
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
  if ( iGX >= ( iScreenWidth - 1 ) )
  {
    int iA = ( FONT_WIDTH * strlen( szHz ) ) + 2;
    iGX -= iA;
  }
  else
  {
    int iA = ( FONT_WIDTH * strlen( szHz ) ) / 2;
    iGX -= iA;
    iGX += iGXSize / 2;
  }
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
