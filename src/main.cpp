// Pulsator firmware
// ESP32 devkit + relay (12 V pump) + WS2812B strip + passive piezo.
// Phone page over the board's own hotspot: countdown -> buildup -> climax.
//
// Phases
//   IDLE       dim slow breathing, pump off
//   BUILDUP    heartbeat that speeds up over the countdown: light swells
//              travelling base -> tip, piezo thump rising in pitch
//   HOLD       1.2 s: lights frozen at full, silence, pump off
//   CLIMAX     N spurts, first long and strong, then shorter with longer gaps.
//              Pump ON during each spurt, white flash + fast wave, piezo chirp
//   AFTERGLOW  warm amber fading out, pump off
//
// The pump is only ever on inside CLIMAX and never longer than PUMP_MAX_ON_MS,
// enforced in one place (pumpApply), whatever else happens.

#include <Arduino.h>
#include <FastLED.h>
#include <esp_arduino_version.h>
#include "config.h"
#include "settings.h"
#include "pulsator.h"
#include "web.h"
#if ESP_ARDUINO_VERSION_MAJOR < 3
#include <esp32-hal-matrix.h>
#include <soc/gpio_sig_map.h>
#endif

// ============================================================================
// Event log
// ============================================================================
static String  logBuf[32];
static uint8_t logHead = 0, logCount = 0;

void logLine(const String& s) {
  String line = "t=" + String(millis() / 1000.0, 3) + " " + s;
  Serial.println(line);
  logBuf[logHead] = line;
  logHead = (logHead + 1) % 32;
  if (logCount < 32) logCount++;
}

String logTail(uint8_t maxLines) {
  if (maxLines > logCount) maxLines = logCount;
  String out;
  for (int i = logCount - maxLines; i < logCount; i++) {
    int idx = (logHead + 32 - logCount + i) % 32;
    out += logBuf[idx] + "\n";
  }
  return out;
}

// ============================================================================
// Pump (relay)
// ============================================================================
static bool     pumpWanted = false, pumpActual = false;
static uint32_t pumpOnSince = 0, pumpOnTotal = 0;

static void pumpWrite(bool on) {
  bool level = settings.relayActiveLow ? !on : on;
  digitalWrite(PIN_RELAY, level ? HIGH : LOW);
  digitalWrite(PIN_BOARD_LED, on ? HIGH : LOW);
}

// The only place the relay pin changes. Applies dry-run and the max-on guard.
static void pumpApply(bool on, Phase phase) {
  if (phase != Phase::CLIMAX) on = false;
  if (on && pumpActual && millis() - pumpOnSince > PUMP_MAX_ON_MS) {
    on = false;
    logLine("pump: max-on guard tripped, forcing off");
  }
  pumpWanted = on;
  bool drive = on && !settings.dryRun;
  if (drive != pumpActual) {
    pumpActual = drive;
    pumpWrite(drive);
    if (drive) pumpOnSince = millis();
    else       pumpOnTotal += millis() - pumpOnSince;
  }
  if (on && settings.dryRun && !pumpActual) { /* shown as "wanted" in the debug view */ }
}

static bool relayPinHigh() { return digitalRead(PIN_RELAY) == HIGH; }

// ============================================================================
// Piezo (LEDC, Arduino core 2.x and 3.x)
// ============================================================================
namespace piezo {
constexpr uint8_t CH = 0;
static uint16_t currentHz = 0;
static uint32_t offAt = 0;

void begin() {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttachChannel(PIN_PIEZO_P, 2000, 10, CH);
  ledcAttachChannel(PIN_PIEZO_N, 2000, 10, CH);
  ledcOutputInvert(PIN_PIEZO_N, true);
  ledcWrite(PIN_PIEZO_P, 0);
#else
  ledcSetup(CH, 2000, 10);
  ledcAttachPin(PIN_PIEZO_P, CH);
  ledcAttachPin(PIN_PIEZO_N, CH);
  pinMatrixOutAttach(PIN_PIEZO_N, LEDC_HS_SIG_OUT0_IDX + CH, true, false);
  ledcWrite(CH, 0);
#endif
}
void tone(uint16_t hz) {
  if (!settings.piezoOn) hz = 0;
  currentHz = hz;
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  if (hz == 0) ledcWrite(PIN_PIEZO_P, 0); else ledcWriteTone(PIN_PIEZO_P, hz);
#else
  if (hz == 0) ledcWrite(CH, 0); else ledcWriteTone(CH, hz);
#endif
}
void off() { tone(0); offAt = 0; }
void beep(uint16_t hz, uint16_t ms) { tone(hz); offAt = millis() + ms; }
void update() { if (offAt && (int32_t)(millis() - offAt) >= 0) off(); }
}  // namespace piezo

