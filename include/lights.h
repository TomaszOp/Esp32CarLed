#pragma once

#include <Arduino.h>
#include <FastLED.h>

#include "config.h"

enum class Turn : uint8_t { None, Left, Right };

// Rainbow..Police dzialaja tylko na bokach, Comet..Neon na calym aucie.
enum class Effect : uint8_t {
  None, Rainbow, Scanner, Breathe, Police,
  Comet, Fire, Sparkle, Revs, Tricolor, Start, Beacon, Strobe, Neon,
  Count
};

enum class CabinMode : uint8_t { Warm, Ambient, Breathe, Rainbow, Race, Count };

struct LightState {
  bool headlights = false;
  Turn turn = Turn::None;   // zapamietany takze w trakcie swiatel awaryjnych
  bool hazard = false;
  Effect effect = Effect::None;
  bool cabin = false;
  CabinMode cabinMode = CabinMode::Warm;  // zapamietany takze przy wylaczonej kabinie
  // jasnosc 0-255 i predkosc w % efektow bocznych i kabiny (OFF ich nie zmienia)
  uint8_t fxBright = FX_BRIGHT_DEFAULT;
  uint16_t fxSpeed = FX_SPEED_DEFAULT;
  uint8_t cabinBright = FX_BRIGHT_DEFAULT;
  uint16_t cabinSpeed = FX_SPEED_DEFAULT;
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
void toggleCabin();
void setCabinMode(CabinMode m);  // wlacza kabine w podanym trybie
void setFxBright(uint8_t v);
void setFxSpeed(uint16_t pct);
void setCabinBright(uint8_t v);
void setCabinSpeed(uint16_t pct);
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
const char* cabinModeName(CabinMode m);
CabinMode cabinModeFromName(const char* name);
const char* turnName(Turn t);

}  // namespace Lights
