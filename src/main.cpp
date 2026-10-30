// Esp32CarLed - sterowanie swiatlami modelu Peugeot 9X8
// ESP32-C3 + WS2812B (25 diod, w tym 3 w kabinie), panel WWW z podgladem na zywo.
// Ustawienia (WiFi, pin, mapowanie diod, kolory, czasy): include/config.h

#include <Arduino.h>

#include "lights.h"
#include "web.h"

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\nEsp32CarLed - start");

  Lights::begin();
  Web::begin();
}

void loop() {
  Web::handleCommands();
  Lights::update();
  Web::broadcast();
}
