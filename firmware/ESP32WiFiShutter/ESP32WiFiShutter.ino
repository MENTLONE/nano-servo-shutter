/*
  ESP32 Wi-Fi Servo Shutter
  Adapted from the Nano Servo Shutter project.
  Hardware: classic ESP32 DevKit / ESP32-WROOM-32 + SG90 servo.
  Servo signal: GPIO18. Servo power: external regulated 5V supply.
  This firmware creates its own Wi-Fi access point and serves a phone-friendly UI.

  IMPORTANT:
  - EXPOSE timer starts when the OPEN command is issued, not when the servo
    physically clears the beam. Measure actual optical timing separately.
  - This servo shutter is NOT a laser safety interlock.
*/

#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>

static const int SERVO_PIN = 18;
static const char* AP_SSID = "HoloShutter-ESP32";
static const char* AP_PASSWORD = "HoloShutter530"; // Change before use (8+ chars).

WebServer server(80);
Servo shutter;

int openAngle = 68;
int closedAngle = 0;
bool isOpen = false;
bool exposureActive = false;
uint32_t closeAt = 0;

const char PAGE[] PROGMEM = R"HOLOPAGE(
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="theme-color" content="#111827">
<title>Holo Shutter</title>
<style>
:root{color-scheme:dark;font-family:-apple-system,BlinkMacSystemFont,system-ui,sans-serif;background:#111827;color:#f3f4f6}
*{box-sizing:border-box}body{max-width:680px;margin:0 auto;padding:22px 16px 40px}
h1{font-size:27px;margin:4px 0}h2{font-size:18px;margin:0 0 14px}
.sub{color:#9ca3af;margin:5px 0 20px}.panel{background:#1f2937;border:1px solid #374151;border-radius:15px;padding:17px;margin:14px 0}
.row{display:flex;gap:10px;flex-wrap:wrap;align-items:center}
button{border:0;border-radius:10px;padding:14px 16px;font-size:16px;font-weight:700;color:white;background:#374151;flex:1;min-width:115px}
button.primary{background:#2563eb}button.danger{background:#b91c1c}button:active{filter:brightness(1.2)}
input{width:110px;max-width:100%;background:#111827;border:1px solid #4b5563;border-radius:8px;color:white;padding:11px;font-size:17px}
label{display:flex;justify-content:space-between;align-items:center;gap:12px;margin:12px 0}
.status{font-size:18px;font-weight:700}.green{color:#86efac}.yellow{color:#fbbf24}
small{color:#9ca3af;line-height:1.5}.warn{border-left:4px solid #f59e0b;background:#292319;padding:13px;border-radius:8px;line-height:1.5}
#msg{color:#93c5fd;min-height:22px;margin-top:12px}
</style>
</head>
<body>
<h1>Holo Shutter</h1>
<p class="sub">ESP32 Wi-Fi control · SG90 mechanical shutter</p>
<section class="panel">
  <div class="row"><span>Status</span><strong id="status" class="status yellow">Checking…</strong></div>
  <p><small id="wifi">Connect your phone to the shutter Wi-Fi network.</small></p>
</section>
<section class="panel">
  <h2>Manual control</h2>
  <div class="row">
    <button class="primary" onclick="act('open')">Open</button>
    <button class="danger" onclick="act('close')">Close</button>
    <button onclick="act('cancel')">Cancel / Close</button>
  </div>
</section>
<section class="panel">
  <h2>Timed exposure</h2>
  <label><span>Duration (milliseconds)</span><input id="duration" type="number" min="1" max="3600000" value="1000" inputmode="numeric"></label>
  <button class="primary" onclick="expose()">Start timed exposure</button>
  <p><small>Timer begins when the ESP32 commands the servo to open. Servo movement is included in the commanded interval.</small></p>
</section>
<section class="panel">
  <h2>Servo calibration</h2>
  <label><span>Open angle</span><input id="openAngle" type="number" min="0" max="180" value="68"></label>
  <label><span>Closed angle</span><input id="closedAngle" type="number" min="0" max="180" value="0"></label>
  <button onclick="saveSettings()">Apply angles</button>
  <p><small>Adjust angles with the laser disabled. Avoid forcing the servo against its mechanical stops.</small></p>
</section>
<p id="msg" role="status"></p>
<p class="warn"><strong>Laser safety:</strong> This servo is not a safety interlock. Never rely on it as the only beam barrier. Use an independent secure beam stop/enclosure. Test all motion with the laser disabled.</p>
<script>
async function api(path){
  const r=await fetch(path,{cache:'no-store'});
  const j=await r.json();
  if(!r.ok||j.ok===false)throw new Error(j.error||'Request failed');
  return j;
}
function message(s){document.getElementById('msg').textContent=s;}
async function act(cmd){try{const j=await api('/api/'+cmd);message(j.event||'Command sent');await refresh();}catch(e){message('Error: '+e.message);}}
async function expose(){
  const ms=Number(document.getElementById('duration').value);
  if(!Number.isInteger(ms)||ms<1||ms>3600000){message('Enter a whole number from 1 to 3,600,000 ms.');return;}
  try{const j=await api('/api/expose?ms='+ms);message(j.event+' ('+ms+' ms requested)');await refresh();}catch(e){message('Error: '+e.message);}
}
async function saveSettings(){
  const o=Number(document.getElementById('openAngle').value);
  const c=Number(document.getElementById('closedAngle').value);
  if(!Number.isInteger(o)||!Number.isInteger(c)||o<0||o>180||c<0||c>180){message('Angles must be whole numbers from 0 to 180.');return;}
  try{await api('/api/settings?open='+o+'&closed='+c);message('Servo angles saved in RAM.');}catch(e){message('Error: '+e.message);}
}
async function refresh(){
  try{
    const s=await api('/api/status');
    const el=document.getElementById('status');
    el.textContent=s.exposureActive?'EXPOSURE ACTIVE':(s.open?'OPEN':'CLOSED');
    el.className='status '+(s.exposureActive?'green':'yellow');
    document.getElementById('openAngle').value=s.openAngle;
    document.getElementById('closedAngle').value=s.closedAngle;
  }catch(e){document.getElementById('status').textContent='Not responding';}
}
setInterval(refresh,1200);refresh();
</script>
</body></html>
)HOLOPAGE";

void sendJson(const String& body, int code = 200) {
  server.sendHeader("Cache-Control", "no-store");
  server.send(code, "application/json", body);
}

void closeShutter(const char* eventName) {
  exposureActive = false;
  shutter.write(closedAngle);
  isOpen = false;
  sendJson(String("{\"ok\":true,\"event\":\"") + eventName + "\"}");
}

void setupRoutes() {
  server.on("/", HTTP_GET, []() {
    server.sendHeader("Cache-Control", "no-store");
    server.send_P(200, "text/html", PAGE);
  });

  server.on("/api/status", HTTP_GET, []() {
    String json = "{\"ok\":true,\"open\":";
    json += isOpen ? "true" : "false";
    json += ",\"exposureActive\":";
    json += exposureActive ? "true" : "false";
    json += ",\"openAngle\":" + String(openAngle);
    json += ",\"closedAngle\":" + String(closedAngle) + "}";
    sendJson(json);
  });

  server.on("/api/open", HTTP_GET, []() {
    exposureActive = false;
    shutter.write(openAngle);
    isOpen = true;
    sendJson("{\"ok\":true,\"event\":\"opened\"}");
  });

  server.on("/api/close", HTTP_GET, []() {
    closeShutter("closed");
  });

  server.on("/api/cancel", HTTP_GET, []() {
    closeShutter("cancelled_closed");
  });

  server.on("/api/expose", HTTP_GET, []() {
    if (!server.hasArg("ms")) {
      sendJson("{\"ok\":false,\"error\":\"missing ms\"}", 400);
      return;
    }
    long ms = server.arg("ms").toInt();
    if (ms < 1 || ms > 3600000L) {
      sendJson("{\"ok\":false,\"error\":\"ms must be 1..3600000\"}", 400);
      return;
    }
    shutter.write(openAngle);
    isOpen = true;
    exposureActive = true;
    closeAt = millis() + (uint32_t)ms;
    sendJson("{\"ok\":true,\"event\":\"exposure_started\"}");
  });

  server.on("/api/settings", HTTP_GET, []() {
    if (!server.hasArg("open") || !server.hasArg("closed")) {
      sendJson("{\"ok\":false,\"error\":\"missing angle\"}", 400);
      return;
    }
    int o = server.arg("open").toInt();
    int c = server.arg("closed").toInt();
    if (o < 0 || o > 180 || c < 0 || c > 180) {
      sendJson("{\"ok\":false,\"error\":\"angles must be 0..180\"}", 400);
      return;
    }
    openAngle = o;
    closedAngle = c;
    shutter.write(isOpen ? openAngle : closedAngle);
    sendJson("{\"ok\":true,\"event\":\"settings_updated\"}");
  });

  server.onNotFound([]() {
    sendJson("{\"ok\":false,\"error\":\"not found\"}", 404);
  });
}

void setup() {
  Serial.begin(115200);
  shutter.setPeriodHertz(50);
  shutter.attach(SERVO_PIN, 500, 2400);
  shutter.write(closedAngle);
  isOpen = false;
  exposureActive = false;
  delay(300);

  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1),
                    IPAddress(192, 168, 4, 1),
                    IPAddress(255, 255, 255, 0));
  if (!WiFi.softAP(AP_SSID, AP_PASSWORD, 6, 0, 2)) {
    Serial.println("ERROR: Wi-Fi access point failed.");
    while (true) delay(1000);
  }

  setupRoutes();
  server.begin();

  Serial.println();
  Serial.println("Holo Shutter ready.");
  Serial.print("Wi-Fi network: "); Serial.println(AP_SSID);
  Serial.print("Control page: http://"); Serial.println(WiFi.softAPIP());
}

void loop() {
  server.handleClient();

  if (exposureActive && (int32_t)(millis() - closeAt) >= 0) {
    exposureActive = false;
    shutter.write(closedAngle);
    isOpen = false;
    Serial.println("Timed exposure closed.");
  }
}
