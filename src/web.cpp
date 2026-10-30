#include "web.h"

#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <WiFi.h>

#include "NvsSettings.h"
#include "config.h"
#include "lights.h"
#include "web_page.h"

namespace {

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// Komendy z WebSocketu przychodza w zadaniu AsyncTCP, a diody renderuje loop().
// Kolejka przenosi je bezpiecznie do loop().
enum class CmdType : uint8_t {
  Head, TurnLeft, TurnRight, Hazard, SetEffect, Neon, Off, Restart, TestSet, TestEnd
};
struct Cmd {
  CmdType type;
  uint8_t arg;
  uint32_t mask;  // TestSet: diody do zapalenia
  CRGB color;     // TestSet: ich kolor, Neon: kolor neonu
};
QueueHandle_t cmdQueue;

// Strona /test, ktora ostatnio ustawila test. Jej rozlaczenie konczy test,
// wiec zamkniecie karty albo utrata WiFi tez przywraca swiatla.
uint32_t testOwner = 0;

CRGB lastSent[NUM_LEDS];
uint32_t lastSendMs = 0;
uint32_t lastCleanupMs = 0;
bool statePending = false;  // zmiana stanu jeszcze niewyslana do stron

void pushCmd(CmdType type, uint8_t arg = 0, uint32_t mask = 0, CRGB color = CRGB::Black) {
  Cmd c{type, arg, mask, color};
  xQueueSend(cmdQueue, &c, 0);
}

constexpr bool OTA_HAS_PASSWORD = sizeof(OTA_PASSWORD) > 1;  // "" = bez hasla
static_assert(sizeof(AP_PASS) == 1 || (sizeof(AP_PASS) >= 9 && sizeof(AP_PASS) <= 64),
              "AP_PASS: puste albo 8-63 znaki");

// ---------------- WiFi ----------------
NvsSettings::WifiSettings savedWifi;  // ustawienia z NVS odczytane przy starcie
uint32_t restartAtMs = 0;             // != 0: restart po zapisaniu ustawien WiFi

// Haslo hotspotu: z NVS, jesli zapisane na /wifi, inaczej z config.h. "" = otwarty.
String apPass() {
  return savedWifi.apPassSet ? savedWifi.apPass : String(AP_PASS);
}

bool validApPass(const String& p) {
  return p.isEmpty() || (p.length() >= 8 && p.length() <= 63);
}

bool connectSta(const char* ssid, const char* pass) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);
#ifdef WIFI_LOW_TX_POWER
  WiFi.setTxPower(WIFI_POWER_8_5dBm);
#endif
  Serial.printf("Lacze z WiFi \"%s\"", ssid);

  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_CONNECT_TIMEOUT_MS) {
    delay(250);
    Serial.print('.');
  }
  Serial.println();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Brak polaczenia.");
    WiFi.disconnect(true);
    return false;
  }
  Serial.printf("Polaczono. Panel: http://%s  (http://%s.local)\n",
                WiFi.localIP().toString().c_str(), MDNS_NAME);
  return true;
}

void startAp() {
  String pass = apPass();
  if (!validApPass(pass)) {
    Serial.println("Nieprawidlowe haslo hotspotu, uruchamiam siec otwarta.");
    pass = "";
  }
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, pass.isEmpty() ? nullptr : pass.c_str());
#ifdef WIFI_LOW_TX_POWER
  WiFi.setTxPower(WIFI_POWER_8_5dBm);
#endif
  Serial.printf("Access Point \"%s\" (%s). Panel: http://%s\n", AP_SSID,
                pass.isEmpty() ? "otwarty" : "z haslem", WiFi.softAPIP().toString().c_str());
}

