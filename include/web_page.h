#pragma once

#include <Arduino.h>

// Panel sterowania: HTML + CSS + JS w pamieci flash.
const char INDEX_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="pl">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1, user-scalable=no">
<meta name="theme-color" content="#0d0f14">
<title>Peugeot 9X8 LED</title>
<style>
:root{--bg:#0d0f14;--card:#171a22;--line:#2a2f3b;--txt:#e8eaf0;--mut:#8a90a0;--acc:#4fd1ff;--amb:#ff9a1f;--red:#ff3b47}
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent}
body{margin:0;background:var(--bg);color:var(--txt);font:15px/1.4 system-ui,-apple-system,Segoe UI,Roboto,sans-serif;display:flex;justify-content:center}
main{width:100%;max-width:440px;padding:16px}
header{display:flex;align-items:center;justify-content:space-between;margin-bottom:8px}
h1{font-size:18px;margin:0;letter-spacing:.5px}
h1 span{color:var(--mut);font-weight:400}
#conn{display:flex;align-items:center;gap:6px;font-size:12px;color:var(--mut)}
#conn i{width:9px;height:9px;border-radius:50%;background:var(--red);display:inline-block}
#conn.on i{background:#2ee66b}
.hr{display:flex;align-items:center;gap:12px}
.ico{display:flex;color:var(--mut);padding:4px}
.ico:hover{color:var(--acc)}
.ico svg{width:22px;height:22px}
.car{background:var(--card);border:1px solid var(--line);border-radius:18px;padding:8px}
svg{width:100%;height:auto;max-height:62vh;display:block;margin:0 auto;user-select:none}
.body{fill:#1f2430;stroke:#3a4152;stroke-width:2}
.glass{fill:#0f1720;stroke:#334;stroke-width:1.5}
.wheel{fill:#0a0b0e}
.wing{fill:#262c39;stroke:#3a4152}
.led{stroke:#000;stroke-width:1;pointer-events:none;transition:fill .05s}
.zone{fill:transparent;cursor:pointer;stroke:transparent;stroke-width:2;rx:12}
.zone:hover{fill:rgba(255,255,255,.04)}
.zone:active{fill:rgba(255,255,255,.1)}
.zone.on{stroke:rgba(79,209,255,.55);stroke-dasharray:5 4}
.lbl{fill:var(--mut);font-size:10px;text-anchor:middle;pointer-events:none;letter-spacing:.5px}
#hz{pointer-events:none}
#hz path{fill:none;stroke:var(--mut);stroke-width:3;stroke-linejoin:round}
#hz.on path{stroke:var(--amb)}
.chips{display:flex;flex-wrap:wrap;gap:6px;margin:12px 0}
.chip{font-size:12px;padding:4px 10px;border-radius:99px;border:1px solid var(--line);color:var(--mut)}
.chip.on{color:#000;background:var(--acc);border-color:var(--acc)}
.chip.amb.on{background:var(--amb);border-color:var(--amb)}
.btns{display:grid;grid-template-columns:1fr 1fr;gap:10px}
button{font:inherit;font-weight:600;border:0;border-radius:14px;padding:14px;cursor:pointer;color:var(--txt);background:var(--card);border:1px solid var(--line)}
button:active{transform:scale(.98)}
#off{background:var(--red);border-color:var(--red);color:#fff;font-size:17px}
#fxBtn.on{border-color:var(--acc);color:var(--acc)}
.hint{color:var(--mut);font-size:12px;text-align:center;margin-top:12px}
dialog{background:var(--card);color:var(--txt);border:1px solid var(--line);border-radius:18px;padding:16px;width:min(92vw,400px)}
dialog::backdrop{background:rgba(0,0,0,.6)}
dialog h2{margin:0 0 12px;font-size:17px}
.fx{display:flex;flex-direction:column;gap:8px}
.fx button{text-align:left;display:flex;flex-direction:column;gap:2px;padding:12px 14px}
.fx button small{font-weight:400;color:var(--mut)}
.fx button.on{border-color:var(--acc);background:#13303b}
.fx button.on::after{content:"aktywny";font-size:11px;color:var(--acc);font-weight:600}
.dlgBtns{display:grid;grid-template-columns:1fr 1fr;gap:8px;margin-top:12px}
</style>
</head>
<body>
<main>
<header>
  <h1>Peugeot 9X8 <span>LED</span></h1>
  <div class="hr">
    <div id="conn"><i></i><span>rozłączono</span></div>
    <a class="ico" href="/update" title="Aktualizacja firmware"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M12 15V3"/><path d="M7 8l5-5 5 5"/><path d="M4 15v4a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2v-4"/></svg></a>
    <a class="ico" href="/wifi" title="Ustawienia WiFi"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round"><path d="M2 8.5a15 15 0 0 1 20 0"/><path d="M5 12a10 10 0 0 1 14 0"/><path d="M8.5 15.5a5 5 0 0 1 7 0"/><circle cx="12" cy="19" r="1" fill="currentColor"/></svg></a>
  </div>
</header>

<div class="car">
<svg id="svg" viewBox="0 0 220 440">
  <rect class="wheel" x="14" y="80" width="16" height="56" rx="5"/>
  <rect class="wheel" x="190" y="80" width="16" height="56" rx="5"/>
  <rect class="wheel" x="14" y="300" width="16" height="60" rx="5"/>
  <rect class="wheel" x="190" y="300" width="16" height="60" rx="5"/>
  <path class="body" d="M62 28 Q110 8 158 28 Q184 44 188 78 Q194 112 186 150 L182 280 Q196 320 192 380 Q190 412 168 418 L52 418 Q30 412 28 380 Q24 320 38 280 L34 150 Q26 112 32 78 Q36 44 62 28 Z"/>
  <path class="glass" d="M110 130 Q140 132 142 175 L140 250 Q138 275 110 278 Q82 275 80 250 L78 175 Q80 132 110 130 Z"/>
  <rect class="wing" x="26" y="404" width="168" height="14" rx="4"/>
  <g id="leds"></g>
  <g id="hz" transform="translate(110 205)"><path d="M0 -18 L17 12 L-17 12 Z"/><path d="M0 -6 L7 6 L-7 6 Z"/></g>
  <text class="lbl" x="110" y="78">ŚWIATŁA</text>
  <text class="lbl" x="110" y="240">AWARYJNE</text>
  <text class="lbl" x="40" y="70">◀ L</text>
  <text class="lbl" x="180" y="70">P ▶</text>
  <text class="lbl" x="40" y="385">◀ L</text>
  <text class="lbl" x="180" y="385">P ▶</text>
  <rect class="zone" id="zHead" data-cmd="head" x="78" y="4" width="64" height="88"/>
  <rect class="zone zL" data-cmd="L" x="4" y="4" width="72" height="88"/>
  <rect class="zone zR" data-cmd="R" x="144" y="4" width="72" height="88"/>
  <rect class="zone zL" data-cmd="L" x="4" y="350" width="104" height="86"/>
  <rect class="zone zR" data-cmd="R" x="112" y="350" width="104" height="86"/>
  <rect class="zone" id="zHaz" data-cmd="hazard" x="66" y="150" width="88" height="110"/>
</svg>
</div>

<div class="chips">
  <span class="chip" id="cHead">Światła</span>
  <span class="chip amb" id="cL">◀ Lewy</span>
  <span class="chip amb" id="cR">Prawy ▶</span>
  <span class="chip amb" id="cHaz">Awaryjne</span>
  <span class="chip" id="cFx">Efekt: brak</span>
</div>

<div class="btns">
  <button id="off">OFF</button>
  <button id="fxBtn">Efekty specjalne</button>
</div>
<p class="hint">Dotknij przodu, rogów lub środka auta.</p>
</main>

<dialog id="dlg">
  <h2>Efekty specjalne</h2>
  <div class="fx" id="fxList"></div>
  <div class="dlgBtns">
    <button id="fxNone">Wyłącz efekt</button>
    <button id="fxClose">Zamknij</button>
  </div>
</dialog>

<script>
const FX = [
  ["rainbow","Tęcza na bokach","Płynna tęcza na 6 + 6 diodach bocznych"],
  ["scanner","Skaner","Czerwony punkt biegający po bokach"],
  ["breathe","Oddychanie","Boki płynnie rozjaśniają się i gasną"],
  ["police","Policja","Czerwono-niebieskie błyski na bokach"]
];
const NS = "http://www.w3.org/2000/svg";
const $ = id => document.getElementById(id);
let ws, st = {}, ledEls = [];

function send(o){ if (ws && ws.readyState === 1) ws.send(JSON.stringify(o)); }

// Rysuje diody wg mapowania z config.h przyslanego przez ESP32.
function buildLeds(m){
  const g = $("leds"); g.innerHTML = ""; ledEls = [];
  const put = (idx, x, y) => {
    const c = document.createElementNS(NS, "circle");
    c.setAttribute("cx", x); c.setAttribute("cy", y); c.setAttribute("r", 7);
    c.setAttribute("class", "led"); g.appendChild(c); ledEls[idx] = c;
  };
  // przod / tyl: od zewnetrznej do wewnetrznej
  m.lf.forEach((i, s) => put(i, 50 + s * 19, 36));
  m.rf.forEach((i, s) => put(i, 170 - s * 19, 36));
  m.lr.forEach((i, s) => put(i, 50 + s * 19, 400));
  m.rr.forEach((i, s) => put(i, 170 - s * 19, 400));
  // boki: od przodu do tylu
  m.ls.forEach((i, s) => put(i, 33, 120 + s * 38));
  m.rs.forEach((i, s) => put(i, 187, 120 + s * 38));
}

function paintLeds(hex){
  for (let i = 0; i < ledEls.length; i++) {
    const el = ledEls[i]; if (!el) continue;
    const h = hex.substr(i * 6, 6);
    if (h === "000000") { el.style.fill = "#2a2e38"; el.style.filter = ""; }
    else { el.style.fill = "#" + h; el.style.filter = "drop-shadow(0 0 5px #" + h + ")"; }
  }
}

function render(){
  $("zHead").classList.toggle("on", !!st.head);
  $("zHaz").classList.toggle("on", !!st.hazard);
  $("hz").classList.toggle("on", !!st.hazard);
  document.querySelectorAll(".zL").forEach(z => z.classList.toggle("on", st.turn === "L"));
  document.querySelectorAll(".zR").forEach(z => z.classList.toggle("on", st.turn === "R"));
  $("cHead").classList.toggle("on", !!st.head);
  $("cL").classList.toggle("on", st.turn === "L");
  $("cR").classList.toggle("on", st.turn === "R");
  $("cHaz").classList.toggle("on", !!st.hazard);
  const fx = FX.find(f => f[0] === st.effect);
  $("cFx").textContent = "Efekt: " + (fx ? fx[1] : "brak");
  $("cFx").classList.toggle("on", !!fx);
  $("fxBtn").classList.toggle("on", !!fx);
  document.querySelectorAll(".fx button").forEach(b => b.classList.toggle("on", b.dataset.fx === st.effect));
}

function connect(){
  ws = new WebSocket("ws://" + location.host + "/ws");
  ws.onopen = () => { $("conn").classList.add("on"); $("conn").lastChild.textContent = "połączono"; };
  ws.onclose = () => {
    $("conn").classList.remove("on"); $("conn").lastChild.textContent = "rozłączono";
    setTimeout(connect, 1500);
  };
  ws.onmessage = e => {
    const d = JSON.parse(e.data);
    if (d.map) { buildLeds(d.map); return; }
    st = d; render(); paintLeds(d.leds);
  };
}

document.querySelectorAll(".zone").forEach(z => z.addEventListener("click", () => {
  const c = z.dataset.cmd;
  if (c === "L" || c === "R") send({cmd: "turn", dir: c});
  else send({cmd: c});
}));

FX.forEach(([id, name, desc]) => {
  const b = document.createElement("button");
  b.dataset.fx = id;
  b.innerHTML = name + "<small>" + desc + "</small>";
  b.onclick = () => send({cmd: "effect", name: st.effect === id ? "none" : id});
  $("fxList").appendChild(b);
});

$("off").onclick = () => send({cmd: "off"});
$("fxBtn").onclick = () => $("dlg").showModal();
$("fxClose").onclick = () => $("dlg").close();
$("fxNone").onclick = () => send({cmd: "effect", name: "none"});
$("dlg").addEventListener("click", e => { if (e.target === $("dlg")) $("dlg").close(); });

connect();
</script>
</body>
</html>
)rawliteral";

// Strona ustawien WiFi (/wifi). Stan pobiera z /wifi.json, zapisuje POST /wifi.
const char WIFI_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="pl">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<meta name="theme-color" content="#0d0f14">
<title>WiFi - Peugeot 9X8 LED</title>
<style>
:root{--bg:#0d0f14;--card:#171a22;--line:#2a2f3b;--txt:#e8eaf0;--mut:#8a90a0;--acc:#4fd1ff;--red:#ff3b47}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--txt);font:15px/1.4 system-ui,-apple-system,Segoe UI,Roboto,sans-serif;display:flex;justify-content:center}
main{width:100%;max-width:440px;padding:16px}
header{display:flex;align-items:center;gap:12px;margin-bottom:12px}
header a{color:var(--mut);text-decoration:none;font-size:22px}
h1{font-size:18px;margin:0}
.card{background:var(--card);border:1px solid var(--line);border-radius:18px;padding:16px;display:flex;flex-direction:column;gap:12px}
label{display:flex;align-items:flex-start;gap:10px;cursor:pointer}
label small{display:block;color:var(--mut);font-size:12px}
input[type=radio]{margin-top:3px;accent-color:var(--acc)}
input[type=text],input[type=password]{font:inherit;width:100%;padding:12px;border-radius:12px;border:1px solid var(--line);background:var(--bg);color:var(--txt)}
#sta{display:flex;flex-direction:column;gap:8px;padding-left:26px}
.show{font-size:13px;color:var(--mut);align-items:center}
button{font:inherit;font-weight:600;border:0;border-radius:14px;padding:14px;cursor:pointer;background:var(--acc);color:#000;font-size:17px}
button:disabled{opacity:.5}
#now,#msg{color:var(--mut);font-size:13px;margin:0}
#msg{min-height:18px;text-align:center}
</style>
</head>
<body>
<main>
<header><a href="/" title="Powrót">&#8592;</a><h1>Ustawienia WiFi</h1></header>
<form class="card" id="f">
  <p id="now">Wczytuję…</p>
  <label><input type="radio" name="mode" value="sta"><span>Połącz z siecią WiFi<small>Gdy się nie uda: otwarty hotspot CarLed</small></span></label>
  <div id="sta">
    <input type="text" name="ssid" id="ssid" placeholder="Nazwa sieci (SSID)" maxlength="32" autocomplete="off">
    <input type="password" name="pass" id="pass" placeholder="Hasło" maxlength="64" autocomplete="off">
    <label class="show"><input type="checkbox" id="showPass"> pokaż hasło</label>
  </div>
  <label><input type="radio" name="mode" value="ap"><span>Tylko hotspot CarLed<small>Otwarta sieć, panel: http://192.168.4.1</small></span></label>
  <label><input type="radio" name="mode" value="none"><span>Domyślne z config.h<small>Usuwa zapisane ustawienia</small></span></label>
  <button type="submit" id="save">Zapisz</button>
  <p id="msg"></p>
</form>
</main>
<script>
const $ = id => document.getElementById(id);
const mode = () => { const r = document.querySelector("input[name=mode]:checked"); return r ? r.value : "none"; };
const showSta = () => { $("sta").style.display = mode() === "sta" ? "" : "none"; };
document.querySelectorAll("input[name=mode]").forEach(r => r.onchange = showSta);
$("showPass").onchange = e => { $("pass").type = e.target.checked ? "text" : "password"; };

fetch("/wifi.json").then(r => r.json()).then(w => {
  const saved = {sta: "sieć „" + w.ssid + "”", ap: "tylko hotspot", none: "brak (config.h)"}[w.saved];
  $("now").textContent = "Teraz: " + (w.now === "ap" ? "hotspot " : "sieć ") + w.nowSsid + ". Zapisane: " + saved + ".";
  document.querySelector("input[name=mode][value=" + w.saved + "]").checked = true;
  $("ssid").value = w.ssid;
  if (w.hasPass) $("pass").placeholder = "Hasło (puste = bez zmian)";
  showSta();
}).catch(() => { $("now").textContent = "Nie udało się odczytać ustawień."; showSta(); });

$("f").onsubmit = e => {
  e.preventDefault();
  if (mode() === "sta" && !$("ssid").value.trim()) { $("msg").textContent = "Podaj nazwę sieci."; return; }
  const body = new URLSearchParams({mode: mode(), ssid: $("ssid").value.trim(), pass: $("pass").value});
  $("save").disabled = true; $("msg").textContent = "Zapisuję…";
  fetch("/wifi", {method: "POST", body}).then(r => r.json()).then(d => {
    if (d.ok) $("msg").textContent = "Zapisano. ESP32 uruchamia się ponownie z nowymi ustawieniami…";
    else { $("msg").textContent = "Błąd: " + d.err; $("save").disabled = false; }
  }).catch(() => { $("msg").textContent = "Brak odpowiedzi od ESP32."; $("save").disabled = false; });
};
</script>
</body>
</html>
)rawliteral";

// Strona aktualizacji firmware (/update). Plik .bin wysyla POST /update.
const char UPDATE_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="pl">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<meta name="theme-color" content="#0d0f14">
<title>Aktualizacja - Peugeot 9X8 LED</title>
<style>
:root{--bg:#0d0f14;--card:#171a22;--line:#2a2f3b;--txt:#e8eaf0;--mut:#8a90a0;--acc:#4fd1ff;--red:#ff3b47}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--txt);font:15px/1.4 system-ui,-apple-system,Segoe UI,Roboto,sans-serif;display:flex;justify-content:center}
main{width:100%;max-width:440px;padding:16px}
header{display:flex;align-items:center;gap:12px;margin-bottom:12px}
header a{color:var(--mut);text-decoration:none;font-size:22px}
h1{font-size:18px;margin:0}
.card{background:var(--card);border:1px solid var(--line);border-radius:18px;padding:16px;display:flex;flex-direction:column;gap:12px}
input[type=file]{font:inherit;color:var(--mut);padding:12px;border-radius:12px;border:1px dashed var(--line);background:var(--bg);width:100%}
button{font:inherit;font-weight:600;border:0;border-radius:14px;padding:14px;cursor:pointer;background:var(--acc);color:#000;font-size:17px}
button:disabled{opacity:.5}
progress{width:100%;height:10px;accent-color:var(--acc)}
#info,#msg,.hint{color:var(--mut);font-size:13px;margin:0}
#msg{min-height:18px;text-align:center}
</style>
</head>
<body>
<main>
<header><a href="/" title="Powrót">&#8592;</a><h1>Aktualizacja firmware</h1></header>
<form class="card" id="f">
  <p id="info">Wczytuję…</p>
  <input type="file" id="file" accept=".bin">
  <p class="hint">Plik: .pio/build/esp32c3/firmware.bin. Nie wyłączaj zasilania w trakcie wgrywania.</p>
  <progress id="prog" max="100" value="0" hidden></progress>
  <button type="submit" id="send">Wgraj</button>
  <p id="msg"></p>
</form>
</main>
<script>
const $ = id => document.getElementById(id);
const kb = b => Math.round(b / 1024) + " KB";

fetch("/update.json").then(r => r.json()).then(i => {
  $("info").textContent = "Obecny firmware: " + i.build + ", " + kb(i.size) + " (miejsce na nowy: " + kb(i.free) + ").";
}).catch(() => { $("info").textContent = "Nie udało się odczytać informacji o firmware."; });

$("f").onsubmit = e => {
  e.preventDefault();
  const file = $("file").files[0];
  if (!file) { $("msg").textContent = "Wybierz plik .bin."; return; }
  const fd = new FormData(); fd.append("firmware", file, file.name);
  const x = new XMLHttpRequest();
  const done = (text, again) => { $("msg").textContent = text; $("send").disabled = !again; };
  x.open("POST", "/update");
  x.upload.onprogress = p => { if (p.lengthComputable) { $("prog").value = p.loaded * 100 / p.total; $("msg").textContent = "Wgrywam… " + Math.round(p.loaded * 100 / p.total) + "%"; } };
  x.onload = () => {
    let d = {}; try { d = JSON.parse(x.responseText); } catch (_) {}
    if (d.ok) { done("Gotowe. ESP32 uruchamia się ponownie…"); setTimeout(() => location.href = "/", 8000); }
    else done("Błąd: " + (d.err || ("HTTP " + x.status)), true);
  };
  x.onerror = () => done("Brak odpowiedzi od ESP32.", true);
  $("prog").hidden = false; $("prog").value = 0; done("Wgrywam…");
  x.send(fd);
};
</script>
</body>
</html>
)rawliteral";