// ============================================================================
// Strip
// ============================================================================
static CRGB leds[MAX_LEDS];
static uint16_t numLeds() { return settings.numLeds; }

static void stripBegin() {
  FastLED.addLeds<WS2812B, PIN_LED_DIN, GRB>(leds, MAX_LEDS);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, STRIP_MAX_MA);
  FastLED.setBrightness(settings.brightness);
  FastLED.clear(true);
}

static void stripShow() {
  for (uint16_t i = numLeds(); i < MAX_LEDS; i++) leds[i] = CRGB::Black;
  FastLED.setBrightness(settings.brightness);
  FastLED.show();
}

// A swell that starts at the base (LED 0) and runs to the tip. pos 0..1 is
// the wave front, width in LEDs. Base colour c, highlighted colour h.
static void wave(const CRGB& base, const CRGB& hi, float pos, float width, uint8_t floorLevel) {
  uint16_t n = numLeds();
  float front = pos * (n + width);
  for (uint16_t i = 0; i < n; i++) {
    float d = front - i;                   // positive once the front has passed
    uint8_t w = 0;
    if (d >= 0 && d < width) w = 255 - (uint8_t)(255.0f * d / width);
    CRGB c = base; c.nscale8(floorLevel);
    CRGB g = hi;   g.nscale8(w);
    leds[i] = c + g;
  }
}

static void fillScaled(const CRGB& c, uint8_t level) {
  CRGB s = c; s.nscale8(level);
  fill_solid(leds, numLeds(), s);
}

// ============================================================================
// State machine
// ============================================================================
static Phase    phase = Phase::IDLE;
static uint32_t phaseSince = 0;
static uint32_t buildupMs = 60000;
static uint32_t lastBeatAt = 0;
static uint16_t beatMs = 1500;
static uint8_t  spurtIndex = 0;
static uint16_t spurtHz = 0;

// Climax plan: on/off pairs in ms, scaled by spurt strength / count.
struct Spurt { uint16_t onMs, gapMs; };
static Spurt plan[12];
static uint8_t planLen = 0;

static void buildPlan() {
  planLen = settings.spurts;
  float k = settings.spurtStrength / 100.0f;
  for (uint8_t i = 0; i < planLen; i++) {
    float t = (float)i / (planLen - 1);              // 0 first .. 1 last
    plan[i].onMs  = (uint16_t)((650 - 430 * t) * k);  // 650 -> 220 ms
    plan[i].gapMs = (uint16_t)(220 + 620 * t * t);    // 220 -> 840 ms, growing faster at the end
    if (plan[i].onMs > PUMP_MAX_ON_MS) plan[i].onMs = PUMP_MAX_ON_MS;
  }
}

const char* phaseName(Phase p) {
  switch (p) {
    case Phase::IDLE: return "IDLE";
    case Phase::BUILDUP: return "BUILDUP";
    case Phase::HOLD: return "HOLD";
    case Phase::CLIMAX: return "CLIMAX";
    default: return "AFTERGLOW";
  }
}

