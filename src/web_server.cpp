#include "web_server.h"
#include "app_config.h"
#include <WiFi.h>
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

<!-- ESPELHAMENTO GLOBAL - sem fundo preto, só a tela proporcional -->
<div id="globalMirror" style="max-width:1200px;margin:14px auto;background:transparent;padding:0;display:none;justify-content:center;overflow:visible">
 <div id="mirrorScreen" style="width:800px;min-width:800px;height:480px;background:linear-gradient(180deg,#070A12,#0E1420);border:2px solid #d4a017;border-radius:12px;overflow:hidden;transform-origin:top center;position:relative;transform:scale(0.55);box-shadow:0 0 0 4px #8c6b00, 0 8px 30px rgba(0,0,0,.4)">
  <div style="height:28px;background:#06080D;border-bottom:1px solid #1E2A3A;display:flex;align-items:center;padding:0 10px;gap:8px">
   <div style="width:4px;height:18px;background:#22D3EE;border-radius:2px"></div>
   <span style="font-size:10px;font-weight:800;letter-spacing:1px">PAINEL FINANCEIRO</span>
   <span style="font-size:8px;color:#7A8699;margin-left:6px">Horario e cambio em tempo real</span>
   <span id="mTime" style="margin-left:auto;font-size:10px;color:#7A8699">--:--:--</span>
   <span id="mIp" style="font-size:8px;color:#00E676">IP: --</span>
  </div>
  <div id="mBody" style="padding:10px;display:grid;gap:8px;height:calc(100% - 28px);align-content:start"></div>
 </div>
</div>

<div class="tabs">
 <div class="tab active" onclick="showTab('dash')">📊 Dashboard</div>
 <div class="tab" onclick="showTab('moedas')">💱 Moedas (6)</div>
 <div class="tab" onclick="showTab('clima')">🌤️ Clima</div>
 <div class="tab" onclick="showTab('sistema')">⚙️ Sistema/Rede</div>
</div>

<div id="tab-dash" class="grid">
 <div class="card accent">
  <h2>LIVE DO PAINEL</h2>
  <div id="live"></div>
  <div class="row"><button class="btn-dark" onclick="loadData()">🔄 Atualizar</button><button class="btn-dark" onclick="testBlink()">💡 Testar brilho</button></div>
 </div>
 <div class="card green">
  <h2>PRÉVIA CÂMBIO (tempo real)</h2>
  <div id="moedaPreview"></div>
  <small style="color:var(--muted)">Atualiza ao trocar a moeda, sem salvar.</small>
 </div>
 <div class="card yellow">
  <h2>PRÉVIA CLIMA</h2>
  <div id="climaPreview">Selecione a cidade para ver</div>
  <button class="btn-dark" onclick="previewClima()">👁️ Ver agora</button>
 </div>
</div>

<div id="tab-moedas" class="grid hidden">
 <div class="card green" style="grid-column:1/-1">
  <h2>ESCOLHA ATÉ 6 MOEDAS — metade da tela em 2 linhas (3x2)</h2>
  <small style="color:var(--muted)">AwesomeAPI • BTC agora com fonte reduzida para caber R$ 401k inteiro • Salve para aplicar</small>
 </div>
 <div class="card green" id="slot1"><h2>MOEDA 1</h2><div style="display:flex;align-items:center;gap:10px"><div class="switch on" id="sw1" onclick="toggle(1)"><div class="knob"></div></div><b>Ativada</b><span id="pv1" class="preview" style="margin-left:auto">--</span></div><label>Par</label><div class="search"><input id="c1" placeholder="USD-BRL" oninput="onMoedaInput(1)"><div id="sg1" class="suggest"></div></div><div class="chips" id="chips1"></div></div>
 <div class="card green" id="slot2"><h2>MOEDA 2</h2><div style="display:flex;align-items:center;gap:10px"><div class="switch on" id="sw2" onclick="toggle(2)"><div class="knob"></div></div><b>Ativada</b><span id="pv2" class="preview" style="margin-left:auto">--</span></div><label>Par</label><div class="search"><input id="c2" placeholder="EUR-BRL" oninput="onMoedaInput(2)"><div id="sg2" class="suggest"></div></div><div class="chips" id="chips2"></div></div>
 <div class="card green" id="slot3"><h2>MOEDA 3</h2><div style="display:flex;align-items:center;gap:10px"><div class="switch on" id="sw3" onclick="toggle(3)"><div class="knob"></div></div><b>Ativada</b><span id="pv3" class="preview" style="margin-left:auto">--</span></div><label>Par</label><div class="search"><input id="c3" placeholder="BTC-BRL" oninput="onMoedaInput(3)"><div id="sg3" class="suggest"></div></div><div class="chips" id="chips3"></div></div>
 <div class="card green" id="slot4"><h2>MOEDA 4</h2><div style="display:flex;align-items:center;gap:10px"><div class="switch" id="sw4" onclick="toggle(4)"><div class="knob"></div></div><b>Desativada</b><span id="pv4" class="preview" style="margin-left:auto">--</span></div><label>Par</label><div class="search"><input id="c4" placeholder="ETH-BRL" oninput="onMoedaInput(4)"><div id="sg4" class="suggest"></div></div><div class="chips" id="chips4"></div></div>
 <div class="card green" id="slot5"><h2>MOEDA 5</h2><div style="display:flex;align-items:center;gap:10px"><div class="switch" id="sw5" onclick="toggle(5)"><div class="knob"></div></div><b>Desativada</b><span id="pv5" class="preview" style="margin-left:auto">--</span></div><label>Par</label><div class="search"><input id="c5" placeholder="GBP-BRL" oninput="onMoedaInput(5)"><div id="sg5" class="suggest"></div></div><div class="chips" id="chips5"></div></div>
 <div class="card green" id="slot6"><h2>MOEDA 6</h2><div style="display:flex;align-items:center;gap:10px"><div class="switch" id="sw6" onclick="toggle(6)"><div class="knob"></div></div><b>Desativada</b><span id="pv6" class="preview" style="margin-left:auto">--</span></div><label>Par</label><div class="search"><input id="c6" placeholder="JPY-BRL" oninput="onMoedaInput(6)"><div id="sg6" class="suggest"></div></div><div class="chips" id="chips6"></div></div>
 <div class="card"><h2>INTERVALO</h2><label>Atualizar cotações a cada (s) <input id="dint" type="number" min="30" max="3600"></label><button class="btn-green" onclick="save()">💾 Salvar moedas (até 6)</button></div>
