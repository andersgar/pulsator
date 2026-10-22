#pragma once
#include <Arduino.h>

struct Settings {
  bool     relayActiveLow;
  bool     dryRun;
  uint16_t buildupS;
  uint16_t afterglowS;
  uint8_t  spurts;
  uint8_t  spurtStrength;   // percent
  uint16_t numLeds;
  uint8_t  brightness;
  uint8_t  hue;             // FastLED hue 0..255
  bool     piezoOn;
  uint16_t piezoHz;
};

extern Settings settings;

void settingsLoad();
void settingsSave();
void settingsResetToDefaults();
