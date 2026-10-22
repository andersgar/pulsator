// Interface between the state machine / hardware (main.cpp) and the web page (web.cpp).
#pragma once
#include <Arduino.h>

enum class Phase { IDLE, BUILDUP, HOLD, CLIMAX, AFTERGLOW };

// Everything the debug view shows, captured once per loop.
struct Status {
  Phase    phase;
  uint32_t phaseMs;        // time in the current phase
  uint32_t remainingS;     // buildup seconds left (BUILDUP only)
  uint8_t  progressPct;    // 0..100 through the buildup
  uint16_t beatMs;         // current beat interval
  uint8_t  spurtIndex;     // CLIMAX: which spurt (1-based), 0 otherwise
  bool     pumpOn;         // what the state machine wants
  bool     relayPinHigh;   // actual level on PIN_RELAY
  bool     dryRun;
  uint8_t  ledR, ledG, ledB, ledBrightness;   // average strip colour + master brightness
  uint16_t piezoHz;        // 0 = silent
  uint32_t pumpOnTotalMs;  // since boot, for the curious
};

const char* phaseName(Phase p);
Status      pulsatorStatus();

void pulsatorStart(uint32_t buildupSeconds);   // 0 = from settings
void pulsatorClimaxNow();
void pulsatorStop();

// Bench tests, blocking for a moment
void pulsatorTestPump(uint16_t ms);
void pulsatorTestLights();
void pulsatorTestPiezo();

// Up to N evenly spaced samples of the strip as "rrggbb" hex, for the page's strip view.
String ledSamples(uint8_t maxSamples);

// Event log ring buffer, newest last. Lines are "t=12.345 message".
void   logLine(const String& s);
String logTail(uint8_t maxLines);
