#pragma once

#include <Arduino.h>
#include <FastLED.h>

// ============================================================
//  WiFi
// ============================================================
// Siec domowa. Jesli polaczenie sie nie uda w WIFI_CONNECT_TIMEOUT_MS,
// ESP32 uruchamia Access Point AP_SSID (panel: http://192.168.4.1).
#define WIFI_SSID               "TwojaSiec"
#define WIFI_PASS               "TwojeHaslo"
#define WIFI_CONNECT_TIMEOUT_MS 15000

#define AP_SSID                 "CarLed"
// Haslo hotspotu: "" = siec otwarta, inaczej 8-63 znaki.
// Haslo ustawione na stronie /wifi ma pierwszenstwo przed tym.
#define AP_PASS                 ""
#define MDNS_NAME               "carled"      // http://carled.local

// Niektore plytki ESP32-C3 SuperMini maja slaba antene i nie lacza sie
// z WiFi przy pelnej mocy nadajnika. Odkomentuj, jesli masz z tym problem.
// #define WIFI_LOW_TX_POWER

// ============================================================
//  Aktualizacja firmware przez WiFi (OTA)
// ============================================================
// Haslo do wgrywania z PlatformIO (env esp32c3_ota) i do strony /update
// (login: OTA_USER). Zmieniajac je, zmien tez --auth w platformio.ini.
// Puste "" = bez hasla (kazdy w sieci/hotspocie moze wgrac firmware).
#define OTA_USER     "admin"
#define OTA_PASSWORD "carled"
#define OTA_PORT     3232

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
#define COLOR_TURN       CRGB(255, 30,   0)  // kierunkowskaz (bursztyn)

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
//  Efekty na calym aucie (wszystkie 22 diody)
//  Rzedy od przodu do tylu: 0 = przod, 1-6 = boki, 7 = tyl.
//  Kierunkowskazy i awaryjne zostaja na wierzchu.
// ============================================================
#define COMET_STEP_MS      60     // czas przejscia glowy o jeden rzad
#define COMET_PAUSE_MS     600    // przerwa przed kolejnym przebiegiem
#define COMET_FADE         45     // dlugosc ogona (wieksze = krotszy)
#define COMET_COLOR        CRGB(170, 210, 255)

#define FIRE_COOLING       45     // max wychlodzenie rzedu na klatke
#define FIRE_SPARKING      120    // szansa (0-255) na nowa iskre z tylu

#define SPARKLE_FADE       12     // tempo gasniecia iskier na klatke

#define REVS_RISE_MS       1500   // wzrost obrotow na jednym biegu
#define REVS_FLASH_MS      120    // blysk przy zmianie biegu
#define REVS_GEARS         4      // liczba biegow, potem od zera
#define REVS_IDLE_MS       600    // przerwa na zerze po ostatnim biegu
#define REVS_FLASH_COLOR   CRGB(255, 255, 255)

#define TRICOLOR_STEP_MS   150    // czas przesuniecia pasow o jeden rzad
#define TRICOLOR_BAND      2      // szerokosc pasa w rzedach

#define START_DARK_MS      800    // zgaszone przed pierwszym swiatlem
#define START_STEP_MS      800    // odstep miedzy kolejnymi swiatlami (5 krokow)
#define START_HOLD_MIN_MS  500    // losowe przytrzymanie wszystkich czerwonych
#define START_HOLD_MAX_MS  2000
#define START_OUT_MS       250    // zgaszone przed zielonym
#define START_GREEN_MS     600    // zielony blysk
#define START_RAINBOW_SPEED 4     // tecza wokol auta po starcie

#define BEACON_PERIOD_MS   600    // jeden obrot punktu wokol auta
#define BEACON_WIDTH       4      // zasieg miekkiego spadku (w diodach)
#define BEACON_COLOR       CRGB(255, 100, 0)

#define STROBE_FLASH_MS    30     // dlugosc blysku
#define STROBE_GAP_MS      70     // przerwa miedzy dwoma blyskami
#define STROBE_PAUSE_MS    500    // pauza po parze blyskow

#define NEON_COLOR         CRGB(0x80, 0x00, 0xFF)  // domyslny, zmiana z panelu (tylko RAM)

// ============================================================
//  Panel WWW
// ============================================================
#define WS_FRAME_MS    66      // max ~15 aktualizacji podgladu diod na sekunde