</div>

<div id="tab-clima" class="grid hidden">
 <div class="card yellow" style="grid-column:1/-1"><h2>TODAS AS CIDADES DO BRASIL</h2><small style="color:var(--muted)">IBGE 5570 municípios + capitais 1 clique • Salve para painel</small></div>
 <div class="card yellow"><h2>BUSCA POR ESTADO/CIDADE</h2><label>Estado</label><select id="uf"><option>Carregando...</option></select><label>Cidade</label><div class="search"><input id="cityInput" placeholder="Digite para filtrar..."><div id="citySg" class="suggest"></div></div><select id="citySel" size="6" style="height:140px;margin-top:8px"></select><button class="btn-dark" onclick="usarCidadeSelecionada()">📍 Usar esta cidade</button></div>
 <div class="card yellow"><h2>CAPITAIS (1 clique)</h2><div class="chips" id="caps"></div><label>Ou digite</label><input id="city" placeholder="Sao Paulo"><div class="row"><div><label>Latitude</label><input id="lat" type="number" step="0.0001"></div><div><label>Longitude</label><input id="lon" type="number" step="0.0001"></div></div><button class="btn-dark" onclick="buscarCidade()">🔍 Buscar lat/lon</button><button class="btn-dark" onclick="previewClima()">🌡️ Prévia</button><label>Intervalo clima (s) <input id="wint" type="number" min="60" max="3600"></label><button class="btn-green" onclick="save()">💾 Salvar clima</button></div>
 <div class="card"><h2>PRÉVIA</h2><div id="climaCard" style="background:var(--card2);border:1px solid var(--border);border-radius:12px;padding:12px;text-align:center"><div style="font-size:11px;letter-spacing:2px;color:var(--yellow)">CLIMA</div><div id="pcity" style="color:var(--muted);font-size:12px">--</div><div id="ptemp" style="font-size:36px;font-weight:900;color:var(--yellow);margin:6px 0">--°</div><div id="pdesc" style="color:var(--muted);font-size:13px">--</div></div></div>
</div>

<div id="tab-sistema" class="grid hidden">
 <div class="card accent"><h2>TELA</h2><label>Brilho <span id="bv" style="float:right;color:var(--accent)"></span><input id="bright" type="range" min="10" max="255"></label><div style="height:10px;background:#0F1622;border-radius:999px;overflow:hidden;margin-top:6px"><div id="brightBar" style="height:100%;background:var(--accent);width:50%"></div></div><label>Fuso</label><select id="tz"><option value="-5">-5 Acre</option><option value="-4">-4 Manaus</option><option value="-3" selected>-3 Brasília</option><option value="-2">-2 Fernando</option></select></div>
 <div class="card"><h2>APARÊNCIA</h2><div class="row"><button class="btn-dark" onclick="setTheme('dark')">🌙 Noturno</button><button class="btn-dark" onclick="setTheme('light')">☀️ Claro</button></div><small style="color:var(--muted)">Alterna site e espelhamento. Salvo no navegador. ESP segue: <span id="espTheme">--</span></small></div>
 <div class="card green" id="otaCard">
  <h2>ATUALIZAÇÃO OTA (GitHub Releases)</h2>
  <div class="kv"><span>Versão atual</span><b id="otaCur">--</b></div>
  <div class="kv"><span>Última no GitHub</span><b id="otaLatest">--</b></div>
  <div class="kv"><span>Status</span><b id="otaStatus">--</b></div>
  <div style="height:8px;background:#0F1622;border-radius:999px;overflow:hidden;margin:8px 0"><div id="otaBar" style="height:100%;background:var(--green);width:0%"></div></div>
  <div id="otaMsg" style="font-size:11px;color:var(--muted);min-height:16px"></div>
  <div class="row"><button class="btn-dark" onclick="otaCheck()">🔍 Verificar</button><button class="btn-green" onclick="otaUpdate()">⬇️ Atualizar agora</button></div>
  <small style="color:var(--muted)">Auto a cada 6h. Bin gerado por Action em <code>antony1727/ESTAGIO-</code> tag <code>v*.*.*</code></small>
 </div>
 <div class="card">
  <h2>WIFI - REDES PRÓXIMAS (sem digitar SSID)</h2>
  <button class="btn-dark" onclick="scanWifi()">🔍 Buscar redes próximas</button>
  <div id="wifiList" style="max-height:220px;overflow:auto;margin-top:8px"></div>
  <label>SSID selecionado <input id="ssid" placeholder="Clique na rede acima ou digite"></label>
  <label>Senha <input id="pass" type="password" placeholder="deixe vazio para manter"></label>
  <div style="font-size:11px;color:var(--red);margin-top:6px">Ao salvar, conecta automaticamente sem digitar SSID manual.</div>
  <button class="btn-green" onclick="save()">💾 Salvar e Conectar</button>
  <button class="btn-dark" onclick="restart()">🔄 Reiniciar</button>
 </div>
 <div class="card"><h2>SOBRE</h2><div id="about" style="font-size:12px;color:var(--muted);line-height:1.6"></div></div>
