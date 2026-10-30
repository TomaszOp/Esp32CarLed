# Esp32CarLed

Sterowanie światłami modelu **Peugeot 9X8** z telefonu. ESP32-C3 steruje taśmą WS2812B (25 diod: 22 na nadwoziu i 3 w kabinie) i wystawia panel WWW ze schematem auta, który na żywo pokazuje, co świeci na modelu.

## Funkcje

- **Światła główne:** przód biały, tył czerwony. Przy włączaniu diody zapalają się po kolei jak kierunkowskaz (zewnętrzna → wewnętrzna) i zostają, potem dwa razy błyskają jaśniej i wracają do normalnej jasności (`HEAD_LEVEL` i `HEAD_*` w `config.h`).
- **Kierunkowskazy:** biegnąca pojedyncza dioda (zewnętrzna → środkowa → wewnętrzna), z przodu i z tyłu danej strony.
- **Światła awaryjne:** oba kierunkowskazy jednocześnie. Włączony wcześniej kierunkowskaz wraca po ich wyłączeniu.
- **Efekty specjalne na bokach:** tęcza, skaner, oddychanie, policja. Reszta auta działa w tym czasie niezależnie.
- **Efekty na całym aucie** (wszystkie 22 diody nadwozia, kabina działa osobno):
  - *Kometa* – biało-niebieska głowa z ogonem biegnie od przodu do tyłu po obu stronach, potem krótka przerwa.
  - *Ogień* – płomienie (Fire2012) od tyłu do przodu, liczone osobno dla lewej i prawej strony.
  - *Iskry* – losowe kolorowe iskry, które powoli gasną.
  - *Obroty silnika* – pasek rośnie od tyłu do przodu (zielony → żółty → czerwony), błysk i spadek do 1/3 przy zmianie biegu; po 4 biegach od zera.
  - *Trójkolorowa flaga* – pasy niebieski, biały, czerwony płynnie przesuwają się od przodu do tyłu.
  - *Start wyścigu* – 5 czerwonych świateł na bokach co 0,8 s, losowe przytrzymanie 0,5–2 s, zgaszenie, zielony błysk, potem tęcza dookoła auta. Ponowny wybór startuje od początku.
  - *Kogut pomocy drogowej* – pomarańczowe światło obiega auto dookoła (obrót w 0,6 s).
  - *Stroboskop* – dwa krótkie białe błyski i pauza.
  - *Neon* – stały kolor na wszystkich diodach; kolor wybierasz w panelu obok przycisku (domyślnie fioletowy, nie jest zapisywany w NVS).

  Światła główne, kierunkowskaz i awaryjne zachowują stan w trakcie efektu (można je też przełączać). Kierunkowskazy i awaryjne świecą na wierzchu efektu, a po wyłączeniu efektu wracają światła takie, jak są ustawione. Czasy i kolory efektów są w `config.h`.

  Jasność i prędkość wszystkich efektów zmieniasz suwakami w oknie **Efekty specjalne**.
- **Kabina:** 3 diody w kokpicie, włączane z panelu niezależnie od reszty auta. Tryby: ciepłe światło, ambient (niebieski), pulsowanie, tęcza, światła startowe (diody zapalają się po kolei na czerwono i gasną razem). Ma własne suwaki jasności i prędkości.
- **OFF:** gasi wszystkie diody.
- **Test diod:** osobna strona do sprawdzania pojedynczych diod, stref i kolorów.
- Po wyłączeniu kierunkowskazu lub efektu diody wracają do poprzedniego stanu.

## Sprzęt

| Element | Opis |
| --- | --- |
| Mikrokontroler | ESP32-C3 (np. DevKitM-1 lub SuperMini) |
| Taśma LED | WS2812B, 25 diod, kolejność kolorów GRB |
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
| Kabina (od lewej do prawej) | 22, 23, 24 |

Diody kabiny to dalszy ciąg taśmy: DIN pierwszej diody kabiny podłącz do DOUT diody 21.

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

