#pragma once

namespace Web {

// WiFi (z przejsciem na Access Point), serwer HTTP i WebSocket.
void begin();

// Wykonuje komendy odebrane ze strony. Wywolywac w loop() przed Lights::update().
void handleCommands();

// Wysyla stan i podglad diod do otwartych stron. Wywolywac po Lights::update().
void broadcast();

}  // namespace Web
