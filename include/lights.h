#pragma once

#include <Arduino.h>
#include <FastLED.h>

enum class Turn : uint8_t { None, Left, Right };

enum class Effect : uint8_t { None, Rainbow, Scanner, Breathe, Police, Count };

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
void setEffect(Effect e);
void allOff();

const LightState& state();
const CRGB* leds();

// true, jesli stan zmienil sie od ostatniego wywolania
bool consumeChanged();

const char* effectName(Effect e);
Effect effectFromName(const char* name);
const char* turnName(Turn t);

}  // namespace Lights