</div>

<div class="toast" id="toast"></div>
<pre id="log" style="max-width:1200px;margin:14px auto;background:#070A12;padding:12px;border-radius:12px;border:1px solid var(--border);font-size:12px;max-height:160px;overflow:auto"></pre>

<script>
const PAIRS=["USD-BRL","EUR-BRL","BTC-BRL","ETH-BRL","GBP-BRL","JPY-BRL","CAD-BRL","AUD-BRL","CHF-BRL","CNY-BRL","ARS-BRL","CLP-BRL","UYU-BRL","PYG-BRL","BOB-BRL","COP-BRL","PEN-BRL","MXN-BRL","ILS-BRL","NZD-BRL","SGD-BRL","HKD-BRL","SEK-BRL","DKK-BRL","NOK-BRL","AED-BRL","SAR-BRL","TRY-BRL","ZAR-BRL","INR-BRL","KRW-BRL","PLN-BRL"];
const CAPS=[["Rio Branco-AC",-9.97499,-67.8243],["Maceió-AL",-9.66583,-35.73528],["Macapá-AP",0.03407,-51.0694],["Manaus-AM",-3.13194,-60.02222],["Salvador-BA",-12.97775,-38.50198],["Fortaleza-CE",-3.71722,-38.54306],["Brasília-DF",-15.79389,-47.88278],["Vitória-ES",-20.3155,-40.31282],["Goiânia-GO",-16.67861,-49.25389],["São Luís-MA",-2.53874,-44.28242],["Cuiabá-MT",-15.60141,-56.09789],["Campo Grande-MS",-20.46971,-54.62011],["Belo Horizonte-MG",-19.91667,-43.93444],["Belém-PA",-1.45583,-48.50444],["João Pessoa-PB",-7.11509,-34.8641],["Curitiba-PR",-25.429,-49.26714],["Recife-PE",-8.04756,-34.87696],["Teresina-PI",-5.08921,-42.8016],["Rio de Janeiro-RJ",-22.90685,-43.1729],["Natal-RN",-5.795,-35.20944],["Porto Alegre-RS",-30.03465,-51.21766],["Porto Velho-RO",-8.76183,-63.90389],["Boa Vista-RR",2.82352,-60.67583],["Florianópolis-SC",-27.59487,-48.54822],["São Paulo-SP",-23.55052,-46.63331],["Aracaju-SE",-10.9472,-37.07308],["Palmas-TO",-10.16891,-48.33178]];
let state={c1en:true,c2en:true,c3en:true,c4en:false,c5en:false,c6en:false};
function toast(m,ok=true){let t=document.getElementById('toast');t.textContent=m;t.style.display='block';t.style.borderColor=ok?'#00E676':'#FF5252';t.style.color=ok?'#00E676':'#FF5252';setTimeout(()=>t.style.display='none',2500)}
function log(m){let e=document.getElementById('log');e.textContent=new Date().toLocaleTimeString()+" "+m+"\n"+e.textContent}
function showTab(n){document.querySelectorAll('.tab').forEach((e,i)=>e.classList.toggle('active',["dash","moedas","clima","sistema"][i]==n));["dash","moedas","clima","sistema"].forEach(k=>document.getElementById('tab-'+k).classList.toggle('hidden',k!=n))}
function toggle(n){state['c'+n+'en']=!state['c'+n+'en'];document.getElementById('sw'+n).classList.toggle('on',state['c'+n+'en']);document.getElementById('sw'+n).nextElementSibling.textContent=state['c'+n+'en']?'Ativada':'Desativada'}
function fillPairs(){for(let i=1;i<=6;i++){let id='chips'+i; if(!document.getElementById(id)) continue; document.getElementById(id).innerHTML=PAIRS.slice(0,12).map(p=>`<div class="chip" onclick="setPair(${i},'${p}')">${p}</div>`).join('')}}
function setPair(n,p){document.getElementById('c'+n).value=p;onMoedaInput(n)}
let debounce;
function onMoedaInput(n){
 clearTimeout(debounce); debounce=setTimeout(()=>previewMoeda(n),350);
 let v=document.getElementById('c'+n).value.toUpperCase(); let sg=document.getElementById('sg'+n); if(!sg) return;
 if(!v){sg.style.display='none';return}
 let fil=PAIRS.filter(p=>p.includes(v)).slice(0,6); sg.innerHTML=fil.map(p=>`<div onclick="setPair(${n},'${p}')">${p}</div>`).join(''); sg.style.display=fil.length?'block':'none';
}
async function previewMoeda(n){
 let pair=document.getElementById('c'+n).value.trim().toUpperCase(); if(!pair||!pair.includes('-')) return;
 document.getElementById('pv'+n).textContent='buscando...';
 try{
  let r=await fetch('https://economia.awesomeapi.com.br/json/last/'+pair); let j=await r.json();
  let key=pair.replace('-',''); let bid=j[key]?.bid; let ask=j[key]?.ask; let varpct=j[key]?.pctChange;
  let txt= bid ? `R$ ${bid} ${varpct?'('+varpct+'%)':''}` : 'não encontrado';
  document.getElementById('pv'+n).textContent=txt;
  let mp=document.getElementById('moedaPreview'); if(mp && n==1) mp.innerHTML=`<div class="kv"><span>${pair}</span><b>${txt}</b></div>`;
 }catch(e){document.getElementById('pv'+n).textContent='erro'}
}
async function previewTodasMoedas(){
 document.getElementById('moedaPreview').innerHTML='';
 for(let i=1;i<=6;i++) if(state['c'+i+'en']) await previewMoeda(i);
}
async function loadUFs(){
 try{
  let r=await fetch('https://servicodados.ibge.gov.br/api/v1/localidades/estados?orderBy=nome'); let j=await r.json();
  let sel=document.getElementById('uf'); sel.innerHTML='<option value="">Selecione o estado</option>'+j.map(u=>`<option value="${u.sigla}">${u.nome} (${u.sigla})</option>`).join('');
 }catch(e){document.getElementById('uf').innerHTML='<option>erro IBGE</option>'}
}
async function loadCidades(uf){
 let sel=document.getElementById('citySel'); sel.innerHTML='<option>carregando...</option>';
 try{
  let r=await fetch(`https://servicodados.ibge.gov.br/api/v1/localidades/estados/${uf}/municipios`); let j=await r.json();
  window._cidades=j; sel.innerHTML=j.map(c=>`<option value="${c.nome}">${c.nome}</option>`).join('');
 }catch(e){sel.innerHTML='<option>erro</option>'}
}
function filtrarCidades(){
 let q=document.getElementById('cityInput').value.toLowerCase(); let list=window._cidades||[];
 let fil= q ? list.filter(c=>c.nome.toLowerCase().includes(q)).slice(0,200) : list.slice(0,200);
 document.getElementById('citySel').innerHTML=fil.map(c=>`<option value="${c.nome}">${c.nome}</option>`).join('');
}
function usarCidadeSelecionada(){let city=document.getElementById('citySel').value; if(!city){toast('Selecione a cidade',false);return} document.getElementById('city').value=city; buscarCidade();}
function fillCaps(){document.getElementById('caps').innerHTML=CAPS.map(c=>`<div class="chip" onclick="useCap('${c[0]}',${c[1]},${c[2]})">${c[0].split('-')[0]}</div>`).join('')}
function useCap(name,lat,lon){document.getElementById('city').value=name.split('-')[0]; document.getElementById('lat').value=lat; document.getElementById('lon').value=lon; previewClima(); toast('Capital '+name+' selecionada');}
async function buscarCidade(){
 let city=document.getElementById('city').value.trim(); if(!city){toast('Digite a cidade',false);return}
 log('Buscando '+city); let r=await fetch('https://geocoding-api.open-meteo.com/v1/search?name='+encodeURIComponent(city)+'&count=1&language=pt&format=json'); let j=await r.json();
 if(j.results && j.results[0]){ document.getElementById('lat').value=j.results[0].latitude; document.getElementById('lon').value=j.results[0].longitude; document.getElementById('city').value=j.results[0].name; log('Encontrado '+j.results[0].name); previewClima();}
 else {toast('Cidade não encontrada',false); log('não encontrada')}
}
async function previewClima(){
 let lat=parseFloat(document.getElementById('lat').value), lon=parseFloat(document.getElementById('lon').value), city=document.getElementById('city').value;
 if(isNaN(lat)||isNaN(lon)){toast('Lat/lon inválidos',false);return}
 document.getElementById('pcity').textContent=city||'--'; document.getElementById('pdesc').textContent='buscando...';
 try{
  let r=await fetch(`https://api.open-meteo.com/v1/forecast?latitude=${lat}&longitude=${lon}&current_weather=true`); let j=await r.json();
  let t=j.current_weather?.temperature; let code=j.current_weather?.weathercode; let descs={0:'Céu limpo',1:'Predom. limpo',2:'Parcialmente nublado',3:'Encoberto',45:'Nevoeiro',51:'Chuvisco',61:'Chuva',71:'Neve',95:'Trovoadas'};
  let desc=descs[code]||('Código '+code);
  document.getElementById('ptemp').textContent= (t!=null? Math.round(t)+'°':'--°');
  document.getElementById('pdesc').textContent=desc;
  document.getElementById('climaPreview').innerHTML=`<div class="kv"><span>${city}</span><b>${Math.round(t)}° ${desc}</b></div><div style="color:var(--muted);font-size:12px">Lat ${lat} Lon ${lon}</div>`;
 }catch(e){document.getElementById('pdesc').textContent='erro'}
}
async function scanWifi(){
 let btn=event.target; let orig=btn.textContent; btn.textContent='🔍 Buscando...'; btn.disabled=true;
 try{
  let r=await fetch('/api/scan'); let j=await r.json();
  let html=j.map(n=>`<div class="wifi-item" onclick="selectWifi('${n.ssid}')"><span>${n.encryption=='open'?'🔓':'🔒'}</span><b>${n.ssid||'(oculta)'}</b><span class="rssi">${n.rssi}dBm ${n.rssi>-60?'▂▄▆█':n.rssi>-70?'▂▄▆':'▂▄'}</span><span style="font-size:11px;color:var(--muted)">${n.encryption}</span></div>`).join('');
  document.getElementById('wifiList').innerHTML= html || '<div style="color:var(--muted);font-size:12px">Nenhuma rede encontrada</div>';
  log('Encontradas '+j.length+' redes');
 }catch(e){toast('Erro scan',false)}
 btn.textContent=orig; btn.disabled=false;
}
function selectWifi(ssid){document.getElementById('ssid').value=ssid; document.getElementById('pass').focus(); toast('Rede '+ssid+' selecionada');}
async function loadConfig(){
 let r=await fetch('/api/config'); let j=await r.json();
 for(let i=1;i<=6;i++){let c=document.getElementById('c'+i); if(c) c.value=j['c'+i]||''; state['c'+i+'en']=j['c'+i+'en']; let sw=document.getElementById('sw'+i); if(sw){sw.classList.toggle('on',state['c'+i+'en']); sw.nextElementSibling.textContent=state['c'+i+'en']?'Ativada':'Desativada'}}
 document.getElementById('city').value=j.city; document.getElementById('lat').value=j.lat; document.getElementById('lon').value=j.lon;
 document.getElementById('bright').value=j.bright; document.getElementById('bv').innerText=j.bright; document.getElementById('brightBar').style.width=(j.bright/255*100)+'%';
 document.getElementById('tz').value=j.tz; document.getElementById('dint').value=j.dint; document.getElementById('wint').value=j.wint;
 document.getElementById('ssid').value=j.ssid; document.getElementById('ipBadge').textContent='IP: '+j.ip; document.getElementById('about').innerHTML=`<div class="kv"><span>IP</span><b>${j.ip}</b></div><div class="kv"><span>SSID</span><b>${j.ssid}</b></div><div class="kv"><span>Lat/Lon</span><b>${j.lat}, ${j.lon}</b></div>`;
 // sincroniza modo claro/escuro do ESP com o site
 if(j.dlight!==undefined){ setTheme(j.dlight? 'light':'dark', false); }
 log('Config carregado'); previewTodasMoedas(); previewClima();
}
function setTheme(m, sendToEsp=true){
 if(m==='light') document.documentElement.setAttribute('data-theme','light');
 else document.documentElement.removeAttribute('data-theme');
 localStorage.setItem('theme',m);
 let b=document.getElementById('themeBtn'); if(b) b.textContent= m==='light'?'☀️ Claro':'🌙 Escuro';
 if(sendToEsp){
  // transfere para o ESP também
  fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({dlight: m==='light'?1:0})}).then(()=>{toast(m==='light'?'Modo claro no ESP e site':'Modo noturno no ESP e site'); setTimeout(()=>loadConfig(),600)});
 } else {
  toast(m==='light'?'Modo claro ativado':'Modo noturno ativado');
 }
}
function toggleTheme(){
 let cur=document.documentElement.getAttribute('data-theme');
 setTheme(cur==='light'?'dark':'light', true);
}
(function(){let t=localStorage.getItem('theme'); if(t==='light'){document.documentElement.setAttribute('data-theme','light'); setTimeout(()=>{let b=document.getElementById('themeBtn'); if(b) b.textContent='☀️ Claro'},100)}})();
function toggleMirror(){let w=document.getElementById('globalMirror'); let b=document.getElementById('globalMirrorBtn'); if(w.style.display==='none' || w.style.display===''){w.style.display='flex'; b.textContent='👁️ Esconder'; b.style.opacity='1'} else {w.style.display='none'; b.textContent='👁️ Tela'; b.style.opacity='0.7'} }
function updateMirror(j){
 let m=document.getElementById('mBody'); if(!m) return;
 document.getElementById('mTime').textContent=j.time; document.getElementById('mIp').textContent='IP: '+j.ip;
 // moedas atuais do /api/data só traz primeira, busca config para todas
 let cnt=0; try{cnt=(state.c1en?1:0)+(state.c2en?1:0)+(state.c3en?1:0)+(state.c4en?1:0)+(state.c5en?1:0)+(state.c6en?1:0)}catch(e){cnt=1}
 if(cnt>3){
   m.style.gridTemplateColumns='340px 1fr'; m.style.gridTemplateRows='140px 148px';
   m.innerHTML=`<div style="background:#0F1622;border:1px solid #1E2A3A;border-radius:12px;padding:10px;grid-row:1/3;display:flex;flex-direction:column;align-items:center;justify-content:center;border-top:3px solid #22D3EE"><div style="font-size:9px;letter-spacing:2px;color:#22D3EE">HORARIO LOCAL</div><div style="font-size:28px;font-weight:900;color:#F8FAFC;margin:8px 0">${j.time}</div><div style="font-size:11px;color:#7A8699">${j.date}</div></div>
   <div style="background:#0F1622;border:1px solid #1E2A3A;border-radius:12px;padding:10px;display:flex;flex-direction:column;align-items:center;justify-content:center;border-top:3px solid #FFB300"><div style="font-size:9px;letter-spacing:2px;color:#FFB300">CLIMA</div><div style="font-size:11px;color:#7A8699">${j.city}</div><div style="font-size:22px;font-weight:900;color:#FFB300">${j.weatherTemp}</div><div style="font-size:11px;color:#7A8699">${j.weatherDesc}</div></div>
   <div style="display:grid;grid-template-columns:repeat(3,1fr);gap:6px"><div style="background:#0F1622;border:1px solid #1E2A3A;border-radius:10px;padding:6px;text-align:center;border-top:2px solid #00E676"><div style="font-size:8px;color:#00E676">${document.getElementById('c1')?.value||'USD-BRL'}</div><div style="font-size:10px;font-weight:800;color:#00E676;margin-top:4px">${j.dolar}</div></div><div style="background:#0F1622;border:1px solid #1E2A3A;border-radius:10px;padding:6px;text-align:center;border-top:2px solid #2979FF"><div style="font-size:8px;color:#2979FF">${document.getElementById('c2')?.value||'EUR-BRL'}</div><div style="font-size:10px;font-weight:800;color:#2979FF">R$ --</div></div><div style="background:#0F1622;border:1px solid #1E2A3A;border-radius:10px;padding:6px;text-align:center;border-top:2px solid #FFAB00"><div style="font-size:8px;color:#FFAB00">${document.getElementById('c3')?.value||'BTC-BRL'}</div><div style="font-size:10px;font-weight:800;color:#FFAB00">R$ --</div></div><div style="background:#0F1622;border:1px solid #1E2A3A;border-radius:10px;padding:6px;text-align:center;border-top:2px solid #FF4081"><div style="font-size:8px;color:#FF4081">${document.getElementById('c4')?.value||'ETH-BRL'}</div><div style="font-size:10px;color:#FF4081">R$ --</div></div><div style="background:#0F1622;border:1px solid #1E2A3A;border-radius:10px;padding:6px;text-align:center;border-top:2px solid #E040FB"><div style="font-size:8px;color:#E040FB">${document.getElementById('c5')?.value||'GBP-BRL'}</div><div style="font-size:10px;color:#E040FB">R$ --</div></div><div style="background:#0F1622;border:1px solid #1E2A3A;border-radius:10px;padding:6px;text-align:center;border-top:2px solid #18FFFF"><div style="font-size:8px;color:#18FFFF">${document.getElementById('c6')?.value||'JPY-BRL'}</div><div style="font-size:10px;color:#18FFFF">R$ --</div></div></div>`;
 } else {
   m.style.gridTemplateColumns=''; m.style.gridTemplateRows='';
   m.style.display='grid'; m.style.gridTemplateColumns=`repeat(${2+cnt},1fr)`;
   let html=`<div style="background:#0F1622;border:1px solid #1E2A3A;border-radius:12px;padding:10px;display:flex;flex-direction:column;align-items:center;justify-content:center;border-top:3px solid #22D3EE"><div style="font-size:9px;letter-spacing:2px;color:#22D3EE">HORARIO LOCAL</div><div style="font-size:22px;font-weight:900;color:#F8FAFC;margin:8px 0">${j.time}</div><div style="font-size:11px;color:#7A8699">${j.date}</div></div>`;
   html+=`<div style="background:#0F1622;border:1px solid #1E2A3A;border-radius:12px;padding:10px;display:flex;flex-direction:column;align-items:center;justify-content:center;border-top:3px solid #FFB300"><div style="font-size:9px;letter-spacing:2px;color:#FFB300">CLIMA</div><div style="font-size:11px;color:#7A8699">${j.city}</div><div style="font-size:18px;font-weight:900;color:#FFB300">${j.weatherTemp}</div><div style="font-size:11px;color:#7A8699">${j.weatherDesc}</div></div>`;
   let colors=['#00E676','#2979FF','#FFAB00','#FF4081','#E040FB','#18FFFF'];
   let pairs=[document.getElementById('c1')?.value||'USD-BRL',document.getElementById('c2')?.value||'EUR-BRL',document.getElementById('c3')?.value||'BTC-BRL',document.getElementById('c4')?.value||'ETH-BRL',document.getElementById('c5')?.value||'GBP-BRL',document.getElementById('c6')?.value||'JPY-BRL'];
   let idx=0;
   for(let i=0;i<6 && idx<cnt;i++){ if(!state['c'+(i+1)+'en']) continue; html+=`<div style="background:#0F1622;border:1px solid #1E2A3A;border-radius:12px;padding:10px;display:flex;flex-direction:column;align-items:center;justify-content:center;border-top:3px solid ${colors[i]}"><div style="font-size:8px;color:${colors[i]}">${pairs[i]}</div><div style="font-size:${pairs[i].includes('BTC')? '14px':'16px'};font-weight:900;color:${colors[i]};margin:6px 0">${idx==0? j.dolar : 'R$ --'}</div><div style="font-size:9px;color:#5A6A80">${pairs[i].replace('-',' / ')}</div></div>`; idx++; }
   m.innerHTML=html;
 }
}
async function loadData(){
 let r=await fetch('/api/data'); let j=await r.json();
 document.getElementById('live').innerHTML=`
  <div class="kv"><span>Hora</span><b>${j.time} ${j.date}</b></div>
  <div class="kv"><span>Câmbio</span><b>${j.dolar}</b></div>
  <div class="kv"><span>Clima</span><b>${j.weatherTemp} ${j.weatherDesc} (${j.city})</b></div>
  <div class="kv"><span>WiFi</span><span class="badge ${j.wifi=='Conectado'?'ok':'off'}">${j.wifi} ${j.ip}</span></div>
  <div class="kv"><span>Uptime</span><b>${j.uptime}s</b></div>
  <div class="kv"><span>Heap</span><b>${j.heap}</b></div>`;
 document.getElementById('ipBadge').textContent='IP: '+j.ip;
 document.getElementById('wifiBadge').textContent=j.wifi; document.getElementById('wifiBadge').className='badge '+(j.wifi=='Conectado'?'ok':'off');
 updateMirror(j);
}
async function save(){
 let body={};
 for(let i=1;i<=6;i++){body['c'+i]=document.getElementById('c'+i).value.trim().toUpperCase(); body['c'+i+'en']=state['c'+i+'en'];}
 body.city=document.getElementById('city').value; body.lat=parseFloat(document.getElementById('lat').value); body.lon=parseFloat(document.getElementById('lon').value);
 body.bright=parseInt(document.getElementById('bright').value); body.tz=parseInt(document.getElementById('tz').value); body.dint=parseInt(document.getElementById('dint').value); body.wint=parseInt(document.getElementById('wint').value);
 body.ssid=document.getElementById('ssid').value; body.pass=document.getElementById('pass').value;
 for(let i=1;i<=6;i++) if(body['c'+i] && !body['c'+i].includes('-')){toast('Moeda '+i+' deve ser ex USD-BRL',false);return}
 let r=await fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});
 let j=await r.json(); toast(j.msg); log(j.msg);
 setTimeout(()=>{loadConfig(); loadData();},800);
}
async function restart(){ if(confirm('Reiniciar ESP?')){await fetch('/api/restart',{method:'POST'}); toast('Reiniciando...')}}
function testBlink(){let b=document.getElementById('bright'); let v=parseInt(b.value); fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({bright:255})}); setTimeout(()=>fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({bright:v})}),800)}
document.getElementById('uf').addEventListener('change',e=>loadCidades(e.target.value));
document.getElementById('cityInput').addEventListener('input',filtrarCidades);
document.getElementById('citySel').addEventListener('dblclick',usarCidadeSelecionada);
document.getElementById('bright').addEventListener('input',e=>{document.getElementById('bv').innerText=e.target.value; document.getElementById('brightBar').style.width=(e.target.value/255*100)+'%';});
let t; document.getElementById('bright').addEventListener('change',e=>{clearTimeout(t); t=setTimeout(()=>fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({bright:parseInt(e.target.value)})}).then(()=>toast('Brilho '+e.target.value)),400)});
async function loadOta(){try{let r=await fetch('/api/version'); let j=await r.json(); document.getElementById('otaCur').textContent=j.current; document.getElementById('otaLatest').textContent=j.latest||'--'; let states=['ocioso','verificando','sem update','atualizando','sucesso','falha']; document.getElementById('otaStatus').textContent=states[j.state]||j.state; document.getElementById('otaMsg').textContent=j.error||''; document.getElementById('otaBar').style.width=j.progress+'%'; let et=document.getElementById('espTheme'); if(et) et.textContent=j.dlight?'claro':'noturno';}catch(e){}}
async function otaCheck(){let r=await fetch('/api/ota/check',{method:'POST'}); let j=await r.json(); toast(j.msg||j.error, j.state!==5); loadOta();}
async function otaUpdate(){if(!confirm('Atualizar para '+document.getElementById('otaLatest').textContent+'? Nao desligue!')) return; toast('Baixando e gravando...'); let r=await fetch('/api/ota/update',{method:'POST'}); let j=await r.json(); toast(j.msg, j.ok); loadOta();}
fillPairs(); fillCaps(); loadUFs(); loadConfig(); loadData(); loadOta(); setInterval(loadData,5000); setInterval(loadOta,10000);
</script>
</body>
</html>
)rawliteral";