Jeśli ESP32 nie połączy się z siecią w 15 s, uruchomi Access Point **CarLed**. Domyślnie jest otwarty; hasło (8–63 znaki) ustawisz w `AP_PASS` w `config.h` albo na stronie `/wifi`, która ma pierwszeństwo. Połącz się z nim i otwórz `http://192.168.4.1`.

### Ustawienia WiFi z panelu

Ikona WiFi w prawym górnym rogu panelu otwiera stronę `/wifi`. Wybrany tryb zapisuje się w pamięci NVS (przetrwa restart i wgranie nowego programu), a po **Zapisz** ESP32 uruchamia się ponownie:

| Zapisany tryb | Start |
| --- | --- |
| Połącz z siecią WiFi | łączy się z zapisaną siecią, a gdy się nie uda, uruchamia hotspot CarLed |
| Tylko hotspot CarLed | od razu hotspot CarLed |
| Domyślne z config.h (brak zapisu) | łączy się z siecią z `config.h`, a gdy się nie uda, uruchamia hotspot CarLed; usuwa też hasło hotspotu zapisane na `/wifi` |

Na tej samej stronie ustawisz hasło hotspotu albo zaznaczysz „otwarty hotspot”. Puste pole hasła zostawia dotychczasowe.

### Aktualizacja firmware przez WiFi (OTA)

Pierwszy raz program trzeba wgrać przez USB. Potem nowe wersje można wgrywać bez kabla, na dwa sposoby. Oba chroni hasło `OTA_PASSWORD` z `config.h` (domyślnie `carled`, login na stronie: `admin`). Zmieniając hasło, zmień też `--auth=` w `platformio.ini`.

- **Z PlatformIO:** `pio run -e esp32c3_ota -t upload`. Komputer musi być w tej samej sieci co ESP32. Domyślnie wysyła na `carled.local`; jeśli nazwa nie działa, wpisz w `upload_port` adres IP z monitora (w hotspocie CarLed: `192.168.4.1`).
- **Z panelu WWW:** ikona strzałki w górę obok ikony WiFi otwiera stronę `/update`. Wybierz plik `.pio/build/esp32c3/firmware.bin` (powstaje po `pio run`) i kliknij **Wgraj**. Po wgraniu ESP32 sam się restartuje.

Ustawienia WiFi w NVS zostają po aktualizacji. Nieudane lub przerwane wgrywanie nie psuje działającego programu, bo nowy trafia do drugiej partycji.

## Obsługa panelu

| Kliknięcie na schemacie | Działanie |
| --- | --- |
| Przód (środek) | światła główne wł./wył. |
| Prawy przód lub prawy tył | prawy kierunkowskaz wł./wył. |
| Lewy przód lub lewy tył | lewy kierunkowskaz wł./wył. |
| Środek auta | światła awaryjne wł./wył. |

Pod schematem: przycisk **OFF** i **Efekty specjalne** (okno z listą efektów w dwóch grupach: *Na bokach* i *Na całym aucie*; ponowne kliknięcie aktywnego efektu go wyłącza, a przy *Starcie wyścigu* uruchamia sekwencję od nowa). Obok *Neonu* jest pole wyboru koloru.

### Komendy WebSocket (`/ws`)

| Komenda | Działanie |
| --- | --- |
| `{"cmd":"head"}` | światła główne wł./wył. |
| `{"cmd":"turn","dir":"L"}` / `"R"` | kierunkowskaz wł./wył. |
| `{"cmd":"hazard"}` | awaryjne wł./wył. |
| `{"cmd":"effect","name":"comet"}` | efekt: `rainbow`, `scanner`, `breathe`, `police`, `comet`, `fire`, `sparkle`, `revs`, `tricolor`, `start`, `beacon`, `strobe`, `neon` albo `none` |
| `{"cmd":"neon","color":"#8000FF"}` | kolor efektu Neon (tylko RAM, nie zmienia aktywnego efektu) |
| `{"cmd":"cabin"}` | kabina wł./wył. |
| `{"cmd":"cabinMode","name":"warm"}` | włącza kabinę w trybie `warm`, `ambient`, `breathe`, `rainbow` albo `race` |
| `{"cmd":"fxBright","v":200}` / `{"cmd":"cabinBright","v":200}` | jasność efektów / kabiny, 10–255 |
| `{"cmd":"fxSpeed","v":150}` / `{"cmd":"cabinSpeed","v":150}` | prędkość efektów / kabiny w %, 25–300 |
| `{"cmd":"off"}` | wszystko wyłączone |