// Ustawienia z NVS maja pierwszenstwo przed config.h:
//   sta  -> siec z NVS, po nieudanej probie AP
//   ap   -> od razu AP
//   brak -> siec z config.h, po nieudanej probie AP
void startWifi() {
  savedWifi = NvsSettings::loadWifi();
  Serial.printf("Ustawienia WiFi w NVS: %s\n", NvsSettings::modeName(savedWifi.mode));

  bool connected = false;
  switch (savedWifi.mode) {
    case NvsSettings::WifiMode::Sta:
      connected = connectSta(savedWifi.ssid.c_str(), savedWifi.pass.c_str());
      break;
    case NvsSettings::WifiMode::Ap:
      break;
    case NvsSettings::WifiMode::None:
      connected = connectSta(WIFI_SSID, WIFI_PASS);
      break;
  }
  if (!connected) startAp();

  if (MDNS.begin(MDNS_NAME)) {
    MDNS.addService("http", "tcp", 80);
    MDNS.enableArduino(OTA_PORT, OTA_HAS_PASSWORD);  // ESP widoczny w Arduino IDE
  }
}

// ---------------- OTA ----------------
// Wgrywanie z PlatformIO / Arduino IDE (espota). handle() blokuje loop() na czas wgrywania.
void startArduinoOta() {
  ArduinoOTA.setHostname(MDNS_NAME);
  ArduinoOTA.setPort(OTA_PORT);
  if (OTA_HAS_PASSWORD) ArduinoOTA.setPassword(OTA_PASSWORD);
  ArduinoOTA.setMdnsEnabled(false);  // mDNS uruchamia startWifi()
  ArduinoOTA
      .onStart([]() { Serial.println("OTA: start"); })
      .onEnd([]() { Serial.println("OTA: koniec, restart"); })
      .onProgress([](unsigned int done, unsigned int total) {
        static uint8_t lastPct = 255;
        uint8_t pct = done * 100 / total;
        if (pct / 10 != lastPct / 10) Serial.printf("OTA: %u%%\n", pct);
        lastPct = pct;
      })
      .onError([](ota_error_t e) { Serial.printf("OTA: blad %u\n", e); });
  ArduinoOTA.begin();
}

bool otaAuthorized(AsyncWebServerRequest* req) {
  return !OTA_HAS_PASSWORD || req->authenticate(OTA_USER, OTA_PASSWORD);
}

// Wgrywanie pliku .bin ze strony /update. Jednoczesnie tylko jedno wgrywanie.
String updateErr;

void handleUpdateUpload(AsyncWebServerRequest* req, const String& filename, size_t index,
                        uint8_t* data, size_t len, bool final) {
  if (index == 0) {
    updateErr = "";
    if (!otaAuthorized(req)) {
      updateErr = "Brak autoryzacji";
      return;
    }
    if (Update.isRunning()) Update.abort();  // przerwane wczesniejsze wgrywanie
    Serial.printf("Aktualizacja: %s\n", filename.c_str());
    if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
      updateErr = Update.errorString();
      return;
    }
  }
  if (!updateErr.isEmpty() || !Update.isRunning()) return;

  if (Update.write(data, len) != len) {
    updateErr = Update.errorString();
    Update.abort();
    return;
  }
  if (final && !Update.end(true)) updateErr = Update.errorString();
}

void handleUpdateDone(AsyncWebServerRequest* req) {
  if (!otaAuthorized(req)) return req->requestAuthentication();
  if (updateErr.isEmpty() && !Update.isFinished()) updateErr = "Niepelny plik";
  if (!updateErr.isEmpty()) {
    Serial.printf("Aktualizacja nieudana: %s\n", updateErr.c_str());
    JsonDocument doc;
    doc["ok"] = false;
    doc["err"] = updateErr;
    String s;
    serializeJson(doc, s);
    return req->send(400, "application/json", s);
  }
  Serial.println("Aktualizacja zakonczona, restart.");
  req->send(200, "application/json", "{\"ok\":true}");
  pushCmd(CmdType::Restart);
}