void handleRoot() {
  webServer.send_P(200, "text/html", HTML_PAGE);
}

void handleGetConfig() {
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
  doc["dlight"] = gConfig.display_light;
  doc["ssid"] = gConfig.wifi_ssid;
  doc["ip"] = WiFi.localIP().toString();
  String out;
  serializeJson(doc, out);
  webServer.send(200, "application/json", out);
}

void handlePostConfig() {
  if (!webServer.hasArg("plain")) {
    webServer.send(400, "application/json", "{\"msg\":\"sem body\"}");
    return;
  }
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, webServer.arg("plain"));
  if (err) {
    webServer.send(400, "application/json", "{\"msg\":\"json invalido\"}");
    return;
  }
  int oldCnt = (gConfig.curr1_enabled?1:0)+(gConfig.curr2_enabled?1:0)+(gConfig.curr3_enabled?1:0)+(gConfig.curr4_enabled?1:0)+(gConfig.curr5_enabled?1:0)+(gConfig.curr6_enabled?1:0);
  if (!doc["c1"].isNull()) strlcpy(gConfig.currency_1, doc["c1"], sizeof(gConfig.currency_1));
  if (!doc["c2"].isNull()) strlcpy(gConfig.currency_2, doc["c2"], sizeof(gConfig.currency_2));
  if (!doc["c3"].isNull()) strlcpy(gConfig.currency_3, doc["c3"], sizeof(gConfig.currency_3));
  if (!doc["c4"].isNull()) strlcpy(gConfig.currency_4, doc["c4"], sizeof(gConfig.currency_4));
  if (!doc["c5"].isNull()) strlcpy(gConfig.currency_5, doc["c5"], sizeof(gConfig.currency_5));
  if (!doc["c6"].isNull()) strlcpy(gConfig.currency_6, doc["c6"], sizeof(gConfig.currency_6));
  if (!doc["c1en"].isNull()) gConfig.curr1_enabled = doc["c1en"];
  if (!doc["c2en"].isNull()) gConfig.curr2_enabled = doc["c2en"];
  if (!doc["c3en"].isNull()) gConfig.curr3_enabled = doc["c3en"];
  if (!doc["c4en"].isNull()) gConfig.curr4_enabled = doc["c4en"];
  if (!doc["c5en"].isNull()) gConfig.curr5_enabled = doc["c5en"];
  if (!doc["c6en"].isNull()) gConfig.curr6_enabled = doc["c6en"];
  int newCnt = (gConfig.curr1_enabled?1:0)+(gConfig.curr2_enabled?1:0)+(gConfig.curr3_enabled?1:0)+(gConfig.curr4_enabled?1:0)+(gConfig.curr5_enabled?1:0)+(gConfig.curr6_enabled?1:0);
  if(oldCnt != newCnt) gNeedsRebuild = true;
  // se trocou par mas cnt igual, força atualização de valores
  if (!doc["c1"].isNull() || !doc["c2"].isNull() || !doc["c3"].isNull() || !doc["c4"].isNull() || !doc["c5"].isNull() || !doc["c6"].isNull()) gNeedsRebuild = true;
  if (!doc["city"].isNull()) strlcpy(gConfig.city, doc["city"], sizeof(gConfig.city));
  if (!doc["lat"].isNull()) gConfig.lat = doc["lat"];
  if (!doc["lon"].isNull()) gConfig.lon = doc["lon"];
  if (!doc["bright"].isNull()) {
    gConfig.brightness = doc["bright"];
    tft.setBrightness(gConfig.brightness);
  }
  if (!doc["tz"].isNull()) {
    gConfig.tz_offset = doc["tz"];
    configTime(gConfig.tz_offset * 3600, 0, "pool.ntp.org", "time.nist.gov");
  }
  if (!doc["dint"].isNull()) gConfig.dolar_interval = doc["dint"];
  if (!doc["wint"].isNull()) gConfig.weather_interval = doc["wint"];
  if (!doc["dlight"].isNull()) {
    bool nl = doc["dlight"].as<bool>() || doc["dlight"].as<int>()==1;
    if(nl != gConfig.display_light){ gConfig.display_light = nl; gNeedsRebuild = true; }
  }
  bool wifiChanged = false;
  if (!doc["ssid"].isNull() && doc["ssid"].as<String>().length()>0) {
    String ns = doc["ssid"]; String np = doc["pass"].isNull() ? "" : doc["pass"].as<String>();
    if (ns != gConfig.wifi_ssid || np.length()>0) {
      ns.toCharArray(gConfig.wifi_ssid, sizeof(gConfig.wifi_ssid));
      if (np.length()>0) np.toCharArray(gConfig.wifi_pass, sizeof(gConfig.wifi_pass));
      wifiChanged = true;
    }
  }
  saveConfig();
  if (wifiChanged) {
    webServer.send(200, "application/json", "{\"msg\":\"WiFi alterado, reconectando...\"}");
    delay(500);
    WiFi.begin(gConfig.wifi_ssid, gConfig.wifi_pass);
  } else {
    webServer.send(200, "application/json", "{\"msg\":\"Salvo! Painel atualizado\"}");
  }
}