static void enter(Phase p) {
  phase = p;
  phaseSince = millis();
  piezo::off();
  spurtIndex = 0;
  spurtHz = 0;
  pumpApply(false, p);
  switch (p) {
    case Phase::IDLE:      logLine("phase IDLE"); break;
    case Phase::BUILDUP:   lastBeatAt = 0; logLine("phase BUILDUP " + String(buildupMs / 1000) + " s"); break;
    case Phase::HOLD:      logLine("phase HOLD"); break;
    case Phase::CLIMAX:    buildPlan(); logLine("phase CLIMAX, " + String(planLen) + " spurts"); break;
    case Phase::AFTERGLOW: logLine("phase AFTERGLOW"); break;
  }
}

static uint32_t elapsed() { int32_t d = (int32_t)(millis() - phaseSince); return d < 0 ? 0 : d; }

// ---- IDLE -------------------------------------------------------------------
static void runIdle() {
  uint8_t b = beatsin8(8, 6, 28);
  fillScaled(CRGB(255, 20, 60), b);
  stripShow();
}

// ---- BUILDUP ----------------------------------------------------------------
static float progress() {
  float p = (float)elapsed() / buildupMs;
  return p > 1 ? 1 : p;
}

static void runBuildup(uint32_t now) {
  float p = progress();
  if (p >= 1) { enter(Phase::HOLD); return; }

  // Beat interval eases from slow to fast; the last stretch is frantic.
  float e = powf(p, 1.6f);
  beatMs = (uint16_t)(1500 - (1500 - 260) * e);

  if (now - lastBeatAt >= beatMs) {
    lastBeatAt = now;
    // Thump: pitch rises with progress, short.
    piezo::beep((uint16_t)(settings.piezoHz * (0.35f + 0.65f * p)), p < 0.7f ? 45 : 35);
  }
  uint32_t sinceBeat = now - lastBeatAt;

  // Light: a swell travelling base -> tip within each beat, plus a floor that rises with progress.
  float wavePos = (float)sinceBeat / beatMs;
  if (wavePos > 1) wavePos = 1;
  uint8_t floorLevel = 10 + (uint8_t)(70 * p);
  CRGB base = CRGB(255, 30, 70);                                   // deep pink
  CRGB hi   = blend(CRGB(255, 60, 90), CRGB(255, 200, 220), (uint8_t)(255 * p));  // towards hot white at the end
  wave(base, hi, wavePos, 6 + 10 * p, floorLevel);
  // "lub-dub": a second, smaller swell early in the countdown
  if (p < 0.6f && sinceBeat > beatMs * 0.3f && sinceBeat < beatMs * 0.3f + 120) {
    for (uint16_t i = 0; i < numLeds(); i++) leds[i] += CRGB(40, 5, 10);
  }
  stripShow();
}

// ---- HOLD ---------------------------------------------------------------------
static void runHold() {
  fillScaled(CRGB(255, 120, 160), 255);
  stripShow();
  if (elapsed() >= 1200) enter(Phase::CLIMAX);
}

// ---- CLIMAX -------------------------------------------------------------------
static void runClimax() {
  uint32_t t = elapsed();
  // Walk the plan to find which spurt (if any) we are in.
  uint32_t cursor = 0;
  bool on = false; uint8_t idx = 0; uint32_t intoSpurt = 0; uint16_t spurtLen = 1;
  for (uint8_t i = 0; i < planLen; i++) {
    if (t < cursor + plan[i].onMs) { on = true; idx = i + 1; intoSpurt = t - cursor; spurtLen = plan[i].onMs; break; }
    cursor += plan[i].onMs;
    if (t < cursor + plan[i].gapMs) { on = false; idx = i + 1; break; }
    cursor += plan[i].gapMs;
    if (i == planLen - 1) { enter(Phase::AFTERGLOW); return; }
  }
  if (idx != spurtIndex) {
    spurtIndex = idx;
    if (on) logLine("spurt " + String(idx) + "/" + String(planLen) + " pump ON " + String(spurtLen) + " ms");
  }
  pumpApply(on, Phase::CLIMAX);

  if (on) {
    // Chirp rising through the spurt, strip: white flash fading into a fast wave.
    spurtHz = (uint16_t)(settings.piezoHz * (0.6f + 0.9f * intoSpurt / spurtLen));
    piezo::tone(spurtHz);
    float pos = (float)intoSpurt / spurtLen;
    uint8_t flash = intoSpurt < 60 ? 255 : 255 - (uint8_t)(200.0f * (intoSpurt - 60) / spurtLen);
    wave(CRGB(255, 200, 220), CRGB::White, pos, 12, flash);
  } else {
    spurtHz = 0;
    piezo::off();
    fillScaled(CRGB(255, 40, 80), 60);
  }
  stripShow();
}

