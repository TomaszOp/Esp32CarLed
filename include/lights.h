#pragma once

#include <Arduino.h>
#include <FastLED.h>

enum class Turn : uint8_t { None, Left, Right };

// Rainbow..Police dzialaja tylko na bokach, Comet..Neon na calym aucie.
enum class Effect : uint8_t {
  None, Rainbow, Scanner, Breathe, Police,
  Comet, Fire, Sparkle, Revs, Tricolor, Start, Beacon, Strobe, Neon,
  Count
};

struct LightState {
  bool headlights = false;
  Turn turn = Turn::None;   // zapamietany takze w trakcie swiatel awaryjnych
  bool hazard = false;
  Effect effect = Effect::None;
};

namespace Lights {

void begin();

// Wywolywac w loop(). Renderuje klatke co FRAME_MS.
void update();

void toggleHeadlights();
void toggleTurn(Turn dir);
void toggleHazard();
// Ponowny wybor aktywnego efektu uruchamia go od poczatku.
void setEffect(Effect e);
// Kolor efektu Neon (tylko RAM, nie zmienia aktywnego efektu).
void setNeonColor(const CRGB& c);
CRGB neonColor();
void allOff();

const LightState& state();
const CRGB* leds();

// Tryb testu diod: diody z maski (bit i = dioda i) swieca kolorem, reszta zgaszona.
// Stan swiatel (LightState) nie zmienia sie w trakcie testu, wiec po testEnd()
// wraca dokladnie to, co swiecilo przed testem.
void testSet(uint32_t mask, const CRGB& color);
void testEnd();
bool testActive();

// true, jesli stan zmienil sie od ostatniego wywolania
bool consumeChanged();

const char* effectName(Effect e);
Effect effectFromName(const char* name);
const char* turnName(Turn t);

}  // namespace Lights
