#pragma once

#include <Arduino.h>
#include <Preferences.h>

// Ustawienia WiFi zapisane w pamieci NVS (przetrwaja restart i wgranie firmware).
// Brak zapisanych ustawien = WiFi wg config.h.
namespace NvsSettings {

enum class WifiMode : uint8_t { None = 0, Sta = 1, Ap = 2 };

struct WifiSettings {
  WifiMode mode = WifiMode::None;
  String ssid;
  String pass;
  bool apPassSet = false;  // false = haslo hotspotu z config.h
  String apPass;           // "" = otwarty hotspot
};

constexpr const char* NVS_NAMESPACE = "carled";

inline const char* modeName(WifiMode m) {
  switch (m) {
    case WifiMode::Sta: return "sta";
    case WifiMode::Ap:  return "ap";
    default:            return "none";
  }
}

inline WifiMode modeFromName(const char* name) {
  if (!strcmp(name, "sta")) return WifiMode::Sta;
  if (!strcmp(name, "ap")) return WifiMode::Ap;
  return WifiMode::None;
}

inline WifiSettings loadWifi() {
  WifiSettings s;
  Preferences p;
  if (!p.begin(NVS_NAMESPACE, true)) return s;  // namespace jeszcze nie istnieje
  s.mode = static_cast<WifiMode>(p.getUChar("wifiMode", 0));
  s.ssid = p.getString("ssid", "");
  s.pass = p.getString("pass", "");
  s.apPassSet = p.isKey("apPass");
  s.apPass = p.getString("apPass", "");
  p.end();
  if (s.mode == WifiMode::Sta && s.ssid.isEmpty()) s.mode = WifiMode::None;
  if (s.mode != WifiMode::Sta && s.mode != WifiMode::Ap) s.mode = WifiMode::None;
  return s;
}

inline bool saveWifi(const WifiSettings& s) {
  Preferences p;
  if (!p.begin(NVS_NAMESPACE, false)) return false;
  p.putUChar("wifiMode", static_cast<uint8_t>(s.mode));
  p.putString("ssid", s.ssid);
  p.putString("pass", s.pass);
  if (s.apPassSet) p.putString("apPass", s.apPass);
  else p.remove("apPass");
  p.end();
  return true;
}

// Usuwa ustawienia WiFi (z haslem hotspotu), nastepny start wg config.h.
inline void clearWifi() {
  Preferences p;
  if (!p.begin(NVS_NAMESPACE, false)) return;
  p.remove("wifiMode");
  p.remove("ssid");
  p.remove("pass");
  p.remove("apPass");
  p.end();
}

}  // namespace NvsSettings
