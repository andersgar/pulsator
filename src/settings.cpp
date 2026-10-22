#include "settings.h"
#include "config.h"
#include <Preferences.h>

Settings settings;
static Preferences prefs;

static void clampAll() {
  if (settings.buildupS < 5) settings.buildupS = 5;
  if (settings.buildupS > 3600) settings.buildupS = 3600;
  if (settings.afterglowS > 120) settings.afterglowS = 120;
  if (settings.spurts < 3) settings.spurts = 3;
  if (settings.spurts > 12) settings.spurts = 12;
  if (settings.spurtStrength < 20) settings.spurtStrength = 20;
  if (settings.spurtStrength > 200) settings.spurtStrength = 200;
  if (settings.numLeds < 1) settings.numLeds = 1;
  if (settings.numLeds > MAX_LEDS) settings.numLeds = MAX_LEDS;
  if (settings.piezoHz < 200) settings.piezoHz = 200;
  if (settings.piezoHz > 8000) settings.piezoHz = 8000;
}

void settingsResetToDefaults() {
  settings = Settings{DEFAULT_RELAY_ACTIVE_LOW, DEFAULT_DRY_RUN, DEFAULT_BUILDUP_S, DEFAULT_AFTERGLOW_S,
                      DEFAULT_SPURTS, DEFAULT_SPURT_STRENGTH, DEFAULT_NUM_LEDS, DEFAULT_BRIGHTNESS, DEFAULT_HUE,
                      DEFAULT_PIEZO_ON, DEFAULT_PIEZO_HZ};
  settingsSave();
}

void settingsLoad() {
  prefs.begin("pulsator", false);
  bool fresh = !prefs.isKey("bup");
  prefs.end();
  if (fresh) settingsResetToDefaults();
  prefs.begin("pulsator", true);
  settings.relayActiveLow = prefs.getBool  ("rlow",  DEFAULT_RELAY_ACTIVE_LOW);
  settings.dryRun         = prefs.getBool  ("dry",   DEFAULT_DRY_RUN);
  settings.buildupS       = prefs.getUShort("bup",   DEFAULT_BUILDUP_S);
  settings.afterglowS     = prefs.getUShort("aft",   DEFAULT_AFTERGLOW_S);
  settings.spurts         = prefs.getUChar ("spn",   DEFAULT_SPURTS);
  settings.spurtStrength  = prefs.getUChar ("sps",   DEFAULT_SPURT_STRENGTH);
  settings.numLeds        = prefs.getUShort("leds",  DEFAULT_NUM_LEDS);
  settings.brightness     = prefs.getUChar ("bri",   DEFAULT_BRIGHTNESS);
  settings.hue            = prefs.getUChar ("hue",   DEFAULT_HUE);
  settings.piezoOn        = prefs.getBool  ("pz",    DEFAULT_PIEZO_ON);
  settings.piezoHz        = prefs.getUShort("pzhz",  DEFAULT_PIEZO_HZ);
  prefs.end();
  clampAll();
}

void settingsSave() {
  clampAll();
  prefs.begin("pulsator", false);
  prefs.putBool  ("rlow",  settings.relayActiveLow);
  prefs.putBool  ("dry",   settings.dryRun);
  prefs.putUShort("bup",   settings.buildupS);
  prefs.putUShort("aft",   settings.afterglowS);
  prefs.putUChar ("spn",   settings.spurts);
  prefs.putUChar ("sps",   settings.spurtStrength);
  prefs.putUShort("leds",  settings.numLeds);
  prefs.putUChar ("bri",   settings.brightness);
  prefs.putUChar ("hue",   settings.hue);
  prefs.putBool  ("pz",    settings.piezoOn);
  prefs.putUShort("pzhz",  settings.piezoHz);
  prefs.end();
}