Stan rozsyłany do stron zawiera m.in. pole `"neon":"#rrggbb"`.

## Test diod

Ikona żarówki w prawym górnym rogu panelu otwiera stronę `/test`. Strona jest ułożona pod telefon i mieści się na jednym ekranie. Wejście na nią zapamiętuje stan świateł (światła, kierunkowskaz, awaryjne, efekt), a wyjście go przywraca.

- **Tryb testu** (lista): *Wszystkie diody*, *Wybrane numery* albo strefa (przód lewy/prawy, cały przód, tył lewy/prawy, cały tył, bok lewy/prawy, oba boki). Pod listą jest pole **Numery diod**, np. `3, 5, 10-12`; wpisanie numerów od razu przełącza tryb na *Wybrane numery*. Dotknięcie diody na schemacie dodaje ją albo usuwa. Strzałki ◀ ▶ przechodzą do poprzedniej/następnej pojedynczej diody, a *Auto* robi to samo co 0,7 s, co ułatwia sprawdzenie mapowania w `config.h`. Dla jednej diody strona podaje jej położenie (np. „przód lewy, środkowa”).
- **Światło:** przycisk ON/OFF, wybór koloru i gotowe kolory. Strona pokazuje wartości R, G, B i kod HEX, a wybrane diody świecą tym kolorem. Pozostałe diody są w trakcie testu zgaszone. Jasność na taśmie jest dodatkowo ograniczona przez `LED_BRIGHTNESS`.
- **Zakończ test** (albo strzałka powrotu) przywraca światła. Zamknięcie karty lub utrata połączenia też kończy test, więc auto nie zostanie w trybie testu.

W trakcie testu panel główny pokazuje „Trwa test diod”, a jego przyciski nie zmieniają świateł, żeby po teście wrócił dokładnie zapamiętany stan.

Niżej sekcja **Kabina**: przycisk **Wł./Wył.** i lista trybów. Wybranie trybu włącza kabinę w tym trybie, ponowne kliknięcie aktywnego trybu ją wyłącza. Kabina pamięta ostatni tryb, także po **OFF**. Pod trybami są suwaki **Jasność** (5–100%) i **Prędkość** (25–300%) kabiny; takie same są w oknie **Efekty specjalne** dla efektów bocznych. OFF ich nie zmienia, ale po restarcie ESP32 wracają do wartości startowych (`FX_BRIGHT_DEFAULT`, `FX_SPEED_DEFAULT`).

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
│   └── web_page.h        panel, strony WiFi, aktualizacji i testu diod (HTML/CSS/JS w PROGMEM)
└── src/
    ├── main.cpp          setup() i loop()
    ├── lights.cpp        warstwy świateł, kierunkowskazy, efekty, kabina, tryb testu
    └── web.cpp           WiFi/AP, serwer HTTP, WebSocket, strony /wifi, /update, /test
```

Biblioteki są w katalogu `lib/`: FastLED 3.9.20, ESPAsyncWebServer 3.12.1 i AsyncTCP 3.5.0 (ESP32Async), ArduinoJson 7.4.3. Z `lib/ESPAsyncWebServer/library.json` usunięto sekcję `dependencies`, bo PlatformIO pobierał z niej biblioteki dla ESP8266 i Raspberry Pi.

## Problemy

- **Nie łączy się z WiFi (SuperMini):** odkomentuj `#define WIFI_LOW_TX_POWER` w `config.h`.
- **Złe kolory (np. czerwony zamiast zielonego):** zmień `GRB` na `RGB` w `FastLED.addLeds` w `src/lights.cpp`.
- **Panel pokazuje „rozłączono”:** strona sama łączy się ponownie co 1,5 s; sprawdź, czy telefon jest w tej samej sieci co ESP32.
