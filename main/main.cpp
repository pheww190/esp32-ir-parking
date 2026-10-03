/*
 * ESP32 IR Parking Assist  (ESP-IDF project, Arduino core as a component)
 * ----------------------------------------------------------------------
 * Two IR obstacle sensors (left + right). The ESP32 hosts a small web page
 * on your Wi-Fi showing a top-down car; the matching side lights up red when
 * that sensor detects an obstacle.
 *
 * Wiring (change the two pins below if you like):
 *   IR LEFT :  VCC -> 3V3   GND -> GND   OUT -> GPIO 4
 *   IR RIGHT:  VCC -> 3V3   GND -> GND   OUT -> GPIO 5
 *
 * Most IR modules (FC-51 / TCRT5000) pull OUT LOW on detection -> ACTIVE_LOW
 * true. If yours reports HIGH on detection, set ACTIVE_LOW to false.
 *
 * After flashing, run `idf.py monitor` to read the printed IP, then open that
 * IP in a browser on the same Wi-Fi.
 *
 * setup()/loop() run automatically because sdkconfig.defaults sets
 * CONFIG_AUTOSTART_ARDUINO=y.
 */

#include "Arduino.h"
#include <WiFi.h>
#include <WebServer.h>

// ---- set these ----
static const char* WIFI_SSID     = "YOUR_WIFI_NAME";
static const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

static const int  IR_LEFT  = 4;   // GPIO for the left sensor OUT
static const int  IR_RIGHT = 5;   // GPIO for the right sensor OUT
static const bool ACTIVE_LOW = true;  // true if OUT goes LOW on detection
// -------------------

static WebServer server(80);

