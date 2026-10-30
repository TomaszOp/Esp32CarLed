#include "lights.h"

#include "config.h"

// Kazda klatka jest skladana od zera z trzech warstw:
//   1. bazowa       - swiatla glowne (przod bialy, tyl czerwony)
//   2. efekt        - animacja na diodach bocznych albo na calym aucie
//   3. kierunkowskaz - zawsze na wierzchu
// Diody kabiny nie naleza do zadnej warstwy, rysuje je osobno drawCabin().
// Wyzsza warstwa tylko przykrywa nizsza, nigdy jej nie nadpisuje,
// wiec po wylaczeniu kierunkowskazu albo efektu diody wracaja do stanu
// sprzed ich wlaczenia (z uwzglednieniem zmian zrobionych w miedzyczasie).

namespace {

CRGB out[NUM_LEDS];  // klatka wysylana na tasme
CRGB fx[NUM_LEDS];   // bufor efektu (trwaly, potrzebny do smug skanera)

LightState st;
bool changed = true;
uint32_t turnStart = 0;
uint32_t headStart = 0;
uint32_t lastFrame = 0;

// Czas animacji efektow i kabiny plynie wg ustawionej predkosci
// (w setnych ms, zeby nie gubic ulamkow przy predkosci != 100%).
uint64_t fxClock = 0;
uint64_t cabinClock = 0;
uint32_t fxTime = 0;      // ms czasu efektow
uint32_t cabinTime = 0;   // ms czasu kabiny
uint32_t cabinStart = 0;  // cabinTime w chwili wlaczenia trybu
uint8_t fxTicks = 0;      // klatki czasu efektow od poprzedniej klatki (Ogien, Iskry)

bool testOn = false;
uint32_t testMask = 0;
CRGB testColor = CRGB::Black;

static_assert(NUM_LEDS <= 32, "maska testu diod miesci 32 diody");

uint32_t fxStart = 0;          // fxTime w chwili wlaczenia efektu
uint16_t startHoldMs = 0;      // efekt Start: losowe przytrzymanie czerwonych
CRGB neon = NEON_COLOR;
uint8_t heat[2][8];            // efekt Ogien: temperatura rzedow, [strona][0 = tyl]

const char* const EFFECT_NAMES[] = {"none",  "rainbow",  "scanner", "breathe", "police",
                                    "comet", "fire",     "sparkle", "revs",    "tricolor",
                                    "start", "beacon",   "strobe",  "neon"};
const char* const CABIN_MODE_NAMES[] = {"warm", "ambient", "breathe", "rainbow", "race"};
// true = efekt przykrywa wszystkie diody, false = tylko boki
const bool WHOLE_CAR[] = {false, false, false, false, false,
                          true,  true,  true,  true,  true,
                          true,  true,  true,  true};
static_assert(sizeof(EFFECT_NAMES) / sizeof(EFFECT_NAMES[0]) ==
                  static_cast<size_t>(Effect::Count), "EFFECT_NAMES");
static_assert(sizeof(WHOLE_CAR) / sizeof(WHOLE_CAR[0]) ==
                  static_cast<size_t>(Effect::Count), "WHOLE_CAR");

// Geometria budowana w begin() z tablic z config.h.
// Rzedy od przodu do tylu: 0 = przod, 1-6 = boki (p = 0-5), 7 = tyl.
// Strona 0 = lewa, 1 = prawa; -1 = brak diody.
constexpr uint8_t ROWS = 8;
constexpr uint8_t ROW_LEDS = 3;
int8_t rows[2][ROWS][ROW_LEDS];
// Pierscien wokol auta zgodnie z ruchem wskazowek zegara (patrzac z gory),
// od lewej przedniej zewnetrznej.
int8_t ring[NUM_LEDS];
uint8_t ringLen = 0;

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

// ---------------- geometria ----------------
template <size_t N>
void setRow(int8_t (&row)[ROW_LEDS], const int8_t (&zone)[N]) {
  static_assert(N <= ROW_LEDS, "za duzo diod w rzedzie");
  for (uint8_t k = 0; k < N; k++) {
    row[k] = (zone[k] >= 0 && zone[k] < NUM_LEDS) ? zone[k] : -1;
  }
}

void addRing(int8_t i) {
  if (i >= 0 && i < NUM_LEDS && ringLen < NUM_LEDS) ring[ringLen++] = i;
}

void buildGeometry() {
  memset(rows, -1, sizeof(rows));
  setRow(rows[0][0], LEFT_FRONT);
  setRow(rows[1][0], RIGHT_FRONT);
  for (uint8_t p = 0; p < 6; p++) {
    rows[0][p + 1][0] = (LEFT_SIDE[p] < NUM_LEDS) ? LEFT_SIDE[p] : -1;
    rows[1][p + 1][0] = (RIGHT_SIDE[p] < NUM_LEDS) ? RIGHT_SIDE[p] : -1;
  }
  setRow(rows[0][7], LEFT_REAR);
  setRow(rows[1][7], RIGHT_REAR);

  // tablice przod/tyl sa {zewnetrzna, srodkowa, wewnetrzna}
  ringLen = 0;
  for (int8_t k = 0; k < 3; k++) addRing(LEFT_FRONT[k]);
  for (int8_t k = 2; k >= 0; k--) addRing(RIGHT_FRONT[k]);
  for (int8_t k = 0; k < 6; k++) addRing(RIGHT_SIDE[k]);
  for (int8_t k = 0; k < 3; k++) addRing(RIGHT_REAR[k]);
  for (int8_t k = 2; k >= 0; k--) addRing(LEFT_REAR[k]);
  for (int8_t k = 5; k >= 0; k--) addRing(LEFT_SIDE[k]);
}

// Kolor calego rzedu; orRow bierze jasniejszy z kolorow (dla smug).
void fillRow(uint8_t side, uint8_t r, const CRGB& c) {
  for (int8_t i : rows[side][r]) {
    if (i >= 0) fx[i] = c;
  }
}

void fillRowBoth(uint8_t r, const CRGB& c) {
  fillRow(0, r, c);
  fillRow(1, r, c);
}

void orRowBoth(uint8_t r, const CRGB& c) {
  for (uint8_t side = 0; side < 2; side++) {
    for (int8_t i : rows[side][r]) {
      if (i >= 0) fx[i] |= c;
    }
  }
}

// Odpowiednik beatsin8 dla czasu efektow: sinus o bpm uderzeniach na minute.
uint8_t sinAt(uint32_t ms, uint8_t bpm, uint8_t lo, uint8_t hi) {
  uint8_t phase = (uint64_t)ms * bpm * 256 / 60000;
  return lo + scale8(sin8(phase), hi - lo);
}

// ---------------- warstwa 1: swiatla glowne ----------------
// Po wlaczeniu: diody zapalaja sie po kolei jak kierunkowskaz, ale zostaja,
// potem blyskaja jasniej i wracaja do normalnej jasnosci HEAD_LEVEL.
void drawHeadZone(const int8_t (&zone)[3], CRGB c, uint8_t lit, uint8_t level) {
  c.nscale8_video(level);
  for (uint8_t s = 0; s < lit; s++) {
    if (zone[s] >= 0 && zone[s] < NUM_LEDS) out[zone[s]] = c;
  }
}

void drawBase(uint32_t now) {
  fill_solid(out, NUM_LEDS, CRGB::Black);
  if (!st.headlights) return;

  const uint32_t lighting = 3UL * HEAD_ON_STEP_MS;
  uint32_t t = now - headStart;
  uint8_t lit = 3;
  uint8_t level = HEAD_LEVEL;
  if (t < lighting) {
    lit = t / HEAD_ON_STEP_MS + 1;
  } else {
    uint32_t f = (t - lighting) / HEAD_FLASH_MS;  // parzyste = blysk, nieparzyste = przerwa
    if (f < 2UL * HEAD_FLASH_COUNT && f % 2 == 0) level = 255;
  }
  drawHeadZone(LEFT_FRONT, COLOR_HEAD_FRONT, lit, level);
  drawHeadZone(RIGHT_FRONT, COLOR_HEAD_FRONT, lit, level);
  drawHeadZone(LEFT_REAR, COLOR_HEAD_REAR, lit, level);
  drawHeadZone(RIGHT_REAR, COLOR_HEAD_REAR, lit, level);
}

// ---------------- warstwa 2: efekty na bokach ----------------
// now = czas efektow (fxTime), plynie wg ustawionej predkosci
void runRainbow(uint32_t now) {
  uint8_t rainbowHue = now / FRAME_MS * RAINBOW_SPEED;
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

void runBreathe(uint32_t now) {
  CRGB c = BREATHE_COLOR;
  c.nscale8_video(sinAt(now, BREATHE_BPM, 10, 255));
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

// ---------------- warstwa 2: efekty na calym aucie ----------------
// t = czas od wlaczenia efektu (ms czasu efektow)

// Jasna glowa biegnie rzedami od przodu do tylu, za nia gasnacy ogon.
void runComet(uint32_t t) {
  fadeToBlackBy(fx, NUM_LEDS, COMET_FADE);
  const uint32_t run = ROWS * COMET_STEP_MS;
  uint32_t u = t % (run + COMET_PAUSE_MS);
  if (u >= run) return;
  uint16_t pos = u * 256 / COMET_STEP_MS;  // pozycja w 1/256 rzedu
  uint8_t r = pos >> 8;
  uint8_t f = pos & 0xFF;
  // glowa rozlozona miedzy dwa rzedy, zeby ruch byl plynny
  orRowBoth(r, CRGB(COMET_COLOR).nscale8_video(255 - f));
  if (r + 1 < ROWS) orRowBoth(r + 1, CRGB(COMET_COLOR).nscale8_video(f));
}

// Fire2012 osobno dla kazdej strony: zrodlo ciepla z tylu, plomien idzie do przodu.
void runFire() {
  for (uint8_t side = 0; side < 2; side++) {
    uint8_t* h = heat[side];
    for (uint8_t i = 0; i < ROWS; i++) h[i] = qsub8(h[i], random8(0, FIRE_COOLING));
    for (uint8_t i = ROWS - 1; i >= 2; i--) h[i] = (h[i - 1] + 2 * h[i - 2]) / 3;
    if (random8() < FIRE_SPARKING) {
      uint8_t y = random8(2);
      h[y] = qadd8(h[y], random8(160, 255));
    }
    for (uint8_t i = 0; i < ROWS; i++) fillRow(side, ROWS - 1 - i, HeatColor(h[i]));
  }
}

// Ogien i Iskry licza sie klatkami: przy innej predkosci krok wykonuje sie
// tyle razy, ile klatek czasu efektow minelo (przy wolnej czasem 0).
void runFireTicks() {
  for (uint8_t k = 0; k < fxTicks; k++) runFire();
}

// Co klatke losowa dioda dostaje losowy kolor, calosc powoli gasnie.
void runSparkle() {
  if (ringLen == 0) return;
  for (uint8_t k = 0; k < fxTicks; k++) {
    fadeToBlackBy(fx, NUM_LEDS, SPARKLE_FADE);
    fx[ring[random8(ringLen)]] = CHSV(random8(), 200, 255);  // bez diod kabiny
  }
}

// Obrotomierz: pasek od tylu do przodu, zielony -> zolty -> czerwony,
// blysk i spadek do 1/3 przy zmianie biegu, po ostatnim biegu od zera.
void runRevs(uint32_t t) {
  const uint32_t gear = REVS_RISE_MS + REVS_FLASH_MS;
  uint32_t u = t % (REVS_GEARS * gear + REVS_IDLE_MS);
  clearFx();
  if (u >= REVS_GEARS * gear) return;  // przerwa na zerze
  uint8_t g = u / gear;
  u %= gear;
  if (u >= REVS_RISE_MS) {
    fill_solid(fx, NUM_LEDS, REVS_FLASH_COLOR);
    return;
  }
  const uint32_t full = ROWS * 256;
  uint32_t from = g == 0 ? 0 : full / 3;
  uint32_t level = from + (full - from) * u / REVS_RISE_MS;  // w 1/256 rzedu
  for (uint8_t k = 0; k < ROWS; k++) {  // k = 0 to tyl
    if (level <= k * 256UL) break;
    uint32_t v = level - k * 256UL;
    uint8_t r = ROWS - 1 - k;
    CRGB c = CHSV(96 * r / (ROWS - 1), 255, 255);  // przod czerwony, tyl zielony
    fillRowBoth(r, c.nscale8_video(v > 255 ? 255 : v));
  }
}

// Pasy niebieski-bialy-czerwony plynnie przesuwaja sie od przodu do tylu.
void runTricolor(uint32_t t) {
  static const CRGB colors[3] = {CRGB(0, 40, 255), CRGB(255, 255, 255), CRGB(255, 0, 0)};
  const int32_t band = TRICOLOR_BAND * 256;
  const int32_t period = 3 * band;
  int32_t offset = (t % (3UL * TRICOLOR_BAND * TRICOLOR_STEP_MS)) * 256 / TRICOLOR_STEP_MS;
  for (uint8_t r = 0; r < ROWS; r++) {
    int32_t v = ((r * 256 - offset) % period + period) % period;
    uint8_t b = v / band;
    int32_t f = v % band;
    // pas jednolity, ostatni rzad pasa przechodzi w kolor nastepnego
    int32_t edge = band - 256;
    uint8_t mix = f > edge ? f - edge : 0;
    fillRowBoth(r, blend(colors[b], colors[(b + 1) % 3], mix));
  }
}

// Swiatla startowe: 5 czerwonych krokow na bokach od tylu, losowe przytrzymanie,
// zgaszenie, zielony blysk, potem tecza wokol auta do wylaczenia efektu.
void runStart(uint32_t t) {
  clearFx();
  if (t < START_DARK_MS) return;
  t -= START_DARK_MS;
  if (t < 5UL * START_STEP_MS + startHoldMs) {
    uint8_t steps = t / START_STEP_MS + 1;
    if (steps > 5) steps = 5;
    // kroki 1-4: po jednej pozycji boku od tylu, krok 5: dwie przednie
    uint8_t lit = steps < 5 ? steps : 6;
    for (uint8_t k = 0; k < lit; k++) fillRowBoth(6 - k, CRGB::Red);
    return;
  }
  t -= 5UL * START_STEP_MS + startHoldMs;
  if (t < START_OUT_MS) return;
  t -= START_OUT_MS;
  if (t < START_GREEN_MS) {
    fill_solid(fx, NUM_LEDS, CRGB::Green);
    return;
  }
  uint8_t rainbowHue = t / FRAME_MS * START_RAINBOW_SPEED;
  for (uint8_t k = 0; k < ringLen; k++) {
    fx[ring[k]] = CHSV(rainbowHue + k * 256 / ringLen, 255, 255);
  }
}

// Pomaranczowy punkt z miekkim spadkiem obiega pierscien.
void runBeacon(uint32_t t) {
  if (ringLen == 0) return;
  const int32_t len = ringLen * 256;
  int32_t pos = (t % BEACON_PERIOD_MS) * len / BEACON_PERIOD_MS;
  const int32_t width = BEACON_WIDTH * 256;
  for (uint8_t k = 0; k < ringLen; k++) {
    int32_t d = abs(k * 256 - pos);
    if (d > len - d) d = len - d;
    uint8_t v = d < width ? 255 - d * 255 / width : 0;
    fx[ring[k]] = CRGB(BEACON_COLOR).nscale8_video(scale8(v, v));
  }
}

// Dwa krotkie biale blyski na wszystkich diodach, potem pauza.
void runStrobe(uint32_t t) {
  const uint32_t pair = STROBE_FLASH_MS + STROBE_GAP_MS;
  uint32_t u = t % (2 * pair + STROBE_PAUSE_MS);
  bool on = u < 2 * pair && u % pair < STROBE_FLASH_MS;
  fill_solid(fx, NUM_LEDS, on ? CRGB::White : CRGB::Black);
}

void runNeon() {
  fill_solid(fx, NUM_LEDS, neon);
}

// now = czas efektow (fxTime), plynie wg ustawionej predkosci
void drawEffect(uint32_t now) {
  uint32_t t = now - fxStart;
  switch (st.effect) {
    case Effect::Rainbow:  runRainbow(now); break;
    case Effect::Scanner:  runScanner(now); break;
    case Effect::Breathe:  runBreathe(now); break;
    case Effect::Police:   runPolice(now); break;
    case Effect::Comet:    runComet(t); break;
    case Effect::Fire:     runFireTicks(); break;
    case Effect::Sparkle:  runSparkle(); break;
    case Effect::Revs:     runRevs(t); break;
    case Effect::Tricolor: runTricolor(t); break;
    case Effect::Start:    runStart(t); break;
    case Effect::Beacon:   runBeacon(t); break;
    case Effect::Strobe:   runStrobe(t); break;
    case Effect::Neon:     runNeon(); break;
    default: return;
  }
  // efekt calego auta przykrywa wszystkie diody nadwozia (kierunkowskazy i tak sa
  // na wierzchu); kabina zostaje niezalezna
  if (WHOLE_CAR[static_cast<uint8_t>(st.effect)]) {
    for (uint8_t k = 0; k < ringLen; k++) out[ring[k]] = CRGB(fx[ring[k]]).nscale8_video(st.fxBright);
    return;
  }
  // efekt na bokach przykrywa tylko diody boczne, reszta auta dziala niezaleznie
  for (uint8_t p = 0; p < 6; p++) {
    if (RIGHT_SIDE[p] >= 0) out[RIGHT_SIDE[p]] = CRGB(fx[RIGHT_SIDE[p]]).nscale8_video(st.fxBright);
    if (LEFT_SIDE[p] >= 0) out[LEFT_SIDE[p]] = CRGB(fx[LEFT_SIDE[p]]).nscale8_video(st.fxBright);
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

// ---------------- kabina ----------------
// "Swiatla startowe": diody zapalaja sie po kolei, chwile swieca wszystkie,
// gasna razem i po przerwie od nowa.
uint8_t raceLit(uint32_t now) {
  const uint32_t lighting = 3UL * CABIN_RACE_STEP_MS;
  const uint32_t period = lighting + CABIN_RACE_HOLD_MS + CABIN_RACE_PAUSE_MS;
  uint32_t t = (now - cabinStart) % period;
  if (t < lighting) return t / CABIN_RACE_STEP_MS + 1;  // 1..3
  return t < lighting + CABIN_RACE_HOLD_MS ? 3 : 0;
}

// now = czas kabiny (cabinTime), plynie wg ustawionej predkosci
void drawCabin(uint32_t now) {
  if (!st.cabin) return;
  CRGB c[3];
  switch (st.cabinMode) {
    case CabinMode::Ambient: fill_solid(c, 3, CABIN_AMBIENT_COLOR); break;
    case CabinMode::Breathe:
      fill_solid(c, 3, CABIN_BREATHE_COLOR);
      for (CRGB& x : c) x.nscale8_video(sinAt(now, CABIN_BREATHE_BPM, 15, 255));
      break;
    case CabinMode::Rainbow: {
      uint8_t hue = now / FRAME_MS * CABIN_RAINBOW_SPEED;
      for (uint8_t p = 0; p < 3; p++) c[p] = CHSV(hue + p * 85, 255, 255);
      break;
    }
    case CabinMode::Race: {
      uint8_t lit = raceLit(now);
      for (uint8_t p = 0; p < 3; p++) c[p] = p < lit ? CABIN_RACE_COLOR : CRGB::Black;
      break;
    }
    default: fill_solid(c, 3, CABIN_WARM_COLOR); break;
  }
  for (uint8_t p = 0; p < 3; p++) {
    if (CABIN[p] >= 0 && CABIN[p] < NUM_LEDS) out[CABIN[p]] = c[p].nscale8_video(st.cabinBright);
  }
}

// ---------------- tryb testu: zamiast wszystkich warstw ----------------
void drawTest() {
  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    out[i] = (testMask >> i) & 1 ? testColor : CRGB::Black;
  }
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
  buildGeometry();
  FastLED.show();
}

void update() {
  uint32_t now = millis();
  uint32_t dt = now - lastFrame;
  if (dt < FRAME_MS) return;
  lastFrame = now;

  fxClock += (uint64_t)dt * st.fxSpeed;
  cabinClock += (uint64_t)dt * st.cabinSpeed;
  uint32_t newFxTime = fxClock / 100;
  fxTicks = min<uint32_t>(newFxTime / FRAME_MS - fxTime / FRAME_MS, 4);
  fxTime = newFxTime;
  cabinTime = cabinClock / 100;

  if (testOn) {
    drawTest();
  } else {
    drawBase(now);
    drawEffect(fxTime);
    drawTurn(now);
    drawCabin(cabinTime);
  }
  FastLED.show();
}

void toggleHeadlights() {
  st.headlights = !st.headlights;
  headStart = millis();  // animacja wlaczania
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
  bool same = e == st.effect;
  st.effect = e;
  // takze przy ponownym wyborze: efekt startuje od poczatku
  fxStart = fxTime;
  startHoldMs = random(START_HOLD_MIN_MS, START_HOLD_MAX_MS + 1);
  memset(heat, 0, sizeof(heat));
  clearFx();
  if (!same) markChanged();
}

void setNeonColor(const CRGB& c) {
  if (c == neon) return;
  neon = c;
  markChanged();
}

CRGB neonColor() {
  return neon;
}

void toggleCabin() {
  st.cabin = !st.cabin;
  cabinStart = cabinTime;
  markChanged();
}

void setCabinMode(CabinMode m) {
  if (m >= CabinMode::Count) m = CabinMode::Warm;
  if (st.cabin && m == st.cabinMode) return;
  st.cabinMode = m;
  st.cabin = true;
  cabinStart = cabinTime;
  markChanged();
}

void setFxBright(uint8_t v) {
  st.fxBright = max<uint8_t>(v, FX_BRIGHT_MIN);
  markChanged();
}

void setFxSpeed(uint16_t pct) {
  st.fxSpeed = constrain(pct, FX_SPEED_MIN, FX_SPEED_MAX);
  markChanged();
}

void setCabinBright(uint8_t v) {
  st.cabinBright = max<uint8_t>(v, FX_BRIGHT_MIN);
  markChanged();
}

void setCabinSpeed(uint16_t pct) {
  st.cabinSpeed = constrain(pct, FX_SPEED_MIN, FX_SPEED_MAX);
  markChanged();
}

void allOff() {
  // tryb kabiny, jasnosc i predkosc zostaja na nastepne wlaczenie
  LightState keep = st;
  st = LightState();
  st.cabinMode = keep.cabinMode;
  st.fxBright = keep.fxBright;
  st.fxSpeed = keep.fxSpeed;
  st.cabinBright = keep.cabinBright;
  st.cabinSpeed = keep.cabinSpeed;
  clearFx();
  markChanged();
}

void testSet(uint32_t mask, const CRGB& color) {
  testOn = true;
  testMask = mask;
  testColor = color;
  markChanged();
}

void testEnd() {
  if (!testOn) return;
  testOn = false;
  clearFx();  // skaner zaczyna bez starych smug
  markChanged();
}

bool testActive() {
  return testOn;
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

const char* cabinModeName(CabinMode m) {
  uint8_t i = static_cast<uint8_t>(m);
  return i < static_cast<uint8_t>(CabinMode::Count) ? CABIN_MODE_NAMES[i] : "warm";
}

CabinMode cabinModeFromName(const char* name) {
  for (uint8_t i = 0; i < static_cast<uint8_t>(CabinMode::Count); i++) {
    if (strcmp(name, CABIN_MODE_NAMES[i]) == 0) return static_cast<CabinMode>(i);
  }
  return CabinMode::Warm;
}

const char* turnName(Turn t) {
  switch (t) {
    case Turn::Left: return "L";
    case Turn::Right: return "R";
    default: return "";
  }
}

}  // namespace Lights