// Informacje o firmware dla strony /update.
String buildUpdateJson() {
  JsonDocument doc;
  doc["build"] = __DATE__ " " __TIME__;
  doc["size"] = ESP.getSketchSize();
  doc["free"] = ESP.getFreeSketchSpace();
  String s;
  serializeJson(doc, s);
  return s;
}

// ---------------- JSON ----------------
template <size_t N>
void appendZone(String& s, const char* key, const int8_t (&zone)[N]) {
  s += '"';
  s += key;
  s += "\":[";
  bool first = true;
  for (int8_t i : zone) {
    if (i < 0) continue;
    if (!first) s += ',';
    s += i;
    first = false;
  }
  s += ']';
}

// Uklad diod z config.h, zeby schemat na stronie zgadzal sie z mapowaniem.
String buildMapJson() {
  String s = "{\"map\":{";
  appendZone(s, "lf", LEFT_FRONT);   s += ',';
  appendZone(s, "rf", RIGHT_FRONT);  s += ',';
  appendZone(s, "lr", LEFT_REAR);    s += ',';
  appendZone(s, "rr", RIGHT_REAR);   s += ',';
  appendZone(s, "ls", LEFT_SIDE);    s += ',';
  appendZone(s, "rs", RIGHT_SIDE);
  s += "},\"n\":";
  s += NUM_LEDS;
  s += '}';
  return s;
}

String buildStateJson() {
  const LightState& st = Lights::state();
  const CRGB* leds = Lights::leds();

  char hex[NUM_LEDS * 6 + 1];
  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    snprintf(hex + i * 6, 7, "%02x%02x%02x", leds[i].r, leds[i].g, leds[i].b);
  }

  CRGB neon = Lights::neonColor();

  char buf[160 + sizeof(hex)];
  snprintf(buf, sizeof(buf),
           "{\"head\":%d,\"turn\":\"%s\",\"hazard\":%d,\"effect\":\"%s\",\"test\":%d,"
           "\"neon\":\"#%02x%02x%02x\",\"leds\":\"%s\"}",
           st.headlights, Lights::turnName(st.turn), st.hazard,
           Lights::effectName(st.effect), Lights::testActive(), neon.r, neon.g, neon.b, hex);
  return String(buf);
}

// Zapisany tryb WiFi i aktualny stan (bez hasla), dla strony /wifi.
String buildWifiJson() {
  JsonDocument doc;
  doc["saved"] = NvsSettings::modeName(savedWifi.mode);
  doc["ssid"] = savedWifi.ssid;
  doc["hasPass"] = !savedWifi.pass.isEmpty();
  doc["now"] = WiFi.getMode() == WIFI_AP ? "ap" : "sta";
  doc["nowSsid"] = WiFi.getMode() == WIFI_AP ? String(AP_SSID) : WiFi.SSID();
  doc["apOpen"] = apPass().isEmpty();
  doc["apSaved"] = savedWifi.apPassSet;
  String s;
  serializeJson(doc, s);
  return s;
}

// POST /wifi (mode, ssid, pass, apOpen, apPass). Puste haslo przy tej samej sieci = zostaw stare.
// Haslo hotspotu: apOpen=1 -> otwarty, niepuste apPass -> nowe haslo, puste -> bez zmian.
void handleWifiSave(AsyncWebServerRequest* req) {
  auto param = [req](const char* name) {
    const AsyncWebParameter* p = req->getParam(name, true);
    return p ? p->value() : String();
  };
  auto fail = [req](const char* err) {
    req->send(400, "application/json", String("{\"ok\":false,\"err\":\"") + err + "\"}");
  };

  NvsSettings::WifiSettings s;
  s.mode = NvsSettings::modeFromName(param("mode").c_str());
  if (s.mode == NvsSettings::WifiMode::None) {
    NvsSettings::clearWifi();
  } else {
    if (s.mode == NvsSettings::WifiMode::Sta) {
      s.ssid = param("ssid");
      s.pass = param("pass");
      if (s.ssid.isEmpty() || s.ssid.length() > 32 || s.pass.length() > 64) {
        return fail("Nieprawidlowa nazwa sieci lub haslo");
      }
      if (s.pass.isEmpty() && s.ssid == savedWifi.ssid) s.pass = savedWifi.pass;
    }
    s.apPassSet = savedWifi.apPassSet;
    s.apPass = savedWifi.apPass;
    if (param("apOpen") == "1") {
      s.apPassSet = true;
      s.apPass = "";
    } else if (!param("apPass").isEmpty()) {
      s.apPassSet = true;
      s.apPass = param("apPass");
      if (!validApPass(s.apPass)) return fail("Haslo hotspotu musi miec 8-63 znaki");
    }
    if (!NvsSettings::saveWifi(s)) return fail("Blad zapisu NVS");
  }
  Serial.printf("Zapisano tryb WiFi: %s, restart.\n", NvsSettings::modeName(s.mode));
  req->send(200, "application/json", "{\"ok\":true}");
  pushCmd(CmdType::Restart);
}

