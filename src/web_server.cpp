#include "web_server.h"
#include "app_config.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>
#include "LGFX_ESP32_8048S070.h"
#include "ota_updater.h"
#include "version.h"

extern LGFX tft;
extern String dolarValue;
extern String weatherTemp;
extern String weatherDesc;
extern String weatherCity;
extern volatile bool gNeedsRebuild;

WebServer webServer(80);
DNSServer dnsServer;

static const char HTML_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Painel Financeiro</title>
<style>
:root{--bg:#070A12;--card:#12121A;--card2:#0F1622;--accent:#22D3EE;--yellow:#FFB300;--green:#00E676;--red:#FF5252;--text:#F8FAFC;--muted:#7A8699;--border:#1E2A3A}
[data-theme="light"]{--bg:#EEF2F7;--card:#FFFFFF;--card2:#F1F5F9;--text:#0F172A;--muted:#64748B;--border:#E2E8F0}
[data-theme="light"] body{background:radial-gradient(1200px 600px at 20% -10%, #dbeafe 0%, transparent 50%), linear-gradient(180deg,#F8FAFC,#EEF2F7)}
*{box-sizing:border-box;font-family:Inter,system-ui,-apple-system,sans-serif}
body{margin:0;background:radial-gradient(1200px 600px at 20% -10%, #1a2a44 0%, transparent 50%), linear-gradient(180deg,#070A12,#0E1420);color:var(--text);padding:16px;min-height:100vh}
[data-theme="light"] .card{box-shadow:0 8px 20px rgba(0,0,0,.08)}
[data-theme="light"] .top h1{color:#0F172A}
.top{max-width:1200px;margin:0 auto;display:flex;align-items:center;gap:12px;flex-wrap:wrap}
.top h1{font-size:22px;margin:0;letter-spacing:1px}
.top small{color:var(--muted);font-size:12px}
.badge{padding:6px 10px;border-radius:20px;font-size:11px;font-weight:800;border:1px solid}
.badge.ok{background:rgba(0,230,118,.12);color:var(--green);border-color:var(--green)}
.badge.off{background:rgba(255,82,82,.12);color:var(--red);border-color:var(--red)}
.tabs{max-width:1200px;margin:16px auto 0;display:flex;gap:8px;overflow:auto;padding-bottom:4px}
.tab{padding:10px 14px;border-radius:999px;border:1px solid var(--border);background:var(--card);color:var(--muted);cursor:pointer;white-space:nowrap;font-weight:700;font-size:13px}
.tab.active{background:var(--accent);color:#000;border-color:var(--accent)}
.grid{max-width:1200px;margin:14px auto;display:grid;grid-template-columns:repeat(auto-fit,minmax(360px,1fr));gap:14px}
.card{background:var(--card);border:1px solid var(--border);border-radius:16px;padding:16px;position:relative;overflow:hidden;box-shadow:0 8px 30px rgba(0,0,0,.35)}
.card::before{content:"";position:absolute;top:0;left:0;right:0;height:4px}
.card.accent::before{background:var(--accent)} .card.green::before{background:var(--green)} .card.yellow::before{background:var(--yellow)}
.card h2{font-size:12px;letter-spacing:2px;margin:0 0 12px;color:var(--accent)} .card.green h2{color:var(--green)} .card.yellow h2{color:var(--yellow)}
label{font-size:11px;color:var(--muted);display:block;margin:10px 0 5px;letter-spacing:.3px}
input,select{width:100%;padding:11px 12px;border-radius:12px;border:1px solid var(--border);background:var(--card2);color:var(--text);font-size:14px;outline:none}
input:focus,select:focus{border-color:var(--accent);box-shadow:0 0 0 3px rgba(34,211,238,.15)}
.row{display:flex;gap:10px} .row>div{flex:1}
.switch{position:relative;width:44px;height:26px;background:#1E2A3A;border-radius:999px;cursor:pointer;transition:.2s}
.switch.on{background:var(--green)}
.knob{position:absolute;top:3px;left:3px;width:20px;height:20px;background:#fff;border-radius:50%;transition:.2s}
.switch.on .knob{left:21px}
.line{display:flex;align-items:center;gap:10px;padding:10px;border:1px solid var(--border);border-radius:12px;background:var(--card2);margin:6px 0}
.preview{font-size:12px;color:var(--muted);margin-top:4px;min-height:16px}
.btn{border:0;padding:12px 14px;border-radius:12px;font-weight:800;cursor:pointer;width:100%;margin-top:10px;font-size:13px}
.btn-accent{background:var(--accent);color:#000} .btn-green{background:var(--green);color:#000} .btn-dark{background:#1E2A3A;color:var(--text)}
.kv{display:flex;justify-content:space-between;padding:7px 0;border-bottom:1px dashed #1E2A3A;font-size:13px}
.chips{display:flex;flex-wrap:wrap;gap:6px;margin-top:6px}
.chip{padding:6px 10px;border-radius:999px;background:#0F1622;border:1px solid var(--border);font-size:12px;cursor:pointer;color:var(--muted)}
.chip:hover{border-color:var(--accent);color:var(--text)}
.toast{position:fixed;bottom:16px;left:50%;transform:translateX(-50%);background:#0F1622;border:1px solid var(--border);padding:10px 14px;border-radius:12px;box-shadow:0 10px 30px rgba(0,0,0,.5);display:none;z-index:99}
.suggest{position:absolute;top:100%;left:0;right:0;background:#0F1622;border:1px solid var(--border);border-radius:12px;max-height:160px;overflow:auto;z-index:10;display:none}
.suggest div{padding:8px 10px;cursor:pointer;font-size:13px;border-bottom:1px solid #1E2A3A}
.suggest div:hover{background:#1E2A3A}
.hidden{display:none!important}
.wifi-item{display:flex;align-items:center;gap:10px;padding:10px;border:1px solid var(--border);border-radius:12px;background:var(--card2);margin:6px 0;cursor:pointer}
.wifi-item:hover{border-color:var(--accent)}
.wifi-item b{flex:1}
.rssi{font-size:11px;color:var(--muted)}
#globalMirror{transition:.3s;overflow:hidden;height:280px}
#mirrorScreen{transform:scale(0.55);margin:0}
@media(max-width:900px){#mirrorScreen{transform:scale(0.40)} #globalMirror{height:200px}}
@media(max-width:600px){#mirrorScreen{transform:scale(0.28)} #globalMirror{height:145px}}
@media(max-width:400px){#mirrorScreen{transform:scale(0.22)} #globalMirror{height:115px}}
</style>
</head>
<body>
<div class="top">
 <h1>PAINEL FINANCEIRO</h1>
 <small id="sub">Horário e câmbio em tempo real</small>
 <button onclick="toggleTheme()" id="themeBtn" title="Alternar claro/escuro" style="margin-left:auto;background:var(--card);border:1px solid var(--border);color:var(--text);padding:6px 10px;border-radius:999px;cursor:pointer;font-size:12px">🌙 Escuro</button>
 <button id="globalMirrorBtn" onclick="toggleMirror()" style="background:var(--card);border:1px solid var(--border);color:var(--text);padding:6px 10px;border-radius:999px;cursor:pointer;font-size:12px;font-weight:700">👁️ Tela</button>
 <span id="ipBadge" class="badge ok">IP: --</span>
 <span id="wifiBadge" class="badge ok">WiFi</span>
</div>

<!-- ESPELHAMENTO GLOBAL - Dashboard proporcional -->
<div id="globalMirror" style="max-width:1200px;margin:14px auto;background:transparent;padding:0;display:none;justify-content:center;overflow:visible">
 <div id="mirrorScreen" style="width:800px;min-width:800px;height:480px;background:#0A0F1D;border:2px solid #1E293B;border-radius:14px;overflow:hidden;transform-origin:top center;position:relative;transform:scale(0.55);box-shadow:0 8px 30px rgba(0,0,0,.5);padding:18px;display:grid;grid-template-columns:360px 380px;gap:20px">
  <!-- Painel Esquerdo -->
  <div style="background:#0F172A;border:1px solid #1E293B;border-radius:16px;padding:16px;display:flex;flex-direction:column;align-items:center;position:relative">
   <div id="mDate" style="font-size:14px;letter-spacing:2px;color:#94A3B8;font-weight:700">TER, 24 OUT</div>
   <div id="mTime" style="font-size:44px;font-weight:900;color:#FFFFFF;margin:8px 0">14:38</div>
   <div style="width:280px;height:1px;background:#1E293B;margin:10px 0"></div>
   <div id="mCity" style="font-size:18px;font-weight:800;color:#E2E8F0;letter-spacing:1px;text-transform:uppercase;margin-bottom:12px">LAVRAS, MG</div>
   <div style="display:flex;align-items:center;gap:14px;width:100%;padding-left:14px">
    <div style="font-size:36px">⛅</div>
    <div>
     <div id="mTemp" style="font-size:38px;font-weight:900;color:#FFFFFF">27°C</div>
     <div id="mDesc" style="font-size:14px;color:#CBD5E1;font-weight:600">Parcialmente Nublado</div>
     <div id="mHum" style="font-size:12px;color:#94A3B8;margin-top:2px">Humidity: 64%</div>
     <div id="mWind" style="font-size:12px;color:#94A3B8">Wind: 14 km/h</div>
    </div>
   </div>
   <div style="margin-top:auto;display:flex;align-items:center;gap:6px;width:100%;padding-left:8px;font-size:11px;color:#94A3B8">
    <div style="width:8px;height:8px;border-radius:50%;background:#00E676"></div>
    <span id="mIp">IP: --</span>
   </div>
  </div>
  <!-- Painel Direito -->
  <div style="display:flex;flex-direction:column;gap:12px">
   <div style="text-align:center;font-size:18px;font-weight:800;letter-spacing:2px;color:#F6C343;margin-bottom:2px">COTAÇÃO DE MOEDAS</div>
   <div id="mCurCards" style="display:flex;flex-direction:column;gap:12px"></div>
  </div>
 </div>
</div>

<div class="tabs">
 <div class="tab active" onclick="tab('moedas')">💵 Moedas</div>
 <div class="tab" onclick="tab('clima')">⛅ Clima</div>
 <div class="tab" onclick="tab('wifi')">📶 Wi-Fi</div>
 <div class="tab" onclick="tab('display')">🖥️ Tela</div>
 <div class="tab" onclick="tab('sistema')">⚙️ Sistema</div>
 <div class="tab" onclick="tab('ota')">🚀 OTA / Update</div>
</div>

<div class="grid">
 <!-- MOEDAS -->
 <div class="card accent" id="tab-moedas">
  <h2>COTAÇÃO DE MOEDAS</h2>
  <div style="font-size:12px;color:var(--muted);margin-bottom:12px">Selecione até 6 pares de moedas para exibir na tela:</div>
  <div id="moedasContainer"></div>
  <label>Sugestões rápidas:</label>
  <div class="chips" id="chips"></div>
  <button class="btn btn-accent" onclick="save()">Salvar Moedas</button>
 </div>

 <!-- CLIMA -->
 <div class="card yellow" id="tab-clima" style="display:none">
  <h2>CONFIGURAÇÃO DO CLIMA</h2>
  <label>Estado (UF)</label>
  <select id="uf"></select>
  <label>Buscar Cidade</label>
  <div style="position:relative">
   <input type="text" id="cityInput" placeholder="Digite para filtrar..." autocomplete="off">
   <div id="citySuggest" class="suggest"></div>
  </div>
  <label>Lista de Cidades</label>
  <select id="citySel" size="5" style="height:120px"></select>
  <div class="row">
   <div><label>Cidade no Display</label><input type="text" id="city"></div>
   <div><label>Fuso Horário (UTC)</label><input type="number" id="tz" value="-3"></div>
  </div>
  <div class="row">
   <div><label>Latitude</label><input type="number" step="0.0001" id="lat"></div>
   <div><label>Longitude</label><input type="number" step="0.0001" id="lon"></div>
  </div>
  <div class="preview" id="pdesc"></div>
  <button class="btn btn-green" onclick="save()">Salvar Clima</button>
 </div>

 <!-- WIFI -->
 <div class="card green" id="tab-wifi" style="display:none">
  <h2>CONEXÃO WI-FI</h2>
  <div style="display:flex;justify-content:space-between;align-items:center">
   <label style="margin:0">Redes Encontradas</label>
   <button onclick="scanWifi()" class="btn btn-dark" style="width:auto;margin:0;padding:6px 12px;font-size:12px">🔍 Buscar Redes</button>
  </div>
  <div id="wifiList" style="max-height:160px;overflow:auto;margin:8px 0"></div>
  <label>Nome da Rede (SSID)</label>
  <input type="text" id="ssid">
  <label>Senha da Rede</label>
  <input type="password" id="pass" placeholder="Deixe em branco se não mudar">
  <button class="btn btn-green" onclick="save()">Conectar ao Wi-Fi</button>
 </div>

 <!-- TELA / DISPLAY -->
 <div class="card" id="tab-display" style="display:none">
  <h2>CONTROLE DO DISPLAY</h2>
  <label>Brilho da Tela (<span id="bv">180</span>/255)</label>
  <input type="range" min="10" max="255" id="bright" style="width:100%">
  <div style="background:var(--card2);height:8px;border-radius:4px;overflow:hidden;margin:6px 0">
   <div id="brightBar" style="height:100%;background:var(--accent);width:70%"></div>
  </div>
  <div class="row" style="margin-top:10px">
   <div><button class="btn btn-dark" onclick="setTheme('dark')">🌙 Modo Noturno</button></div>
   <div><button class="btn btn-dark" onclick="setTheme('light')">☀️ Modo Claro</button></div>
  </div>
  <button class="btn btn-accent" onclick="testBlink()">✨ Piscar Display (Teste)</button>
 </div>

 <!-- SISTEMA -->
 <div class="card" id="tab-sistema" style="display:none">
  <h2>STATUS DO SISTEMA</h2>
  <div id="live"></div>
  <div style="margin-top:14px">
   <label>Intervalo de Atualização das Moedas (segundos)</label>
   <input type="number" id="dint" value="60">
   <label>Intervalo de Atualização do Clima (segundos)</label>
   <input type="number" id="wint" value="600">
  </div>
  <div class="row">
   <div><button class="btn btn-dark" onclick="save()">Salvar Intervalos</button></div>
   <div><button class="btn btn-dark" style="color:var(--red)" onclick="restart()">🔄 Reiniciar ESP</button></div>
  </div>
 </div>

 <!-- OTA -->
 <div class="card" id="tab-ota" style="display:none">
  <h2>ATUALIZAÇÃO DE FIRMWARE (OTA)</h2>
  <div class="kv"><span>Versão Atual</span><b id="otaCur">--</b></div>
  <div class="kv"><span>Versão no GitHub</span><b id="otaLatest">--</b></div>
  <div class="kv"><span>Status</span><b id="otaStatus">--</b></div>
  <div class="kv"><span>Mensagem</span><span id="otaMsg" style="color:var(--muted)">--</span></div>
  <div style="background:var(--card2);height:10px;border-radius:6px;overflow:hidden;margin:10px 0">
   <div id="otaBar" style="height:100%;background:var(--green);width:0%;transition:.3s"></div>
  </div>
  <div class="row">
   <div><button class="btn btn-accent" onclick="otaCheck()">🔍 Verificar Atualizações</button></div>
   <div><button class="btn btn-green" onclick="otaUpdate()">⬇️ Atualizar Agora</button></div>
  </div>
 </div>
</div>

<div class="toast" id="toast"></div>

<script>
let state={};
const POPULARES=['USD-BRL','EUR-BRL','BTC-BRL','ETH-BRL','GBP-BRL','JPY-BRL','CAD-BRL','CHF-BRL','ARS-BRL','USDT-BRL','SOL-BRL'];
const CAPITAIS=[
 {uf:'SP',c:'São Paulo',lat:-23.5505,lon:-46.6333},
 {uf:'MG',c:'Belo Horizonte',lat:-19.9167,lon:-43.9345},
 {uf:'MG',c:'Lavras',lat:-21.2461,lon:-44.9992},
 {uf:'RJ',c:'Rio de Janeiro',lat:-22.9068,lon:-43.1729},
 {uf:'PR',c:'Curitiba',lat:-25.4284,lon:-49.2733},
 {uf:'RS',c:'Porto Alegre',lat:-30.0346,lon:-51.2177},
 {uf:'DF',c:'Brasília',lat:-15.7975,lon:-47.8919},
 {uf:'BA',c:'Salvador',lat:-12.9714,lon:-38.5014},
 {uf:'SC',c:'Florianópolis',lat:-27.5954,lon:-48.5480},
 {uf:'PE',c:'Recife',lat:-8.0476,lon:-34.8770},
 {uf:'CE',c:'Fortaleza',lat:-3.7172,lon:-38.5433},
 {uf:'GO',c:'Goiânia',lat:-16.6869,lon:-49.2648}
];

function tab(name){
 ['moedas','clima','wifi','display','sistema','ota'].forEach(t=>{
  document.getElementById('tab-'+t).style.display = t===name?'block':'none';
 });
 document.querySelectorAll('.tab').forEach((el,i)=>{
  el.classList.toggle('active', ['moedas','clima','wifi','display','sistema','ota'][i]===name);
 });
}

function toast(msg,ok=true){
 let t=document.getElementById('toast');
 t.textContent=msg; t.style.borderColor=ok?'var(--green)':'var(--red)';
 t.style.display='block';
 setTimeout(()=>t.style.display='none',3000);
}
function log(m){console.log(m)}

function fillPairs(){
 let c=document.getElementById('moedasContainer');
 let h='';
 for(let i=1;i<=6;i++){
  h+=`<div class="line">
   <span style="font-weight:800;font-size:12px;color:var(--accent);width:20px">${i}</span>
   <div class="switch on" id="sw${i}" onclick="toggleSw(${i})"><div class="knob"></div></div>
   <span style="font-size:11px;color:var(--muted);width:60px">Ativada</span>
   <input type="text" id="c${i}" placeholder="Ex: USD-BRL" style="flex:1;text-transform:uppercase" oninput="previewMoeda(${i})">
   <span class="preview" id="pv${i}" style="width:80px;text-align:right"></span>
  </div>`;
 }
 c.innerHTML=h;

 let ch=document.getElementById('chips');
 ch.innerHTML=POPULARES.map(p=>`<span class="chip" onclick="addChip('${p}')">+ ${p}</span>`).join('');
}

function toggleSw(i){
 let sw=document.getElementById('sw'+i);
 state['c'+i+'en'] = !state['c'+i+'en'];
 sw.classList.toggle('on', state['c'+i+'en']);
 sw.nextElementSibling.textContent = state['c'+i+'en']?'Ativada':'Desativada';
}
function addChip(p){
 for(let i=1;i<=6;i++){
  let input=document.getElementById('c'+i);
  if(!input.value.trim()){
   input.value=p;
   state['c'+i+'en']=true;
   let sw=document.getElementById('sw'+i);
   if(sw){sw.classList.add('on'); sw.nextElementSibling.textContent='Ativada'}
   previewMoeda(i);
   toast('Adicionado '+p+' na posição '+i);
   return;
  }
 }
 toast('Todos os 6 slots preenchidos!',false);
}

async function previewMoeda(i){
 let v=document.getElementById('c'+i).value.trim().toUpperCase();
 if(!v.includes('-')) return;
 try{
  let r=await fetch('https://economia.awesomeapi.com.br/json/last/'+v);
  let j=await r.json();
  let k=v.replace('-','');
  if(j[k]){
   let bid=parseFloat(j[k].bid);
   document.getElementById('pv'+i).textContent= 'R$ ' + (bid>1000? Math.round(bid):bid.toFixed(2));
  }
 }catch(e){}
}
function previewTodasMoedas(){for(let i=1;i<=6;i++) previewMoeda(i);}

function fillCaps(){
 let sel=document.getElementById('citySel');
 sel.innerHTML=CAPITAIS.map(c=>`<option value="${c.c}" data-lat="${c.lat}" data-lon="${c.lon}">${c.c} (${c.uf})</option>`).join('');
}
function loadUFs(){
 const UFS=['AC','AL','AP','AM','BA','CE','DF','ES','GO','MA','MT','MS','MG','PA','PB','PR','PE','PI','RJ','RN','RS','RO','RR','SC','SP','SE','TO'];
 document.getElementById('uf').innerHTML='<option value="">Selecione o Estado</option>'+UFS.map(u=>`<option value="${u}">${u}</option>`).join('');
}
async function loadCidades(uf){
 if(!uf) return;
 let r=await fetch(`https://servicodados.ibge.gov.br/api/v1/localidades/estados/${uf}/municipios`);
 let list=await r.json();
 let sel=document.getElementById('citySel');
 sel.innerHTML=list.map(m=>`<option value="${m.nome}">${m.nome}</option>`).join('');
}
function filtrarCidades(){
 let filter=document.getElementById('cityInput').value.toLowerCase();
 let sel=document.getElementById('citySel');
 for(let opt of sel.options){
  opt.style.display=opt.text.toLowerCase().includes(filter)?'':'none';
 }
}
function usarCidadeSelecionada(){
 let sel=document.getElementById('citySel');
 let opt=sel.options[sel.selectedIndex];
 if(!opt) return;
 document.getElementById('city').value=opt.value;
 if(opt.dataset.lat){
  document.getElementById('lat').value=opt.dataset.lat;
  document.getElementById('lon').value=opt.dataset.lon;
  previewClima();
 } else {
  // busca coordenadas pelo nominatim
  fetch(`https://nominatim.openstreetmap.org/search?format=json&q=${encodeURIComponent(opt.value+', Brasil')}`)
   .then(r=>r.json()).then(j=>{
    if(j.length){
     document.getElementById('lat').value=parseFloat(j[0].lat).toFixed(4);
     document.getElementById('lon').value=parseFloat(j[0].lon).toFixed(4);
     previewClima();
    }
   });
 }
}
async function previewClima(){
 let lat=document.getElementById('lat').value;
 let lon=document.getElementById('lon').value;
 if(!lat||!lon) return;
 try{
  let r=await fetch(`https://api.open-meteo.com/v1/forecast?latitude=${lat}&longitude=${lon}&current=temperature_2m,weather_code,relative_humidity_2m,wind_speed_10m`);
  let j=await r.json();
  if(j.current){
   document.getElementById('pdesc').textContent=`Previsão: ${j.current.temperature_2m}°C | Umidade: ${j.current.relative_humidity_2m}% | Vento: ${j.current.wind_speed_10m} km/h`;
  }
 }catch(e){}
}

async function scanWifi(){
 let btn=event.target; let orig=btn.textContent; btn.textContent='🔍 Buscando...'; btn.disabled=true;
 try{
  let r=await fetch('/api/scan'); let j=await r.json();
  let html=j.map(n=>`<div class="wifi-item" onclick="selectWifi('${n.ssid}')"><span>${n.encryption=='open'?'🔓':'🔒'}</span><b>${n.ssid||'(oculta)'}</b><span class="rssi">${n.rssi}dBm</span></div>`).join('');
  document.getElementById('wifiList').innerHTML= html || '<div style="color:var(--muted);font-size:12px">Nenhuma rede encontrada</div>';
 }catch(e){toast('Erro no scan',false)}
 btn.textContent=orig; btn.disabled=false;
}
function selectWifi(ssid){document.getElementById('ssid').value=ssid; document.getElementById('pass').focus(); toast('Rede '+ssid+' selecionada');}

async function loadConfig(){
 let r=await fetch('/api/config'); let j=await r.json();
 for(let i=1;i<=6;i++){
  let c=document.getElementById('c'+i); if(c) c.value=j['c'+i]||'';
  state['c'+i+'en']=j['c'+i+'en'];
  let sw=document.getElementById('sw'+i);
  if(sw){sw.classList.toggle('on',state['c'+i+'en']); sw.nextElementSibling.textContent=state['c'+i+'en']?'Ativada':'Desativada'}
 }
 document.getElementById('city').value=j.city; document.getElementById('lat').value=j.lat; document.getElementById('lon').value=j.lon;
 document.getElementById('bright').value=j.bright; document.getElementById('bv').innerText=j.bright; document.getElementById('brightBar').style.width=(j.bright/255*100)+'%';
 document.getElementById('tz').value=j.tz; document.getElementById('dint').value=j.dint; document.getElementById('wint').value=j.wint;
 document.getElementById('ssid').value=j.ssid;
 document.getElementById('ipBadge').textContent='IP: '+j.ip;
 previewTodasMoedas(); previewClima();
}

function setTheme(m, sendToEsp=true){
 if(m==='light') document.documentElement.setAttribute('data-theme','light');
 else document.documentElement.removeAttribute('data-theme');
 localStorage.setItem('theme',m);
 let b=document.getElementById('themeBtn'); if(b) b.textContent= m==='light'?'☀️ Claro':'🌙 Escuro';
 if(sendToEsp){
  fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({dlight: m==='light'?1:0})})
   .then(()=>toast(m==='light'?'Modo claro ativado':'Modo noturno ativado'));
 }
}
function toggleTheme(){
 let cur=document.documentElement.getAttribute('data-theme');
 setTheme(cur==='light'?'dark':'light', true);
}
function toggleMirror(){
 let w=document.getElementById('globalMirror'); let b=document.getElementById('globalMirrorBtn');
 if(w.style.display==='none' || w.style.display===''){w.style.display='flex'; b.textContent='👁️ Esconder'; b.style.opacity='1'}
 else {w.style.display='none'; b.textContent='👁️ Tela'; b.style.opacity='0.7'}
}

function updateMirror(j){
 if(!j) return;
 document.getElementById('mTime').textContent=j.time.slice(0,5);
 document.getElementById('mDate').textContent=j.date;
 document.getElementById('mCity').textContent=j.city;
 document.getElementById('mTemp').textContent=j.weatherTemp;
 document.getElementById('mDesc').textContent=j.weatherDesc;
 document.getElementById('mIp').textContent='IP: '+j.ip;

 // 3 cards de moedas
 let curBox=document.getElementById('mCurCards');
 let flags={'USD-BRL':'🇺🇸','EUR-BRL':'🇪🇺','BTC-BRL':'₿','ETH-BRL':'Ξ','GBP-BRL':'🇬🇧','JPY-BRL':'🇯🇵'};
 let names={'USD-BRL':'Dólar','EUR-BRL':'Euro','BTC-BRL':'Bitcoin','ETH-BRL':'Ethereum','GBP-BRL':'Libra','JPY-BRL':'Iene'};

 let cardsHtml='';
 let pairs=[document.getElementById('c1')?.value||'USD-BRL',document.getElementById('c2')?.value||'EUR-BRL',document.getElementById('c3')?.value||'BTC-BRL'];
 let prices=[j.dolar||'R$ 4,92','R$ 5,21','R$ 171.450'];
 let pcts=['+0.35% ▲','-0.12% ▼','+2.1%'];

 for(let i=0;i<3;i++){
  let p=pairs[i]||'USD-BRL';
  let isPos=!pcts[i].includes('-');
  cardsHtml+=`
  <div style="background:#111C2E;border:1px solid #1E293B;border-radius:14px;padding:12px 14px;display:flex;align-items:center;justify-content:space-between">
   <div style="display:flex;align-items:center;gap:12px">
    <div style="font-size:24px;width:34px;text-align:center">${flags[p]||'💰'}</div>
    <div>
     <div style="font-size:16px;font-weight:800;color:#FFFFFF">${p.replace('-','/')}</div>
     <div style="font-size:12px;color:#94A3B8">${names[p]||'Moeda'}</div>
    </div>
   </div>
   <div style="text-align:right">
    <div style="font-size:16px;font-weight:800;color:#FFFFFF">${prices[i]}</div>
    <div style="font-size:12px;font-weight:700;color:${isPos?'#22C55E':'#EF4444'}">${pcts[i]}</div>
   </div>
  </div>`;
 }
 curBox.innerHTML=cardsHtml;
}

async function loadData(){
 try{
  let r=await fetch('/api/data'); let j=await r.json();
  document.getElementById('live').innerHTML=`
   <div class="kv"><span>Hora</span><b>${j.time} ${j.date}</b></div>
   <div class="kv"><span>Câmbio</span><b>${j.dolar}</b></div>
   <div class="kv"><span>Clima</span><b>${j.weatherTemp} ${j.weatherDesc} (${j.city})</b></div>
   <div class="kv"><span>WiFi</span><span class="badge ${j.wifi=='Conectado'?'ok':'off'}">${j.wifi} ${j.ip}</span></div>
   <div class="kv"><span>Uptime</span><b>${j.uptime}s</b></div>
   <div class="kv"><span>Heap Livre</span><b>${j.heap} bytes</b></div>`;
  document.getElementById('ipBadge').textContent='IP: '+j.ip;
  document.getElementById('wifiBadge').textContent=j.wifi;
  document.getElementById('wifiBadge').className='badge '+(j.wifi=='Conectado'?'ok':'off');
  updateMirror(j);
 }catch(e){}
}

async function save(){
 let body={};
 for(let i=1;i<=6;i++){
  let val=document.getElementById('c'+i).value.trim().toUpperCase();
  body['c'+i]=val;
  body['c'+i+'en']=state['c'+i+'en'];
  if(val && !val.includes('-')){toast('Moeda '+i+' deve ter hífen (ex: USD-BRL)',false); return;}
 }
 body.city=document.getElementById('city').value;
 body.lat=parseFloat(document.getElementById('lat').value);
 body.lon=parseFloat(document.getElementById('lon').value);
 body.bright=parseInt(document.getElementById('bright').value);
 body.tz=parseInt(document.getElementById('tz').value);
 body.dint=parseInt(document.getElementById('dint').value);
 body.wint=parseInt(document.getElementById('wint').value);
 body.ssid=document.getElementById('ssid').value;
 body.pass=document.getElementById('pass').value;

 let r=await fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});
 let j=await r.json();
 toast(j.msg);
 setTimeout(()=>{loadConfig(); loadData();},800);
}

async function restart(){
 if(confirm('Deseja reiniciar o ESP32?')){
  await fetch('/api/restart',{method:'POST'});
  toast('Reiniciando ESP32...');
 }
}

function testBlink(){
 let b=document.getElementById('bright'); let v=parseInt(b.value);
 fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({bright:255})});
 setTimeout(()=>fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({bright:v})}),800);
}

document.getElementById('uf').addEventListener('change',e=>loadCidades(e.target.value));
document.getElementById('cityInput').addEventListener('input',filtrarCidades);
document.getElementById('citySel').addEventListener('dblclick',usarCidadeSelecionada);
document.getElementById('bright').addEventListener('input',e=>{document.getElementById('bv').innerText=e.target.value; document.getElementById('brightBar').style.width=(e.target.value/255*100)+'%';});

async function loadOta(){
 try{
  let r=await fetch('/api/version'); let j=await r.json();
  document.getElementById('otaCur').textContent=j.current;
  document.getElementById('otaLatest').textContent=j.latest||'--';
  let states=['ocioso','verificando','sem update','atualizando','sucesso','falha'];
  document.getElementById('otaStatus').textContent=states[j.state]||j.state;
  document.getElementById('otaMsg').textContent=j.error||'';
  document.getElementById('otaBar').style.width=j.progress+'%';
 }catch(e){}
}
async function otaCheck(){let r=await fetch('/api/ota/check',{method:'POST'}); let j=await r.json(); toast(j.msg||j.error, j.state!==5); loadOta();}
async function otaUpdate(){if(!confirm('Atualizar para '+document.getElementById('otaLatest').textContent+'? Não desligue!')) return; toast('Baixando e gravando...'); let r=await fetch('/api/ota/update',{method:'POST'}); let j=await r.json(); toast(j.msg, j.ok); loadOta();}

fillPairs(); fillCaps(); loadUFs(); loadConfig(); loadData(); loadOta();
setInterval(loadData,5000);
setInterval(loadOta,10000);
</script>
</body>
</html>
)rawliteral";

void handleRoot() {
  Serial.printf("[Web] GET %s (page %d bytes)\n", webServer.uri().c_str(), (int)strlen_P(HTML_PAGE));
  webServer.send_P(200, "text/html", HTML_PAGE);
}

void handleGetConfig() {
  Serial.println("[Web] GET /api/config");
  JsonDocument doc;
  doc["c1"] = gConfig.currency_1;
  doc["c2"] = gConfig.currency_2;
  doc["c3"] = gConfig.currency_3;
  doc["c4"] = gConfig.currency_4;
  doc["c5"] = gConfig.currency_5;
  doc["c6"] = gConfig.currency_6;
  doc["c1en"] = gConfig.curr1_enabled;
  doc["c2en"] = gConfig.curr2_enabled;
  doc["c3en"] = gConfig.curr3_enabled;
  doc["c4en"] = gConfig.curr4_enabled;
  doc["c5en"] = gConfig.curr5_enabled;
  doc["c6en"] = gConfig.curr6_enabled;
  doc["city"] = gConfig.city;
  doc["lat"] = gConfig.lat;
  doc["lon"] = gConfig.lon;
  doc["bright"] = gConfig.brightness;
  doc["tz"] = gConfig.tz_offset;
  doc["dint"] = gConfig.dolar_interval;
  doc["wint"] = gConfig.weather_interval;
  doc["ssid"] = gConfig.wifi_ssid;
  doc["ip"] = WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : WiFi.softAPIP().toString();
  doc["dlight"] = gConfig.display_light;

  String out;
  serializeJson(doc, out);
  webServer.send(200, "application/json", out);
}

void handlePostConfig() {
  Serial.printf("[Web] POST /api/config args=%d hasPlain=%d body=%dB uri='%s'\n",
                webServer.args(), webServer.hasArg("plain")?1:0,
                (int)webServer.arg("plain").length(), webServer.uri().c_str());
  if (!webServer.hasArg("plain")) {
    webServer.send(400, "application/json", "{\"msg\":\"sem body\"}");
    return;
  }
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, webServer.arg("plain"));
  if (err) {
    webServer.send(400, "application/json", "{\"msg\":\"JSON invalido\"}");
    return;
  }

  if (doc["c1"].is<const char*>()) strncpy(gConfig.currency_1, doc["c1"], sizeof(gConfig.currency_1));
  if (doc["c2"].is<const char*>()) strncpy(gConfig.currency_2, doc["c2"], sizeof(gConfig.currency_2));
  if (doc["c3"].is<const char*>()) strncpy(gConfig.currency_3, doc["c3"], sizeof(gConfig.currency_3));
  if (doc["c4"].is<const char*>()) strncpy(gConfig.currency_4, doc["c4"], sizeof(gConfig.currency_4));
  if (doc["c5"].is<const char*>()) strncpy(gConfig.currency_5, doc["c5"], sizeof(gConfig.currency_5));
  if (doc["c6"].is<const char*>()) strncpy(gConfig.currency_6, doc["c6"], sizeof(gConfig.currency_6));

  if (doc["c1en"].is<bool>()) gConfig.curr1_enabled = doc["c1en"].as<bool>();
  if (doc["c2en"].is<bool>()) gConfig.curr2_enabled = doc["c2en"].as<bool>();
  if (doc["c3en"].is<bool>()) gConfig.curr3_enabled = doc["c3en"].as<bool>();
  if (doc["c4en"].is<bool>()) gConfig.curr4_enabled = doc["c4en"].as<bool>();
  if (doc["c5en"].is<bool>()) gConfig.curr5_enabled = doc["c5en"].as<bool>();
  if (doc["c6en"].is<bool>()) gConfig.curr6_enabled = doc["c6en"].as<bool>();

  if (doc["city"].is<const char*>()) {
    strncpy(gConfig.city, doc["city"], sizeof(gConfig.city));
    weatherCity = String(gConfig.city);
  }
  if (doc["lat"].is<float>()) gConfig.lat = doc["lat"].as<float>();
  if (doc["lon"].is<float>()) gConfig.lon = doc["lon"].as<float>();
  if (doc["tz"].is<int>()) gConfig.tz_offset = doc["tz"].as<int>();
  if (doc["dint"].is<int>()) gConfig.dolar_interval = doc["dint"].as<int>();
  if (doc["wint"].is<int>()) gConfig.weather_interval = doc["wint"].as<int>();
  if (doc["dlight"].is<int>()) {
    gConfig.display_light = (doc["dlight"].as<int>() == 1);
    gNeedsRebuild = true;
  }

  if (doc["bright"].is<int>()) {
    gConfig.brightness = doc["bright"].as<int>();
    tft.setBrightness(gConfig.brightness);
  }

  bool wifiChanged = false;
  if (doc["ssid"].is<const char*>()) {
    String ns = doc["ssid"].as<String>();
    String np = doc["pass"].is<const char*>() ? doc["pass"].as<String>() : "";
    if (ns != gConfig.wifi_ssid || np.length() > 0) {
      ns.toCharArray(gConfig.wifi_ssid, sizeof(gConfig.wifi_ssid));
      if (np.length() > 0) np.toCharArray(gConfig.wifi_pass, sizeof(gConfig.wifi_pass));
      wifiChanged = true;
    }
  }

  saveConfig();
  gNeedsRebuild = true;

  if (wifiChanged) {
    webServer.send(200, "application/json", "{\"msg\":\"WiFi alterado, reconectando...\"}");
    delay(500);
    WiFi.begin(gConfig.wifi_ssid, gConfig.wifi_pass);
  } else {
    webServer.send(200, "application/json", "{\"msg\":\"Salvo com sucesso! Painel atualizado\"}");
  }
}

void handleGetData() {
  JsonDocument doc;
  struct tm ti;
  char timeStr[16] = "--:--";
  char dateStr[32] = "";
  if (getLocalTime(&ti)) {
    snprintf(timeStr, sizeof(timeStr), "%02d:%02d", ti.tm_hour, ti.tm_min);
    static const char *weekdays[] = {"DOM", "SEG", "TER", "QUA", "QUI", "SEX", "SAB"};
    static const char *months[] = {"JAN", "FEV", "MAR", "ABR", "MAI", "JUN", "JUL", "AGO", "SET", "OUT", "NOV", "DEZ"};
    snprintf(dateStr, sizeof(dateStr), "%s, %02d %s", weekdays[ti.tm_wday], ti.tm_mday, months[ti.tm_mon]);
  }
  doc["time"] = timeStr;
  doc["date"] = dateStr;
  doc["dolar"] = dolarValue;
  doc["weatherTemp"] = weatherTemp;
  doc["weatherDesc"] = weatherDesc;
  doc["city"] = weatherCity;
  doc["wifi"] = WiFi.status() == WL_CONNECTED ? "Conectado" : "Desconectado";
  doc["ip"] = WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : WiFi.softAPIP().toString();
  doc["uptime"] = millis() / 1000;
  doc["heap"] = ESP.getFreeHeap();
  doc["bright"] = gConfig.brightness;

  String out;
  serializeJson(doc, out);
  webServer.send(200, "application/json", out);
}

void handleScan() {
  int n = WiFi.scanNetworks();
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  for (int i = 0; i < n; i++) {
    JsonObject o = arr.add<JsonObject>();
    o["ssid"] = WiFi.SSID(i);
    o["rssi"] = WiFi.RSSI(i);
    o["encryption"] = WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "open" : "wpa";
    o["channel"] = WiFi.channel(i);
  }
  String out;
  serializeJson(arr, out);
  webServer.send(200, "application/json", out);
  WiFi.scanDelete();
}

void handleVersion() {
  JsonDocument doc;
  doc["current"] = FIRMWARE_VERSION;
  doc["latest"] = gOta.latest;
  doc["state"] = (int)gOta.state;
  doc["progress"] = gOta.progress;
  doc["error"] = gOta.error;
  doc["url"] = gOta.downloadUrl;
  doc["dlight"] = gConfig.display_light;
  String out;
  serializeJson(doc, out);
  webServer.send(200, "application/json", out);
}

void handleOtaCheck() {
  bool has = otaCheck(true);
  JsonDocument doc;
  doc["hasUpdate"] = has;
  doc["latest"] = gOta.latest;
  doc["current"] = gOta.current;
  doc["msg"] = gOta.error;
  doc["state"] = (int)gOta.state;
  String out;
  serializeJson(doc, out);
  webServer.send(200, "application/json", out);
}

void handleOtaUpdate() {
  otaRequestUpdate();
  webServer.send(200, "application/json", "{\"msg\":\"iniciando OTA em background, aguarde...\",\"ok\":true}");
}

void handleRestart() {
  Serial.println("[Web] POST /api/restart -> reiniciando");
  webServer.send(200, "application/json", "{\"msg\":\"reiniciando...\"}");
  delay(500);
  ESP.restart();
}

void handleNotFound() {
  if (isApMode()) {
    webServer.sendHeader("Location", String("http://") + WiFi.softAPIP().toString() + "/", true);
    webServer.send(302, "text/plain", "");
    Serial.printf("[Web] captive redirect para %s -> /\n", webServer.hostHeader().c_str());
  } else {
    webServer.send(404, "text/plain", "Nao encontrado");
  }
}

bool isApMode() {
  return (WiFi.getMode() & WIFI_MODE_AP) != 0;
}

void webServerInit() {
  webServer.on("/", HTTP_GET, handleRoot);
  webServer.on("/api/config", HTTP_GET, handleGetConfig);
  webServer.on("/api/config", HTTP_POST, handlePostConfig);
  webServer.on("/api/data", HTTP_GET, handleGetData);
  webServer.on("/api/scan", HTTP_GET, handleScan);
  webServer.on("/api/version", HTTP_GET, handleVersion);
  webServer.on("/api/ota/check", HTTP_POST, handleOtaCheck);
  webServer.on("/api/ota/update", HTTP_POST, handleOtaUpdate);
  webServer.on("/api/restart", HTTP_POST, handleRestart);

  // Captive portal handlers
  webServer.on("/generate_204", HTTP_GET, handleRoot);
  webServer.on("/gen_204", HTTP_GET, handleRoot);
  webServer.on("/hotspot-detect.html", HTTP_GET, handleRoot);
  webServer.on("/canonical.html", HTTP_GET, handleRoot);
  webServer.on("/success.txt", HTTP_GET, handleRoot);
  webServer.on("/ncsi.txt", HTTP_GET, handleRoot);
  webServer.on("/connecttest.txt", HTTP_GET, handleRoot);
  webServer.on("/wpad.dat", HTTP_GET, handleRoot);
  webServer.on("/fwlink", HTTP_GET, handleRoot);
  webServer.onNotFound(handleNotFound);
  webServer.begin();

  IPAddress apIP = WiFi.softAPIP();
  IPAddress ip = WiFi.localIP();
  Serial.printf("[Web] HTTP: http://%s/ (AP) | http://%s/ (STA)\n",
                apIP.toString().c_str(), ip.toString().c_str());

  if (isApMode()) {
    bool dnsOk = dnsServer.start(53, "*", apIP);
    Serial.printf("[Web] DNS captive porta 53 %s -> http://192.168.4.1/\n", dnsOk ? "OK" : "FALHOU");
  }

  // Inicia mDNS (http://painel.local/)
  if (MDNS.begin("painel")) {
    MDNS.addService("http", "tcp", 80);
    Serial.println("[Web] mDNS responder ativo: http://painel.local/");
  }
}

void webServerLoop() {
  if (isApMode()) {
    dnsServer.processNextRequest();
  }
  webServer.handleClient();
}
