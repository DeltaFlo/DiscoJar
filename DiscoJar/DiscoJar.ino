// If you want to control the DiscoJar via WIFI using an ESP8266, uncomment this:
#define USE_WIFI

/* 
This is the code for the instructable DiscoJar, done by Florian Link (at) gmx.de

It is based on the following:
 
Led matrix array Spectrum Analyser
22/11/2015 Nick Metcalfe

Built with Teensy 3.2 controller
https://www.pjrc.com/store/teensy32.html
and the Teensy Audio Library
http://www.pjrc.com/teensy/td_libs_Audio.html

and the FastLED library.

Portions of code based on:

Logarithmic band calculations http://code.compartmental.net/2007/03/21/fft-averages/
PlazINT  -  Fast Plasma Generator using Integer Math Only / Edmund "Skorn" Horn March 4,2013

Copyright (c) 2015 Nick Metcalfe  valves@alphalink.com.au
Copyright (c) 2016 Florian Link
All right reserved.

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.
This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.
You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifdef USE_WIFI
#include "WIFIControl.h"
#endif

//----- Global state -----------------------------------------------------------------------
State _myState;

//----- Audio part -----------------------------------------------------------------------
#define analyzer
#ifdef analyzer
#include <analyze_fft1024.h>
#include <input_adc.h>

AudioInputAnalog         adc1(A2);        //xy=144,119
AudioAnalyzeFFT1024      fft1024_1;       //xy=498,64
AudioConnection          patchCord1(adc1, fft1024_1);

const int m_amountOfRows = 255;
const int m_amountOfColumns = 288; //128;  //Number of outputs per row
unsigned int logGen1024[m_amountOfColumns][2] = {0}; //Linear1024 -> Log64 mapping
float m_bandVal[m_amountOfColumns] = {0.0};      //band FP value
const unsigned int logAmpSteps = 256;             //How many steps in the log amp converter
int logAmpOffset[logAmpSteps] = {0};             //Linear64 -> Log16 mapping

unsigned char m_bands[m_amountOfColumns] = {0};  //band pixel height
unsigned char m_peaks[m_amountOfColumns] = {0};  //peak pixel height
float m_peakVal[m_amountOfColumns] = {0.0};      //peak FP value

//------------------------------------------------------------------------------------------------
// Logarithmic band calculations
// http://code.compartmental.net/2007/03/21/fft-averages/

//Changing the gain changes the overall height of the bars in the display 
//by a proportionate amount. Note that increasing the gain for low-level 
//signals also increases background noise to a point where it may start to 
//show along the bottom of the display. m_shift can hide this.

//Shifts the bars down to conceal the zero-point as well as any additional 
//background noise. If random data shows at the very bottom of the display 
//when no input is present, increase this value by steps of 0.1 to just past 
//the point where it disappears. This should be 0.1 in a good quiet design.
const float m_shift = 1.;                       //Shift bars down to mask background noise

//Enable showing the red peak trace. Turn it off for a pure spectrum display.
const bool m_showPeak = false;                    //Show peaks

//How many 20ms cycles the peak line holds on the display before resetting. As the
//peak timer is restarted by peaks being nudged up, this should remain short or 
//the peak display tends to get stuck.
const int m_peakCounter = 5;

//How many pixels of spectrum need to show on the display for the peak line to appear.
//This hides the peak when no input is present in order to blank and save the display.
const int m_peakDisplayThreshold = 12;           //Minimum number of pixels under peak for it to show

//The noise gate mutes the input when the peak is below m_peakDisplayThreshold. This
//can be used to conceal narrow band background noise.
const bool m_noiseGate = true;

const int sampleRate = 16384;
const int timeSize = 1024;    //FFT points
const int bandShift = 2;      //First two bands contain 50hz and display switching noise. Hide them..

//Shape of spread of frequency steps within the passband. Shifts the middle part left
//or right for visual balance. 0.05 for a hard logarithmic response curve to 0.40 for 
//a much more linear curve on two displays.
const float logScale = 0.14;  //Scale the logarithm curve deeper or shallower

int freqToIndex(int freq);

//Calculate a logarithmic set of band ranges
void calcBands(void) {
  int bandOffset;     //Bring us back toward zero for larger values of logScale
  for (int i = 0; i < m_amountOfColumns; i++)
  {
    int lowFreq = (int)((sampleRate/2) / (float)pow(logScale + 1, m_amountOfColumns - i)) - 1;
    int hiFreq = (int)((sampleRate/2) / (float)pow(logScale + 1, m_amountOfColumns - 1 - i)) - 1;
    int lowBound = freqToIndex(lowFreq);
    int hiBound = freqToIndex(hiFreq);
    if (i == 0) bandOffset = lowBound;
    lowBound -= bandOffset;
    hiBound -= bandOffset + 1;
    if (lowBound < i + bandShift) lowBound = i + bandShift;
    if (hiBound < i + bandShift) hiBound = i + bandShift;
    if (lowBound > hiBound) lowBound = hiBound;
    if (i == m_amountOfColumns - 1) hiBound = 511;
    logGen1024[i][0] = lowBound;
    logGen1024[i][1] = hiBound;
#ifdef SERIAL_DEBUG
    Serial.print(i);
    Serial.print(" - Bounds:");
    Serial.print(lowBound);
    Serial.print(", ");
    Serial.println(hiBound);
#endif
  }
}

//Determine the FFT sample bandwidth
float getBandWidth()
{
  return (2.0/(float)timeSize) * (sampleRate / 2.0);
}

//Convert a frequency to a FFT bin index 
int freqToIndex(int freq)
{
  // special case: freq is lower than the bandwidth of spectrum[0]
  if ( freq < getBandWidth()/2 ) return 0;
  // special case: freq is within the bandwidth of spectrum[512]
  if ( freq > sampleRate/2 - getBandWidth()/2 ) return (timeSize / 2) - 1;
  // all other cases
  float fraction = (float)freq/(float) sampleRate;
  int i = (int)(timeSize * fraction);
  return i;
}

//Calculate a logarithmic amplitude lookup table
void calcAmplitudes() {
  for (int i = 0; i < logAmpSteps; i++)
  {  
    float db = 1.0 - ((float) i / (float)logAmpSteps);
    db = (1.0 - (db * db)) * (m_amountOfRows + 1);
    if (db < 0) logAmpOffset[i] = -1;    
    else logAmpOffset[i] = (int)db;
#ifdef SERIAL_DEBUG
    Serial.print(i);
    Serial.print(" - Amp:");
    Serial.println(logAmpOffset[i]);
#endif
  }
}


void updateFFT() {
  static int peakCount = 0;      //Peak delay counter
  int barValue, barPeak;         //current values for bar and peak
  float maxPeak = 0;             //Sum of all peak values in frame
  static bool drawPeak = true;   //Show peak on display
  if (fft1024_1.available())
  {
    for (int band = 0; band < m_amountOfColumns; band++) {
      //Get FFT data
      float fval = fft1024_1.read(logGen1024[band][0], logGen1024[band][1]);
      fval = fval * _myState.gain - m_shift;
      if (fval > logAmpSteps) fval = logAmpSteps;            //don't saturate the band

      //process bands bar value
      if (m_bandVal[band] > 0) m_bandVal[band] -= _myState.decay;   //decay current band
      if ((drawPeak || !m_noiseGate) && fval > m_bandVal[band]) m_bandVal[band] = fval; //set to current value if it's greater
      barValue = (int)m_bandVal[band];                       //reduce to a pixel location
      if (barValue > logAmpSteps - 1) barValue = logAmpSteps - 1; //apply limits
      if (barValue < 0) barValue = 0;
      barValue = logAmpOffset[barValue];

      //process peak bar value
      if (drawPeak || !m_noiseGate) fval = m_bandVal[band] + 0.1; //examine band data transposed slightly higher
      else fval += 0.1;
      if (fval > m_peakVal[band]) {                //if value is greater than stored data
        m_peakVal[band] = fval;                    //update stored data
        peakCount = m_peakCounter;                 //Start the peak display counter
      }
      barPeak = (int)m_peakVal[band];              //extract the pixel location of peak
      if (barPeak > logAmpSteps - 1) barPeak = logAmpSteps - 1; //apply limits
      if (barPeak < 0.0) barPeak = 0.0;
      barPeak = logAmpOffset[barPeak];
      maxPeak += barPeak;                          //sum up all the peaks

      m_bands[band] = barValue;
      m_peaks[band] = barPeak;
    }
      
    //Peak counter timeout
    if (peakCount > 0) {                                      //if the peak conter is active
      if (--peakCount == 0) {                                 //and decrementing it deactivates it
        for (int band = 0; band < m_amountOfColumns; band++) {  //clear the peak values
          m_peakVal[band] = 0;
        }
      }
    }
  }
}

void initAudio()
{
  AudioMemory(12);
  calcBands();
  calcAmplitudes();
}
#endif
// --- End of FFT/AUDIO ----------------------------------------------------------------------------------

#include "FastLED.h"

FASTLED_USING_NAMESPACE

#define LED_TYPE    APA102
#define NUM_LEDS    288

#define ROWS_LEDs 16
#define COLS_LEDs 19

// We have more leds than there really are to avoid out of bounds
// access with x/y adressing.
CRGB leds[ROWS_LEDs * COLS_LEDs];

// precalculated rainbow
CRGB rainbow[NUM_LEDS * 3];

void setup()
{ 
  // tell FastLED about the LED strip configuration
  FastLED.addLeds<LED_TYPE, 11, 13, BGR>(leds, NUM_LEDS);
  FastLED.setCorrection(TypicalLEDStrip);
  // set master brightness control
  FastLED.setBrightness(_myState.brightness);

#ifdef analyzer
  initAudio();
#endif

  for (int i = 0;i<NUM_LEDS * 3;i++) {
    rainbow[i] = CHSV(i & 0xff,255,255);
  }
  for (int i = 0;i<NUM_LEDS;i++) {
    leds[i] = rainbow[i];
  }
  FastLED.show();

#ifdef USE_WIFI
  setupWebServer();
#endif
}

#ifdef USE_WIFI
void serialEvent1()
{
  loopWebServer();
  _myState = getWebServerState();
  FastLED.setBrightness(_myState.brightness);
}
#endif

// address the LEDs by x,y (more or less, since it is a spiral and it has 3 places that have a different distance)
int getPos(int x, int y) {
  int r = x + (y * 19 - (y / 4));
  return r;
}

//PlazINT  -  Fast Plasma Generator using Integer Math Only
//Edmund "Skorn" Horn
//March 4,2013

//Byte val 2PI Cosine Wave, offset by 1 PI 
//supports fast trig calcs and smooth LED fading/pulsing.
uint8_t const cos_wave[256] PROGMEM =  
{0,0,0,0,1,1,1,2,2,3,4,5,6,6,8,9,10,11,12,14,15,17,18,20,22,23,25,27,29,31,33,35,38,40,42,
45,47,49,52,54,57,60,62,65,68,71,73,76,79,82,85,88,91,94,97,100,103,106,109,113,116,119,
122,125,128,131,135,138,141,144,147,150,153,156,159,162,165,168,171,174,177,180,183,186,
189,191,194,197,199,202,204,207,209,212,214,216,218,221,223,225,227,229,231,232,234,236,
238,239,241,242,243,245,246,247,248,249,250,251,252,252,253,253,254,254,255,255,255,255,
255,255,255,255,254,254,253,253,252,252,251,250,249,248,247,246,245,243,242,241,239,238,
236,234,232,231,229,227,225,223,221,218,216,214,212,209,207,204,202,199,197,194,191,189,
186,183,180,177,174,171,168,165,162,159,156,153,150,147,144,141,138,135,131,128,125,122,
119,116,113,109,106,103,100,97,94,91,88,85,82,79,76,73,71,68,65,62,60,57,54,52,49,47,45,
42,40,38,35,33,31,29,27,25,23,22,20,18,17,15,14,12,11,10,9,8,6,6,5,4,3,2,2,1,1,1,0,0,0,0
};


inline uint8_t fastCosineCalc( uint16_t preWrapVal)
{
  uint8_t wrapVal = (preWrapVal % 255);
  if (wrapVal<0) wrapVal=255+wrapVal;
  return (pgm_read_byte_near(cos_wave+wrapVal)); 
}

unsigned long frameCount=25500;  // arbitrary seed to calculate the three time displacement variables t,t2,t3

void doPlasma()
{
  if (_myState.mode != Plasma) {
    updateFFT();
  }
  frameCount++ ; 
  uint16_t t = fastCosineCalc((42 * frameCount)/100);  //time displacement - fiddle with these til it looks good...
  uint16_t t2 = fastCosineCalc((35 * frameCount)/100); 
  uint16_t t3 = fastCosineCalc((38 * frameCount)/100);
  int i = 0;
  for (uint8_t y = 0; y < ROWS_LEDs; y++) {
    int p = getPos(0,y);
    for (uint8_t x = 0; x < COLS_LEDs ; x++) {
      //Calculate 3 seperate plasma waves, one for each color channel
      uint8_t r = fastCosineCalc(((x << 3) + (t >> 1) + fastCosineCalc((t2 + (y << 3)))));
      uint8_t g = fastCosineCalc(((y << 3) + t + fastCosineCalc(((t3 >> 2) + (x << 3)))));
      uint8_t b = fastCosineCalc(((y << 3) + t2 + fastCosineCalc((t + x + (g >> 2)))));
      if (_myState.mode == Plasma) {
        leds[p].red = r;
        leds[p].green = g;
        leds[p].blue = b;
      } else {
        int scale = m_bands[i];
        leds[p].red = (r * scale) >> 8;
        leds[p].green = (g * scale) >> 8;
        leds[p].blue = (b * scale) >> 8;
      }
      p++;
      i++;
    }
  }
}

//-----------------------------------------------------------------------------------------------

int pos = 0;

void doSpectrum()
{
  updateFFT();
  pos--;
  if (pos < 0) {
    pos = 254;
  }
  for (int i = 0;i<m_amountOfColumns;i++)
  {
    leds[i].red = ((int)rainbow[i + pos].red * m_bands[i]) >> 8;
    leds[i].green = ((int)rainbow[i + pos].green * m_bands[i]) >> 8;
    leds[i].blue = ((int)rainbow[i + pos].blue * m_bands[i]) >> 8;
  }
}

//-----------------------------------------------------------------------------------------------

uint8_t gHue = 0; // rotating "base color" used by many of the patterns
void doConfetti() 
{
  // random colored speckles that blink in and fade smoothly
  fadeToBlackBy( leds, NUM_LEDS, 10);
  int pos = random16(NUM_LEDS);
  leds[pos] += CHSV( gHue + random8(64), 200, 255);
  EVERY_N_MILLISECONDS( 20 ) { gHue++; } // slowly cycle the "base color" through the rainbow
}

void doGradient()
{
  float p = 0;
  float a = 255./NUM_LEDS;
  for (int i = 0;i<NUM_LEDS;i++) {
    int factor1 = (int)p;
    int factor2 = (int)(1.-p);

    leds[i].red =     (_myState.color0[0] * factor1 + _myState.color1[0] * factor2) >> 8;
    leds[i].green =   (_myState.color0[1] * factor1 + _myState.color1[1] * factor2) >> 8;
    leds[i].blue =    (_myState.color0[2] * factor1 + _myState.color1[2] * factor2) >> 8;
    p += a;
  }
}

void loop() {
  if (_myState.mode == Plasma || _myState.mode == SpectrumWithPlasma) {
    doPlasma();
  } else if (_myState.mode == Spectrum) {
    doSpectrum();
  } else if (_myState.mode == Confetti) {
    doConfetti();
  } else if (_myState.mode == Gradient) {
    doGradient();
  }
  FastLED.show();  
  delay(10);
}

