#include "web.h"
#include "config.h"
#include "settings.h"
#include "pulsator.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>

static WebServer server(80);
static DNSServer dns;

static const char PAGE[] PROGMEM = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Pulsator</title>
<style>
 body{font-family:system-ui,sans-serif;background:#120a10;color:#f3e8ee;margin:0;padding:16px;max-width:520px;margin:auto}
 h1{font-size:1.3rem;margin:8px 0 14px;letter-spacing:.12em}
 .card{background:#1e1219;border-radius:14px;padding:16px;margin-bottom:14px}
 .phase{font-size:1rem;text-transform:uppercase;letter-spacing:.18em;color:#b89aa8}
 .phase.BUILDUP{color:#ff6b9d}.phase.HOLD{color:#ffd1e3}.phase.CLIMAX{color:#fff}.phase.AFTERGLOW{color:#ffb070}.phase.IDLE{color:#8fd19e}
 .clock{font-size:4.2rem;font-weight:700;font-variant-numeric:tabular-nums;text-align:center;margin:4px 0}
 .bar{height:10px;border-radius:5px;background:#2e1c27;overflow:hidden;margin:8px 0}
 .bar>div{height:100%;background:linear-gradient(90deg,#ff3b7a,#ffd1e3);width:0;transition:width .3s}
 .heart{width:26px;height:26px;border-radius:50%;background:#ff3b7a;margin:6px auto;transform:scale(.8);transition:transform .08s}
 .heart.beat{transform:scale(1.25)}
 .row{display:flex;gap:8px;flex-wrap:wrap;margin-top:8px}
 button{flex:1;min-width:110px;padding:14px 10px;font-size:1rem;border:0;border-radius:10px;color:#fff;background:#3a2531;cursor:pointer}
 button.go{background:#c2185b}button.now{background:#e65100}button.stop{background:#555}
 label{display:block;font-size:.85rem;color:#b89aa8;margin-top:10px}
 input,select{width:100%;box-sizing:border-box;padding:10px;font-size:1rem;border-radius:8px;border:1px solid #4a3340;background:#271822;color:#f3e8ee}
 .two{display:grid;grid-template-columns:1fr 1fr;gap:10px}
 .kv{display:grid;grid-template-columns:auto 1fr;gap:4px 12px;font-size:.9rem;font-variant-numeric:tabular-nums}
 .kv b{color:#b89aa8;font-weight:500}
 .pump{display:inline-block;padding:4px 12px;border-radius:999px;background:#333;font-weight:700}
 .pump.on{background:#2e7d32}.pump.dry{background:#7a5a00}
 .sw{display:inline-block;width:100%;height:22px;border-radius:6px;border:1px solid #4a3340}
 pre{background:#000;color:#9f9;padding:8px;border-radius:8px;white-space:pre-wrap;word-break:break-all;font-size:.75rem;max-height:220px;overflow:auto;margin:6px 0 0}
 small{color:#8c7280}
 details summary{cursor:pointer;color:#b89aa8;padding:6px 0}
</style></head><body>
<h1>PULSATOR</h1>

<div class="card">
 <div class="phase" id="phase">...</div>
 <div class="clock" id="clock">--:--</div>
 <div class="heart" id="heart"></div>
 <div class="bar"><div id="bar"></div></div>
 <div class="row">
  <input type="number" id="secs" min="5" max="3600" style="flex:1" title="countdown seconds">
  <button class="go" style="flex:2" onclick="post('/api/start?secs='+secs.value)">Start countdown</button>
 </div>
 <div class="row">
  <button class="now" onclick="post('/api/climax')">Ejaculate now</button>
  <button class="stop" onclick="post('/api/stop')">Stop</button>
 </div>
</div>

<div class="card">
 <div class="phase">Debug: what the ESP is sending</div>
 <div class="kv" style="margin-top:8px">
  <b>Pump</b><span><span class="pump" id="pump">off</span> <small id="relay"></small></span>
  <b>LED strip</b><span><span class="sw" id="sw"></span><small id="led"></small></span>
  <b>Piezo</b><span id="piezo">silent</span>
  <b>Beat</b><span id="beat">-</span>
  <b>Spurt</b><span id="spurt">-</span>
  <b>Pump total</b><span id="total">0 s</span>
 </div>
 <div class="row">
  <button onclick="post('/api/test?what=pump')">Pump 300 ms</button>
  <button onclick="post('/api/test?what=lights')">Lights</button>
  <button onclick="post('/api/test?what=piezo')">Piezo</button>
 </div>
 <label>Event log</label>
 <pre id="log"></pre>
</div>

<div class="card">
 <details><summary>Settings (saved in flash)</summary>
 <form id="f" onsubmit="event.preventDefault();saveSettings()">
  <div class="two">
   <div><label>Countdown (s)</label><input name="bup" type="number" min="5" max="3600"></div>
   <div><label>Afterglow (s)</label><input name="aft" type="number" min="0" max="120"></div>
  </div>
  <div class="two">
   <div><label>Spurts (3-12)</label><input name="spn" type="number" min="3" max="12"></div>
   <div><label>Spurt strength % (20-200)</label><input name="sps" type="number" min="20" max="200"></div>
  </div>
  <div class="two">
   <div><label>LED count (1-300)</label><input name="leds" type="number" min="1" max="300"></div>
   <div><label>Brightness (0-255)</label><input name="bri" type="number" min="0" max="255"></div>
  </div>
  <div class="two">
   <div><label>Piezo</label><select name="pz"><option value="1">on</option><option value="0">off</option></select></div>
   <div><label>Piezo base tone (Hz)</label><input name="pzhz" type="number" min="200" max="8000"></div>
  </div>
  <div class="two">
   <div><label>Relay input</label><select name="rlow"><option value="1">active low (most modules)</option><option value="0">active high</option></select></div>
   <div><label>Dry run</label><select name="dry"><option value="0">off: pump runs</option><option value="1">on: relay never energised</option></select></div>
  </div>
  <div class="row"><button class="go" type="submit">Save settings</button></div>
  <small>Everything applies immediately. Dry run lets you rehearse the whole show with the pump disconnected or disabled; the debug box still shows what the pump would do.</small>
 </form>
 </details>
</div>

<script>
const $=id=>document.getElementById(id);
let editing=false; secs.onfocus=()=>editing=true; secs.onblur=()=>editing=false;
let lastBeatMs=0,lastBeatT=0,heartTimer=null;
function fmt(s){return String(Math.floor(s/60)).padStart(2,'0')+':'+String(s%60).padStart(2,'0')}
async function post(u){try{await fetch(u,{method:'POST'})}catch(e){} poll()}
function thump(){const h=$('heart');h.classList.add('beat');setTimeout(()=>h.classList.remove('beat'),90)}
async function poll(){
 try{
  const s=await (await fetch('/api/status')).json();
  $('phase').textContent=s.phase; $('phase').className='phase '+s.phase;
  $('clock').textContent=s.phase==='BUILDUP'?fmt(s.remaining):(s.phase==='IDLE'?fmt(s.buildup):(s.phase==='CLIMAX'?'!!!!':'00:00'));
  $('bar').style.width=s.progress+'%';
  if(!editing) secs.value=s.buildup;
  const p=$('pump'); p.textContent=s.pump?'ON':'off'; p.className='pump'+(s.pump?(s.dry?' dry':' on'):'');
  $('relay').textContent='relay pin '+(s.relayHigh?'HIGH':'LOW')+(s.dry?' (dry run)':'');
  $('sw').style.background=`rgb(${s.r},${s.g},${s.b})`;
  $('led').textContent=` rgb(${s.r},${s.g},${s.b}) avg, brightness ${s.bri}/255, ${s.leds} LEDs`;
  $('piezo').textContent=s.hz?s.hz+' Hz':'silent';
  $('beat').textContent=s.beat?s.beat+' ms ('+(60000/s.beat).toFixed(0)+' bpm)':'-';
  $('spurt').textContent=s.spurt?s.spurt+' of '+s.spurts:'-';
  $('total').textContent=(s.pumpTotal/1000).toFixed(1)+' s';
  $('log').textContent=s.log;
  // local heartbeat animation at the reported beat interval
  if(s.phase==='BUILDUP'&&s.beat){ if(s.beat!==lastBeatMs){lastBeatMs=s.beat; clearInterval(heartTimer); heartTimer=setInterval(thump,s.beat);} }
  else if(s.phase==='CLIMAX'){ if(s.pump) thump(); }
  else { clearInterval(heartTimer); heartTimer=null; lastBeatMs=0; }
 }catch(e){$('phase').textContent='no contact';$('phase').className='phase'}
}
async function loadSettings(){
 const s=await (await fetch('/api/settings')).json();
 for(const k in s){const el=f.elements[k]; if(el) el.value=s[k]}
}
async function saveSettings(){
 const body=new URLSearchParams(new FormData(f));
 try{const r=await fetch('/api/settings',{method:'POST',body}); await loadSettings(); alert(r.ok?'Saved':'Save failed');}catch(e){alert('Save failed: '+e)}
}
loadSettings(); poll(); setInterval(poll,200);
</script>
</body></html>
)HTML";

static String jsonEsc(const String& s) {
  String o;
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '"' || c == '\\') o += '\\';
    if (c == '\n') { o += "\\n"; continue; }
    o += c;
  }
  return o;
}

static void handleStatus() {
  Status s = pulsatorStatus();
  String j = "{";
  j += "\"phase\":\"" + String(phaseName(s.phase)) + "\",";
  j += "\"remaining\":" + String(s.remainingS) + ",";
  j += "\"buildup\":" + String(settings.buildupS) + ",";
  j += "\"progress\":" + String(s.progressPct) + ",";
  j += "\"beat\":" + String(s.beatMs) + ",";
  j += "\"spurt\":" + String(s.spurtIndex) + ",";
  j += "\"spurts\":" + String(settings.spurts) + ",";
  j += "\"pump\":" + String(s.pumpOn ? "true" : "false") + ",";
  j += "\"relayHigh\":" + String(s.relayPinHigh ? "true" : "false") + ",";
  j += "\"dry\":" + String(s.dryRun ? "true" : "false") + ",";
  j += "\"r\":" + String(s.ledR) + ",\"g\":" + String(s.ledG) + ",\"b\":" + String(s.ledB) + ",";
  j += "\"bri\":" + String(s.ledBrightness) + ",\"leds\":" + String(settings.numLeds) + ",";
  j += "\"hz\":" + String(s.piezoHz) + ",";
  j += "\"pumpTotal\":" + String(s.pumpOnTotalMs) + ",";
  j += "\"log\":\"" + jsonEsc(logTail(20)) + "\"";
  j += "}";
  server.send(200, "application/json", j);
}

static void handleSettingsGet() {
  String j = "{";
  j += "\"bup\":" + String(settings.buildupS) + ",";
  j += "\"aft\":" + String(settings.afterglowS) + ",";
  j += "\"spn\":" + String(settings.spurts) + ",";
  j += "\"sps\":" + String(settings.spurtStrength) + ",";
  j += "\"leds\":" + String(settings.numLeds) + ",";
  j += "\"bri\":" + String(settings.brightness) + ",";
  j += "\"pz\":\"" + String(settings.piezoOn ? 1 : 0) + "\",";
  j += "\"pzhz\":" + String(settings.piezoHz) + ",";
  j += "\"rlow\":\"" + String(settings.relayActiveLow ? 1 : 0) + "\",";
  j += "\"dry\":\"" + String(settings.dryRun ? 1 : 0) + "\"";
  j += "}";
  server.send(200, "application/json", j);
}

static void handleSettingsPost() {
  if (server.hasArg("bup"))  settings.buildupS      = server.arg("bup").toInt();
  if (server.hasArg("aft"))  settings.afterglowS    = server.arg("aft").toInt();
  if (server.hasArg("spn"))  settings.spurts        = server.arg("spn").toInt();
  if (server.hasArg("sps"))  settings.spurtStrength = server.arg("sps").toInt();
  if (server.hasArg("leds")) settings.numLeds       = server.arg("leds").toInt();
  if (server.hasArg("bri"))  settings.brightness    = server.arg("bri").toInt();
  if (server.hasArg("pz"))   settings.piezoOn       = server.arg("pz") == "1";
  if (server.hasArg("pzhz")) settings.piezoHz       = server.arg("pzhz").toInt();
  if (server.hasArg("rlow")) settings.relayActiveLow= server.arg("rlow") == "1";
  if (server.hasArg("dry"))  settings.dryRun        = server.arg("dry") == "1";
  settingsSave();
  logLine("settings saved: countdown " + String(settings.buildupS) + " s, " + String(settings.spurts) + " spurts @" +
          String(settings.spurtStrength) + "%, " + String(settings.numLeds) + " LEDs, relay " +
          (settings.relayActiveLow ? "active-low" : "active-high") + (settings.dryRun ? ", DRY RUN" : ""));
  server.send(200, "text/plain", "ok");
}

void webBegin() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);
  dns.start(53, "*", WiFi.softAPIP());
  Serial.printf("[web] hotspot %s, open http://%s/\n", AP_SSID, WiFi.softAPIP().toString().c_str());

  server.on("/", HTTP_GET, []() { server.send_P(200, "text/html", PAGE); });
  server.on("/api/status",   HTTP_GET,  handleStatus);
  server.on("/api/settings", HTTP_GET,  handleSettingsGet);
  server.on("/api/settings", HTTP_POST, handleSettingsPost);
  server.on("/api/start",    HTTP_POST, []() { long s = server.arg("secs").toInt(); pulsatorStart(s > 0 ? s : 0); server.send(200, "text/plain", "ok"); });
  server.on("/api/climax",   HTTP_POST, []() { pulsatorClimaxNow(); server.send(200, "text/plain", "ok"); });
  server.on("/api/stop",     HTTP_POST, []() { pulsatorStop(); server.send(200, "text/plain", "ok"); });
  server.on("/api/test",     HTTP_POST, []() {
    String w = server.arg("what");
    if (w == "pump") pulsatorTestPump(300);
    else if (w == "lights") pulsatorTestLights();
    else if (w == "piezo") pulsatorTestPiezo();
    server.send(200, "text/plain", "ok");
  });
  server.onNotFound([]() {  // captive-portal probes land on the page
    server.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/", true);
    server.send(302, "text/plain", "");
  });
  server.begin();
}

void webLoop() {
  dns.processNextRequest();
  server.handleClient();
}
