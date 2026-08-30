#pragma once

#include <Arduino.h>

// English-only captive portal UI stored in flash (PROGMEM).
static const char EWP_PORTAL_HTML[] PROGMEM = R"EWPHTML(
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP32 Wi-Fi Setup</title>
<style>
:root{font-family:system-ui,-apple-system,Segoe UI,Roboto,sans-serif;color-scheme:light dark}
body{margin:0;min-height:100vh;display:grid;place-items:center;background:#f3f5f7;color:#18202a}
.card{width:min(92vw,440px);background:#fff;border-radius:16px;padding:24px;box-shadow:0 12px 35px #0002}
h1{margin:0 0 6px;font-size:1.45rem}.muted{color:#68717d;margin:0 0 20px}
label{display:block;font-weight:600;margin:14px 0 6px}select,input,button{width:100%;box-sizing:border-box;border-radius:10px;border:1px solid #cbd2da;padding:12px;font:inherit}
button{margin-top:18px;border:0;background:#0d6efd;color:#fff;font-weight:700;cursor:pointer}.row{display:flex;gap:8px}.row select{flex:1}.row button{width:auto;margin:0;padding:0 14px}
#msg{min-height:1.2em;margin-top:12px;font-size:.92rem}.small{font-size:.82rem;color:#7b8490;margin-top:16px}
@media(prefers-color-scheme:dark){body{background:#11151a;color:#edf2f7}.card{background:#1c2229}.muted,.small{color:#aab3bd}select,input{background:#12171d;color:#edf2f7;border-color:#3b4550}}
</style>
</head>
<body>
<main class="card">
<h1>Wi-Fi Setup</h1>
<p class="muted">Select a network and enter its password.</p>
<form id="wifiForm" method="post" action="/save">
<label for="ssid">Network</label>
<div class="row"><select id="ssid" name="ssid" required><option>Scanning...</option></select><button type="button" id="refresh">Scan</button></div>
<label for="password">Password</label>
<input id="password" name="password" type="password" autocomplete="current-password" maxlength="63" placeholder="Leave empty for an open network">
<button type="submit">Save and Connect</button>
</form>
<div id="msg"></div>
<div class="small">Device setup portal</div>
</main>
<script>
const ssid=document.getElementById('ssid'),msg=document.getElementById('msg');
async function scan(){
  msg.textContent='Scanning Wi-Fi networks...';
  try{
    const r=await fetch('/scan',{cache:'no-store'}),j=await r.json();
    ssid.innerHTML='';
    if(!j.networks.length){ssid.innerHTML='<option value="">No networks found</option>';msg.textContent='No networks found.';return;}
    j.networks.forEach(n=>{const o=document.createElement('option');o.value=n.ssid;o.textContent=`${n.ssid} (${n.rssi} dBm)${n.open?' - Open':''}`;ssid.appendChild(o)});
    msg.textContent='';
  }catch(e){msg.textContent='Scan failed. Tap Scan to retry.'}
}
document.getElementById('refresh').onclick=scan;
document.getElementById('wifiForm').addEventListener('submit',()=>{msg.textContent='Saving settings...';});
scan();
</script>
</body>
</html>
)EWPHTML";

static const char EWP_CONNECTING_HTML[] PROGMEM = R"EWPHTML(
<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Connecting</title>
<style>body{font-family:system-ui,-apple-system,Segoe UI,sans-serif;margin:0;min-height:100vh;display:grid;place-items:center;background:#f3f5f7}.c{width:min(88vw,420px);background:#fff;padding:26px;border-radius:16px;box-shadow:0 12px 35px #0002}h1{font-size:1.4rem}p{line-height:1.5;color:#58616d}a{display:none;margin-top:16px}</style></head>
<body><div class="c"><h1>Connecting...</h1><p id="msg">The ESP32 is testing the selected Wi-Fi network.</p><a id="retry" href="/">Return to Wi-Fi setup</a></div>
<script>
async function check(){try{const r=await fetch('/status',{cache:'no-store'}),j=await r.json();if(j.connected){document.getElementById('msg').textContent='Connected successfully. You can close this page.';return;}if(j.error){document.getElementById('msg').textContent=j.error;document.getElementById('retry').style.display='inline-block';return;}setTimeout(check,800)}catch(e){document.getElementById('msg').textContent='The device changed networks. If setup succeeded, you can close this page.'}}check();
</script></body></html>
)EWPHTML";
