/*
  Arduino Nano ESP32 — Holo Shutter Wi-Fi firmware
  Servo signal: D9 (Nano ESP32 pin mapping / GPIO18).
  SG90 power: external regulated 5V supply; connect supply GND to board GND.
  Wi-Fi AP: HoloShutter-ESP32, password HoloShutter530 (change before use).

  Exposure timer accepts microseconds. It starts when the servo-open command
  is issued, NOT when the shutter physically clears the beam.
  An SG90 cannot provide sub-millisecond optical shuttering. The 0.5 ms and
  1 ms presets are electronic timing tests only.
  This is NOT a laser safety interlock.
*/

#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>

static const int SERVO_PIN = 18; // Nano ESP32 D9
static const char* AP_SSID = "HoloShutter-ESP32";
static const char* AP_PASSWORD = "HoloShutter530";

WebServer server(80);
Servo shutter;
int openAngle = 68;
int closedAngle = 0;
bool isOpen = false;
bool exposureActive = false;
uint32_t closeAtUs = 0;

// Maximum interval is 210 seconds (3.5 minutes), safely below the
// half-range limit required for wrap-safe 32-bit micros() comparison.

const char PAGE[] PROGMEM = R"HOLOPAGE(
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="theme-color" content="#101827">
<title>Holo Shutter Controller</title>
<style>
:root{color-scheme:dark;--bg:#101827;--panel:#1b2738;--line:#34445a;--muted:#9aa9bc;--text:#f3f6fb;--blue:#2563eb;--red:#b4232f;--green:#36b37e;--amber:#f0b44b}
*{box-sizing:border-box}
body{margin:0 auto;max-width:720px;padding:calc(18px + env(safe-area-inset-top)) 16px calc(28px + env(safe-area-inset-bottom));background:var(--bg);color:var(--text);font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,Arial,sans-serif}
header{display:flex;justify-content:space-between;align-items:center;gap:12px;margin:4px 0 18px}
h1{font-size:25px;letter-spacing:-.4px;margin:0}.subtitle{color:var(--muted);font-size:13px;margin-top:5px}
.pill{border:1px solid var(--line);border-radius:99px;padding:7px 10px;font-size:12px;color:var(--muted);white-space:nowrap}
.panel{background:var(--panel);border:1px solid var(--line);border-radius:16px;padding:17px;margin:13px 0;box-shadow:0 6px 18px #0002}
.panel h2{font-size:17px;margin:0 0 14px}.statusrow{display:flex;align-items:center;justify-content:space-between;gap:12px}
.state{font-size:17px;font-weight:750}.state.closed{color:#86d7b1}.state.open{color:var(--amber)}.state.active{color:#83b7ff}
.muted,small{color:var(--muted);line-height:1.5}
.buttons{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:10px}
button{appearance:none;border:0;border-radius:11px;padding:14px 10px;font-size:14px;font-weight:750;color:white;background:#3b4a60;min-height:50px;cursor:pointer;touch-action:manipulation}
button:active{filter:brightness(1.18);transform:translateY(1px)}button:disabled{opacity:.45;cursor:not-allowed}
.primary{background:var(--blue)}.danger{background:var(--red)}
label{display:block;font-size:14px;color:#d8e0eb;margin:12px 0 7px}
input{width:100%;border:1px solid #53647a;background:#101827;color:white;border-radius:10px;padding:13px;font-size:18px}
.inputrow{display:grid;grid-template-columns:1fr 1fr;gap:12px}
.hint{font-size:12px;color:var(--muted);margin:8px 0 0}
.message{min-height:22px;color:#9fc5ff;font-size:13px;margin:12px 2px 0;overflow-wrap:anywhere}
.warning{border-left:4px solid var(--amber);background:#342d20;border-radius:9px;padding:13px;font-size:12px;line-height:1.55;color:#f3dfb7}
.footer{text-align:center;font-size:11px;color:#8190a5;margin:18px 0}
@media(min-width:560px){.buttons.three{grid-template-columns:repeat(3,minmax(0,1fr))}}
</style>
</head>
<body>
<header><div><h1>Holo Shutter</h1><div class="subtitle">Arduino Nano ESP32 · Wi-Fi servo controller</div></div><div class="pill" id="connection">Connecting…</div></header>

<section class="panel">
  <div class="statusrow"><div><div class="muted">SHUTTER STATUS</div><div id="state" class="state">Checking…</div></div>
  <button type="button" style="min-width:92px;flex:0 0 auto;padding:10px 13px" onclick="refreshStatus()">Refresh</button></div>
  <div class="hint">Local Wi-Fi control from your iPhone. No internet connection required.</div>
</section>

<section class="panel">
  <h2>Manual shutter control</h2>
  <div class="buttons three">
    <button class="primary" type="button" onclick="sendCommand('/api/open','Opening shutter…')">OPEN</button>
    <button class="danger" type="button" onclick="sendCommand('/api/close','Closing shutter…')">CLOSE</button>
    <button type="button" onclick="sendCommand('/api/cancel','Canceling exposure and closing…')">CANCEL</button>
  </div>
</section>

<section class="panel">
  <h2>Timed exposure</h2>
  <label for="duration">Custom duration (milliseconds)</label>
  <input id="duration" type="number" min="0.5" max="210000" step="0.5" value="1000" inputmode="decimal">
  <div class="buttons" style="margin-top:12px">
    <button class="primary" type="button" onclick="startExposure()">START CUSTOM EXPOSURE</button>
    <button type="button" onclick="setDuration(120000)">2 minutes</button>
    <button type="button" onclick="setDuration(150000)">2.5 minutes</button>
    <button type="button" onclick="setDuration(180000)">3 minutes</button>
    <button type="button" onclick="setDuration(210000)">3.5 minutes</button>
    <button type="button" onclick="setDuration(1)">1 millisecond</button>
    <button type="button" onclick="setDuration(0.5)">0.5 milliseconds</button>
  </div>
  <p class="hint">Preset buttons fill the duration field. Press START CUSTOM EXPOSURE to send the command.</p>
</section>

<section class="panel">
  <h2>Servo calibration</h2>
  <div class="inputrow">
    <div><label for="openAngle">Open angle (°)</label><input id="openAngle" type="number" min="0" max="180" step="1" value="68" inputmode="numeric"></div>
    <div><label for="closedAngle">Closed angle (°)</label><input id="closedAngle" type="number" min="0" max="180" step="1" value="0" inputmode="numeric"></div>
  </div>
  <button type="button" style="width:100%;margin-top:12px" onclick="saveSettings()">APPLY SERVO ANGLES</button>
  <p class="hint">Adjust angles with the laser disabled. Values are held in RAM and reset after power cycling.</p>
</section>

<div id="message" class="message" role="status" aria-live="polite"></div>
<section class="warning">
  <strong>LASER SAFETY:</strong> The SG90 shutter is not a safety interlock. A servo can stall, lose power, or fail mechanically. Test with the laser disabled and use an independent, secure beam stop/enclosure. Never rely on this servo as the only laser barrier.
  <br><br><strong>IMPORTANT TIMING LIMITATION:</strong> The timer begins when the ESP32 commands the servo to move, not when the shutter physically clears the beam. An SG90 cannot physically open and close in 0.5 or 1 millisecond. Those two presets are electronic timer test values only, not usable optical exposure durations. Measure actual optical exposure separately.
</section>
<div class="footer">Holo Shutter · Local Wi-Fi control · No internet required</div>

<script>
"use strict";
const $ = id => document.getElementById(id);
let requestBusy = false;
function showMessage(text, isError=false) {
  $("message").textContent = text;
  $("message").style.color = isError ? "#ff9b9b" : "#9fc5ff";
}
async function api(path) {
  const response = await fetch(path,{method:"GET",cache:"no-store",headers:{"Accept":"application/json"}});
  let data;
  try { data = await response.json(); } catch (_) { throw new Error("Invalid response from controller."); }
  if (!response.ok || data.ok === false) throw new Error(data.error || ("HTTP " + response.status));
  return data;
}
async function sendCommand(path,pendingText) {
  if (requestBusy) return;
  requestBusy = true; showMessage(pendingText);
  try { const result=await api(path); showMessage(result.event || "Command accepted."); await refreshStatus(); }
  catch(error) { showMessage(error.message,true); $("connection").textContent="Controller unavailable"; }
  finally { requestBusy=false; }
}
function setDuration(ms) {
  $("duration").value=String(ms);
  showMessage("Preset selected: "+(ms===0.5?"0.5 milliseconds":ms===1?"1 millisecond":(ms/60000)+" minutes")+". Press START CUSTOM EXPOSURE to run it.");
}
async function startExposure() {
  const ms=Number($("duration").value);
  if (!Number.isFinite(ms) || ms<0.5 || ms>210000) {
    showMessage("Enter a duration from 0.5 to 210,000 milliseconds.",true); return;
  }
  const us=Math.round(ms*1000);
  await sendCommand("/api/expose?us="+us,"Sending "+ms+" ms exposure command…");
}
async function saveSettings() {
  const open=Number($("openAngle").value), closed=Number($("closedAngle").value);
  if (!Number.isInteger(open)||!Number.isInteger(closed)||open<0||open>180||closed<0||closed>180) {
    showMessage("Servo angles must be whole numbers from 0 to 180.",true); return;
  }
  await sendCommand("/api/settings?open="+open+"&closed="+closed,"Applying servo calibration…");
}
async function refreshStatus() {
  try {
    const s=await api("/api/status"); $("connection").textContent="Wi-Fi connected";
    const state=$("state");
    if(s.exposureActive){state.textContent="EXPOSURE ACTIVE";state.className="state active";}
    else if(s.open){state.textContent="OPEN";state.className="state open";}
    else{state.textContent="CLOSED";state.className="state closed";}
    if(Number.isInteger(s.openAngle))$("openAngle").value=s.openAngle;
    if(Number.isInteger(s.closedAngle))$("closedAngle").value=s.closedAngle;
  } catch(error) {
    $("connection").textContent="Not connected";$("state").textContent="No response";$("state").className="state";
  }
}
window.addEventListener("online",refreshStatus);
window.addEventListener("pageshow",refreshStatus);
refreshStatus();setInterval(refreshStatus,1200);
</script>
</body>
</html>
)HOLOPAGE";

void sendJson(const String& body, int code=200) {
  server.sendHeader("Cache-Control","no-store");
  server.send(code,"application/json",body);
}

void closeShutter(const char* eventName) {
  exposureActive=false;
  shutter.write(closedAngle);
  isOpen=false;
  sendJson(String("{\"ok\":true,\"event\":\"")+eventName+"\"}");
}

void setupRoutes() {
  server.on("/",HTTP_GET,[](){
    server.sendHeader("Cache-Control","no-store");
    server.send_P(200,"text/html",PAGE);
  });

  server.on("/api/status",HTTP_GET,[](){
    String json="{\"ok\":true,\"open\":";
    json += isOpen ? "true":"false";
    json += ",\"exposureActive\":";
    json += exposureActive ? "true":"false";
    json += ",\"openAngle\":"+String(openAngle);
    json += ",\"closedAngle\":"+String(closedAngle)+"}";
    sendJson(json);
  });

  server.on("/api/open",HTTP_GET,[](){
    exposureActive=false;
    shutter.write(openAngle);
    isOpen=true;
    sendJson("{\"ok\":true,\"event\":\"opened\"}");
  });
  server.on("/api/close",HTTP_GET,[](){closeShutter("closed");});
  server.on("/api/cancel",HTTP_GET,[](){closeShutter("cancelled_closed");});

  server.on("/api/expose",HTTP_GET,[](){
    if(!server.hasArg("us")){
      sendJson("{\"ok\":false,\"error\":\"missing us (microseconds)\"}",400);
      return;
    }
    uint32_t durationUs=(uint32_t)strtoul(server.arg("us").c_str(),nullptr,10);
    if(durationUs<500UL || durationUs>210000000UL){
      sendJson("{\"ok\":false,\"error\":\"us must be 500..210000000\"}",400);
      return;
    }
    shutter.write(openAngle);
    isOpen=true;
    exposureActive=true;
    closeAtUs=micros()+durationUs;
    sendJson("{\"ok\":true,\"event\":\"exposure_started\",\"durationUs\":"+String(durationUs)+"}");
  });

  server.on("/api/settings",HTTP_GET,[](){
    if(!server.hasArg("open")||!server.hasArg("closed")){
      sendJson("{\"ok\":false,\"error\":\"missing angle\"}",400);
      return;
    }
    int o=server.arg("open").toInt();
    int c=server.arg("closed").toInt();
    if(o<0||o>180||c<0||c>180){
      sendJson("{\"ok\":false,\"error\":\"angles must be 0..180\"}",400);
      return;
    }
    openAngle=o; closedAngle=c;
    shutter.write(isOpen?openAngle:closedAngle);
    sendJson("{\"ok\":true,\"event\":\"settings_updated\"}");
  });

  server.onNotFound([](){sendJson("{\"ok\":false,\"error\":\"not found\"}",404);});
}

void setup() {
  Serial.begin(115200);
  shutter.setPeriodHertz(50);
  shutter.attach(SERVO_PIN,500,2400);
  shutter.write(closedAngle);
  delay(300);

  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(IPAddress(192,168,4,1),IPAddress(192,168,4,1),IPAddress(255,255,255,0));
  if(!WiFi.softAP(AP_SSID,AP_PASSWORD,6,0,2)){
    Serial.println("ERROR: Wi-Fi access point failed.");
    while(true) delay(1000);
  }
  setupRoutes();
  server.begin();
  Serial.println();
  Serial.println("Holo Shutter ready.");
  Serial.print("Wi-Fi: ");Serial.println(AP_SSID);
  Serial.print("Control page: http://");Serial.println(WiFi.softAPIP());
}

void loop() {
  server.handleClient();
  if(exposureActive && (int32_t)(micros()-closeAtUs)>=0){
    exposureActive=false;
    shutter.write(closedAngle);
    isOpen=false;
    Serial.println("Timed exposure closed.");
  }
}
