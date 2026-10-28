#include "lights.h"

#include "config.h"

// Kazda klatka jest skladana od zera z trzech warstw:
//   1. bazowa       - swiatla glowne (przod bialy, tyl czerwony)
//   2. efekt        - animacja na diodach bocznych
//   3. kierunkowskaz - zawsze na wierzchu
// Wyzsza warstwa tylko przykrywa nizsza, nigdy jej nie nadpisuje,
// wiec po wylaczeniu kierunkowskazu albo efektu diody wracaja do stanu
// sprzed ich wlaczenia (z uwzglednieniem zmian zrobionych w miedzyczasie).

namespace {

CRGB out[NUM_LEDS];  // klatka wysylana na tasme
CRGB fx[NUM_LEDS];   // bufor efektu (trwaly, potrzebny do smug skanera)

LightState st;
bool changed = true;
uint32_t turnStart = 0;
uint32_t lastFrame = 0;
uint8_t rainbowHue = 0;

const char* const EFFECT_NAMES[] = {"none", "rainbow", "scanner", "breathe", "police"};

template <size_t N>
void fillZone(const int8_t (&zone)[N], const CRGB& c) {
  for (int8_t i : zone) {
    if (i >= 0 && i < NUM_LEDS) out[i] = c;
  }
}

inline void setFx(int8_t i, const CRGB& c) {
  if (i >= 0 && i < NUM_LEDS) fx[i] = c;
}

void clearFx() {
  fill_solid(fx, NUM_LEDS, CRGB::Black);
}

// ---------------- warstwa 1: swiatla glowne ----------------
void drawBase() {
  fill_solid(out, NUM_LEDS, CRGB::Black);
  if (!st.headlights) return;
  fillZone(LEFT_FRONT, COLOR_HEAD_FRONT);
  fillZone(RIGHT_FRONT, COLOR_HEAD_FRONT);
  fillZone(LEFT_REAR, COLOR_HEAD_REAR);
  fillZone(RIGHT_REAR, COLOR_HEAD_REAR);
}

// ---------------- warstwa 2: efekty na bokach ----------------
void runRainbow() {
  rainbowHue += RAINBOW_SPEED;
  for (uint8_t p = 0; p < 6; p++) {
    // barwa przesuwa sie od przodu do tylu po obu bokach
    CRGB c = CHSV(rainbowHue - p * RAINBOW_HUE_STEP, 255, 255);
    setFx(RIGHT_SIDE[p], c);
    setFx(LEFT_SIDE[p], c);
  }
}

void runScanner(uint32_t now) {
  for (uint8_t p = 0; p < 6; p++) {
    if (RIGHT_SIDE[p] >= 0) fx[RIGHT_SIDE[p]].fadeToBlackBy(60);
    if (LEFT_SIDE[p] >= 0) fx[LEFT_SIDE[p]].fadeToBlackBy(60);
  }
  uint8_t t = (now / SCANNER_STEP_MS) % 10;  // 0..9
  uint8_t p = t < 6 ? t : 10 - t;            // 0,1,2,3,4,5,4,3,2,1
  setFx(RIGHT_SIDE[p], SCANNER_COLOR);
  setFx(LEFT_SIDE[p], SCANNER_COLOR);
}

void runBreathe() {
  CRGB c = BREATHE_COLOR;
  c.nscale8_video(beatsin8(BREATHE_BPM, 10, 255));
  for (uint8_t p = 0; p < 6; p++) {
    setFx(RIGHT_SIDE[p], c);
    setFx(LEFT_SIDE[p], c);
  }
}

void runPolice(uint32_t now) {
  // dwa blyski czerwone po prawej, potem dwa niebieskie po lewej
  uint8_t phase = (now / POLICE_STEP_MS) % 8;
  CRGB right = (phase == 0 || phase == 2) ? CRGB::Red : CRGB::Black;
  CRGB left = (phase == 4 || phase == 6) ? CRGB::Blue : CRGB::Black;
  for (uint8_t p = 0; p < 6; p++) {
    setFx(RIGHT_SIDE[p], right);
    setFx(LEFT_SIDE[p], left);
  }
}

void drawEffect(uint32_t now) {
  switch (st.effect) {
    case Effect::Rainbow: runRainbow(); break;
    case Effect::Scanner: runScanner(now); break;
    case Effect::Breathe: runBreathe(); break;
    case Effect::Police:  runPolice(now); break;
    default: return;
  }
  // efekt przykrywa tylko diody boczne, reszta auta dziala niezaleznie
  for (uint8_t p = 0; p < 6; p++) {
    if (RIGHT_SIDE[p] >= 0) out[RIGHT_SIDE[p]] = fx[RIGHT_SIDE[p]];
    if (LEFT_SIDE[p] >= 0) out[LEFT_SIDE[p]] = fx[LEFT_SIDE[p]];
  }
}

// ---------------- warstwa 3: kierunkowskazy ----------------
// Biegnaca pojedyncza dioda: zewnetrzna -> srodkowa -> wewnetrzna -> przerwa.
// Na czas migania strefa nalezy w calosci do kierunkowskazu.
void drawTurnSide(const int8_t (&front)[3], const int8_t (&rear)[3], int8_t step) {
  for (int8_t s = 0; s < 3; s++) {
    CRGB c = (s == step) ? COLOR_TURN : CRGB::Black;
    if (front[s] >= 0 && front[s] < NUM_LEDS) out[front[s]] = c;
    if (rear[s] >= 0 && rear[s] < NUM_LEDS) out[rear[s]] = c;
  }
}

void drawTurn(uint32_t now) {
  bool left = st.hazard || st.turn == Turn::Left;
  bool right = st.hazard || st.turn == Turn::Right;
  if (!left && !right) return;

  const uint32_t period = 3UL * TURN_STEP_MS + TURN_PAUSE_MS;
  uint32_t t = (now - turnStart) % period;
  int8_t step = t < 3UL * TURN_STEP_MS ? t / TURN_STEP_MS : -1;  // -1 = przerwa

  if (left) drawTurnSide(LEFT_FRONT, LEFT_REAR, step);
  if (right) drawTurnSide(RIGHT_FRONT, RIGHT_REAR, step);
}

void markChanged() {
  changed = true;
}

}  // namespace

