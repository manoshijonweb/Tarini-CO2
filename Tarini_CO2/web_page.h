#pragma once
#include <Arduino.h>

// Dashboard served at "/". Self-contained (no CDN) so it works on the offline hotspot.
const char INDEX_HTML[] PROGMEM = R"HTML(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Tarini CO2</title>
<style>
:root{--bg:#fafafa;--fg:#111;--muted:#787878;--line:#e6e6e6;--soft:#efefef;--on:#16a34a;
--good:#16a34a;--fair:#ca8a04;--poor:#ea580c;--bad:#dc2626}
@media (prefers-color-scheme:dark){:root{--bg:#0b0b0c;--fg:#ececec;--muted:#8a8a8f;--line:#262628;--soft:#18181a;--on:#22c55e;
--good:#22c55e;--fair:#eab308;--poor:#fb923c;--bad:#f87171}}
*{box-sizing:border-box}
[hidden]{display:none!important}
body{margin:0;background:var(--bg);color:var(--fg);font:15px/1.5 system-ui,-apple-system,"Segoe UI",Roboto,sans-serif;-webkit-font-smoothing:antialiased}
.wrap{max-width:760px;margin:0 auto;padding:0 20px 48px}
header{display:flex;align-items:center;justify-content:space-between;gap:16px;padding:22px 0 0}
.brand{display:flex;align-items:center;gap:9px;font-weight:600}
.dot{width:7px;height:7px;border-radius:50%;background:var(--muted)}
.dot.on{background:var(--on)}.dot.off{background:var(--bad)}
nav{display:flex;gap:22px}
nav button{border:0;background:none;padding:6px 0;font:inherit;color:var(--muted);cursor:pointer;border-bottom:1.5px solid transparent}
nav button.active{color:var(--fg);border-color:var(--fg)}
section{padding:30px 0;border-top:1px solid var(--line)}
section:first-child{border-top:0}
h2{margin:0;font-size:13px;font-weight:500;color:var(--muted)}
.sh{display:flex;align-items:center;justify-content:space-between;gap:12px;margin-bottom:14px}
.muted{color:var(--muted)}
.small{font-size:13px}

.hero{padding:44px 0 34px}
.big{display:flex;align-items:baseline;gap:10px}
#ppm{font-size:clamp(72px,20vw,120px);font-weight:300;line-height:.95;letter-spacing:-.04em;font-variant-numeric:tabular-nums}
.big .u{font-size:18px;color:var(--muted)}
.lvl{display:flex;align-items:center;gap:8px;margin-top:16px;font-weight:500}
.lvl i{width:9px;height:9px;border-radius:50%;background:var(--muted)}
#sub{margin:4px 0 0}

.item{display:flex;align-items:center;justify-content:space-between;gap:12px;padding:12px 0}
.item+.item{border-top:1px solid var(--line)}
.state{font-size:13px;color:var(--muted)}
.item.on .state{color:var(--on)}
.sw{position:relative;flex:none;width:44px;height:26px;border:0;border-radius:13px;background:var(--line);cursor:pointer;transition:background .2s}
.sw::after{content:"";position:absolute;top:3px;left:3px;width:20px;height:20px;border-radius:50%;background:#fff;
box-shadow:0 1px 2px rgba(0,0,0,.3);transition:transform .2s}
.sw[aria-checked=true]{background:var(--on)}
.sw[aria-checked=true]::after{transform:translateX(18px)}
.seg{display:inline-flex;gap:2px;background:var(--soft);border-radius:8px;padding:2px}
.seg button{border:0;background:none;padding:4px 11px;border-radius:6px;font:inherit;font-size:13px;color:var(--muted);cursor:pointer}
.seg button.active{background:var(--bg);color:var(--fg);box-shadow:0 1px 2px rgba(0,0,0,.12)}
#note{margin:10px 0 0}
canvas{display:block;width:100%;height:220px}

label{display:block;font-size:13px;color:var(--muted);margin-bottom:6px}
input[type=number],input[type=text],input[type=password]{width:100%;padding:9px 0;border:0;border-bottom:1px solid var(--line);
border-radius:0;background:none;color:var(--fg);font:inherit;outline:none}
input:focus{border-color:var(--fg)}
.g2{display:grid;grid-template-columns:1fr 1fr;gap:20px;margin-bottom:18px}
.check{display:flex;align-items:center;gap:10px;color:var(--fg);font-size:14px;margin:0 0 4px}
.check input{width:16px;height:16px;margin:0;accent-color:var(--fg)}
p.hint{font-size:13px;color:var(--muted);margin:0 0 18px}
.btn{border:1px solid var(--line);background:none;color:var(--fg);padding:8px 16px;border-radius:8px;font:inherit;font-size:14px;cursor:pointer}
.btn.primary{background:var(--fg);border-color:var(--fg);color:var(--bg)}
dl{display:grid;grid-template-columns:auto 1fr;gap:10px 20px;margin:0;font-size:14px}
.wstat{display:flex;align-items:baseline;gap:9px;font-size:14px;margin:0 0 20px}
.wstat:empty{display:none}
.wstat .dot{flex:none;transform:translateY(-1px)}
.wstat a{color:inherit}
.dot.wait{background:var(--fair);animation:blink 1s ease-in-out infinite}
@keyframes blink{50%{opacity:.25}}
dt{color:var(--muted)}dd{margin:0;text-align:right;overflow-wrap:anywhere;font-variant-numeric:tabular-nums}
.toast{position:fixed;left:50%;bottom:22px;transform:translateX(-50%);max-width:calc(100% - 40px);background:var(--fg);color:var(--bg);
padding:9px 16px;border-radius:8px;font-size:14px;opacity:0;transition:opacity .2s;pointer-events:none}
.toast.show{opacity:1}
</style>
</head>
<body>
<div class="wrap">
<header>
  <div class="brand"><span class="dot" id="dot" title="Connection"></span>CO&#8322; Monitor</div>
  <nav><button data-tab="dash">Dashboard</button><button data-tab="setup">Setup</button></nav>
</header>

<main id="dash">
  <section class="hero">
    <div class="big"><span id="ppm">--</span><span class="u">ppm</span></div>
    <div class="lvl"><i id="lvdot"></i><span id="level">Connecting&hellip;</span></div>
    <p class="muted small" id="sub"></p>
  </section>

  <section>
    <div class="sh"><h2>Fans</h2><div class="seg" id="mode"><button data-mode="auto">Auto</button><button data-mode="manual">Manual</button></div></div>
    <div class="item" id="intake"><div><div>Intake</div><div class="state">&mdash;</div></div><button class="sw" role="switch" aria-checked="false" aria-label="Intake fan"></button></div>
    <div class="item" id="exhaust"><div><div>Exhaust</div><div class="state">&mdash;</div></div><button class="sw" role="switch" aria-checked="false" aria-label="Exhaust fan"></button></div>
    <p class="muted small" id="note"></p>
  </section>

  <section>
    <div class="sh"><h2>History</h2><div class="seg" id="range"><button data-r="900">15m</button><button data-r="1800">30m</button><button data-r="3600" class="active">1h</button></div></div>
    <canvas id="chart"></canvas>
    <p class="muted small" style="margin:10px 0 0">Shaded areas show when the fans were running.</p>
  </section>
</main>

<main id="setup" hidden>
  <section>
    <div class="sh"><h2>Automatic control</h2></div>
    <form id="settings">
      <div class="g2">
        <div><label for="onPpm">Fans on at (ppm)</label><input type="number" id="onPpm" min="450" max="5000" step="10" required></div>
        <div><label for="offPpm">Fans off at (ppm)</label><input type="number" id="offPpm" min="400" max="4950" step="10" required></div>
      </div>
      <label class="check"><input type="checkbox" id="abc">Automatic baseline calibration (ABC)</label>
      <p class="hint">Keep ABC on only if the room gets fresh air (about 400&nbsp;ppm) at least once a day.</p>
      <button class="btn primary" type="submit">Save</button>
    </form>
  </section>

  <section>
    <div class="sh"><h2>Calibration</h2></div>
    <p class="hint">Leave the sensor in fresh outdoor air for 20 minutes, then calibrate. The current reading becomes 400&nbsp;ppm.</p>
    <button class="btn" id="cal">Calibrate to 400 ppm</button>
  </section>

  <section>
    <div class="sh"><h2>Wi-Fi</h2></div>
    <p class="wstat" id="wstat"></p>
    <form id="wifi">
      <div class="g2">
        <div><label for="ssid">Network name</label><input type="text" id="ssid" maxlength="32" autocomplete="off"></div>
        <div><label for="pass">Password</label><input type="password" id="pass" maxlength="63"></div>
      </div>
      <p class="hint">2.4&nbsp;GHz networks only. The Tarini-CO2 hotspot stays on, but your phone may drop off it for a few seconds while the device joins. Leave the name empty to use the hotspot only.</p>
      <button class="btn primary" type="submit">Connect</button>
    </form>
  </section>

  <section>
    <div class="sh"><h2>Device</h2></div>
    <dl id="dev"></dl>
  </section>
</main>
</div>
<div class="toast" id="toast" role="status"></div>
<script>
const $=id=>document.getElementById(id);
const css=v=>getComputedStyle(document.documentElement).getPropertyValue(v).trim();
const LV={good:'Good',fair:'Fair',poor:'Poor',bad:'Bad'};
let S=null,H=null,range=3600,lastOk=0,dirty=false;

function toast(m){const t=$('toast');t.textContent=m;t.classList.add('show');clearTimeout(t.h);t.h=setTimeout(()=>t.classList.remove('show'),2800)}
function esc(v){return String(v).replace(/[&<>"]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]))}
function uptime(s){const d=s/86400|0,h=s%86400/3600|0,m=s%3600/60|0;return(d?d+'d ':'')+(d||h?h+'h ':'')+m+'m'}
function signal(r){return r>=-55?'excellent':r>=-67?'good':r>=-75?'fair':'weak'}
async function post(u,d){
  const r=await fetch(u,{method:'POST',body:new URLSearchParams(d)});let j={};
  try{j=await r.json()}catch(e){}
  if(!r.ok)throw new Error(j.error||('Request failed ('+r.status+')'));return j}
function act(p){return p.then(poll).catch(e=>toast(e.message))}

function tab(t){
  for(const b of document.querySelectorAll('nav button'))b.classList.toggle('active',b.dataset.tab===t);
  $('dash').hidden=t!=='dash';$('setup').hidden=t!=='setup';
  history.replaceState(null,'','#'+t);if(t==='dash')draw()}

async function poll(){
  try{const r=await fetch('/api/status',{cache:'no-store'});if(!r.ok)throw 0;S=await r.json();lastOk=Date.now();render()}catch(e){}
  conn()}
function conn(){const ok=Date.now()-lastOk<6000;$('dot').className='dot '+(ok?'on':'off');$('dot').title=ok?'Connected':'Offline'}

function render(){
  const s=S,ready=s.warm&&s.sensorOk&&s.ppm>0;
  let col='--muted',txt,sub='';
  if(!s.sensorOk){col='--bad';txt='Sensor error';sub='No reply from the MH-Z19E. Check its wiring and 5V supply.'}
  else if(!s.warm){txt='Sensor warming up';sub='Ready in '+s.warmupLeft+' s'}
  else{col='--'+s.level;txt=LV[s.level]||'—';sub='Sensor '+s.temp+' °C'}
  $('ppm').textContent=ready?s.ppm:'--';
  $('level').textContent=txt;$('lvdot').style.background='var('+col+')';$('sub').textContent=sub;

  for(const k of ['intake','exhaust']){const on=s[k],el=$(k);el.classList.toggle('on',on);
    el.querySelector('.state').textContent=on?'Running':'Off';el.querySelector('.sw').setAttribute('aria-checked',on)}
  for(const b of document.querySelectorAll('#mode button'))b.classList.toggle('active',b.dataset.mode===s.mode);
  $('note').textContent=s.mode==='auto'?`On at ${s.onPpm} ppm, off at ${s.offPpm} ppm.`:'Manual. Fans stay as you set them.';
  if(!dirty){$('onPpm').value=s.onPpm;$('offPpm').value=s.offPpm;$('abc').checked=s.abc}

  const ssid=`<b>${esc(s.ssid)}</b>`,ip=esc(s.ip);
  $('wstat').innerHTML={
    connected:`<i class="dot on"></i><span>Connected to ${ssid}. On that network, open <a href="http://${ip}">http://${ip}</a></span>`,
    connecting:`<i class="dot wait"></i><span>Connecting to ${ssid}&hellip;</span>`,
    failed:`<i class="dot off"></i><span>Couldn't connect to ${ssid}: ${esc(s.wifiError)}. Retrying every 2 minutes.</span>`,
    off:`<i class="dot"></i><span>Not connected. Using the ${esc(s.apSsid)} hotspot.</span>`}[s.wifi]||'';
  const net={connected:`${s.ssid} (${signal(s.rssi)})`,connecting:'Connecting to Wi-Fi…',failed:'Not connected',off:'Not set up'}[s.wifi];
  const rows=[['Wi-Fi',net],['Address',s.staConnected?`http://${s.ip}`:'—'],['Local name',`http://${s.mdns}.local`],
    ['Hotspot',`${s.apSsid} · ${s.apIp}`],['Uptime',uptime(s.uptime)],['Last restart',`${s.resetReason} (boot #${s.boots})`],['LCD',s.lcd?'Connected':'Not detected'],
    ['Sensor errors',s.errors],['Free memory',(s.heap/1024|0)+' KB']];
  $('dev').innerHTML=rows.map(r=>`<dt>${r[0]}</dt><dd>${esc(r[1])}</dd>`).join('');
}

async function hist(){try{const r=await fetch('/api/history',{cache:'no-store'});if(r.ok){H=await r.json();draw()}}catch(e){}}
function draw(){
  const c=$('chart'),dpr=window.devicePixelRatio||1,w=c.clientWidth,h=c.clientHeight;if(!w)return;
  c.width=w*dpr;c.height=h*dpr;const g=c.getContext('2d');g.setTransform(dpr,0,0,dpr,0,0);
  const L=38,R=4,T=8,B=24,pw=w-L-R,ph=h-T-B,muted=css('--muted');
  g.font='11px system-ui,sans-serif';g.fillStyle=muted;
  const iv=H?H.interval:10,n=H?Math.min(H.ppm.length,Math.floor(range/iv)):0;
  const P=n?H.ppm.slice(-n):[],F=n?H.fan.slice(-n):[],vals=P.filter(v=>v>0);
  if(!vals.length){g.textAlign='center';g.textBaseline='middle';
    g.fillText(S&&!S.warm?'Starts after warm-up':'No data yet',w/2,h/2);return}
  let lo=Math.min(400,...vals),hi=Math.max(S?S.onPpm+200:1200,...vals);
  const step=hi-lo>2400?1000:400;lo=Math.floor(lo/step)*step;hi=Math.ceil(hi/step)*step;
  const X=i=>L+pw-(n-1-i)*iv/range*pw,Y=v=>T+(1-(v-lo)/(hi-lo))*ph,bw=iv/range*pw;

  g.fillStyle=css('--soft');
  for(let i=0;i<n;i++){if(!(F[i]&3))continue;const a=i;while(i+1<n&&F[i+1]&3)i++;
    const x0=Math.max(L,Math.round(X(a)-bw));g.fillRect(x0,T,Math.round(X(i))-x0,ph)}

  g.fillStyle=muted;g.textAlign='left';g.textBaseline='middle';
  for(let v=lo;v<=hi;v+=step)g.fillText(v,0,Y(v));
  g.strokeStyle=css('--line');g.lineWidth=1;g.beginPath();g.moveTo(L,T+ph+.5);g.lineTo(w-R,T+ph+.5);g.stroke();
  g.textBaseline='top';
  for(let k=0;k<=2;k++){g.textAlign=['left','center','right'][k];
    g.fillText(k===2?'now':'-'+Math.round(range*(2-k)/2/60)+'m',L+pw*k/2,h-B+8)}

  if(S){g.setLineDash([3,4]);g.strokeStyle=muted;g.textAlign='right';g.textBaseline='bottom';
    for(const [v,t] of [[S.onPpm,'on'],[S.offPpm,'off']]){if(v<lo||v>hi)continue;const y=Math.round(Y(v))+.5;
      g.beginPath();g.moveTo(L,y);g.lineTo(w-R,y);g.stroke();g.fillText(t,w-R,y-3)}
    g.setLineDash([])}

  g.strokeStyle=css('--fg');g.lineWidth=1.75;g.lineJoin='round';g.beginPath();let pen=false;
  for(let i=0;i<n;i++){const v=P[i];if(v<=0){pen=false;continue}const x=X(i),y=Y(v);pen?g.lineTo(x,y):g.moveTo(x,y);pen=true}
  g.stroke();
}

for(const b of document.querySelectorAll('nav button'))b.onclick=()=>tab(b.dataset.tab);
for(const b of document.querySelectorAll('#mode button'))b.onclick=()=>act(post('/api/mode',{mode:b.dataset.mode}));
for(const k of ['intake','exhaust'])$(k).querySelector('.sw').onclick=()=>{if(!S)return;const wasAuto=S.mode==='auto';
  act(post('/api/fan',{fan:k,state:S[k]?'off':'on'}).then(()=>{if(wasAuto)toast('Switched to Manual')}))};
for(const b of document.querySelectorAll('#range button'))b.onclick=()=>{range=+b.dataset.r;
  for(const x of document.querySelectorAll('#range button'))x.classList.toggle('active',x===b);draw()};
$('settings').oninput=()=>{dirty=true};
$('settings').onsubmit=e=>{e.preventDefault();
  act(post('/api/settings',{onPpm:$('onPpm').value,offPpm:$('offPpm').value,abc:$('abc').checked?1:0})
    .then(()=>{dirty=false;toast('Saved');draw()}))};
$('cal').onclick=()=>{if(confirm('This sets the current reading as 400 ppm.\n\nOnly continue if the sensor has been in fresh outdoor air for at least 20 minutes.'))
  act(post('/api/calibrate',{}).then(()=>toast('Calibration sent')))};
$('wifi').onsubmit=e=>{e.preventDefault();const ssid=$('ssid').value.trim();
  if(!ssid&&!confirm('Disconnect from Wi-Fi and use the hotspot only?'))return;
  act(post('/api/wifi',{ssid,pass:$('pass').value}).then(()=>{$('pass').value='';toast(ssid?'Connecting to Wi-Fi…':'Wi-Fi cleared')}))};

window.addEventListener('resize',draw);
tab(location.hash==='#setup'?'setup':'dash');poll();hist();
setInterval(poll,2000);setInterval(hist,10000);setInterval(conn,1000);
</script>
</body>
</html>
)HTML";
