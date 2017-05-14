#include <Arduino.h>

// Define this to get debug output to USB Serial:
//#define PRINT_DEBUG

#define Spectrum 0
#define SpectrumWithPlasma 1
#define Plasma 2
#define Confetti 3
#define Gradient 4

// Struct that controls the state of the lamp.
// If you don't use the WiFi remote control,
// change the state here and recompile:
struct State {
  State() {
    mode = SpectrumWithPlasma;  
    brightness = 32;
    color0[0]=255;
    color0[1]=0;
    color0[2]=0;
    color1[0]=0;
    color1[1]=255;
    color1[2]=0;
    decay = 0.4;
    gain = 340.;
  }
  byte mode;
  byte brightness;
  byte color0[3];
  byte color1[3];
  byte param0; // not yet used
  byte param1; // not yet used
  float decay;
  float gain;
};

// Starts the web server
void setupWebServer();

// Handles GET/POST requests
void loopWebServer();

// Get the current server state
State& getWebServerState();