namespace Lights {

void begin() {
  FastLED.addLeds<WS2812B, LED_PIN, GRB>(out, NUM_LEDS);
  FastLED.setBrightness(LED_BRIGHTNESS);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, MAX_MILLIAMPS);
  fill_solid(out, NUM_LEDS, CRGB::Black);
  clearFx();
  FastLED.show();
}

void update() {
  uint32_t now = millis();
  if (now - lastFrame < FRAME_MS) return;
  lastFrame = now;

  drawBase();
  drawEffect(now);
  drawTurn(now);
  FastLED.show();
}

void toggleHeadlights() {
  st.headlights = !st.headlights;
  markChanged();
}

void toggleTurn(Turn dir) {
  // lewy i prawy wykluczaja sie; ponowne klikniecie tej samej strony wylacza
  st.turn = (st.turn == dir) ? Turn::None : dir;
  if (!st.hazard) turnStart = millis();
  markChanged();
}

void toggleHazard() {
  st.hazard = !st.hazard;
  turnStart = millis();
  markChanged();
}

void setEffect(Effect e) {
  if (e >= Effect::Count) e = Effect::None;
  if (e == st.effect) return;
  st.effect = e;
  clearFx();
  markChanged();
}

void allOff() {
  st = LightState();
  clearFx();
  markChanged();
}

const LightState& state() {
  return st;
}

const CRGB* leds() {
  return out;
}

bool consumeChanged() {
  bool c = changed;
  changed = false;
  return c;
}

const char* effectName(Effect e) {
  uint8_t i = static_cast<uint8_t>(e);
  return i < static_cast<uint8_t>(Effect::Count) ? EFFECT_NAMES[i] : "none";
}

Effect effectFromName(const char* name) {
  for (uint8_t i = 0; i < static_cast<uint8_t>(Effect::Count); i++) {
    if (strcmp(name, EFFECT_NAMES[i]) == 0) return static_cast<Effect>(i);
  }
  return Effect::None;
}

const char* turnName(Turn t) {
  switch (t) {
    case Turn::Left: return "L";
    case Turn::Right: return "R";
    default: return "";
  }
}

}  // namespace Lights
