#include "web.h"

#include <ArduinoJson.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>
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
enum class CmdType : uint8_t { Head, TurnLeft, TurnRight, Hazard, SetEffect, Off, Restart };
struct Cmd {
  CmdType type;
  uint8_t arg;
};
QueueHandle_t cmdQueue;

CRGB lastSent[NUM_LEDS];
uint32_t lastSendMs = 0;
uint32_t lastCleanupMs = 0;

void pushCmd(CmdType type, uint8_t arg = 0) {
  Cmd c{type, arg};
  xQueueSend(cmdQueue, &c, 0);
}

// ---------------- WiFi ----------------
NvsSettings::WifiSettings savedWifi;  // ustawienia z NVS odczytane przy starcie
uint32_t restartAtMs = 0;             // != 0: restart po zapisaniu ustawien WiFi

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
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID);  // siec otwarta
#ifdef WIFI_LOW_TX_POWER
  WiFi.setTxPower(WIFI_POWER_8_5dBm);
#endif
  Serial.printf("Access Point \"%s\". Panel: http://%s\n", AP_SSID,
                WiFi.softAPIP().toString().c_str());
}

// Ustawienia z NVS maja pierwszenstwo przed config.h:
//   sta  -> siec z NVS, po nieudanej probie otwarty AP
//   ap   -> od razu otwarty AP
//   brak -> siec z config.h, po nieudanej probie otwarty AP
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

  if (MDNS.begin(MDNS_NAME)) MDNS.addService("http", "tcp", 80);
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

  char buf[128 + sizeof(hex)];
  snprintf(buf, sizeof(buf),
           "{\"head\":%d,\"turn\":\"%s\",\"hazard\":%d,\"effect\":\"%s\",\"leds\":\"%s\"}",
           st.headlights, Lights::turnName(st.turn), st.hazard,
           Lights::effectName(st.effect), hex);
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
  String s;
  serializeJson(doc, s);
  return s;
}

// POST /wifi (mode, ssid, pass). Puste haslo przy tej samej sieci = zostaw stare.
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
    if (!NvsSettings::saveWifi(s)) return fail("Blad zapisu NVS");
  }
  Serial.printf("Zapisano tryb WiFi: %s, restart.\n", NvsSettings::modeName(s.mode));
  req->send(200, "application/json", "{\"ok\":true}");
  pushCmd(CmdType::Restart);
}

// ---------------- WebSocket ----------------
void handleMessage(const uint8_t* data, size_t len) {
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
  } else if (!strcmp(cmd, "off")) {
    pushCmd(CmdType::Off);
  }
}

void onWsEvent(AsyncWebSocket*, AsyncWebSocketClient* client, AwsEventType type,
               void* arg, uint8_t* data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    client->text(buildMapJson());
    client->text(buildStateJson());
  } else if (type == WS_EVT_DATA) {
    AwsFrameInfo* info = static_cast<AwsFrameInfo*>(arg);
    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
      handleMessage(data, len);
    }
  }
}

}  // namespace

namespace Web {

void begin() {
  cmdQueue = xQueueCreate(16, sizeof(Cmd));
  startWifi();

  ws.onEvent(onWsEvent);
  server.addHandler(&ws);
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
    req->send(200, "text/html", reinterpret_cast<const uint8_t*>(INDEX_HTML),
              strlen_P(INDEX_HTML));
  });
  server.on("/wifi", HTTP_GET, [](AsyncWebServerRequest* req) {
    req->send(200, "text/html", reinterpret_cast<const uint8_t*>(WIFI_HTML),
              strlen_P(WIFI_HTML));
  });
  server.on("/wifi.json", HTTP_GET, [](AsyncWebServerRequest* req) {
    req->send(200, "application/json", buildWifiJson());
  });
  server.on("/wifi", HTTP_POST, handleWifiSave);
  server.onNotFound([](AsyncWebServerRequest* req) { req->redirect("/"); });
  server.begin();
}

void handleCommands() {
  Cmd c;
  while (xQueueReceive(cmdQueue, &c, 0) == pdTRUE) {
    switch (c.type) {
      case CmdType::Head:      Lights::toggleHeadlights(); break;
      case CmdType::TurnLeft:  Lights::toggleTurn(Turn::Left); break;
      case CmdType::TurnRight: Lights::toggleTurn(Turn::Right); break;
      case CmdType::Hazard:    Lights::toggleHazard(); break;
      case CmdType::SetEffect: Lights::setEffect(static_cast<Effect>(c.arg)); break;
      case CmdType::Off:       Lights::allOff(); break;
      case CmdType::Restart:   restartAtMs = millis() + 1000; break;  // czas na odpowiedz
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

  bool stateChanged = Lights::consumeChanged();
  if (ws.count() == 0) return;

  bool ledsChanged = memcmp(lastSent, Lights::leds(), sizeof(lastSent)) != 0;
  if (!stateChanged && !(ledsChanged && now - lastSendMs >= WS_FRAME_MS)) return;
  if (!ws.availableForWriteAll()) return;

  ws.textAll(buildStateJson());
  memcpy(lastSent, Lights::leds(), sizeof(lastSent));
  lastSendMs = now;
}

}  // namespace Web