// ---------------- WebSocket ----------------
void handleMessage(AsyncWebSocketClient* client, const uint8_t* data, size_t len) {
  JsonDocument doc;
  if (deserializeJson(doc, data, len)) return;

  const char* cmd = doc["cmd"] | "";
  if (!strcmp(cmd, "head")) {
    pushCmd(CmdType::Head);
  } else if (!strcmp(cmd, "turn")) {
    const char* dir = doc["dir"] | "";
    if (!strcmp(dir, "L")) pushCmd(CmdType::TurnLeft);
    else if (!strcmp(dir, "R")) pushCmd(CmdType::TurnRight);
  } else if (!strcmp(cmd, "hazard")) {
    pushCmd(CmdType::Hazard);
  } else if (!strcmp(cmd, "effect")) {
    Effect e = Lights::effectFromName(doc["name"] | "none");
    pushCmd(CmdType::SetEffect, static_cast<uint8_t>(e));
  } else if (!strcmp(cmd, "neon")) {
    // {"cmd":"neon","color":"#RRGGBB"}
    const char* hex = doc["color"] | "";
    if (hex[0] == '#' && strlen(hex) == 7) {
      char* end = nullptr;
      uint32_t rgb = strtoul(hex + 1, &end, 16);
      if (end == hex + 7) pushCmd(CmdType::Neon, 0, 0, CRGB(rgb));
    }
  } else if (!strcmp(cmd, "off")) {
    pushCmd(CmdType::Off);
  } else if (!strcmp(cmd, "test")) {
    // {"cmd":"test","mask":<bit i = dioda i>,"on":true,"r":..,"g":..,"b":..}; on=false gasi diody testu
    bool on = doc["on"] | false;
    uint32_t mask = on ? (doc["mask"] | 0u) : 0u;
    if (NUM_LEDS < 32) mask &= (1UL << NUM_LEDS) - 1;
    CRGB color(doc["r"] | 0, doc["g"] | 0, doc["b"] | 0);
    testOwner = client->id();
    pushCmd(CmdType::TestSet, 0, mask, color);
  } else if (!strcmp(cmd, "testEnd")) {
    testOwner = 0;
    pushCmd(CmdType::TestEnd);
  }
}

void onWsEvent(AsyncWebSocket*, AsyncWebSocketClient* client, AwsEventType type,
               void* arg, uint8_t* data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    client->text(buildMapJson());
    client->text(buildStateJson());
  } else if (type == WS_EVT_DISCONNECT) {
    if (testOwner && client->id() == testOwner) {
      testOwner = 0;
      pushCmd(CmdType::TestEnd);
    }
  } else if (type == WS_EVT_DATA) {
    AwsFrameInfo* info = static_cast<AwsFrameInfo*>(arg);
    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
      handleMessage(client, data, len);
    }
  }
}

}  // namespace

