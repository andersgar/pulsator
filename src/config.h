// Compile-time defaults. Most of these are also settings on the web page and
// are then stored in flash; config.h only matters on first boot or after a
// settings reset (serial command F).
#pragma once

// ---- Hotspot --------------------------------------------------------------
#define AP_SSID   "Pulsator"
#define AP_PASS   "pulsator1234"      // min 8 characters

// ---- Pins -----------------------------------------------------------------
#define PIN_RELAY      25   // relay module IN
#define PIN_LED_DIN    13   // -> 330 ohm -> WS2812B DIN
#define PIN_PIEZO_P    27   // piezo terminal 1
#define PIN_PIEZO_N    26   // piezo terminal 2 (driven in antiphase for volume)
#define PIN_BOARD_LED   2   // devkit's blue LED, mirrors the pump so you can see it without hardware

// ---- Defaults (editable on the page) ----------------------------------------
#define DEFAULT_RELAY_ACTIVE_LOW  true   // most relay modules: IN low = relay on. Flip if it is inverted.
#define DEFAULT_DRY_RUN           false  // true = never energise the relay; everything else runs. Good for the bench.
#define DEFAULT_BUILDUP_S         60     // countdown length
#define DEFAULT_AFTERGLOW_S       10
#define DEFAULT_SPURTS            7      // 3..12
#define DEFAULT_SPURT_STRENGTH    100    // percent: scales how long each spurt keeps the pump on
#define DEFAULT_NUM_LEDS          60     // up to MAX_LEDS
#define DEFAULT_BRIGHTNESS        200    // 0..255
#define DEFAULT_HUE               96     // FastLED hue 0..255: 0 red, 32 orange, 64 yellow, 96 green, 160 blue, 224 pink
#define DEFAULT_PIEZO_ON          true
#define DEFAULT_PIEZO_HZ          2800   // base tone, use the serial sweep to find the loud spot

#define MAX_LEDS                  300
#define STRIP_MAX_MA              1500   // FastLED power cap at 5 V

// Safety: the pump is never on longer than this in one go, whatever the state machine says.
#define PUMP_MAX_ON_MS            3000