// ---- AFTERGLOW ----------------------------------------------------------------
static void runAfterglow() {
  uint32_t total = settings.afterglowS * 1000UL;
  uint32_t t = elapsed();
  if (t >= total) { enter(Phase::IDLE); return; }
  uint8_t fade = 255 - (uint8_t)(255UL * t / total);
  uint8_t breath = beatsin8(10, 120, 255);
  CRGB c = CRGB(255, 110, 30); c.nscale8(scale8(fade, breath));
  fill_solid(leds, numLeds(), c);
  stripShow();
}

// ============================================================================
// Public API
// ============================================================================
void pulsatorStart(uint32_t s) {
  buildupMs = (s ? s : settings.buildupS) * 1000UL;
  enter(Phase::BUILDUP);
}
void pulsatorClimaxNow() { enter(Phase::CLIMAX); }
void pulsatorStop() {
  logLine("STOP");
  pumpApply(false, Phase::IDLE);
  enter(Phase::IDLE);
}

Status pulsatorStatus() {
  Status s;
  s.phase = phase;
  s.phaseMs = elapsed();
  s.remainingS = phase == Phase::BUILDUP ? (buildupMs - min(elapsed(), buildupMs) + 999) / 1000 : 0;
  s.progressPct = phase == Phase::BUILDUP ? (uint8_t)(progress() * 100) : (phase == Phase::IDLE ? 0 : 100);
  s.beatMs = phase == Phase::BUILDUP ? beatMs : 0;
  s.spurtIndex = phase == Phase::CLIMAX ? spurtIndex : 0;
  s.pumpOn = pumpWanted;
  s.relayPinHigh = relayPinHigh();
  s.dryRun = settings.dryRun;
  uint32_t r = 0, g = 0, b = 0; uint16_t n = numLeds();
  for (uint16_t i = 0; i < n; i++) { r += leds[i].r; g += leds[i].g; b += leds[i].b; }
  s.ledR = n ? r / n : 0; s.ledG = n ? g / n : 0; s.ledB = n ? b / n : 0;
  s.ledBrightness = settings.brightness;
  s.piezoHz = piezo::currentHz;
  s.pumpOnTotalMs = pumpOnTotal + (pumpActual ? millis() - pumpOnSince : 0);
  return s;
}

// ---- bench tests ---------------------------------------------------------------
void pulsatorTestPump(uint16_t ms) {
  if (ms > PUMP_MAX_ON_MS) ms = PUMP_MAX_ON_MS;
  logLine("test: pump " + String(ms) + " ms" + (settings.dryRun ? " (dry run, relay stays off)" : ""));
  Phase keep = phase;
  phase = Phase::CLIMAX;           // the guard only allows the pump in CLIMAX
  pumpApply(true, Phase::CLIMAX);
  delay(ms);
  pumpApply(false, Phase::CLIMAX);
  phase = keep;
}
void pulsatorTestLights() {
  logLine("test: lights");
  for (uint16_t i = 0; i < numLeds(); i++) { fill_solid(leds, numLeds(), CRGB::Black); leds[i] = CRGB::White; stripShow(); delay(12); }
  for (int k = 0; k < 2; k++) { fillScaled(CRGB(255, 30, 70), 255); stripShow(); delay(250); fillScaled(CRGB::Black, 0); stripShow(); delay(200); }
}
void pulsatorTestPiezo() {
  logLine("test: piezo");
  for (uint16_t f = 400; f <= 3000; f += 100) { piezo::tone(f); delay(40); }
  piezo::off();
}

