#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <math.h>
#define FREQUENCY_868
#include <LoRa_E220.h>
#include <SPI.h>
#include <SD.h>

//SD vars
SPIClass spiSD(HSPI);
char *testfilename = "/kkt.txt";
#define SD_CS   27
#define SD_MOSI 13
#define SD_MISO 34
#define SD_SCK  14

//LoRa vars
#define LORA_M0 2
#define LORA_M1 4
#define LORA_RX 17 //tx pin on esp
#define LORA_TX 16 //rx pin on esp
#define LORA_AUX 5
LoRa_E220 e220ttl(LORA_RX, LORA_TX, &Serial2, LORA_AUX, LORA_M0, LORA_M1, UART_BPS_RATE_9600);

const char* SSID = "arct-signalis.local";
const char* PASS = "12345678";

#define LED_PIN 2
const uint32_t BLINK_MS = 400;

WebServer server(80);
IPAddress apIP(192,168,4,1);

const char INDEX_HTML[] PROGMEM = R"HTML(
<!doctype html><html><head><meta charset="utf-8"/>
<meta name="viewport" content="width=device-width,initial-scale=1"/>
<title>ARCT-SIGNALIS</title>
<style>
html,body{height:100%}body{margin:0;display:flex;align-items:center;justify-content:center;background:#0f1220;font-family:system-ui,-apple-system,Segoe UI,Roboto,Helvetica,Arial,sans-serif;color:#e6e8ef}
.wrap{text-align:center}
a.btn{display:inline-block;background:#fff;color:#0f1220;text-decoration:none;padding:14px 20px;border-radius:12px;font-weight:700}
p{font-size:12px;opacity:.7;margin-top:10px}
</style></head>
<body>
<div class="wrap">
<a class="btn" href="/app">Open dashboard</a>
<p>Or open http://arct-signalis.local/app or http://192.168.4.1/app</p>
</div>
</body></html>
)HTML";

const char APP_HTML[] PROGMEM = R"HTML(
<!doctype html><html><head><meta charset="utf-8"/>
<meta name="viewport" content="width=device-width,initial-scale=1"/>
<title>ARCT-SIGNALIS • Dashboard</title>
<style>
:root{
  --bg:#f8f9fa;
  --card:#ffffff;
  --muted:#6c757d;
  --fg:#212529;
  --grid:#dee2e6;
  --accent:#0d6efd;
}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--fg);font-family:system-ui,-apple-system,Segoe UI,Roboto,Helvetica,Arial,sans-serif}
.container{max-width:1400px;margin:24px auto;padding:16px}
h1{margin:0 0 10px 0;font-size:22px}
h2{margin:0 0 8px 0;font-size:18px}
.card{background:var(--card);padding:16px;border-radius:14px;box-shadow:0 2px 18px rgba(0,0,0,.1)}
.grid{display:grid;grid-template-columns:minmax(420px,1.2fr) minmax(320px,.8fr);gap:16px}
@media(max-width:1100px){.grid{grid-template-columns:1fr}}
.row{display:flex;align-items:center;gap:12px;flex-wrap:wrap}
.badge{font-size:11px;background:var(--accent);color:#ffffff;border-radius:999px;padding:4px 8px}
small{color:var(--muted)}
pre{white-space:pre-wrap;word-break:break-word;background:#e9ecef;color:#212529;padding:12px;border-radius:8px;font-size:12px}
.kv{display:grid;grid-template-columns:repeat(3,1fr);gap:8px}
.kv div{background:#f1f3f5;border:1px solid #ced4da;border-radius:10px;padding:10px}
.kv label{display:block;font-size:11px;color:var(--muted);margin-bottom:6px}
.kv span{font-weight:700;font-size:16px}
.canvasWrap{position:relative;touch-action:none}
#gl{width:100%;aspect-ratio:16/9;height:auto;min-height:260px;max-height:620px;display:block;border-radius:14px;border:1px solid #ced4da;background:linear-gradient(#e9ecef,#f8f9fa)}
.overlay-axes{position:absolute;left:10px;bottom:10px;width:120px;height:120px;border-radius:10px;border:1px solid #ced4da;background:#ffffff;display:grid;place-items:center}
.overlay-axes canvas{width:100%;height:100%;display:block}
.charts{display:grid;grid-template-columns:repeat(3,1fr);gap:12px}
@media(max-width:1200px){.charts{grid-template-columns:repeat(2,1fr)}}
@media(max-width:900px){.charts{grid-template-columns:1fr}}
.chartCard{background:#f1f3f5;border:1px solid #dee2e6;border-radius:12px;padding:12px}
.chartCard h3{margin:0 0 6px 0;font-size:14px;color:#495057}
canvas.plot{width:100%;height:170px;display:block}
footer{margin-top:10px;text-align:center;color:var(--muted);font-size:12px}
.dirLabel{display:inline-block;margin-left:8px;color:#212529;font-weight:700}

/* Tab navigation */
.tabs{display:flex;gap:8px;margin-bottom:16px;border-bottom:2px solid #dee2e6}
.tab{padding:10px 20px;background:none;border:none;color:var(--muted);cursor:pointer;font-size:14px;font-weight:600;border-bottom:3px solid transparent;transition:all 0.2s}
.tab:hover{color:var(--fg);background:#f1f3f5}
.tab.active{color:var(--accent);border-bottom-color:var(--accent)}
.tab:disabled{opacity:0.4;cursor:not-allowed}
.tab-content{display:none}
.tab-content.active{display:block}
.sd-warning{background:#fff3cd;border:1px solid #ffeeba;color:#856404;padding:12px;border-radius:8px;margin-bottom:16px;font-size:13px}
.sd-status{display:inline-flex;align-items:center;gap:6px;font-size:12px;padding:4px 10px;border-radius:6px;background:#f1f3f5}
.sd-status.available{color:#198754;border:1px solid #145c32}
.sd-status.unavailable{color:#dc3545;border:1px solid #842029}
.status-dot{width:8px;height:8px;border-radius:50%;background:currentColor}

/* Preflight */
.pf-list{display:grid;gap:10px;margin-top:10px}
.pf-item{display:flex;align-items:center;justify-content:space-between;gap:12px;padding:12px;border-radius:12px;border:1px solid #dee2e6;background:#f1f3f5}
.pf-left{display:flex;align-items:center;gap:10px;min-width:0}
.pf-dot{width:10px;height:10px;border-radius:50%;background:#adb5bd;flex:0 0 auto}
.pf-label{font-weight:700}
.pf-right{font-variant-numeric:tabular-nums;color:var(--muted);white-space:nowrap}
.pf-ok .pf-dot{background:#198754}
.pf-bad .pf-dot{background:#dc3545}
.pf-warn .pf-dot{background:#ffc107}
.pf-summary{margin-top:12px;padding:12px;border-radius:12px;border:1px solid #dee2e6;background:#ffffff}
.pf-summary strong{display:inline-block;margin-right:8px}

</style></head>
<body>
<div class="container">
  <div class="row">
    <h1>Dashboard</h1>
    <div class="sd-status" id="sdStatus">
      <span class="status-dot"></span>
      <span id="sdStatusText">Checking SD...</span>
    </div>
  </div>

  <!-- Tab Navigation -->
  <div class="tabs">
    <button class="tab active" data-tab="preflight" id="preflightTab">Preflight</button>
    <button class="tab" data-tab="telemetry" id="telemetryTab" disabled>Telemetry</button>
    <button class="tab" data-tab="navigation" id="navTab">Replay Simulation</button>
  </div>

  <!-- Tab: Preflight -->
  <div class="tab-content active" id="preflight">
    <div class="card">
      <h2>Preflight status</h2>

      <div class="pf-list">
        <div class="pf-item" id="pf_gps">
          <div class="pf-left">
            <span class="pf-dot"></span>
            <span class="pf-label">GPS lock</span>
          </div>
          <span class="pf-right" id="pf_gps_val">Checking...</span>
        </div>

        <div class="pf-item" id="pf_sensors">
          <div class="pf-left">
            <span class="pf-dot"></span>
            <span class="pf-label">Sensors calibrated</span>
          </div>
          <span class="pf-right" id="pf_sensors_val">Checking...</span>
        </div>

        <div class="pf-item" id="pf_radio">
          <div class="pf-left">
            <span class="pf-dot"></span>
            <span class="pf-label">Radio link</span>
          </div>
          <span class="pf-right" id="pf_radio_val">Checking...</span>
        </div>

        <div class="pf-item" id="pf_batt">
          <div class="pf-left">
            <span class="pf-dot"></span>
            <span class="pf-label">Battery voltage</span>
          </div>
          <span class="pf-right" id="pf_batt_val">Checking...</span>
        </div>
      </div>

      <div class="pf-summary" id="pf_ready_box">
        <strong>Ready for launch:</strong>
        <span id="pf_ready_val">Checking...</span>
      </div>

      <footer>ARCT-SIGNALIS</footer>
    </div>
  </div>


  <!-- Tab: Telemetry -->
  <div class="tab-content" id="telemetry">
    <div class="grid">
      <div class="card">
        <h2>Values</h2>
        <div class="kv">
          <div><label>Latitude</label><span id="lat">-</span></div>
          <div><label>Longitude</label><span id="lon">-</span></div>
          <div><label>Elevation [m]</label><span id="elev">-</span></div>
          <div><label>Yaw [°]</label><span id="yaw">-</span></div>
          <div><label>Pitch [°]</label><span id="pitch">-</span></div>
          <div><label>Roll [°]</label><span id="roll">-</span></div>
        </div>
        <h2 style="margin-top:14px">Raw (hex dump)</h2>
        <pre id="raw">-</pre>
      </div>
    </div>

    <div class="card" style="margin-top:16px">
      <h2>Live charts</h2>
      <div class="charts">
        <div class="chartCard"><h3>Latitude</h3><canvas class="plot" id="c_lat"></canvas></div>
        <div class="chartCard"><h3>Longitude</h3><canvas class="plot" id="c_lon"></canvas></div>
        <div class="chartCard"><h3>Elevation [m]</h3><canvas class="plot" id="c_elev"></canvas></div>
        <div class="chartCard"><h3>Yaw</h3><canvas class="plot" id="att_yaw"></canvas></div>
        <div class="chartCard"><h3>Pitch</h3><canvas class="plot" id="att_pitch"></canvas></div>
        <div class="chartCard"><h3>Roll</h3><canvas class="plot" id="att_roll"></canvas></div>

      </div>
      <footer>ARCT-SIGNALIS</footer>
    </div>
  </div>

  <!-- Tab: Navigation (SD Card Required) -->
  <div class="tab-content" id="navigation">
    <div class="sd-warning" id="sdWarning" style="display:none">
      SD card not detected. Insert SD card to access replay features.
    </div>
    
    <div class="card" id="navContent">
      <h2>3D Simulation</h2>
      <div class="canvasWrap">
        <canvas id="gl"></canvas>
        <div class="overlay-axes"><canvas id="axes2d"></canvas></div>
      </div>
      
      <h2 style="margin-top:20px">Flight Path Log</h2>
      <p style="color:var(--muted);font-size:13px">Flight data is being logged to SD card: /flight_log.txt</p>
    </div>
  </div>
</div>

<script>
let sdAvailable = false;

async function checkSDCard(){
  try{
    const r = await fetch('/sd-status');
    const data = await r.json();
    sdAvailable = data.available;
    
    const statusEl = document.getElementById('sdStatus');
    const statusText = document.getElementById('sdStatusText');
    const navTab = document.getElementById('navTab');
    const sdWarning = document.getElementById('sdWarning');
    const navContent = document.getElementById('navContent');
    
    if(sdAvailable){
      statusEl.className = 'sd-status available';
      statusText.textContent = 'SD Card OK';
      navTab.disabled = false;
      sdWarning.style.display = 'none';
      navContent.style.display = 'block';
    } else {
      statusEl.className = 'sd-status unavailable';
      statusText.textContent = 'No SD Card';
      navTab.disabled = true;
      sdWarning.style.display = 'block';
      navContent.style.display = 'none';
    }
  }catch(e){
    console.error('SD check failed:', e);
  }
}

//tab switching
document.querySelectorAll('.tab').forEach(tab => {
  tab.addEventListener('click', ()=>{
    if(tab.disabled) return;
    
    document.querySelectorAll('.tab').forEach(t => t.classList.remove('active'));
    document.querySelectorAll('.tab-content').forEach(c => c.classList.remove('active'));
    
    tab.classList.add('active');
    const targetId = tab.dataset.tab;
    document.getElementById(targetId).classList.add('active');
    
    if(targetId === 'navigation'){
      requestAnimationFrame(()=>{
        resizeGL();
        drawMiniAxes2D(smooth.yaw, smooth.pitch, smooth.roll);
      });
    }

    if(targetId === 'telemetry'){
      requestAnimationFrame(resizeAllPlots);
    }

  });
});

checkSDCard();
setInterval(checkSDCard, 5000);

const $t=document.getElementById('typeBadge');
const $lat=document.getElementById('lat'); const $lon=document.getElementById('lon'); const $e=document.getElementById('elev');
const $yaw=document.getElementById('yaw'); const $pit=document.getElementById('pitch'); const $rol=document.getElementById('roll');
const $raw=document.getElementById('raw');

function d2r(d){return d*Math.PI/180;}
function headingLabel(deg){ const dirs=["N","NE","E","SE","S","SW","W","NW"]; let x=((deg%360)+360)%360; return dirs[Math.round(x/45)%8]; }
function hexDump(buf){
  const u = new Uint8Array(buf);
  const dv = new DataView(buf);
  const lines = [];
  function chunk(from, len, label, valStr){
    const bytes = [];
    for(let i=0;i<len;i++){ bytes.push(u[from + i].toString(16).padStart(2,'0')); }
    const off = from.toString(16).padStart(4,'0');
    let line = off + ": " + bytes.join(' ');
    if(label){ line = line.padEnd(38, ' ') + " | " + label + (valStr != null ? " = " + valStr : ""); }
    lines.push(line);
  }
  chunk(4, 4, "lon (float32 LE)",   dv.getFloat32(4,  true).toFixed(6));
  chunk(8, 4, "lat (float32 LE)",   dv.getFloat32(8,  true).toFixed(6));
  chunk(12,4, "elev (float32 LE)",  dv.getFloat32(12, true).toFixed(2));
  chunk(16,4, "roll (float32 LE)",  dv.getFloat32(16, true).toFixed(1));
  chunk(20,4, "pitch (float32 LE)", dv.getFloat32(20, true).toFixed(1));
  chunk(24,4, "yaw (float32 LE)",   dv.getFloat32(24, true).toFixed(1));
  return lines.join('\n');
}

let target={yaw:0,pitch:0,roll:0,elev:450,lat:0,lon:0};
let smooth={yaw:0,pitch:0,roll:0,elev:450};

function AxisPlot(canvas, cap=300, opts={}){
  const ctx=canvas.getContext('2d'), d=window.devicePixelRatio||1, a=[];
  const padL=48*d, padR=8*d, padT=8*d, padB=22*d;
  function resize(){ canvas.width=canvas.clientWidth*d; canvas.height=canvas.clientHeight*d; draw(); }
  function push(v){ if(!Number.isFinite(v)) return; a.push(v); if(a.length>cap)a.shift(); draw(); }
  function draw(){
    const w=canvas.width, h=canvas.height, ox=padL, oy=padT, pw=w-padL-padR, ph=h-padT-padB;
    ctx.clearRect(0,0,w,h);
    ctx.strokeStyle="#1a2040"; ctx.lineWidth=1*d; ctx.strokeRect(ox,oy,pw,ph);
    if(a.length<2) return;
    let mn=Infinity,mx=-Infinity; for(const v of a){ if(v<mn)mn=v; if(v>mx)mx=v; }
    if(mx===mn){ mx=mn+1; } const pad=(mx-mn)*.15; mn-=pad; mx+=pad;
    ctx.fillStyle="#464952ff"; ctx.font=(10*d)+"px system-ui";
    const yt=[mn,(mn+mx)/2,mx];
    yt.forEach(tv=>{ const y=oy+ph-(tv-mn)/(mx-mn)*ph; ctx.beginPath(); ctx.moveTo(ox,y); ctx.lineTo(ox+pw,y); ctx.stroke(); const lab=(opts.fixed!=null)? tv.toFixed(opts.fixed) : tv.toFixed(2); ctx.fillText(lab, 6*d, y+3*d); });
    const xt=[0, Math.max(0,(a.length-1)>>1), a.length-1];
    xt.forEach(ix=>{ const x=ox+(ix/(a.length-1))*pw; ctx.beginPath(); ctx.moveTo(x,oy+ph); ctx.lineTo(x,oy+ph+4*d); ctx.stroke(); });
    ctx.fillText("old", ox, oy+ph+14*d); ctx.fillText("now", ox+pw-26*d, oy+ph+14*d);
    const sx=pw/(a.length-1), sy=ph/(mx-mn); ctx.beginPath();
    for(let i=0;i<a.length;i++){ const X=ox+i*sx, Y=oy+ph-(a[i]-mn)*sy; if(i==0) ctx.moveTo(X,Y); else ctx.lineTo(X,Y); }
    ctx.strokeStyle="#0300b4ff"; ctx.lineWidth=2*d; ctx.stroke();
  }
  addEventListener('resize',resize); requestAnimationFrame(resize);
  return { push, resize };

}
const plots={
  lat:AxisPlot(document.getElementById('c_lat'), 300, {fixed:6}),
  lon:AxisPlot(document.getElementById('c_lon'), 300, {fixed:6}),
  elev:AxisPlot(document.getElementById('c_elev'), 300, {fixed:1}),
};

function resizeAllPlots(){
  Object.values(plots).forEach(p => p && p.resize && p.resize());
}

function safePush(n,v){ if(Number.isFinite(v)) plots[n]?.push(v); }

async function poll(){
  try{
    const r=await fetch('/data'); if(!r.ok) throw 0;
    const buf=await r.arrayBuffer(); const dv=new DataView(buf);
    const type=dv.getUint8(0);
    const lon =dv.getFloat32(4,true), lat  =dv.getFloat32(8,true);
    const elev=dv.getFloat32(12,true), roll =dv.getFloat32(16,true),
          pitch=dv.getFloat32(20,true), yaw =dv.getFloat32(24,true);

    target={yaw,pitch,roll,elev,lat,lon};
    if($t) $t.textContent = "type: " + type;
    $lat.textContent=lat.toFixed(6); $lon.textContent=lon.toFixed(6);
    $e.textContent=elev.toFixed(1); $yaw.textContent=yaw.toFixed(1);
    $pit.textContent=pitch.toFixed(1); $rol.textContent=roll.toFixed(1);
    drawYaw(yaw);
    drawPitch(pitch);
    drawRoll(roll);

    $raw.textContent=hexDump(buf);

    safePush('lat', lat); safePush('lon', lon);
    safePush('elev', elev);
  }catch(e){}
  setTimeout(poll,140);
}
poll();

const telemetryTab = document.getElementById('telemetryTab');

const pf = {
  gpsBox:   document.getElementById('pf_gps'),
  gpsVal:   document.getElementById('pf_gps_val'),
  senBox:   document.getElementById('pf_sensors'),
  senVal:   document.getElementById('pf_sensors_val'),
  radBox:   document.getElementById('pf_radio'),
  radVal:   document.getElementById('pf_radio_val'),
  batBox:   document.getElementById('pf_batt'),
  batVal:   document.getElementById('pf_batt_val'),
  readyBox: document.getElementById('pf_ready_box'),
  readyVal: document.getElementById('pf_ready_val'),
};

function setPfItem(box, valEl, state, text){
  box.classList.remove('pf-ok','pf-bad','pf-warn');
  if(state === 'ok') box.classList.add('pf-ok');
  else if(state === 'bad') box.classList.add('pf-bad');
  else box.classList.add('pf-warn');
  valEl.textContent = text;
}


async function pollPreflight(){
  try{
    const r = await fetch('/preflight');
    if(!r.ok) throw 0;
    const d = await r.json();

    const gpsOk = !!d.gps_lock;
    const senOk = !!d.sensors_cal;
    const radOk = !!d.radio_ok;
    const rssi  = (typeof d.rssi_dbm === 'number') ? d.rssi_dbm : null;
    const batt  = (typeof d.batt_v === 'number') ? d.batt_v : null;
    const ready = !!d.ready;

    setPfItem(pf.gpsBox, pf.gpsVal, gpsOk ? 'ok' : 'bad', gpsOk ? 'OK' : 'NO');
    setPfItem(pf.senBox, pf.senVal, senOk ? 'ok' : 'bad', senOk ? 'OK' : 'NO');
    setPfItem(pf.radBox, pf.radVal, radOk ? 'ok' : 'bad', radOk ? `OK (${rssi} dBm)` : `NO (${rssi} dBm)`);
    setPfItem(pf.batBox, pf.batVal, (batt!=null && batt>=3.3) ? 'ok' : 'warn', batt!=null ? `${batt.toFixed(2)} V` : '—');

    pf.readyBox.classList.remove('pf-ok','pf-bad');
    pf.readyBox.classList.add(ready ? 'pf-ok' : 'pf-bad');
    pf.readyVal.textContent = ready ? 'OK' : 'NO';

    telemetryTab.disabled = !ready;

    const telemetryActive = document.getElementById('telemetry').classList.contains('active');
    if(telemetryActive && !ready){
      document.querySelectorAll('.tab').forEach(t => t.classList.remove('active'));
      document.querySelectorAll('.tab-content').forEach(c => c.classList.remove('active'));
      document.querySelector('[data-tab="preflight"]').classList.add('active');
      document.getElementById('preflight').classList.add('active');
    }

  }catch(e){
    telemetryTab.disabled = true;
  }
  setTimeout(pollPreflight, 500);
}
pollPreflight();


const glc=document.getElementById('gl');
const gl = glc.getContext('webgl',{antialias:true,alpha:false});
function resizeGL(){
  const dpr=window.devicePixelRatio||1;
  const rect=glc.getBoundingClientRect();
  const w=Math.max(300, Math.floor(rect.width || 900));
  const cssH=Math.max(260, Math.min(620, Math.round(w * 9/16)));
  glc.width = w*dpr; glc.height = cssH*dpr; glc.style.width = w+'px'; glc.style.height = cssH+'px';
}
addEventListener('resize', resizeGL); requestAnimationFrame(resizeGL);

function compile(t,s){ const sh=gl.createShader(t); gl.shaderSource(sh,s); gl.compileShader(sh); if(!gl.getShaderParameter(sh,gl.COMPILE_STATUS)) throw gl.getShaderInfoLog(sh); return sh; }
function program(vs,fs){ const p=gl.createProgram(); gl.attachShader(p,compile(gl.VERTEX_SHADER,vs)); gl.attachShader(p,compile(gl.FRAGMENT_SHADER,fs)); gl.linkProgram(p); if(!gl.getProgramParameter(p,gl.LINK_STATUS)) throw gl.getProgramInfoLog(p); return p; }

//rocket shaders
const VS=`attribute vec3 aPos; attribute vec3 aNor; attribute vec3 aCol;
uniform mat4 uMVP; uniform mat3 uNrm;
varying vec3 vN; varying vec3 vC; varying float vDepth;
void main(){ vec4 p=uMVP*vec4(aPos,1.0); vN=normalize(uNrm*aNor); vC=aCol; vDepth=p.z/p.w; gl_Position=p; }`;
const FS=`precision mediump float;
varying vec3 vN; varying vec3 vC; varying float vDepth;
uniform vec3 uLight;
void main(){
  vec3 N=normalize(vN), L=normalize(uLight), V=normalize(vec3(0.0,0.0,1.0));
  float ndl=max(dot(N,L),0.0);
  float spec=pow(max(dot(normalize(L+V), N),0.0), 20.0)*0.35;
  float rim=pow(1.0-max(dot(N,V),0.0),2.0)*0.40;
  vec3 color = vC*(0.18 + 0.78*ndl) + vec3(1.0)*spec + vec3(0.35,0.55,1.0)*rim*0.35;
  float fog=clamp((vDepth+1.0)*0.32,0.0,1.0); vec3 bg=vec3(0.05,0.07,0.13);
  gl_FragColor=vec4(mix(color,bg,fog),1.0);
}`;
const prog=program(VS,FS);
const loc={aPos:gl.getAttribLocation(prog,"aPos"),aNor:gl.getAttribLocation(prog,"aNor"),aCol:gl.getAttribLocation(prog,"aCol"),
           uMVP:gl.getUniformLocation(prog,"uMVP"),uNrm:gl.getUniformLocation(prog,"uNrm"),uLight:gl.getUniformLocation(prog,"uLight")};

//particles
const VSP=`attribute vec3 aPos; attribute float aAge; uniform mat4 uPV; uniform float uSize; varying float vAge;
void main(){ vec4 clip=uPV*vec4(aPos,1.0); float att=clamp(1.0/max(0.0001,clip.w),0.0,3.0); gl_Position=clip; gl_PointSize=uSize*(0.8+aAge*1.4)*att; vAge=aAge; }`;
const FSP=`precision mediump float; varying float vAge;
void main(){ vec2 uv=gl_PointCoord-0.5; float r=length(uv)*2.0; float soft=1.0-smoothstep(0.0,1.0,r); float alpha=soft*pow(1.0-vAge,2.6);
vec3 col=mix(vec3(0.82,0.84,0.90), vec3(0.56,0.60,0.64), vAge); gl_FragColor=vec4(col,alpha*0.85); }`;
const progP=program(VSP,FSP);
const locP={aPos:gl.getAttribLocation(progP,"aPos"), aAge:gl.getAttribLocation(progP,"aAge"), uPV:gl.getUniformLocation(progP,"uPV"), uSize:gl.getUniformLocation(progP,"uSize")};

//trail
const VST=`attribute vec3 aPos; uniform mat4 uPV; varying float vDepth; void main(){ vec4 p=uPV*vec4(aPos,1.0); vDepth=p.z/p.w; gl_Position=p; }`;
const FST=`precision mediump float; varying float vDepth; uniform vec3 uColor; void main(){ float fog=clamp((vDepth+1.0)*0.32,0.0,1.0);
vec3 bg=vec3(0.05,0.07,0.13); vec3 col=mix(uColor,bg,fog*0.6); gl_FragColor=vec4(col,1.0); }`;
const progT=program(VST,FST);
const locT={aPos:gl.getAttribLocation(progT,"aPos"), uPV:gl.getUniformLocation(progT,"uPV"), uColor:gl.getUniformLocation(progT,"uColor")};

//ground grid
const VSG=`attribute vec3 aPos; uniform mat4 uM; uniform mat4 uMVP; varying vec3 vW; varying float vDepth;
void main(){ vec4 w=uM*vec4(aPos,1.0); vW=w.xyz; vec4 p=uMVP*vec4(aPos,1.0); vDepth=p.z/p.w; gl_Position=p; }`;
const FSG=`precision mediump float; varying vec3 vW; varying float vDepth; uniform float uScale; uniform vec3 uColMinor; uniform vec3 uColMajor;
void main(){ float gx=vW.x*uScale; float gz=vW.z*uScale; float fx=abs(fract(gx)-0.5); float fz=abs(fract(gz)-0.5); float w=0.02;
float ax=smoothstep(w,0.0,fx); float az=smoothstep(w,0.0,fz); float minor=max(ax,az);
float gxM=abs(fract(gx/10.0)-0.5); float gzM=abs(fract(gz/10.0)-0.5); float aw=0.02;
float mx=smoothstep(aw,0.0,gxM); float mz=smoothstep(aw,0.0,gzM); float major=max(mx,mz);
vec3 col=mix(uColMinor,uColMajor,major); col*=max(minor,major); float dist=clamp(length(vW.xz)/200.0,0.0,1.0);
float fog=clamp((vDepth+1.0)*0.40,0.0,1.0); vec3 bg=vec3(0.05,0.07,0.13); vec3 c=mix(col*(1.0-dist), bg, fog); gl_FragColor=vec4(c,1.0); }`;
const progG=program(VSG,FSG);
const locG={aPos:gl.getAttribLocation(progG,"aPos"), uM:gl.getUniformLocation(progG,"uM"), uMVP:gl.getUniformLocation(progG,"uMVP"),
            uScale:gl.getUniformLocation(progG,"uScale"), uColMinor:gl.getUniformLocation(progG,"uColMinor"), uColMajor:gl.getUniformLocation(progG,"uColMajor")};

//math
function mat4Identity(){ return new Float32Array([1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1]); }
function mat4Mul(a,b){ const r=new Float32Array(16); for(let i=0;i<4;i++)for(let j=0;j<4;j++) r[j*4+i]=a[0*4+i]*b[j*4+0]+a[1*4+i]*b[j*4+1]+a[2*4+i]*b[j*4+2]+a[3*4+i]*b[j*4+3]; return r; }
function translate(x,y,z){ const m=mat4Identity(); m[12]=x; m[13]=y; m[14]=z; return m; }
function rotX(a){ const c=Math.cos(a),s=Math.sin(a); return new Float32Array([1,0,0,0, 0,c,s,0, 0,-s,c,0,  0,0,0,1]); }
function rotY(a){ const c=Math.cos(a),s=Math.sin(a); return new Float32Array([c,0,-s,0,  0,1,0,0,  s,0,c,0,  0,0,0,1]); }
function rotZ(a){ const c=Math.cos(a),s=Math.sin(a); return new Float32Array([c,s,0,0, -s,c,0,0, 0,0,1,0,  0,0,0,1]); }
function perspective(fov,aspect,near,far){ const f=1/Math.tan(fov/2), nf=1/(near-far);
  return new Float32Array([f/aspect,0,0,0, 0,f,0,0,  0,0,(far+near)*nf,-1,  0,0,(2*far*near)*nf,0]); }
function mat3FromMat4(m){ return new Float32Array([m[0],m[1],m[2], m[4],m[5],m[6], m[8],m[9],m[10]]); }
function m4mulv(m,x,y,z,w){ return {x:m[0]*x+m[4]*y+m[8]*z+m[12]*w, y:m[1]*x+m[5]*y+m[9]*z+m[13]*w, z:m[2]*x+m[6]*y+m[10]*z+m[14]*w, w:m[3]*x+m[7]*y+m[11]*z+m[15]*w}; }
function lookAt(eye, target, up){ const fx=target.x-eye.x, fy=target.y-eye.y, fz=target.z-eye.z;
  const fl=Math.hypot(fx,fy,fz)||1; const f0=fx/fl, f1=fy/fl, f2=fz/fl;
  const sx=f1*up.z - f2*up.y, sy=f2*up.x - f0*up.z, sz=f0*up.y - f1*up.x;
  const sl=Math.hypot(sx,sy,sz)||1; const s0=sx/sl, s1=sy/sl, s2=sz/sl;
  const ux=s1*f2 - s2*f1, uy=s2*f0 - s0*f2, uz=s0*f1 - s1*f0;
  const out=new Float32Array(16);
  out[0]=s0; out[1]=ux; out[2]=-f0; out[3]=0;
  out[4]=s1; out[5]=uy; out[6]=-f1; out[7]=0;
  out[8]=s2; out[9]=uz; out[10]=-f2; out[11]=0;
  out[12]=-(s0*eye.x + s1*eye.y + s2*eye.z);
  out[13]=-(ux*eye.x + uy*eye.y + uz*eye.z);
  out[14]=(f0*eye.x + f1*eye.y + f2*eye.z);
  out[15]=1; return out; }

function buildRocket(){
  const pos=[], nor=[], col=[], idx=[];
  const R=0.45, L=2.1, cone=0.90, seg=24;

  const engNeckLen=0.18, engBellLen=0.22;
  const engNeckR = R*0.28, engBellR = R*0.60;
  const zBodyBot = -L/2, zNeckBot = zBodyBot - engNeckLen, zBellBot = zNeckBot - engBellLen;

  const C_BODY=[0.72,0.76,0.90], C_NOSE=[0.86,0.38,0.38], C_FIN=[0.86,0.38,0.38], C_ENG=[0.30,0.32,0.38];

  //cylinder
  for(let i=0;i<=seg;i++){
    const a=i*2*Math.PI/seg, ca=Math.cos(a), sa=Math.sin(a);
    pos.push(R*ca, R*sa, zBodyBot); nor.push(ca,sa,0); col.push(...C_BODY);
    pos.push(R*ca, R*sa,  L/2);     nor.push(ca,sa,0); col.push(...C_BODY);
  }
  for(let i=0;i<seg;i++){ const a=i*2,b=a+1,c=a+2,d=a+3; idx.push(a,b,c, b,d,c); }

  //bottom cap
  const capCenter = pos.length/3; pos.push(0,0,zBodyBot); nor.push(0,0,-1); col.push(...C_BODY);
  const capRing = pos.length/3;
  for(let i=0;i<=seg;i++){ const a=i*2*Math.PI/seg, ca=Math.cos(a), sa=Math.sin(a); pos.push(R*ca,R*sa,zBodyBot); nor.push(0,0,-1); col.push(...C_BODY); }
  for(let i=0;i<seg;i++){ idx.push(capCenter, capRing+i+1, capRing+i); }

  //nose
  const baseStart = pos.length/3;
  for(let i=0;i<=seg;i++){
    const a=i*2*Math.PI/seg, ca=Math.cos(a), sa=Math.sin(a);
    pos.push(R*ca, R*sa, L/2); const sl=R/Math.hypot(R,cone);
    nor.push(ca*sl, sa*sl, Math.min(1,cone/Math.hypot(R,cone))); col.push(...C_NOSE);
  }
  const tipIndex=pos.length/3; pos.push(0,0,L/2+cone); nor.push(0,0,1); col.push(...C_NOSE);
  for(let i=0;i<seg;i++){ const a=baseStart+i, b=baseStart+i+1; idx.push(a, tipIndex, b); }

  //engine neck
  const neckA = pos.length/3;
  for(let i=0;i<=seg;i++){
    const a=i*2*Math.PI/seg, ca=Math.cos(a), sa=Math.sin(a);
    pos.push(engNeckR*ca, engNeckR*sa, zBodyBot); nor.push(ca,sa,0); col.push(...C_ENG);
    pos.push(engNeckR*ca, engNeckR*sa, zNeckBot); nor.push(ca,sa,0); col.push(...C_ENG);
  }
  for(let i=0;i<seg;i++){ const a=neckA+i*2,b=a+1,c=a+2,d=a+3; idx.push(a,b,c, b,d,c); }

  //engine bell
  const bellA = pos.length/3; const slope=(engBellR-engNeckR)/engBellLen;
  for(let i=0;i<=seg;i++){
    const a=i*2*Math.PI/seg, ca=Math.cos(a), sa=Math.sin(a);
    pos.push(engNeckR*ca, engNeckR*sa, zNeckBot); {const nx=ca,ny=sa,nz=-slope,l=1/Math.hypot(nx,ny,nz); nor.push(nx*l,ny*l,nz*l); col.push(...C_ENG);}
    pos.push(engBellR*ca, engBellR*sa, zBellBot); {const nx=ca,ny=sa,nz=-slope,l=1/Math.hypot(nx,ny,nz); nor.push(nx*l,ny*l,nz*l); col.push(...C_ENG);}
  }
  for(let i=0;i<seg;i++){ const a=bellA+i*2,b=a+1,c=a+2,d=a+3; idx.push(a,b,c, b,d,c); }

  //fins
  function addTri(a,b,c,cc){
    const i=pos.length/3; pos.push(...a,...b,...c);
    const ux=b[0]-a[0],uy=b[1]-a[1],uz=b[2]-a[2]; const vx=c[0]-a[0],vy=c[1]-a[1],vz=c[2]-a[2];
    const nx=uy*vz-uz*vy, ny=uz*vx-ux*vz, nz=ux*vy-uy*vx; const l=Math.hypot(nx,ny,nz)||1;
    for(let k=0;k<3;k++){ nor.push(nx/l,ny/l,nz/l); col.push(...cc); } idx.push(i,i+1,i+2);
  }
  const finZ=zBodyBot+0.20, finOut=R*1.35, finBack=zBodyBot-0.18, finUp=0.22;
  [[[ R,0,finZ],[ finOut,0,finZ-finUp],[ R,0,finBack]],
   [[-R,0,finZ],[-finOut,0,finZ-finUp],[-R,0,finBack]],
   [[0, R,finZ],[0, finOut,finZ-finUp],[0, R,finBack]],
   [[0,-R,finZ],[0,-finOut,finZ-finUp],[0,-R,finBack]]].forEach(f=>addTri(f[0],f[1],f[2],C_FIN));

  return {pos:new Float32Array(pos), nor:new Float32Array(nor), col:new Float32Array(col), idx:new Uint16Array(idx), L:L+cone};
}
const rocket=buildRocket();

//vbo
const inter=new Float32Array((rocket.pos.length/3)*9);
for(let i=0,p=0;i<rocket.pos.length/3;i++){
  inter[p++]=rocket.pos[i*3+0]; inter[p++]=rocket.pos[i*3+1]; inter[p++]=rocket.pos[i*3+2];
  inter[p++]=rocket.nor[i*3+0]; inter[p++]=rocket.nor[i*3+1]; inter[p++]=rocket.nor[i*3+2];
  inter[p++]=rocket.col[i*3+0]; inter[p++]=rocket.col[i*3+1]; inter[p++]=rocket.col[i*3+2];
}
const vbo=gl.createBuffer(); gl.bindBuffer(gl.ARRAY_BUFFER,vbo); gl.bufferData(gl.ARRAY_BUFFER,inter,gl.STATIC_DRAW);
const ebo=gl.createBuffer(); gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER,ebo); gl.bufferData(gl.ELEMENT_ARRAY_BUFFER,rocket.idx,gl.STATIC_DRAW);
const stride=9*4;
gl.enableVertexAttribArray(loc.aPos); gl.vertexAttribPointer(loc.aPos,3,gl.FLOAT,false,stride,0);
gl.enableVertexAttribArray(loc.aNor); gl.vertexAttribPointer(loc.aNor,3,gl.FLOAT,false,stride,3*4);
gl.enableVertexAttribArray(loc.aCol); gl.vertexAttribPointer(loc.aCol,3,gl.FLOAT,false,stride,6*4);

const MAXP=16000;
const posP=new Float32Array(MAXP*3), ageP=new Float32Array(MAXP), velP=new Float32Array(MAXP*3);
let head=0;
const vboP=gl.createBuffer(); gl.bindBuffer(gl.ARRAY_BUFFER,vboP); gl.bufferData(gl.ARRAY_BUFFER,posP,gl.DYNAMIC_DRAW);
const vboPAge=gl.createBuffer(); gl.bindBuffer(gl.ARRAY_BUFFER,vboPAge); gl.bufferData(gl.ARRAY_BUFFER,ageP,gl.DYNAMIC_DRAW);

const MAXTRAIL=24000;
const trail=new Float32Array(MAXTRAIL*3); let trailLen=0, trailStart=0; let lastTX=NaN,lastTY=NaN,lastTZ=NaN;
const vboTrail=gl.createBuffer(); gl.bindBuffer(gl.ARRAY_BUFFER,vboTrail); gl.bufferData(gl.ARRAY_BUFFER,trail,gl.DYNAMIC_DRAW);
function addTrailPoint(x,y,z){
  if(Number.isFinite(lastTX)){ const dx=x-lastTX,dy=y-lastTY,dz=z-lastTZ; if(Math.hypot(dx,dy,dz)<0.05) return; }
  lastTX=x; lastTY=y; lastTZ=z;
  const i=(trailStart+trailLen)%MAXTRAIL;
  trail[i*3+0]=x; trail[i*3+1]=y; trail[i*3+2]=z;
  if(trailLen<MAXTRAIL) trailLen++; else trailStart=(trailStart+1)%MAXTRAIL;
  gl.bindBuffer(gl.ARRAY_BUFFER,vboTrail); gl.bufferSubData(gl.ARRAY_BUFFER,0,trail);
}

const GRID_EXT=400.0;
const gridVerts=new Float32Array([-GRID_EXT,0,-GRID_EXT, GRID_EXT,0,-GRID_EXT, -GRID_EXT,0,GRID_EXT,  GRID_EXT,0,GRID_EXT]);
const vboGrid=gl.createBuffer(); gl.bindBuffer(gl.ARRAY_BUFFER,vboGrid); gl.bufferData(gl.ARRAY_BUFFER,gridVerts,gl.STATIC_DRAW);

//camera control
const pos3={x:0,y:0,z:0};
const baseSpeed=1.1;
let dragging=false,lastX=0,lastY=0; let camYaw=0, camPitch=0; let vYaw=0, vPitch=0; let camDist=6.2;
const springK=5.0, damp=6.0;

glc.addEventListener('mousedown', e=>{ dragging=true; lastX=e.clientX; lastY=e.clientY; });
window.addEventListener('mouseup', ()=> dragging=false);
window.addEventListener('mouseleave', ()=> dragging=false);
window.addEventListener('mousemove', e=>{
  if(!dragging) return;
  const dx=e.clientX-lastX, dy=e.clientY-lastY; lastX=e.clientX; lastY=e.clientY;
  const kx=0.005, ky=0.003; camYaw+=dx*kx; camPitch+=dy*ky; camPitch=Math.max(-1.0, Math.min(1.0, camPitch));
  vYaw = dx*kx/(1/60); vPitch = dy*ky/(1/60);
});
glc.addEventListener('wheel', e=>{ e.preventDefault(); const s=Math.exp((e.deltaY>0?1:-1)*0.12); camDist=Math.min(30, Math.max(3, camDist*s)); },{passive:false});

const activePtrs=new Map(); let pinchStartDist=null; let pinchStartCamDist=null; let pointerDragging=false;
function ptrDistance(a,b){ const dx=a.x-b.x, dy=a.y-b.y; return Math.hypot(dx,dy); }
glc.addEventListener('pointerdown', e=>{
  glc.setPointerCapture(e.pointerId); activePtrs.set(e.pointerId,{x:e.clientX,y:e.clientY});
  if(activePtrs.size===1){ pointerDragging=true; lastX=e.clientX; lastY=e.clientY; }
  else if(activePtrs.size===2){ const [p1,p2]=[...activePtrs.values()]; pinchStartDist=ptrDistance(p1,p2); pinchStartCamDist=camDist; }
});
glc.addEventListener('pointermove', e=>{
  if(!activePtrs.has(e.pointerId)) return;
  const curr={x:e.clientX,y:e.clientY}; activePtrs.set(e.pointerId,curr);
  if(activePtrs.size===1 && pointerDragging){
    const dx=curr.x-lastX, dy=curr.y-lastY; lastX=curr.x; lastY=curr.y;
    const kx=0.005, ky=0.003; camYaw+=dx*kx; camPitch+=dy*ky; camPitch=Math.max(-1.0,Math.min(1.0,camPitch));
    vYaw=dx*kx/(1/60); vPitch=dy*ky/(1/60);
  }else if(activePtrs.size===2 && pinchStartDist){
    const [a,b]=[...activePtrs.values()]; const d=ptrDistance(a,b); if(d>0){ const scale=pinchStartDist/d; camDist=Math.min(30,Math.max(3,pinchStartCamDist*scale)); }
  }
});
function endPointer(e){ activePtrs.delete(e.pointerId); if(activePtrs.size<2){pinchStartDist=null;pinchStartCamDist=null;} if(activePtrs.size===0){pointerDragging=false;} }
glc.addEventListener('pointerup', endPointer); glc.addEventListener('pointercancel', endPointer); glc.addEventListener('pointerleave', endPointer);


let lastTapTime=0; glc.addEventListener('pointerdown', e=>{ const now=performance.now(); if(now-lastTapTime<300){ camYaw=0; camPitch=0; } lastTapTime=now; });

function perspectiveMat(){ const aspect=glc.width/glc.height; return perspective(45*Math.PI/180, aspect, 0.1, 400.0); }
function rocketModel(){ const y=d2r(smooth.yaw), p=d2r(smooth.pitch), r=d2r(smooth.roll);
  const Rm=mat4Mul(rotY(y),mat4Mul(rotX(p),mat4Mul(rotZ(r),rotY(Math.PI)))); return mat4Mul(translate(pos3.x,pos3.y,pos3.z),Rm); }
function cameraView(){ const ce=Math.cos(camPitch),se=Math.sin(camPitch),ca=Math.cos(camYaw),sa=Math.sin(camYaw);
  const eye={x:pos3.x+camDist*sa*ce,y:pos3.y+camDist*se,z:pos3.z+camDist*ca*ce}; return lookAt(eye,pos3,{x:0,y:1,z:0}); }
function randN(){ return (Math.random()*2-1)+(Math.random()*2-1)+(Math.random()*2-1); }
function shortestDeltaDeg(a,b){ return ((b-a+540)%360)-180; }

let lastTime=performance.now(); const ALT_SCALE=0.06;

function emitSmoke(M,flightDir){
  const tailZ=-rocket.L/2-0.7; const tail=m4mulv(M,0,0,tailZ,1.0);
  const bx=-flightDir.x, by=-flightDir.y, bz=-flightDir.z; const count=12;
  for(let k=0;k<count;k++){
    const i=head; head=(head+1)%MAXP; const spread=0.18;
    posP[i*3+0]=tail.x+(Math.random()-0.5)*spread;
    posP[i*3+1]=tail.y+(Math.random()-0.5)*spread;
    posP[i*3+2]=tail.z+(Math.random()-0.5)*spread;
    const sp=0.8+Math.random()*0.6; const j=0.22;
    velP[i*3+0]=bx*sp+randN()*j; velP[i*3+1]=by*sp+randN()*j+0.02; velP[i*3+2]=bz*sp+randN()*j; ageP[i]=0.0;
  }
}
function updateSmoke(dt,flightDir){
  const ax=-flightDir.x*2.4,ay=-flightDir.y*2.4,az=-flightDir.z*2.4;
  for(let i=0;i<MAXP;i++){
    const drag=Math.max(0.0,1.0-0.12*dt);
    velP[i*3+0]*=drag; velP[i*3+1]*=drag; velP[i*3+2]*=drag;
    velP[i*3+0]+= (Math.random()-0.5)*0.05*dt;
    velP[i*3+2]+= (Math.random()-0.5)*0.05*dt;
    posP[i*3+0]+= (velP[i*3+0]+ax)*dt;
    posP[i*3+1]+= (velP[i*3+1]+ay+0.05)*dt;
    posP[i*3+2]+= (velP[i*3+2]+az)*dt;
    ageP[i]+= dt*0.03; if(ageP[i]>1.0) ageP[i]=1.0;
  }
}

const att = {
  yaw: document.getElementById('att_yaw'),
  pitch: document.getElementById('att_pitch'),
  roll: document.getElementById('att_roll'),
};

function resizeCanvasToCSS(c){
  if(!c) return null;
  const dpr = window.devicePixelRatio || 1;
  const w = Math.max(10, Math.floor(c.clientWidth || 0));
  const h = Math.max(10, Math.floor(c.clientHeight || 0));
  c.width  = Math.floor(w * dpr);
  c.height = Math.floor(h * dpr);
  const ctx = c.getContext('2d');
  ctx.setTransform(dpr,0,0,dpr,0,0);
  return {ctx, w, h};
}

function drawFrame(ctx,w,h,title,val){
  ctx.clearRect(0,0,w,h);
  ctx.fillStyle = "#000";
  ctx.fillRect(0,0,w,h);

  ctx.strokeStyle = "#2a2a2a";
  ctx.lineWidth = 1;
  ctx.strokeRect(0.5,0.5,w-1,h-1);

  ctx.fillStyle = "#111";
  ctx.fillRect(0,0,w,24);
  ctx.strokeStyle="#2a2a2a";
  ctx.beginPath(); ctx.moveTo(0,24.5); ctx.lineTo(w,24.5); ctx.stroke();

  ctx.fillStyle="#fff";
  ctx.font="700 12px system-ui";
  ctx.fillText(title, 8, 16);

  ctx.fillStyle="#fff";
  ctx.font="700 12px system-ui";
  const t = `${val.toFixed(2)} °`;
  ctx.fillText(t, 8, 40);
}

function drawRocketSilhouette(ctx, cx, cy, s){
  ctx.fillStyle="#fff";
  ctx.beginPath();
  ctx.moveTo(cx - 0.18*s, cy + 0.45*s);
  ctx.lineTo(cx + 0.18*s, cy + 0.45*s);
  ctx.lineTo(cx + 0.18*s, cy - 0.10*s);
  ctx.lineTo(cx,           cy - 0.55*s);
  ctx.lineTo(cx - 0.18*s, cy - 0.10*s);
  ctx.closePath();
  ctx.fill();

  ctx.beginPath();
  ctx.moveTo(cx - 0.18*s, cy + 0.25*s);
  ctx.lineTo(cx - 0.36*s, cy + 0.35*s);
  ctx.lineTo(cx - 0.18*s, cy + 0.35*s);
  ctx.closePath();
  ctx.fill();

  ctx.beginPath();
  ctx.moveTo(cx + 0.18*s, cy + 0.25*s);
  ctx.lineTo(cx + 0.36*s, cy + 0.35*s);
  ctx.lineTo(cx + 0.18*s, cy + 0.35*s);
  ctx.closePath();
  ctx.fill();
}

function drawYaw(yawDeg){
  const r = resizeCanvasToCSS(att.yaw); if(!r) return;
  const {ctx,w,h} = r;
  drawFrame(ctx,w,h,"YAW",yawDeg);

  ctx.save();
  const cx0 = w/2, cy0 = (h+24)/2;
  const s = Math.min(w,h-24)*0.35;

  ctx.translate(cx0, cy0);
  ctx.rotate((-yawDeg) * Math.PI/180);
  drawRocketSilhouette(ctx, 0, 0, s);
  ctx.restore();
}

function drawPitch(pitchDeg){
  const r = resizeCanvasToCSS(att.pitch); if(!r) return;
  const {ctx,w,h} = r;
  drawFrame(ctx,w,h,"PITCH",pitchDeg);

  ctx.save();
  const cx0 = w/2, cy0 = (h+24)/2;
  const s = Math.min(w,h-24)*0.35;

  ctx.translate(cx0, cy0);
  ctx.rotate((pitchDeg) * Math.PI/180);
  drawRocketSilhouette(ctx, 0, 0, s);
  ctx.restore();
}

function drawRoll(rollDeg){
  const r = resizeCanvasToCSS(att.roll); if(!r) return;
  const {ctx,w,h} = r;
  drawFrame(ctx,w,h,"ROLL",rollDeg);

  const cx0 = w/2, cy0 = (h+24)/2;
  const R = Math.min(w,h-24)*0.18;

  ctx.save();
  ctx.translate(cx0, cy0);
  ctx.rotate((rollDeg) * Math.PI/180);

  ctx.fillStyle="#fff";
  ctx.beginPath(); ctx.arc(0,0,R*0.55,0,Math.PI*2); ctx.fill();

  ctx.strokeStyle="#fff";
  ctx.lineWidth=2;
  for(let k=0;k<4;k++){
    const a = k*Math.PI/2;
    ctx.beginPath();
    ctx.moveTo(Math.cos(a)*R*0.9,  Math.sin(a)*R*0.9);
    ctx.lineTo(Math.cos(a)*R*1.6,  Math.sin(a)*R*1.6);
    ctx.stroke();
  }
  ctx.restore();
}

function drawAttitudeAll(){
  drawPitch(smooth.pitch);
  drawYaw(smooth.yaw);
  drawRoll(smooth.roll);
}

addEventListener('resize', ()=> requestAnimationFrame(drawAttitudeAll));


function render(){
  const now=performance.now(); const dt=Math.min(0.1,(now-lastTime)/1000); lastTime=now;

  const response=8.0, alpha=1-Math.exp(-response*dt);
  const dy=shortestDeltaDeg(smooth.yaw,target.yaw); smooth.yaw+=alpha*dy;
  smooth.pitch+=alpha*(target.pitch-smooth.pitch);
  smooth.roll +=alpha*(target.roll -smooth.roll);
  smooth.elev +=alpha*(target.elev -smooth.elev);

  if(!dragging){
    vYaw+=(-camYaw)*springK*dt; vPitch+=(-camPitch)*springK*dt;
    const dmp=Math.exp(-damp*dt); vYaw*=dmp; vPitch*=dmp;
    camYaw+=vYaw*dt; camPitch+=vPitch*dt; camPitch=Math.max(-1.0,Math.min(1.0,camPitch));
  }

  const Rf=mat4Mul(rotY(d2r(smooth.yaw)),mat4Mul(rotX(d2r(smooth.pitch)),mat4Mul(rotZ(d2r(smooth.roll)),rotY(Math.PI))));
  const fwd=m4mulv(Rf,0,0,1,0.0); const fl=Math.hypot(fwd.x,fwd.y,fwd.z)||1;
  const flightDir={x:fwd.x/fl,y:fwd.y/fl,z:fwd.z/fl};

  pos3.x+=flightDir.x*baseSpeed*dt; pos3.z+=flightDir.z*baseSpeed*dt; pos3.y=smooth.elev*ALT_SCALE;

  addTrailPoint(pos3.x,pos3.y,pos3.z);

  const P=perspectiveMat(), V=cameraView(), PV=mat4Mul(P,V), M=rocketModel(), Nrm=mat3FromMat4(M);

  emitSmoke(M,flightDir); updateSmoke(dt,flightDir);

  gl.viewport(0,0,glc.width,glc.height);
  gl.enable(gl.DEPTH_TEST);
  gl.clearColor(0.05,0.07,0.13,1); gl.clear(gl.COLOR_BUFFER_BIT|gl.DEPTH_BUFFER_BIT);

  const Mgrid=translate(0,0.0,0), MVPgrid=mat4Mul(PV,Mgrid);
  gl.useProgram(progG); gl.bindBuffer(gl.ARRAY_BUFFER,vboGrid);
  gl.enableVertexAttribArray(locG.aPos); gl.vertexAttribPointer(locG.aPos,3,gl.FLOAT,false,0,0);
  gl.uniformMatrix4fv(locG.uM,false,Mgrid); gl.uniformMatrix4fv(locG.uMVP,false,MVPgrid);
  gl.uniform1f(locG.uScale,0.5); gl.uniform3f(locG.uColMinor,0.16,0.22,0.45); gl.uniform3f(locG.uColMajor,0.35,0.55,0.95);
  gl.drawArrays(gl.TRIANGLE_STRIP,0,4);

  gl.useProgram(prog);
  gl.uniformMatrix4fv(loc.uMVP,false,mat4Mul(PV,M));
  gl.uniformMatrix3fv(loc.uNrm,false,Nrm);
  gl.uniform3f(loc.uLight,0.3,0.6,0.8);
  gl.bindBuffer(gl.ARRAY_BUFFER,vbo);
  gl.enableVertexAttribArray(loc.aPos); gl.vertexAttribPointer(loc.aPos,3,gl.FLOAT,false,stride,0);
  gl.enableVertexAttribArray(loc.aNor); gl.vertexAttribPointer(loc.aNor,3,gl.FLOAT,false,stride,3*4);
  gl.enableVertexAttribArray(loc.aCol); gl.vertexAttribPointer(loc.aCol,3,gl.FLOAT,false,stride,6*4);
  gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER,ebo);
  gl.drawElements(gl.TRIANGLES,rocket.idx.length,gl.UNSIGNED_SHORT,0);

  if(trailLen>1){
    gl.useProgram(progT);
    gl.uniformMatrix4fv(locT.uPV,false,PV);
    gl.uniform3f(locT.uColor,0.45,0.86,0.92);
    gl.bindBuffer(gl.ARRAY_BUFFER,vboTrail);
    gl.enableVertexAttribArray(locT.aPos); gl.vertexAttribPointer(locT.aPos,3,gl.FLOAT,false,0,0);
    const first=trailStart; const len1=Math.min(trailLen,MAXTRAIL-first);
    gl.drawArrays(gl.LINE_STRIP,first,len1); const len2=trailLen-len1; if(len2>0) gl.drawArrays(gl.LINE_STRIP,0,len2);
  }

  gl.useProgram(progP);
  gl.uniformMatrix4fv(locP.uPV,false,PV);
  gl.enable(gl.BLEND); gl.blendFunc(gl.SRC_ALPHA,gl.ONE_MINUS_SRC_ALPHA); gl.depthMask(false);
  const dpr=window.devicePixelRatio||1; gl.uniform1f(locP.uSize,18.0*dpr);
  gl.bindBuffer(gl.ARRAY_BUFFER,vboP); gl.bufferSubData(gl.ARRAY_BUFFER,0,posP);
  gl.enableVertexAttribArray(locP.aPos); gl.vertexAttribPointer(locP.aPos,3,gl.FLOAT,false,0,0);
  gl.bindBuffer(gl.ARRAY_BUFFER,vboPAge); gl.bufferSubData(gl.ARRAY_BUFFER,0,ageP);
  gl.enableVertexAttribArray(locP.aAge); gl.vertexAttribPointer(locP.aAge,1,gl.FLOAT,false,0,0);
  gl.drawArrays(gl.POINTS,0,MAXP);
  gl.depthMask(true); gl.disable(gl.BLEND);

  drawMiniAxes2D(smooth.yaw,smooth.pitch,smooth.roll);
  drawAttitudeAll();
  requestAnimationFrame(render);
}
requestAnimationFrame(()=>{ resizeGL(); render(); });

const axes2d=document.getElementById('axes2d');
function drawMiniAxes2D(yawDeg,pitchDeg,rollDeg){
  const rect=axes2d.getBoundingClientRect(); const dpr=window.devicePixelRatio||1;
  const W=Math.max(20,Math.floor(rect.width)), H=Math.max(20,Math.floor(rect.height));
  axes2d.width=W*dpr; axes2d.height=H*dpr; axes2d.style.width=W+'px'; axes2d.style.height=H+'px';
  const ctx=axes2d.getContext('2d'); ctx.setTransform(dpr,0,0,dpr,0,0); ctx.clearRect(0,0,W,H);
  ctx.strokeStyle="#22305d"; ctx.lineWidth=1; ctx.strokeRect(6,6,W-12,H-12);
  const cx=W/2, cy=H/2, scale=Math.min(W,H)*0.32;
  const Y=d2r(yawDeg), P=d2r(pitchDeg), R=d2r(rollDeg);
  function rot3(v){ let x=v[0],y=v[1],z=v[2];
    let x1= x*Math.cos(R)+ z*Math.sin(R), y1=y, z1=-x*Math.sin(R)+ z*Math.cos(R);
    let x2= x1, y2= y1*Math.cos(P)+ z1*Math.sin(P), z2=-y1*Math.sin(P)+ z1*Math.cos(P);
    let x3= x2*Math.cos(Y)- y2*Math.sin(Y), y3= x2*Math.sin(Y)+ y2*Math.cos(Y); return [x3,y3,z2];
  }
  function proj(v){ const f=1.6, z=v[2]+2.2; return [cx + (v[0]/z)*scale*f, cy - (v[1]/z)*scale*f]; }
  const O=proj([0,0,0]), X=proj(rot3([1,0,0])), Yv=proj(rot3([0,1,0])), Z=proj(rot3([0,0,1]));
  ctx.lineWidth=2;
  ctx.beginPath(); ctx.moveTo(O[0],O[1]); ctx.lineTo(X[0],X[1]); ctx.strokeStyle="#f26b6b"; ctx.stroke();
  ctx.beginPath(); ctx.moveTo(O[0],O[1]); ctx.lineTo(Yv[0],Yv[1]); ctx.strokeStyle="#5be38a"; ctx.stroke();
  ctx.beginPath(); ctx.moveTo(O[0],O[1]); ctx.lineTo(Z[0],Z[1]); ctx.strokeStyle="#7aa8ff"; ctx.stroke();
  ctx.font="11px system-ui"; ctx.fillStyle="#cbd5ff"; ctx.fillText("X", X[0]+4, X[1]-4); ctx.fillText("Y", Yv[0]+4, Yv[1]-4); ctx.fillText("Z", Z[0]+4, Z[1]-4);
}

</script>
</body></html>
)HTML";


void sendIndex(){ server.send_P(200,"text/html",INDEX_HTML); }
void sendApp(){ server.send_P(200,"text/html",APP_HTML); }

static inline void writeLE32(uint8_t* buf, int off, float v){
  union { float f; uint8_t b[4]; } u; u.f = v;
  buf[off+0]=u.b[0]; buf[off+1]=u.b[1]; buf[off+2]=u.b[2]; buf[off+3]=u.b[3];
}
static inline float clampf(float v, float lo, float hi){ return v<lo?lo:(v>hi?hi:v); }
static inline float d2r(float deg) { return deg * (float)M_PI / 180.0f; }
static inline float wrapDeg180(float a){ while(a>180.f)a-=360.f; while(a<=-180.f)a+=360.f; return a; }

struct PhysSim {
  float lat0_deg, lon0_deg, h0_m;
  float xE_m, yN_m, zU_m;
  float vE_mps, vN_mps, vU_mps;
  float yaw_deg, pitch_deg, roll_deg;
  float m_kg, m_dry_kg, mdot_kgps, thrust_N, CdA;
  float yaw_rate_cmd_dps, pitch_prog_deg;
  uint32_t last_ms; bool init;
} R = {};

static void simInit(){
  R.lat0_deg = 48.1486f; R.lon0_deg = 17.1077f; R.h0_m = 0.0f;
  R.xE_m = 0; R.yN_m = 0; R.zU_m = 50.f;
  R.vE_mps = 0; R.vN_mps = 0; R.vU_mps = 0;
  R.yaw_deg = 45.f; R.pitch_deg = 85.f; R.roll_deg = 0.f;
  R.m_kg = 50.f; R.m_dry_kg = 28.f; R.mdot_kgps = 2.5f; R.thrust_N = 1200.f; R.CdA = 0.08f;
  R.yaw_rate_cmd_dps = 0.8f; R.pitch_prog_deg = 85.f;
  R.last_ms = millis(); R.init = true;
}

static void simStepFill(uint8_t out[28]){
  if(!R.init) simInit();

  const uint32_t now = millis();
  float dt = (now - R.last_ms) / 1000.f; 
  R.last_ms = now;
  dt = clampf(dt, 0.005f, 0.05f);

  const float t = now * 0.001f;

  const float SPEED_GAIN = 10.0f;
  const float DRAG_SCALE = 0.5f;
  const float THRUST_CAP = R.thrust_N * SPEED_GAIN;

  const float g0      = 9.80665f;
  const float R_earth = 6371000.f;
  const float rho0    = 1.225f;
  const float H       = 8500.f;

  float alt_m = R.h0_m + R.zU_m;
  float rho   = rho0 * expf(-alt_m / H);

  float vE = R.vE_mps, vN = R.vN_mps, vU = R.vU_mps;
  float v2 = vE*vE + vN*vN + vU*vU;
  float v  = sqrtf(v2) + 1e-6f;
  float uE = vE / v, uN = vN / v, uU = vU / v;

  static float yawRate_dps = 0.0f;
  {
    const float THETA = 0.28f, SIGMA = 9.0f;
    float ur = (float)esp_random() * (1.0f/4294967296.0f);
    float eta = 2.0f*ur - 1.0f;
    yawRate_dps += THETA*(0.0f - yawRate_dps)*dt + SIGMA*sqrtf(dt)*eta;
    yawRate_dps += 7.0f*sinf(1.4f*t) + 5.0f*sinf(2.3f*t + 1.1f);
    static float spikeY = 0.0f;
    if ((float)esp_random() * (1.0f/4294967296.0f) < 3.2f*dt) {
      float A = 22.0f + 28.0f*((float)esp_random()*(1.0f/4294967296.0f));
      float sgn = ((float)esp_random()*(1.0f/4294967296.0f)) < 0.5f ? -1.0f : 1.0f;
      spikeY = sgn*A;
    }
    spikeY *= expf(-4.5f*dt);
    yawRate_dps += spikeY;
    if (yawRate_dps > 45.0f) yawRate_dps = 45.0f;
    if (yawRate_dps < -45.0f) yawRate_dps = -45.0f;
    R.yaw_deg = wrapDeg180(R.yaw_deg + yawRate_dps * dt);
  }

  static float pitchRate_dps = 0.0f;
  {
    const float THETAp = 0.30f, SIGMAp = 12.0f;
    float ur = (float)esp_random() * (1.0f/4294967296.0f);
    float eta = 2.0f*ur - 1.0f;
    pitchRate_dps += THETAp*(0.0f - pitchRate_dps)*dt + SIGMAp*sqrtf(dt)*eta;
    pitchRate_dps += 8.0f*sinf(1.7f*t + 0.3f) + 6.0f*sinf(2.6f*t + 2.0f);
  }

  float yawRad   = R.yaw_deg        * (float)M_PI/180.f;
  float pitchRad = R.pitch_prog_deg * (float)M_PI/180.f;
  float fN = cosf(pitchRad) * cosf(yawRad);
  float fE = cosf(pitchRad) * sinf(yawRad);
  float fU = sinf(pitchRad);

  const float a_gE = 0.0f, a_gN = 0.0f;
  const float a_gU = -g0 * (R_earth*R_earth)/((R_earth+alt_m)*(R_earth+alt_m));

  const float invM = 1.0f / fmaxf(R.m_kg, 1e-3f);
  const float drag = DRAG_SCALE * 0.5f * rho * R.CdA * v2 * invM;
  const float a_dE = -drag * uE;
  const float a_dN = -drag * uN;
  const float a_dU = -drag * uU;

  const float alt_sp = 500.0f + 60.0f*sinf(0.02f*t);
  static float i_h = 0.0f;
  const float e_h = alt_sp - alt_m;
  const float Kp = 0.060f;
  const float Ki = 0.0080f;
  const float Kv = 0.80f;
  i_h += e_h * dt; 
  i_h = clampf(i_h, -7000.f, 7000.f);
  const float aU_des = Kp*e_h + Ki*i_h - Kv*R.vU_mps;

  float need = aU_des - (a_dU + a_gU);
  float a_tU_max = (THRUST_CAP * invM);
  float fU_des = need / fmaxf(a_tU_max, 1e-3f);
  const float fU_min = sinf(25.0f*(float)M_PI/180.f);
  const float fU_max = sinf(80.0f*(float)M_PI/180.f);
  fU_des = clampf(fU_des, fU_min, fU_max);

  float pitch_des_deg = asinf(fU_des) * 180.f/(float)M_PI;

  bool needClimbHard = (e_h > 20.0f && R.vU_mps < 3.0f) || (alt_m < 60.0f) || (vU < -0.5f);
  if (needClimbHard) {
    float tau = 0.18f;
    R.pitch_prog_deg += (pitch_des_deg - R.pitch_prog_deg) * (dt / tau);
    pitchRate_dps *= 0.2f;
  } else {
    R.pitch_prog_deg += pitchRate_dps * dt;
    R.pitch_prog_deg += (pitch_des_deg - R.pitch_prog_deg) * (dt / 1.2f);
  }

  if (R.pitch_prog_deg < 10.0f) R.pitch_prog_deg = 10.0f;
  if (R.pitch_prog_deg > 85.0f) R.pitch_prog_deg = 85.0f;

  pitchRad = R.pitch_prog_deg * (float)M_PI/180.f;
  fN = cosf(pitchRad) * cosf(yawRad);
  fE = cosf(pitchRad) * sinf(yawRad);
  fU = sinf(pitchRad);

  float thrust_cmd;
  {
    float denom = fmaxf(fU, 0.05f);
    float T_req = (need / denom) * (1.0f/invM);
    bool emergency = (alt_m < 8.0f) || (alt_m < 25.0f && vU < -1.5f);
    if (emergency) T_req = THRUST_CAP;
    thrust_cmd = clampf(T_req, 0.0f, THRUST_CAP);
  }

  const float a_tE = (thrust_cmd * invM) * fE;
  const float a_tN = (thrust_cmd * invM) * fN;
  const float a_tU = (thrust_cmd * invM) * fU;

  const float windE = 2.0f + 0.003f*alt_m + 1.2f*sinf(0.05f*t) + 0.6f*sinf(0.13f*t + 2.0f);
  const float windN = -1.0f + 0.001f*alt_m + 0.9f*sinf(0.04f*t + 0.7f);

  const float aE = a_tE + a_dE + a_gE;
  const float aN = a_tN + a_dN + a_gN;
  const float aU = a_tU + a_dU + a_gU;

  R.vE_mps += aE * dt;
  R.vN_mps += aN * dt;
  R.vU_mps += aU * dt;

  R.xE_m += (R.vE_mps + windE) * dt;
  R.yN_m += (R.vN_mps + windN) * dt;
  R.zU_m +=  R.vU_mps * dt;

  if (R.zU_m < 0.f) {
    R.zU_m = 0.f; R.vU_mps = 0.f; 
    static float i_h_local = 0.f;
    i_h_local += 400.0f;
    R.pitch_prog_deg = fmaxf(R.pitch_prog_deg, 60.0f);
  }

  const float gE = R.vE_mps + windE;
  const float gN = R.vN_mps + windN;
  const float horiz = sqrtf(gN*gN + gE*gE) + 1e-6f;
  const float yaw_meas   = atan2f(gE, gN) * 180.f/(float)M_PI;
  const float pitch_meas = atanf(R.vU_mps / horiz) * 180.f/(float)M_PI;
  if (horiz > 5.0f)
    R.yaw_deg = wrapDeg180(R.yaw_deg + 0.004f * wrapDeg180(yaw_meas - R.yaw_deg));
  R.pitch_deg = R.pitch_deg + 0.20f * (pitch_meas - R.pitch_deg);
  R.roll_deg  = 10.0f * sinf(1.2f*t);

  const float dLat = (R.yN_m / R_earth) * 180.f/(float)M_PI;
  const float dLon = (R.xE_m / (R_earth * cosf(R.lat0_deg*(float)M_PI/180.f))) * 180.f/(float)M_PI;
  const float outLat  = 48.1486f + dLat;
  const float outLon  = 17.1077f + dLon;
  const float outElev = R.h0_m + R.zU_m;

  memset(out, 0, 28);
  out[0] = 0x03;
  writeLE32(out,  4, outLon);
  writeLE32(out,  8, outLat);
  writeLE32(out, 12, outElev);
  writeLE32(out, 16, R.roll_deg);
  writeLE32(out, 20, R.pitch_deg);
  writeLE32(out, 24, R.yaw_deg);
}



void handleData(){
  uint8_t buf[28];
  simStepFill(buf);

  WiFiClient c = server.client();
  if(!c) return;

  c.setNoDelay(true);
  c.print(F("HTTP/1.1 200 OK\r\n"
            "Content-Type: application/octet-stream\r\n"
            "Cache-Control: no-store, max-age=0\r\n"
            "Connection: keep-alive\r\n"
            "Content-Length: 28\r\n\r\n"));
  c.write(buf, sizeof(buf));
}

void handleSDStatus(){
  bool available = false;
  spiSD.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  if (SD.begin(SD_CS, spiSD)) {
    available = true;
  }
  SD.end();
  String json = "{\"available\":" + String(available ? "true" : "false") + "}";
  server.send(200, "application/json", json);
}

void onWiFiEvent(WiFiEvent_t e, WiFiEventInfo_t info){
  switch(e){
    case ARDUINO_EVENT_WIFI_AP_STACONNECTED:
      Serial.printf("[AP] STA connect %02X:%02X:%02X:%02X:%02X:%02X\n",
        info.wifi_ap_staconnected.mac[0],info.wifi_ap_staconnected.mac[1],
        info.wifi_ap_staconnected.mac[2],info.wifi_ap_staconnected.mac[3],
        info.wifi_ap_staconnected.mac[4],info.wifi_ap_staconnected.mac[5]); break;
    case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
      Serial.printf("[AP] STA disconnect\n"); break;
    default: break;
  }
}

static int getFirstStationRssiDbm(){
  wifi_sta_list_t stas;
  if (esp_wifi_ap_get_sta_list(&stas) != ESP_OK) return -127;
  if (stas.num <= 0) return -127;
  return stas.sta[0].rssi;
}

void handlePreflight(){
  static uint32_t t0 = 0;
  if(!t0) t0 = millis();

  const float t = (millis() - t0) * 0.001f;

  //gps lock after 4s
  const bool gps_ok = (t > 4.0f);

  //sensors calibrated after 7s
  const bool sensors_ok = (t > 7.0f);

  const int staCount = WiFi.softAPgetStationNum();
  int rssi_dbm = (staCount > 0) ? getFirstStationRssiDbm() : -127;

  if(staCount > 0 && rssi_dbm <= -120){
    rssi_dbm = -62 + (int)(8.0f * sinf(0.9f * t));
  }

  const bool radio_ok = (staCount > 0) && (rssi_dbm > -85);

  float batt_v = 3.55f - 0.0035f * t;
  batt_v = clampf(batt_v, 3.10f, 3.60f);

  const bool batt_ok = (batt_v >= 3.0f);

  const bool ready = gps_ok && sensors_ok && radio_ok && batt_ok;

  char json[240];
  snprintf(json, sizeof(json),
    "{\"gps_lock\":%s,"
    "\"sensors_cal\":%s,"
    "\"radio_ok\":%s,"
    "\"rssi_dbm\":%d,"
    "\"batt_v\":%.2f,"
    "\"ready\":%s}",
    gps_ok ? "true" : "false",
    sensors_ok ? "true" : "false",
    radio_ok ? "true" : "false",
    rssi_dbm,
    batt_v,
    ready ? "true" : "false"
  );

  server.send(200, "application/json", json);
}

void setup(){
  Serial.begin(115200);
  WiFi.onEvent(onWiFiEvent);

  pinMode(LED_PIN, OUTPUT); digitalWrite(LED_PIN, LOW);

  WiFi.persistent(false);
  WiFi.mode(WIFI_AP);
  WiFi.setSleep(false);       
  esp_wifi_set_ps(WIFI_PS_NONE);

  wifi_country_t country = {"EU", 1, 13, 0, WIFI_COUNTRY_POLICY_AUTO};
  esp_wifi_set_country(&country);
  esp_wifi_set_bandwidth(WIFI_IF_AP, WIFI_BW_HT20);
  esp_wifi_set_protocol(WIFI_IF_AP, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);

  WiFi.setTxPower(WIFI_POWER_19_5dBm);

  WiFi.softAPConfig(apIP, apIP, IPAddress(255,255,255,0));
  const int channel = 11;
  WiFi.softAP(SSID, PASS, channel, /*hidden*/false, /*max_conn*/4);

  wifi_config_t conf{};
  esp_wifi_get_config(WIFI_IF_AP, &conf);
  conf.ap.beacon_interval = 100;
  esp_wifi_set_config(WIFI_IF_AP, &conf);

  MDNS.begin("arct-signalis");

  server.on("/", sendIndex);
  server.on("/app", sendApp);
  server.on("/data", HTTP_GET, handleData);
  server.on("/sd-status", HTTP_GET, handleSDStatus);
  server.on("/preflight", HTTP_GET, handlePreflight);
  server.begin();

  //simInit();
}

void loop(){
  server.handleClient();
  delay(1);
  static uint32_t lastToggle=0; static bool ledState=false;
  int connected=WiFi.softAPgetStationNum();
  if(connected>0) digitalWrite(LED_PIN, HIGH);
  else{
    uint32_t now=millis();
    if(now-lastToggle>=BLINK_MS){ ledState=!ledState; digitalWrite(LED_PIN, ledState?HIGH:LOW); lastToggle=now; }
  }

  static uint32_t lastHeap=0;
  if(millis() - lastHeap > 5000){
    Serial.printf("[HEAP] free=%u\n", (unsigned)ESP.getFreeHeap());
    lastHeap = millis();
  }
}