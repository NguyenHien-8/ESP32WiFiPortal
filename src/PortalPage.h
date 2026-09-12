/**
 * @file PortalPage.h
 * @author Tran Nguyen Hien (trannguyenhien29085@gmail.com)
 * @brief ESP32 Wi-Fi captive portal HTML pages stored in flash (PROGMEM)
 * @version 2.1.1
 * @date 2026-09-10
 *
 * @copyright Copyright (c) 2026 Tran Nguyen Hien. All rights reserved.
 */

#pragma once

#include <Arduino.h>

// English-only captive portal UI stored in flash (PROGMEM).
static const char EWP_PORTAL_HTML[] PROGMEM = R"EWPHTML(
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<meta name="theme-color" content="#f3f3f3">
<title>ESP32 WiFi Portal</title>
<style>
:root{font-family:"Segoe UI",system-ui,-apple-system,sans-serif;color:#1b1b1b;color-scheme:light;--accent:#0067c0;--panel:#f3f3f3;--selected:#e6e6e6;--line:#d1d1d1}
*{box-sizing:border-box}
body{margin:0;min-height:100vh;padding:24px;background:linear-gradient(135deg,#eef4f8,#f8f8f8 48%,#eef3f5);display:grid;place-items:center}
button,input{font:inherit}
button{touch-action:manipulation}
.shell{width:min(100%,560px);min-height:min(720px,calc(100vh - 48px));border-radius:12px;background:#f3f3f3f7;box-shadow:0 18px 55px #0002;overflow:hidden}
.top{position:relative;display:flex;align-items:center;justify-content:space-between;padding:24px 24px 16px}
h1{font-size:1.55rem;font-weight:600;margin:0}.brand{margin:2px 0 0;color:#5b5b5b;font-size:.88rem}
.icon-btn{width:44px;height:44px;display:grid;place-items:center;border:0;border-radius:4px;background:transparent;color:#202020;cursor:pointer}
.icon-btn:hover{background:#e5e5e5}.icon-btn:focus-visible,.network-main:focus-visible,.btn:focus-visible,input:focus-visible,.advanced-link:focus-visible,.back-link:focus-visible{outline:2px solid var(--accent);outline-offset:2px}
.icon-btn svg{width:20px;height:20px}.icon-btn:disabled{opacity:.45;cursor:default}
.advanced-link{display:inline-block;margin-top:7px;color:var(--accent);font-size:.88rem;text-decoration:underline;text-underline-offset:3px}
.scan-progress{position:absolute;right:24px;bottom:0;left:24px;height:3px;overflow:hidden;border-radius:999px;background:rgba(0,103,192,.16);opacity:0;visibility:hidden;transition:opacity .15s ease}
.scan-progress.active{opacity:1;visibility:visible}.scan-progress:before{content:"";position:absolute;top:0;bottom:0;left:0;width:36%;border-radius:inherit;background:linear-gradient(90deg,transparent,var(--accent) 18%,var(--accent) 82%,transparent);transform:translate3d(-130%,0,0)}.scan-progress.active:before{will-change:transform;animation:scan-progress 1.15s linear infinite}
.scan-status{min-height:38px;padding:7px 24px 10px;color:#5b5b5b;font-size:.9rem}
.network-list{padding:0 8px 20px}
.network{position:relative;border-radius:5px;overflow:hidden}
.network+.network{margin-top:2px}.network:hover{background:#eaeaea}.network.selected{background:var(--selected)}
.network.selected:before{content:"";position:absolute;left:0;top:20px;bottom:20px;width:4px;border-radius:3px;background:var(--accent)}
.network-main{width:100%;min-height:76px;padding:15px 20px;display:grid;grid-template-columns:38px minmax(0,1fr);gap:10px;align-items:center;border:0;background:transparent;color:inherit;text-align:left;cursor:pointer}
.network-copy{min-width:0}.ssid{display:block;overflow:hidden;text-overflow:ellipsis;white-space:nowrap;font-size:1rem}.subtitle{display:none;margin-top:2px;color:#5b5b5b;font-size:.91rem}.network.selected .subtitle{display:block}
.signal{width:30px;height:30px;overflow:visible;color:#202020}.signal .wave{fill:none;stroke:currentColor;stroke-width:1.8;stroke-linecap:round;opacity:.17}.signal .dot{fill:currentColor}.signal.l2 .inner,.signal.l3 .inner,.signal.l3 .middle,.signal.l4 .inner,.signal.l4 .middle,.signal.l4 .outer{opacity:1}.signal .lock{fill:currentColor;stroke:none}.signal .lock-loop{fill:none;stroke:currentColor;stroke-width:1.5;stroke-linecap:round}
.network-details{display:none;padding:0 15px 14px 65px}.network.selected .network-details{display:block}
.choose-panel,.password-panel{min-height:48px}.password-panel[hidden],.choose-panel[hidden]{display:none}
.actions{display:flex;justify-content:flex-end;gap:6px}.btn{min-width:122px;min-height:42px;padding:8px 18px;border:1px solid #b8b8b8;border-radius:4px;background:#fbfbfb;color:#171717;cursor:pointer}
.btn:hover{background:#fff}.btn.primary{border-color:var(--accent);background:var(--accent);color:#fff}.btn.primary:hover{background:#005a9e}.btn:disabled{border-color:#c8c8c8;background:#c8c8c8;color:#fff;cursor:default}
.password-label{display:block;margin:0 0 6px;font-size:.92rem}.password-field{position:relative;border-radius:4px;overflow:hidden}.password-input{display:block;width:100%;height:41px;padding:8px 46px 8px 10px;border:1px solid #8b8b8b;border-radius:4px;background:#fff;color:#171717}.password-input:focus{border-color:var(--accent);border-bottom-width:3px;outline:0}.password-reveal{position:absolute;top:4px;right:4px;width:34px;height:33px;display:grid;place-items:center;padding:0;border:0;border-radius:6px;background:transparent;color:#555;cursor:pointer;touch-action:none}.password-reveal:hover,.password-reveal.active{background:#f5f5f5;color:var(--accent)}.password-reveal:focus-visible{outline:2px solid var(--accent);outline-offset:2px}.password-reveal svg{width:20px;height:20px;pointer-events:none}
.password-panel .actions{margin-top:14px}.network.busy .network-details{padding-top:3px}.connecting{display:flex;align-items:center;gap:10px;min-height:40px;color:#444}.spinner{width:19px;height:19px;border:2px solid #b9b9b9;border-top-color:var(--accent);border-radius:50%;animation:spin .8s linear infinite}
.empty{margin:8px 16px;padding:28px 18px;text-align:center;color:#5b5b5b;border:1px solid var(--line);border-radius:6px;background:#fafafa}.empty strong{display:block;margin-bottom:5px;color:#202020}.empty .btn{display:block;margin:16px auto 0}
.advanced-view{padding:24px}.advanced-view[hidden],#scanView[hidden]{display:none}.advanced-view h2{margin:18px 0 5px;font-size:1.35rem;font-weight:600}.advanced-copy{margin:0 0 24px;color:#5b5b5b;line-height:1.45}.back-link{display:inline-block;color:var(--accent);text-decoration:underline;text-underline-offset:3px}.field-label{display:block;margin:0 0 18px;font-size:.92rem}.field-label>span{display:block;margin-bottom:6px}.text-input{display:block;width:100%;height:41px;padding:8px 10px;border:1px solid #8b8b8b;border-radius:4px;background:#fff;color:#171717}.text-input:focus{border-color:var(--accent);border-bottom-width:3px;outline:0}.field-help{display:block;margin-top:6px;color:#5b5b5b;font-size:.82rem}.manual-error{min-height:1.4em;margin:4px 0 12px;color:#c42b1c;font-size:.88rem}.manual-form .actions{margin-top:4px}
.sr-only{position:absolute;width:1px;height:1px;padding:0;margin:-1px;overflow:hidden;clip:rect(0,0,0,0);white-space:nowrap;border:0}
@keyframes spin{to{transform:rotate(360deg)}}
@keyframes scan-progress{from{transform:translate3d(-130%,0,0)}to{transform:translate3d(380%,0,0)}}
@media(min-width:768px){h1{font-size:1.65rem}.brand{font-size:.92rem}.ssid{font-size:1.03rem}}
@media(max-width:480px){body{display:block;padding:0;background:var(--panel)}.shell{width:100%;min-height:100vh;border-radius:0;box-shadow:none}.top{padding:18px 16px 14px}.scan-progress{right:16px;left:16px}.scan-status{min-height:36px;padding:6px 16px 10px}.network-list{padding:0 4px 16px}.network-main{min-height:70px;padding:12px 13px;grid-template-columns:34px minmax(0,1fr);gap:8px}.signal{width:28px;height:28px}.network-details{padding-left:55px}.actions{display:grid;grid-template-columns:1fr 1fr}.choose-panel .actions{display:flex}.btn{min-width:0;width:100%}.advanced-view{padding:20px 16px}}
@media(prefers-reduced-motion:reduce){.spinner{animation:none}.scan-progress.active:before{animation:none;transform:translate3d(178%,0,0)}}
</style>
</head>
<body>
<main class="shell">
<div id="scanView">
<header class="top">
<div><h1>Wi-Fi Portal</h1><a class="advanced-link" id="advancedLink" href="#advanced">Manual Configure Wi-Fi</a></div>
<button class="icon-btn" id="refresh" type="button" aria-label="Scan for Wi-Fi networks" title="Refresh networks">
<svg viewBox="0 0 24 24" aria-hidden="true"><path fill="currentColor" d="M17.66 6.34A7.96 7.96 0 0 0 12 4a8 8 0 1 0 7.75 10h-2.08A6 6 0 1 1 12 6c1.66 0 3.16.68 4.24 1.76L13 11h8V3l-3.34 3.34Z"/></svg>
</button>
<div class="scan-progress" id="scanProgress" role="progressbar" aria-label="Scanning for Wi-Fi networks" aria-hidden="true"></div>
</header>
<div class="scan-status" id="scanStatus" role="status" aria-live="polite">Scanning for networks...</div>
<section class="network-list" id="networkList" aria-label="Available Wi-Fi networks"></section>
</div>
<section class="advanced-view" id="advancedView" aria-labelledby="advancedTitle" hidden>
<a class="back-link" id="advancedBack" href="#scan">Back Home Screen</a>
<h2 id="advancedTitle">Manual Configure Wi-Fi</h2>
<form class="manual-form" id="manualForm" novalidate>
<label class="field-label" for="manualSSID"><span>Network name</span><input class="text-input" id="manualSSID" name="manual-ssid" maxlength="32" autocomplete="off" autocapitalize="none" spellcheck="false" required></label>
<label class="field-label" for="manualPassword"><span>Password</span><span class="password-field"><input class="password-input" id="manualPassword" name="manual-password" type="password" maxlength="63" autocomplete="current-password" placeholder="Leave empty for an open network"><button class="password-reveal" id="manualReveal" type="button" aria-label="Show password" aria-pressed="false" title="Show password"><svg viewBox="0 0 24 24" aria-hidden="true"><path d="M2.5 12s3.5-6 9.5-6 9.5 6 9.5 6-3.5 6-9.5 6S2.5 12 2.5 12Z" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linejoin="round"/><circle cx="12" cy="12" r="2.6" fill="none" stroke="currentColor" stroke-width="1.7"/></svg></button></span></label>
<p class="manual-error" id="manualError" role="alert" aria-live="polite"></p>
<div class="actions"><button class="btn primary" id="manualConnect" type="submit">Connect</button></div>
</form>
</section>
<form id="wifiForm" method="post" action="/save" hidden>
<input id="formSSID" name="ssid">
<input id="formPassword" name="password" type="password">
</form>
</main>
)EWPHTML"
R"EWPHTML(<script>
const list=document.getElementById('networkList'),scanStatus=document.getElementById('scanStatus'),scanProgress=document.getElementById('scanProgress'),refresh=document.getElementById('refresh'),form=document.getElementById('wifiForm'),formSSID=document.getElementById('formSSID'),formPassword=document.getElementById('formPassword'),scanView=document.getElementById('scanView'),advancedView=document.getElementById('advancedView'),manualForm=document.getElementById('manualForm'),manualSSID=document.getElementById('manualSSID'),manualPassword=document.getElementById('manualPassword'),manualReveal=document.getElementById('manualReveal'),manualError=document.getElementById('manualError');
const NS='http://www.w3.org/2000/svg';
let selected=null,scanning=false,submitting=false,passwordFieldSequence=0;
function node(tag,cls,text){const e=document.createElement(tag);if(cls)e.className=cls;if(text!==undefined)e.textContent=text;return e}
function svgNode(tag,attrs){const e=document.createElementNS(NS,tag);Object.keys(attrs).forEach(k=>e.setAttribute(k,attrs[k]));return e}
function utf8Length(value){if(window.TextEncoder)return new TextEncoder().encode(value).length;return encodeURIComponent(value).replace(/%[0-9A-F]{2}|./g,'x').length}
function credentialError(ssid,password){const s=utf8Length(ssid),p=utf8Length(password);if(s<1||s>32)return'SSID must contain 1-32 bytes.';if(p>63||(p>0&&p<8))return'Password must be empty or contain 8-63 bytes.';return''}
function signalLevel(rssi){return rssi>=-55?4:rssi>=-67?3:rssi>=-75?2:1}
function signalIcon(rssi,open){
  const s=svgNode('svg',{viewBox:'0 0 24 24','aria-hidden':'true',class:'signal l'+signalLevel(rssi)});
  [['outer','M2.7 8.4a14.2 14.2 0 0 1 18.6 0'],['middle','M6.2 12a9.1 9.1 0 0 1 11.6 0'],['inner','M9.4 15.6a4.1 4.1 0 0 1 5.2 0']].forEach(p=>s.appendChild(svgNode('path',{class:'wave '+p[0],d:p[1]})));
  s.appendChild(svgNode('circle',{class:'dot',cx:'12',cy:'19',r:'1.25'}));
  if(!open){s.appendChild(svgNode('rect',{class:'lock',x:'15.2',y:'15.4',width:'7.4',height:'6.4',rx:'1'}));s.appendChild(svgNode('path',{class:'lock-loop',d:'M16.8 15.4v-1.1a2.1 2.1 0 0 1 4.2 0v1.1'}))}
  return s;
}
function collapseCurrent(){
  if(!selected)return;
  setPasswordVisible(selected,false);
  selected.item.classList.remove('selected');selected.main.setAttribute('aria-expanded','false');
  selected.input.value='';selected=null;
}
function selectNetwork(entry){
  if(submitting||selected&&selected.item===entry.item)return;
  collapseCurrent();selected=entry;entry.item.classList.add('selected');entry.main.setAttribute('aria-expanded','true');entry.choose.hidden=false;entry.passwordPanel.hidden=true;entry.input.value='';entry.next.disabled=true;
  setTimeout(()=>entry.item.scrollIntoView({block:'nearest'}),0);
}
function showPassword(entry){entry.choose.hidden=true;entry.passwordPanel.hidden=false;entry.input.focus()}
function setPasswordVisible(entry,visible){entry.input.type=visible?'text':'password';entry.reveal.classList.toggle('active',visible);entry.reveal.setAttribute('aria-pressed',visible?'true':'false')}
function cancelPassword(entry){setPasswordVisible(entry,false);entry.input.value='';entry.next.disabled=true;entry.passwordPanel.hidden=true;entry.choose.hidden=false;entry.main.focus()}
function submitNetwork(entry,password){
  if(submitting)return;
  if(credentialError(entry.network.ssid,password))return;
  submitting=true;formSSID.value=entry.network.ssid;formPassword.value=password;
  try{sessionStorage.setItem('ewpSSID',entry.network.ssid)}catch(e){}
  document.querySelectorAll('button').forEach(b=>b.disabled=true);entry.input.disabled=true;entry.item.classList.add('busy');
  entry.choose.hidden=true;entry.passwordPanel.hidden=true;entry.details.replaceChildren();
  const state=node('div','connecting');state.setAttribute('role','status');state.append(node('span','spinner'),node('span','',`Connecting to ${entry.network.ssid}...`));entry.details.appendChild(state);
  setTimeout(()=>form.submit(),80);
}
function makeNetwork(network){
  const item=node('article','network'),main=node('button','network-main'),copy=node('span','network-copy'),ssid=node('span','ssid',network.ssid),subtitle=node('span','subtitle',network.open?'Open network':'Secured'),details=node('div','network-details'),choose=node('div','choose-panel'),chooseActions=node('div','actions'),connect=node('button','btn primary','Connect'),passwordPanel=node('div','password-panel'),label=node('label','password-label','Enter the password'),passwordField=node('div','password-field'),input=node('input','password-input'),reveal=node('button','password-reveal'),passwordActions=node('div','actions'),next=node('button','btn primary','Next'),cancel=node('button','btn','Cancel');
  main.type='button';main.setAttribute('aria-expanded','false');main.title=network.ssid;copy.append(ssid,subtitle);main.append(signalIcon(network.rssi,network.open),copy);item.append(main,details);
  connect.type='button';chooseActions.appendChild(connect);choose.appendChild(chooseActions);details.appendChild(choose);
  passwordPanel.hidden=true;input.type='password';input.id='ewp-password-'+(++passwordFieldSequence);input.maxLength=63;input.autocomplete='current-password';input.placeholder='Password';label.htmlFor=input.id;reveal.type='button';reveal.title='Hold to show password';reveal.setAttribute('aria-label','Hold to show password');reveal.setAttribute('aria-pressed','false');const eye=svgNode('svg',{viewBox:'0 0 24 24','aria-hidden':'true'});eye.appendChild(svgNode('path',{d:'M2.5 12s3.5-6 9.5-6 9.5 6 9.5 6-3.5 6-9.5 6S2.5 12 2.5 12Z',fill:'none',stroke:'currentColor','stroke-width':'1.7','stroke-linejoin':'round'}));eye.appendChild(svgNode('circle',{cx:'12',cy:'12',r:'2.6',fill:'none',stroke:'currentColor','stroke-width':'1.7'}));reveal.appendChild(eye);passwordField.append(input,reveal);next.type='button';next.disabled=true;cancel.type='button';passwordActions.append(next,cancel);passwordPanel.append(label,passwordField,passwordActions);details.appendChild(passwordPanel);
  const entry={network,item,main,details,choose,passwordPanel,input,reveal,next};
  main.onclick=()=>selectNetwork(entry);connect.onclick=()=>network.open?submitNetwork(entry,''):showPassword(entry);cancel.onclick=()=>cancelPassword(entry);next.onclick=()=>{if(!credentialError(network.ssid,input.value))submitNetwork(entry,input.value)};
  reveal.onpointerdown=event=>{if(event.button!==0)return;event.preventDefault();setPasswordVisible(entry,true);try{reveal.setPointerCapture(event.pointerId)}catch(error){}};
  reveal.onpointerup=()=>setPasswordVisible(entry,false);reveal.onpointercancel=()=>setPasswordVisible(entry,false);reveal.onlostpointercapture=()=>setPasswordVisible(entry,false);reveal.onblur=()=>setPasswordVisible(entry,false);
  reveal.onkeydown=event=>{if((event.key===' '||event.key==='Enter')&&!event.repeat){event.preventDefault();setPasswordVisible(entry,true)}};reveal.onkeyup=event=>{if(event.key===' '||event.key==='Enter'){event.preventDefault();setPasswordVisible(entry,false)}};
  input.oninput=()=>next.disabled=!!credentialError(network.ssid,input.value);input.onkeydown=e=>{if(e.key==='Escape'){e.preventDefault();cancelPassword(entry)}else if(e.key==='Enter'&&!next.disabled){e.preventDefault();submitNetwork(entry,input.value)}};
  return item;
}
function showEmpty(title,message,retry){
  const box=node('div','empty'),strong=node('strong','',title);box.append(strong,document.createTextNode(message));
  if(retry){const button=node('button','btn','Try again');button.type='button';button.onclick=scan;box.appendChild(button)}
  list.replaceChildren(box);
}
async function scan(){
  if(scanning||submitting)return;
  scanning=true;collapseCurrent();refresh.disabled=true;scanProgress.classList.add('active');scanProgress.setAttribute('aria-hidden','false');scanStatus.textContent='Scanning for networks...';list.replaceChildren();
  try{
    let response;
    do{response=await fetch('/scan',{cache:'no-store'});if(response.status===202){await response.text();await new Promise(resolve=>setTimeout(resolve,400))}}while(response.status===202);
    if(!response.ok)throw new Error();
    const data=await response.json();if(!data||!Array.isArray(data.networks))throw new Error();
    const networks=data.networks.filter(n=>n&&typeof n.ssid==='string'&&n.ssid.length>0).map(n=>({ssid:n.ssid,rssi:Number.isFinite(Number(n.rssi))?Number(n.rssi):-100,open:n.open===true}));
    if(!networks.length){scanStatus.textContent='No networks found';showEmpty('No Wi-Fi networks found','Move closer to the router, then scan again.',true);return}
    const fragment=document.createDocumentFragment();networks.forEach(n=>fragment.appendChild(makeNetwork(n)));list.replaceChildren(fragment);scanStatus.textContent=networks.length+' network'+(networks.length===1?'':'s')+' found';
  }catch(e){scanStatus.textContent='Scan failed';showEmpty('Unable to scan','Check the ESP32 setup connection and try again.',true)}
  finally{scanning=false;refresh.disabled=false;scanProgress.classList.remove('active');scanProgress.setAttribute('aria-hidden','true')}
}
refresh.onclick=scan;
function showView(){const advanced=location.hash==='#advanced';scanView.hidden=advanced;advancedView.hidden=!advanced;if(advanced)setTimeout(()=>manualSSID.focus(),0)}
manualReveal.onclick=()=>{const visible=manualPassword.type==='text';manualPassword.type=visible?'password':'text';manualReveal.classList.toggle('active',!visible);manualReveal.setAttribute('aria-pressed',visible?'false':'true');manualReveal.setAttribute('aria-label',visible?'Show password':'Hide password');manualReveal.title=visible?'Show password':'Hide password'};
manualSSID.oninput=manualPassword.oninput=()=>manualError.textContent='';
manualForm.onsubmit=event=>{event.preventDefault();if(submitting)return;const error=credentialError(manualSSID.value,manualPassword.value);if(error){manualError.textContent=error;(utf8Length(manualSSID.value)<1||utf8Length(manualSSID.value)>32?manualSSID:manualPassword).focus();return}submitting=true;formSSID.value=manualSSID.value;formPassword.value=manualPassword.value;try{sessionStorage.setItem('ewpSSID',manualSSID.value)}catch(e){}document.querySelectorAll('button').forEach(control=>control.disabled=true);manualSSID.disabled=true;manualPassword.disabled=true;manualError.textContent='Connecting...';setTimeout(()=>form.submit(),80)};
window.addEventListener('hashchange',showView);showView();
scan();
</script>
</body>
</html>
)EWPHTML";

static const char EWP_CONNECTING_HTML[] PROGMEM = R"EWPHTML(
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<meta name="theme-color" content="#f3f3f3">
<title>Connecting</title>
<style>
:root{font-family:"Segoe UI",system-ui,-apple-system,sans-serif;color:#1b1b1b;color-scheme:light;--accent:#0067c0}
*{box-sizing:border-box}body{margin:0;min-height:100vh;display:grid;place-items:center;background:linear-gradient(135deg,#eef4f8,#f8f8f8 48%,#eef3f5)}
.panel{width:min(92vw,430px);padding:42px 34px;background:#f3f3f3f7;box-shadow:0 18px 55px #0002;text-align:center}.wifi{width:62px;height:62px;color:#202020}
h1{margin:18px 0 5px;font-size:1.55rem;font-weight:600}.network{margin:0;min-height:1.45em;color:#4d4d4d;overflow-wrap:anywhere}.message{margin:22px auto 0;max-width:320px;line-height:1.5;color:#4d4d4d}.ip{margin:8px 0 0;color:#5b5b5b;font-size:.9rem}
.spinner{width:24px;height:24px;margin:24px auto 0;border:2px solid #b9b9b9;border-top-color:var(--accent);border-radius:50%;animation:spin .8s linear infinite}.success{color:#0f7b0f}.error{color:#c42b1c}
.back{display:none;margin:26px auto 0;min-width:180px;padding:10px 20px;border:1px solid var(--accent);border-radius:4px;background:var(--accent);color:#fff;text-decoration:none}.back:hover{background:#005a9e}
@keyframes spin{to{transform:rotate(360deg)}}@media(max-width:480px){body{background:#f3f3f3}.panel{width:100%;min-height:100vh;padding-top:20vh;box-shadow:none}}@media(prefers-reduced-motion:reduce){.spinner{animation:none}}
</style>
</head>
<body>
<main class="panel" aria-live="polite">
<svg class="wifi" viewBox="0 0 24 24" aria-hidden="true"><path d="M2.7 8.4a14.2 14.2 0 0 1 18.6 0M6.2 12a9.1 9.1 0 0 1 11.6 0M9.4 15.6a4.1 4.1 0 0 1 5.2 0" fill="none" stroke="currentColor" stroke-width="1.5" stroke-linecap="round"/><circle cx="12" cy="19" r="1.25" fill="currentColor"/></svg>
<h1 id="title">Connecting...</h1>
<p class="network" id="network"></p>
<div class="spinner" id="spinner" role="status"></div>
<p class="message" id="message">The ESP32 is testing the selected Wi-Fi network.</p>
<p class="ip" id="ip"></p>
<a class="back" id="retry" href="/wifi">Back to Wi-Fi networks</a>
</main>
<script>
const title=document.getElementById('title'),network=document.getElementById('network'),spinner=document.getElementById('spinner'),message=document.getElementById('message'),ip=document.getElementById('ip'),retry=document.getElementById('retry');
let finished=false,failures=0,ssid='';try{ssid=sessionStorage.getItem('ewpSSID')||''}catch(e){}if(ssid)network.textContent=ssid;
function stop(){finished=true;spinner.style.display='none'}
function showError(text){stop();title.textContent="Couldn't connect";title.className='error';message.textContent=text||'Unable to connect. Check the network password and try again.';retry.style.display='inline-block'}
async function check(){
  if(finished)return;
  try{
    const response=await fetch('/status',{cache:'no-store'});if(!response.ok)throw new Error();const status=await response.json();failures=0;
    if(status.connected){stop();title.textContent='Connected';title.className='success';message.textContent='Connected successfully. You can close this page.';if(status.ip)ip.textContent='IP address: '+status.ip;try{sessionStorage.removeItem('ewpSSID')}catch(e){}return}
    if(status.error){showError(status.error);return}
    setTimeout(check,1000);
  }catch(e){
    if(++failures<2){setTimeout(check,1000);return}
    stop();title.textContent='Connection finishing';message.textContent='The ESP32 setup network is no longer reachable. If it joined the selected Wi-Fi, you can close this page.';retry.style.display='inline-block';
  }
}
check();
</script>
</body>
</html>
)EWPHTML";
