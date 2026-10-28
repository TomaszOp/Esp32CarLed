#pragma once

#include <Arduino.h>
#include <FastLED.h>

// ============================================================
//  WiFi
// ============================================================
// Siec domowa. Jesli polaczenie sie nie uda w WIFI_CONNECT_TIMEOUT_MS,
// ESP32 uruchamia otwarty Access Point AP_SSID (panel: http://192.168.4.1).
#define WIFI_SSID               "TwojaSiec"
#define WIFI_PASS               "TwojeHaslo"
#define WIFI_CONNECT_TIMEOUT_MS 15000

#define AP_SSID                 "CarLed"      // otwarty, bez hasla
#define MDNS_NAME               "carled"      // http://carled.local

// Niektore plytki ESP32-C3 SuperMini maja slaba antene i nie lacza sie
// z WiFi przy pelnej mocy nadajnika. Odkomentuj, jesli masz z tym problem.
// #define WIFI_LOW_TX_POWER

// ============================================================
//  Tasma LED
// ============================================================
#define LED_PIN        4
#define NUM_LEDS       22
#define LED_BRIGHTNESS 100     // 0-255, bezpieczny limit dla ladowarki 5 V
#define MAX_MILLIAMPS  1000    // FastLED przyciemni tasme ponad ten pobor

#define FRAME_MS       20      // odswiezanie animacji (50 klatek/s)

// ============================================================
//  Mapowanie diod (indeksy 0-21)
// ============================================================
// Strefy przod/tyl: kolejnosc  { zewnetrzna, srodkowa, wewnetrzna }.
// Zewnetrzna = przy rogu auta, wewnetrzna = blizej srodka.
// -1 oznacza brak diody w tym miejscu (tyl ma tylko 2 diody).
// Ta sama kolejnosc steruje biegnacym swiatlem kierunkowskazu.
constexpr int8_t LEFT_FRONT[3]  = {16, 17, 18};
constexpr int8_t RIGHT_FRONT[3] = {21, 20, 19};
constexpr int8_t LEFT_REAR[3]   = { 9,  8, -1};
constexpr int8_t RIGHT_REAR[3]  = { 6,  7, -1};

// Boki: kolejnosc od przodu do tylu auta (uzywane przez efekty).
constexpr int8_t RIGHT_SIDE[6]  = { 0,  1,  2,  3,  4,  5};
constexpr int8_t LEFT_SIDE[6]   = {15, 14, 13, 12, 11, 10};

// ============================================================
//  Kolory
// ============================================================
#define COLOR_HEAD_FRONT CRGB(255, 255, 255)  // swiatla glowne, przod
#define COLOR_HEAD_REAR  CRGB(255,   0,   0)  // swiatla glowne, tyl
#define COLOR_TURN       CRGB(255,  90,   0)  // kierunkowskaz (bursztyn)

// ============================================================
//  Kierunkowskazy: biegnaca pojedyncza dioda
//  zewnetrzna -> srodkowa -> wewnetrzna -> przerwa -> od nowa
// ============================================================
#define TURN_STEP_MS   150     // jak dlugo swieci kazda dioda
#define TURN_PAUSE_MS  200     // przerwa (wszystko zgaszone) przed kolejnym cyklem

// ============================================================
//  Efekty specjalne (tylko diody boczne)
// ============================================================
#define RAINBOW_SPEED    3     // przesuniecie barwy na klatke
#define RAINBOW_HUE_STEP 20    // roznica barwy miedzy sasiednimi diodami
#define SCANNER_STEP_MS  90
#define SCANNER_COLOR    CRGB(255, 0, 0)
#define BREATHE_BPM      12
#define BREATHE_COLOR    CRGB(0, 160, 255)
#define POLICE_STEP_MS   70

// ============================================================
//  Panel WWW
// ============================================================
#define WS_FRAME_MS    66      // max ~15 aktualizacji podgladu diod na sekunde