namespace Web {

void begin() {
  cmdQueue = xQueueCreate(16, sizeof(Cmd));
  startWifi();
  startArduinoOta();

  ws.onEvent(onWsEvent);
  server.addHandler(&ws);
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
    req->send(200, "text/html", reinterpret_cast<const uint8_t*>(INDEX_HTML),
              strlen_P(INDEX_HTML));
  });
  server.on("/test", HTTP_GET, [](AsyncWebServerRequest* req) {
    req->send(200, "text/html", reinterpret_cast<const uint8_t*>(TEST_HTML),
              strlen_P(TEST_HTML));
  });
  server.on("/wifi", HTTP_GET, [](AsyncWebServerRequest* req) {
    req->send(200, "text/html", reinterpret_cast<const uint8_t*>(WIFI_HTML),
              strlen_P(WIFI_HTML));
  });
  server.on("/wifi.json", HTTP_GET, [](AsyncWebServerRequest* req) {
    req->send(200, "application/json", buildWifiJson());
  });
  server.on("/wifi", HTTP_POST, handleWifiSave);
  server.on("/update", HTTP_GET, [](AsyncWebServerRequest* req) {
    if (!otaAuthorized(req)) return req->requestAuthentication();
    req->send(200, "text/html", reinterpret_cast<const uint8_t*>(UPDATE_HTML),
              strlen_P(UPDATE_HTML));
  });
  server.on("/update.json", HTTP_GET, [](AsyncWebServerRequest* req) {
    req->send(200, "application/json", buildUpdateJson());
  });
  server.on("/update", HTTP_POST, handleUpdateDone, handleUpdateUpload);
  server.onNotFound([](AsyncWebServerRequest* req) { req->redirect("/"); });
  server.begin();
}

void handleCommands() {
  ArduinoOTA.handle();

  Cmd c;
  while (xQueueReceive(cmdQueue, &c, 0) == pdTRUE) {
    // W trakcie testu stan swiatel jest zamrozony, zeby po tescie wrocil bez zmian.
    if (Lights::testActive() && c.type <= CmdType::Off) continue;
    switch (c.type) {
      case CmdType::Head:      Lights::toggleHeadlights(); break;
      case CmdType::TurnLeft:  Lights::toggleTurn(Turn::Left); break;
      case CmdType::TurnRight: Lights::toggleTurn(Turn::Right); break;
      case CmdType::Hazard:    Lights::toggleHazard(); break;
      case CmdType::SetEffect: Lights::setEffect(static_cast<Effect>(c.arg)); break;
      case CmdType::Neon:      Lights::setNeonColor(c.color); break;
      case CmdType::Off:       Lights::allOff(); break;
      case CmdType::Restart:   restartAtMs = millis() + 1000; break;  // czas na odpowiedz
      case CmdType::TestSet:   Lights::testSet(c.mask, c.color); break;
      case CmdType::TestEnd:   Lights::testEnd(); break;
    }
  }
  if (restartAtMs && (int32_t)(millis() - restartAtMs) >= 0) ESP.restart();
}

void broadcast() {
  uint32_t now = millis();
  if (now - lastCleanupMs > 1000) {
    ws.cleanupClients();
    lastCleanupMs = now;
  }

  // Zmiana stanu czeka, az da sie ja wyslac (gdy bufor klienta jest pelny, nie przepada).
  // Bez klientow nie ma po co jej trzymac: nowa strona dostaje pelny stan przy polaczeniu.
  if (Lights::consumeChanged()) statePending = true;
  if (ws.count() == 0) {
    statePending = false;
    return;
  }

  bool ledsChanged = memcmp(lastSent, Lights::leds(), sizeof(lastSent)) != 0;
  if (!statePending && !(ledsChanged && now - lastSendMs >= WS_FRAME_MS)) return;
  if (!ws.availableForWriteAll()) return;

  ws.textAll(buildStateJson());
  memcpy(lastSent, Lights::leds(), sizeof(lastSent));
  lastSendMs = now;
  statePending = false;
}

}  // namespace Web