// ============================================================================
// Serial console
// ============================================================================
static void printHelp() {
  Serial.println(
    "Commands:\n"
    "  h          help\n"
    "  s [secs]   start the countdown (default from settings)\n"
    "  c          climax now\n"
    "  x          stop\n"
    "  p [ms]     pump test pulse (default 300 ms)\n"
    "  l          lights test\n"
    "  t          piezo sweep 400-3000 Hz, note the loudest and set it on the page\n"
    "  d          dump status\n"
    "  D          toggle dry run (relay never energised)\n"
    "  F          reset settings to config.h defaults");
}

static void dumpStatus() {
  Status s = pulsatorStatus();
  Serial.printf("phase=%s t=%lu ms remaining=%lu s progress=%u%% beat=%u ms spurt=%u pump=%s relayPin=%s dry=%s led=(%u,%u,%u)@%u piezo=%u Hz pumpTotal=%lu ms leds=%u\n",
    phaseName(s.phase), (unsigned long)s.phaseMs, (unsigned long)s.remainingS, s.progressPct, s.beatMs, s.spurtIndex,
    s.pumpOn ? "ON" : "off", s.relayPinHigh ? "HIGH" : "LOW", s.dryRun ? "yes" : "no",
    s.ledR, s.ledG, s.ledB, s.ledBrightness, s.piezoHz, (unsigned long)s.pumpOnTotalMs, settings.numLeds);
}

static void handleLine(char* line) {
  while (*line == ' ') line++;
  char cmd = line[0];
  long arg = line[1] ? atol(line + 1) : -1;
  switch (cmd) {
    case 'h': case '?': printHelp(); break;
    case 's': pulsatorStart(arg > 0 ? arg : 0); break;
    case 'c': pulsatorClimaxNow(); break;
    case 'x': pulsatorStop(); break;
    case 'p': pulsatorTestPump(arg > 0 ? arg : 300); break;
    case 'l': pulsatorTestLights(); break;
    case 't': pulsatorTestPiezo(); break;
    case 'd': dumpStatus(); break;
    case 'D': settings.dryRun = !settings.dryRun; settingsSave(); logLine(String("dry run ") + (settings.dryRun ? "ON" : "OFF")); break;
    case 'F': settingsResetToDefaults(); logLine("settings reset to defaults, rebooting"); delay(200); ESP.restart(); break;
    case 0: break;
    default: Serial.printf("unknown command '%c'\n", cmd); printHelp();
  }
}

static void pollSerial() {
  static char buf[32]; static uint8_t len = 0;
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') { buf[len] = 0; if (len) handleLine(buf); len = 0; }
    else if (len < sizeof(buf) - 1) buf[len++] = c;
  }
}

// ============================================================================
void setup() {
  // Relay off before anything else, so a reboot never leaves the pump running.
  settingsLoad();
  pinMode(PIN_RELAY, OUTPUT);
  pinMode(PIN_BOARD_LED, OUTPUT);
  pumpWrite(false);

  Serial.begin(115200);
  piezo::begin();
  stripBegin();
  webBegin();
  logLine(String("Pulsator ready. relay=") + (settings.relayActiveLow ? "active-low" : "active-high") +
          " dryRun=" + (settings.dryRun ? "yes" : "no") + " leds=" + settings.numLeds + ". 'h' for commands.");
  enter(Phase::IDLE);
}

void loop() {
  pollSerial();
  piezo::update();
  webLoop();
  uint32_t now = millis();
  switch (phase) {
    case Phase::IDLE:      runIdle(); break;
    case Phase::BUILDUP:   runBuildup(now); break;
    case Phase::HOLD:      runHold(); break;
    case Phase::CLIMAX:    runClimax(); break;
    case Phase::AFTERGLOW: runAfterglow(); break;
  }
  // Belt and braces: outside CLIMAX the pump is off, every loop.
  if (phase != Phase::CLIMAX && (pumpActual || pumpWanted)) pumpApply(false, phase);
  delay(4);
}