void handleGetData() {
  JsonDocument doc;
  struct tm ti;
  char timeStr[16] = "--:--:--";
  char dateStr[32] = "";
  if (getLocalTime(&ti)) {
    snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d", ti.tm_hour, ti.tm_min, ti.tm_sec);
    snprintf(dateStr, sizeof(dateStr), "%02d/%02d/%04d", ti.tm_mday, ti.tm_mon+1, ti.tm_year+1900);
  }
  doc["time"] = timeStr;
  doc["date"] = dateStr;
  doc["dolar"] = dolarValue;
  doc["weatherTemp"] = weatherTemp;
  doc["weatherDesc"] = weatherDesc;
  doc["city"] = weatherCity;
  doc["wifi"] = WiFi.status()==WL_CONNECTED ? "Conectado" : "Desconectado";
  doc["ip"] = WiFi.localIP().toString();
  doc["uptime"] = millis()/1000;
  doc["heap"] = ESP.getFreeHeap();
  doc["bright"] = gConfig.brightness;
  String out; serializeJson(doc, out);
  webServer.send(200, "application/json", out);
}

void handleScan() {
  int n = WiFi.scanNetworks();
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  for(int i=0;i<n;i++){
    JsonObject o = arr.add<JsonObject>();
    o["ssid"] = WiFi.SSID(i);
    o["rssi"] = WiFi.RSSI(i);
    o["encryption"] = WiFi.encryptionType(i)==WIFI_AUTH_OPEN ? "open" : "wpa";
    o["channel"] = WiFi.channel(i);
  }
  String out; serializeJson(arr,out);
  webServer.send(200,"application/json",out);
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
  String out; serializeJson(doc,out);
  webServer.send(200,"application/json",out);
}
void handleOtaCheck() {
  bool has = otaCheck(true);
  JsonDocument doc;
  doc["hasUpdate"] = has;
  doc["latest"] = gOta.latest;
  doc["current"] = gOta.current;
  doc["msg"] = gOta.error;
  doc["state"] = (int)gOta.state;
  String out; serializeJson(doc,out);
  webServer.send(200,"application/json",out);
}
void handleOtaUpdate() {
  // agenda update em task background - nao bloqueia o webserver nem o loop
  otaRequestUpdate();
  webServer.send(200,"application/json","{\"msg\":\"iniciando OTA em background, aguarde...\",\"ok\":true}");
}

