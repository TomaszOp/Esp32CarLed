# Esp32CarLed

Sterowanie światłami modelu **Peugeot 9X8** z telefonu. ESP32-C3 steruje taśmą WS2812B (22 diody) i wystawia panel WWW ze schematem auta, który na żywo pokazuje, co świeci na modelu.

## Funkcje

- **Światła główne:** przód biały, tył czerwony.
- **Kierunkowskazy:** biegnąca pojedyncza dioda (zewnętrzna → środkowa → wewnętrzna), z przodu i z tyłu danej strony.
- **Światła awaryjne:** oba kierunkowskazy jednocześnie. Włączony wcześniej kierunkowskaz wraca po ich wyłączeniu.
- **Efekty specjalne na bokach:** tęcza, skaner, oddychanie, policja. Reszta auta działa w tym czasie niezależnie.
- **OFF:** gasi wszystkie diody.
- Po wyłączeniu kierunkowskazu lub efektu diody wracają do poprzedniego stanu.

## Sprzęt

| Element | Opis |
| --- | --- |
| Mikrokontroler | ESP32-C3 (np. DevKitM-1 lub SuperMini) |
| Taśma LED | WS2812B, 22 diody, kolejność kolorów GRB |
| Zasilanie | ładowarka USB 5 V |
| Pin danych | GPIO 4 (zmiana w `config.h`) |

Połączenia: **5V** taśmy do 5V, **GND** do GND, **DIN** taśmy do GPIO 4. Zalecany rezystor 330 Ω w linii danych i kondensator 470–1000 µF na zasilaniu taśmy.

Jasność jest ograniczona do 100/255, a pobór prądu do 1 A (`LED_BRIGHTNESS`, `MAX_MILLIAMPS`), żeby nie przeciążyć ładowarki.

## Mapowanie diod

| Strefa | Indeksy |
| --- | --- |
| Prawy bok | 0–5 |
| Prawy tył | 6, 7 |
| Lewy tył | 8, 9 |
| Lewy bok | 10–15 |
| Lewy przód | 16, 17, 18 |
| Prawy przód | 19, 20, 21 |

Kolejność diod kierunkowskazu ustawiasz w `include/config.h` jako `{ zewnętrzna, środkowa, wewnętrzna }`, np.:

```cpp
constexpr int8_t LEFT_FRONT[3]  = {16, 17, 18};
constexpr int8_t RIGHT_FRONT[3] = {21, 20, 19};
constexpr int8_t LEFT_REAR[3]   = { 9,  8, -1};   // -1 = brak diody
constexpr int8_t RIGHT_REAR[3]  = { 6,  7, -1};
```

Jeśli kierunkowskaz biegnie w złą stronę, odwróć kolejność w danej tablicy. Schemat auta na stronie rysuje się według tej konfiguracji.

## Uruchomienie

1. Otwórz folder `Esp32CarLed` w VS Code z PlatformIO.
2. W `include/config.h` wpisz swoją sieć:
   ```cpp
   #define WIFI_SSID "TwojaSiec"
   #define WIFI_PASS "TwojeHaslo"
   ```
3. Masz płytkę **ESP32-C3 SuperMini**? Odkomentuj w `platformio.ini` dwie linie `-DARDUINO_USB_...`, inaczej Serial Monitor nic nie pokaże.
4. Wgraj program: `pio run -t upload`, potem `pio device monitor`.
5. Otwórz adres z monitora szeregowego albo `http://carled.local`.

Jeśli ESP32 nie połączy się z siecią w 15 s, uruchomi otwarty Access Point **CarLed**. Połącz się z nim i otwórz `http://192.168.4.1`.

### Ustawienia WiFi z panelu

Ikona WiFi w prawym górnym rogu panelu otwiera stronę `/wifi`. Wybrany tryb zapisuje się w pamięci NVS (przetrwa restart i wgranie nowego programu), a po **Zapisz** ESP32 uruchamia się ponownie:

| Zapisany tryb | Start |
| --- | --- |
| Połącz z siecią WiFi | łączy się z zapisaną siecią, a gdy się nie uda, uruchamia otwarty hotspot CarLed |
| Tylko hotspot CarLed | od razu otwarty hotspot CarLed |
| Domyślne z config.h (brak zapisu) | łączy się z siecią z `config.h`, a gdy się nie uda, uruchamia hotspot CarLed |

## Obsługa panelu

| Kliknięcie na schemacie | Działanie |
| --- | --- |
| Przód (środek) | światła główne wł./wył. |
| Prawy przód lub prawy tył | prawy kierunkowskaz wł./wył. |
| Lewy przód lub lewy tył | lewy kierunkowskaz wł./wył. |
| Środek auta | światła awaryjne wł./wył. |

Pod schematem: przycisk **OFF** i **Efekty specjalne** (okno z listą efektów; ponowne kliknięcie aktywnego efektu go wyłącza).

## Struktura projektu

```
Esp32CarLed/
├── platformio.ini        płytka, ustawienia kompilacji
├── lib/                  biblioteki (kopie lokalne, bez pobierania z sieci)
├── include/
│   ├── config.h          WiFi, pin, mapowanie diod, kolory, czasy
│   ├── lights.h          interfejs logiki świateł
│   ├── NvsSettings.h     ustawienia WiFi zapisane w NVS
│   ├── web.h             interfejs WiFi i serwera
│   └── web_page.h        panel i strona WiFi (HTML/CSS/JS w PROGMEM)
└── src/
    ├── main.cpp          setup() i loop()
    ├── lights.cpp        warstwy świateł, kierunkowskazy, efekty
    └── web.cpp           WiFi/AP, serwer HTTP, WebSocket, strona /wifi
```

Biblioteki są w katalogu `lib/`: FastLED 3.9.20, ESPAsyncWebServer 3.12.1 i AsyncTCP 3.5.0 (ESP32Async), ArduinoJson 7.4.3. Z `lib/ESPAsyncWebServer/library.json` usunięto sekcję `dependencies`, bo PlatformIO pobierał z niej biblioteki dla ESP8266 i Raspberry Pi.

## Problemy

- **Nie łączy się z WiFi (SuperMini):** odkomentuj `#define WIFI_LOW_TX_POWER` w `config.h`.
- **Złe kolory (np. czerwony zamiast zielonego):** zmień `GRB` na `RGB` w `FastLED.addLeds` w `src/lights.cpp`.
- **Panel pokazuje „rozłączono”:** strona sama łączy się ponownie co 1,5 s; sprawdź, czy telefon jest w tej samej sieci co ESP32.