static const char PAGE_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>IR Parking Assist</title>
<style>
  :root{
    --bg:#0b0f1a; --panel:#131a2b; --line:#24304a;
    --ok:#2ee6a6; --danger:#ff3b5c; --txt:#e8eefc; --dim:#7f8ba6;
  }
  *{box-sizing:border-box}
  html,body{height:100%}
  body{
    margin:0;font-family:system-ui,"Segoe UI",Roboto,sans-serif;color:var(--txt);
    background:radial-gradient(1200px 700px at 50% -10%,#17203a,#0b0f1a 60%);
    display:flex;flex-direction:column;align-items:center;justify-content:center;
    gap:16px;padding:22px;
  }
  h1{font-size:15px;letter-spacing:.22em;text-transform:uppercase;color:var(--dim);
     font-weight:600;margin:0}

  /* indicator row */
  .indicators{display:flex;gap:26px;align-items:center}
  .ind{display:flex;align-items:center;gap:9px;font-size:13px;letter-spacing:.1em;color:var(--dim)}
  .dot{width:12px;height:12px;border-radius:50%;background:#2a3550;
       box-shadow:0 0 0 0 rgba(0,0,0,0);transition:.15s}
  .ind.on .dot{background:var(--danger);box-shadow:0 0 14px 3px rgba(255,59,92,.7)}
  .ind.on{color:var(--danger)}

  /* stage */
  .stage{
    position:relative;width:420px;max-width:94vw;aspect-ratio:420/460;
    background:linear-gradient(180deg,#0f1626,#0a1020);
    border:1px solid var(--line);border-radius:24px;overflow:hidden;
    box-shadow:0 30px 70px -40px #000, inset 0 1px 0 rgba(255,255,255,.03);
    transition:box-shadow .15s,border-color .15s;
  }
  .stage.alertL{border-color:rgba(255,59,92,.55);box-shadow:0 0 60px -10px rgba(255,59,92,.35)}
  .stage.alertR{border-color:rgba(255,59,92,.55);box-shadow:0 0 60px -10px rgba(255,59,92,.35)}
  .stage.alertB{border-color:rgba(255,59,92,.7);box-shadow:0 0 70px -8px rgba(255,59,92,.5)}

  .lane{position:absolute;inset:0;opacity:.45;
    background-image:repeating-linear-gradient(180deg,transparent 0 30px,#1c2740 30px 34px);
    -webkit-mask-image:radial-gradient(ellipse at 50% 60%,#000 40%,transparent 78%);
            mask-image:radial-gradient(ellipse at 50% 60%,#000 40%,transparent 78%);}
  .grid{position:absolute;inset:0;opacity:.35;
    background-image:linear-gradient(#141d33 1px,transparent 1px),
                     linear-gradient(90deg,#141d33 1px,transparent 1px);
    background-size:42px 42px}

  /* detection zones */
  .zone{position:absolute;bottom:-24px;width:250px;height:210px;cursor:pointer;
        opacity:0;transition:opacity .16s ease;border-radius:50%}
  .zone.l{left:-56px}
  .zone.r{right:-56px}
  .zone::before{content:"";position:absolute;inset:0;border-radius:50%;
        background:radial-gradient(ellipse at center,rgba(46,230,166,.30),rgba(46,230,166,0) 68%)}
  .zone.active{opacity:1}
  .zone.active::before{background:radial-gradient(ellipse at center,rgba(255,59,92,.55),rgba(255,59,92,0) 70%);
        animation:pulse 1s ease-in-out infinite}
  @keyframes pulse{0%,100%{opacity:.85}50%{opacity:1}}

  .obst{position:absolute;width:34px;height:34px;border-radius:8px;
        background:#1a2338;border:2px solid var(--danger);opacity:0;
        transform:scale(.6);transition:.18s;left:calc(50% - 17px);bottom:34px;
        box-shadow:0 0 16px rgba(255,59,92,.6)}
  .zone.active .obst{opacity:1;transform:scale(1)}
  .zone.r .obst{left:auto;right:calc(50% - 17px)}

  /* car */
  .car{position:absolute;left:50%;top:44%;transform:translate(-50%,-50%);
       width:150px;filter:drop-shadow(0 18px 30px rgba(0,0,0,.55))}
  .car svg{width:100%;display:block}
  .car.idle{animation:idle 2.6s ease-in-out infinite}
  @keyframes idle{0%,100%{transform:translate(-50%,-50%)}50%{transform:translate(-50%,calc(-50% - 2px))}}
  .car.nudgeL{transform:translate(calc(-50% + 10px),-50%)}
  .car.nudgeR{transform:translate(calc(-50% - 10px),-50%)}

  /* rear sensor beams */
  .beam{position:absolute;bottom:26px;width:4px;height:74px;border-radius:3px;
        background:linear-gradient(180deg,rgba(46,230,166,.9),rgba(46,230,166,0));
        opacity:.5;transform-origin:top center;transition:.15s}
  .beam.l{left:calc(50% - 46px);transform:rotate(24deg)}
  .beam.r{left:calc(50% + 46px);transform:rotate(-24deg)}
  .beam.on{background:linear-gradient(180deg,rgba(255,59,92,1),rgba(255,59,92,0));opacity:1}

  .caption{position:absolute;top:14px;left:0;right:0;text-align:center;
     font-size:12px;letter-spacing:.16em;color:var(--dim);text-transform:uppercase}

  .mode{font-size:12px;letter-spacing:.12em;text-transform:uppercase;padding:7px 14px;
        border-radius:999px;border:1px solid var(--line);color:var(--dim);background:var(--panel)}
  .mode.live{color:var(--ok);border-color:rgba(46,230,166,.45)}
</style>
</head>
<body>
  <h1>IR Parking Assist</h1>

  <div class="indicators">
    <div class="ind" id="indL"><span class="dot"></span>LEFT</div>
    <div class="ind" id="indR"><span class="dot"></span>RIGHT</div>
  </div>

  <div class="stage" id="stage">
    <div class="grid"></div>
    <div class="lane"></div>
    <div class="caption">reverse view</div>

    <div class="beam l" id="beamL"></div>
    <div class="beam r" id="beamR"></div>

    <div class="zone l" id="zoneL" title="Left IR sensor">
      <div class="obst"></div>
    </div>
    <div class="zone r" id="zoneR" title="Right IR sensor">
      <div class="obst"></div>
    </div>

    <div class="car idle" id="car">
      <svg viewBox="0 0 150 260" xmlns="http://www.w3.org/2000/svg">
        <!-- wheels -->
        <rect x="2"  y="52"  width="24" height="46" rx="9" fill="#0c1120"/>
        <rect x="124" y="52" width="24" height="46" rx="9" fill="#0c1120"/>
        <rect x="2"  y="162" width="24" height="46" rx="9" fill="#0c1120"/>
        <rect x="124" y="162" width="24" height="46" rx="9" fill="#0c1120"/>
        <!-- body -->
        <rect x="20" y="18" width="110" height="224" rx="40" fill="#c7d2e6"/>
        <rect x="20" y="18" width="110" height="224" rx="40" fill="url(#g)" opacity=".5"/>
        <!-- cabin -->
        <rect x="34" y="74" width="82" height="112" rx="26" fill="#0e1730"/>
        <rect x="40" y="82" width="70" height="42" rx="18" fill="#1b2743"/>
        <rect x="40" y="136" width="70" height="42" rx="18" fill="#1b2743"/>
        <!-- lights -->
        <rect x="30" y="20" width="24" height="10" rx="5" fill="#fff7cc"/>
        <rect x="96" y="20" width="24" height="10" rx="5" fill="#fff7cc"/>
        <rect x="30" y="230" width="24" height="10" rx="5" fill="#ff3b5c"/>
        <rect x="96" y="230" width="24" height="10" rx="5" fill="#ff3b5c"/>
        <defs>
          <linearGradient id="g" x1="0" y1="0" x2="1" y2="1">
            <stop offset="0" stop-color="#ffffff"/>
            <stop offset="1" stop-color="#7f8ba6"/>
          </linearGradient>
        </defs>
      </svg>
    </div>
  </div>

  <div class="mode" id="mode">demo &middot; tap a zone to simulate</div>

<script>
(function(){
  const stage = document.getElementById('stage');
  const car   = document.getElementById('car');
  const zoneL = document.getElementById('zoneL');
  const zoneR = document.getElementById('zoneR');
  const beamL = document.getElementById('beamL');
  const beamR = document.getElementById('beamR');
  const indL  = document.getElementById('indL');
  const indR  = document.getElementById('indR');
  const modeEl= document.getElementById('mode');

  let live = false;
  let state = { left:false, right:false };
  let demo = { left:false, right:false };

  function render(){
    const s = live ? state : demo;
    zoneL.classList.toggle('active', s.left);
    zoneR.classList.toggle('active', s.right);
    beamL.classList.toggle('on', s.left);
    beamR.classList.toggle('on', s.right);
    indL.classList.toggle('on', s.left);
    indR.classList.toggle('on', s.right);

    stage.classList.toggle('alertL', s.left && !s.right);
    stage.classList.toggle('alertR', s.right && !s.left);
    stage.classList.toggle('alertB', s.left && s.right);

    car.classList.toggle('nudgeR', s.left);   // steer away from left obstacle
    car.classList.toggle('nudgeL', s.right);
  }

  async function poll(){
    try{
      const r = await fetch('status', {cache:'no-store'});
      if(!r.ok) throw new Error('bad');
      const d = await r.json();
      state = { left: !!d.left, right: !!d.right };
      if(!live){ live = true; modeEl.textContent = 'live \u00b7 esp32 connected'; modeEl.className='mode live'; }
      render();
      setTimeout(poll, 150);
    }catch(e){
      if(live){ live = false; modeEl.textContent = 'demo \u00b7 tap a zone to simulate'; modeEl.className='mode'; }
      render();
      setTimeout(poll, 900);
    }
  }

  function toggleDemo(which){
    if(live) return;
    demo[which] = !demo[which];
    render();
  }

  zoneL.addEventListener('click', ()=>toggleDemo('left'));
  zoneR.addEventListener('click', ()=>toggleDemo('right'));

  render();
  poll();
})();
</script>
</body>
</html>

)rawliteral";

static bool detect(int pin) {
  int v = digitalRead(pin);
  return ACTIVE_LOW ? (v == LOW) : (v == HIGH);
}

static void handleRoot() {
  server.send_P(200, "text/html", PAGE_HTML);
}

static void handleStatus() {
  String json = "{\"left\":";
  json += detect(IR_LEFT)  ? "true" : "false";
  json += ",\"right\":";
  json += detect(IR_RIGHT) ? "true" : "false";
  json += "}";
  server.send(200, "application/json", json);
}

static void handleNotFound() {
  server.send(404, "text/plain", "Not found");
}

void setup() {
  Serial.begin(115200);
  delay(200);

  pinMode(IR_LEFT,  INPUT);
  pinMode(IR_RIGHT, INPUT);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(400);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Connected. Open this in a browser:  http://");
  Serial.println(WiFi.localIP());

  server.on("/", handleRoot);
  server.on("/status", handleStatus);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("Web server started.");
}

void loop() {
  server.handleClient();
}