void handleRestart() {
  webServer.send(200, "application/json", "{\"msg\":\"reiniciando...\"}");
  delay(500);
  ESP.restart();
}

void handleNotFound() {
  if(isApMode()){
    // Captive portal: redireciona qualquer dominio desconhecido para o portal
    webServer.sendHeader("Location", String("http://") + WiFi.softAPIP().toString() + "/", true);
    webServer.send(302, "text/plain", "");
    Serial.printf("[Web] captive redirect para %s -> /\n", webServer.hostHeader().c_str());
  } else {
    webServer.send(404, "text/plain", "Nao encontrado");
  }
}

bool isApMode(){
  return (WiFi.getMode() & WIFI_MODE_AP) != 0;
}

void webServerInit() {
  // rotas principais
  webServer.on("/", HTTP_GET, handleRoot);
  webServer.on("/api/config", HTTP_GET, handleGetConfig);
  webServer.on("/api/config", HTTP_POST, handlePostConfig);
  webServer.on("/api/data", HTTP_GET, handleGetData);
  webServer.on("/api/scan", HTTP_GET, handleScan);
  webServer.on("/api/version", HTTP_GET, handleVersion);
  webServer.on("/api/ota/check", HTTP_POST, handleOtaCheck);
  webServer.on("/api/ota/update", HTTP_POST, handleOtaUpdate);
  webServer.on("/api/restart", HTTP_POST, handleRestart);
  // Captive portal - URLs que Android/iOS/Windows usam para detectar portal
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

  // Inicia DNS captive portal se estiver em AP
  if(isApMode()){
    IPAddress apIP = WiFi.softAPIP();
    dnsServer.start(53, "*", apIP);
    Serial.printf("[Web] AP mode - DNS captive em %s porta 53\n", apIP.toString().c_str());
    Serial.printf("[Web] Portal disponivel em http://%s/  e http://192.168.4.1/\n", apIP.toString().c_str());
  } else {
    IPAddress ip = WiFi.localIP();
    Serial.printf("[Web] STA mode - http://%s/\n", ip.toString().c_str());
  }
}

void webServerLoop() {
  if(isApMode()){
    dnsServer.processNextRequest();
  }
  webServer.handleClient();
}
