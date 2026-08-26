#include "web_server.h"
#include "app_config.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include <Update.h>
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
<title>SMART DASHBOARD Web Configurator</title>
<style>
:root {
  --bg-main: #0B111E;
  --bg-sidebar: #131D2D;
  --bg-card: #172437;
  --bg-card-hover: #1C2B42;
  --bg-input: #0F1726;
  --border: #22324A;
  --text-main: #F8FAFC;
  --text-muted: #8E9DB2;
  --accent-blue: #38BDF8;
  --accent-btn: #7DD3FC;
  --accent-btn-text: #082F49;
  --accent-green: #22C55E;
  --accent-red: #EF4444;
  --accent-gold: #F59E0B;
}
[data-theme="light"] {
  --bg-main: #F1F5F9;
  --bg-sidebar: #0F172A;
  --bg-card: #FFFFFF;
  --bg-card-hover: #F8FAFC;
  --bg-input: #F1F5F9;
  --border: #CBD5E1;
  --text-main: #0F172A;
  --text-muted: #64748B;
  --accent-btn: #0284C7;
  --accent-btn-text: #FFFFFF;
}
* { box-sizing: border-box; margin: 0; padding: 0; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif; }
body {
  background-color: var(--bg-main);
  color: var(--text-main);
  display: flex;
  min-height: 100vh;
}

/* SIDEBAR */
.sidebar {
  width: 250px;
  background-color: var(--bg-sidebar);
  border-right: 1px solid var(--border);
  display: flex;
  flex-direction: column;
  padding: 20px 0;
  flex-shrink: 0;
}
.brand {
  display: flex;
  align-items: center;
  gap: 12px;
  padding: 0 20px 24px;
  border-bottom: 1px solid var(--border);
}
.brand-logo {
  width: 38px;
  height: 38px;
  background: linear-gradient(135deg, #0284C7, #38BDF8);
  border-radius: 8px;
  display: flex;
  align-items: center;
  justify-content: center;
  font-weight: 900;
  font-size: 18px;
  color: #FFFFFF;
  letter-spacing: -1px;
}
.brand-title {
  font-size: 14px;
  font-weight: 800;
  letter-spacing: 1px;
  line-height: 1.2;
}
.brand-title span { display: block; font-size: 11px; font-weight: 600; color: var(--text-muted); }

.nav-list { list-style: none; padding: 18px 12px; display: flex; flex-direction: column; gap: 6px; }
.nav-item {
  display: flex;
  align-items: center;
  gap: 12px;
  padding: 12px 16px;
  border-radius: 8px;
  color: var(--text-muted);
  cursor: pointer;
  font-weight: 600;
  font-size: 14px;
  transition: all 0.2s;
}
.nav-item:hover { background-color: rgba(255,255,255,0.05); color: var(--text-main); }
.nav-item.active {
  background-color: #24354D;
  color: #38BDF8;
}
.nav-item svg { width: 18px; height: 18px; fill: currentColor; }

/* MAIN CONTENT */
.main-wrapper {
  flex: 1;
  display: flex;
  flex-direction: column;
  height: 100vh;
  overflow-y: auto;
}
.top-header {
  padding: 16px 32px;
  display: flex;
  align-items: center;
  justify-content: space-between;
}
.top-header .subtitle { font-size: 11px; font-weight: 700; color: var(--text-muted); letter-spacing: 1px; }
.top-header .page-title { font-size: 24px; font-weight: 800; margin-top: 2px; }
.user-badge {
  display: flex;
  align-items: center;
  gap: 10px;
  background: var(--bg-card);
  padding: 6px 12px;
  border-radius: 999px;
  border: 1px solid var(--border);
  font-size: 12px;
  font-weight: 700;
}
.user-avatar {
  width: 24px;
  height: 24px;
  border-radius: 50%;
  background: #22C55E;
  display: flex;
  align-items: center;
  justify-content: center;
  color: #fff;
  font-size: 12px;
}

/* CONTENT GRID */
.content-grid {
  padding: 0 32px 40px;
  display: grid;
  grid-template-columns: repeat(12, 1fr);
  gap: 20px;
}

.col-6 { grid-column: span 6; }
.col-12 { grid-column: span 12; }

@media (max-width: 1024px) {
  .col-6 { grid-column: span 12; }
  body { flex-direction: column; }
  .sidebar { width: 100%; height: auto; }
}

.card {
  background-color: var(--bg-card);
  border: 1px solid var(--border);
  border-radius: 12px;
  padding: 20px;
  display: flex;
  flex-direction: column;
  gap: 16px;
}
.card-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
}
.card-title {
  font-size: 15px;
  font-weight: 700;
  color: var(--text-main);
}

.form-group { display: flex; flex-direction: column; gap: 6px; }
.form-label { font-size: 12px; font-weight: 600; color: var(--text-muted); }
.form-input, .form-select {
  background-color: var(--bg-input);
  border: 1px solid var(--border);
  color: var(--text-main);
  padding: 10px 12px;
  border-radius: 8px;
  font-size: 13px;
  outline: none;
  width: 100%;
}
.form-input:focus, .form-select:focus { border-color: var(--accent-blue); }

.row-inputs { display: flex; gap: 12px; }
.row-inputs > div { flex: 1; }

.btn-primary {
  background-color: var(--accent-btn);
  color: var(--accent-btn-text);
  border: none;
  padding: 11px 16px;
  border-radius: 8px;
  font-weight: 700;
  font-size: 13px;
  cursor: pointer;
  text-align: center;
  transition: opacity 0.2s;
}
.btn-primary:hover { opacity: 0.9; }

.btn-small {
  padding: 6px 12px;
  font-size: 12px;
  border-radius: 6px;
}

/* LISTAS / STATUS */
.status-line {
  display: flex;
  font-size: 13px;
  color: var(--text-muted);
  gap: 8px;
}
.status-line b { color: var(--text-main); }
.status-line span.active { color: var(--accent-green); font-weight: 700; }

.wifi-list {
  display: flex;
  flex-direction: column;
  gap: 8px;
  max-height: 140px;
  overflow-y: auto;
}
.wifi-row {
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 8px 12px;
  border-radius: 6px;
  background-color: var(--bg-input);
  font-size: 13px;
  cursor: pointer;
}
.wifi-row:hover { background-color: var(--bg-card-hover); }
.wifi-signal { color: var(--accent-green); font-weight: bold; }

/* TABELA DE MOEDAS */
.currency-table {
  width: 100%;
  border-collapse: collapse;
  font-size: 13px;
}
.currency-table th {
  text-align: left;
  padding: 8px;
  color: var(--text-muted);
  font-size: 11px;
  border-bottom: 1px solid var(--border);
}
.currency-table td {
  padding: 10px 8px;
  border-bottom: 1px solid var(--border);
}
.currency-item {
  display: flex;
  align-items: center;
  gap: 8px;
}
.btn-action {
  background: none;
  border: none;
  cursor: pointer;
  font-size: 15px;
  opacity: 0.7;
}
.btn-action:hover { opacity: 1; }

.progress-bar-bg {
  width: 100%;
  height: 8px;
  background: var(--bg-input);
  border-radius: 4px;
  overflow: hidden;
  display: none;
  margin-top: 6px;
}
.progress-bar-fill {
  height: 100%;
  width: 0%;
  background: linear-gradient(90deg, #38BDF8, #22C55E);
  transition: width 0.3s;
}

.toast {
  position: fixed;
  bottom: 24px;
  right: 24px;
  background-color: #1E293B;
  color: #fff;
  padding: 12px 20px;
  border-radius: 8px;
  border: 1px solid var(--accent-green);
  box-shadow: 0 10px 25px rgba(0,0,0,0.5);
  font-size: 13px;
  font-weight: 600;
  display: none;
  z-index: 100;
}
</style>
</head>
<body>

<!-- SIDEBAR -->
<div class="sidebar">
  <div class="brand">
    <div class="brand-logo">SD</div>
    <div class="brand-title">SMART<span>DASHBOARD</span></div>
  </div>
  <ul class="nav-list">
    <li class="nav-item active" onclick="switchNav('visao')">
      <svg viewBox="0 0 24 24"><path d="M4 13h6c.55 0 1-.45 1-1V4c0-.55-.45-1-1-1H4c-.55 0-1 .45-1 1v8c0 .55.45 1 1 1zm0 8h6c.55 0 1-.45 1-1v-4c0-.55-.45-1-1-1H4c-.55 0-1 .45-1 1v4c0 .55.45 1 1 1zm10 0h6c.55 0 1-.45 1-1v-8c0-.55-.45-1-1-1h-6c-.55 0-1 .45-1 1v8c0 .55.45 1 1 1zm0-18v4c0 .55.45 1 1 1h6c.55 0 1-.45 1-1V4c0-.55-.45-1-1-1h-6c-.55 0-1 .45-1 1z"/></svg>
      Visão Geral
    </li>
    <li class="nav-item" onclick="switchNav('config')">
      <svg viewBox="0 0 24 24"><path d="M19.14 12.94c.04-.3.06-.61.06-.94 0-.32-.02-.64-.07-.94l2.03-1.58c.18-.14.23-.41.12-.61l-1.92-3.32c-.12-.22-.37-.29-.59-.22l-2.39.96c-.5-.38-1.03-.7-1.62-.94l-.36-2.54c-.04-.24-.24-.41-.48-.41h-3.84c-.24 0-.43.17-.47.41l-.36 2.54c-.59.24-1.13.57-1.62.94l-2.39-.96c-.22-.08-.47 0-.59.22L2.74 8.87c-.12.21-.08.47.12.61l2.03 1.58c-.05.3-.09.63-.09.94s.02.64.07.94l-2.03 1.58c-.18.14-.23.41-.12.61l1.92 3.32c.12.22.37.29.59.22l2.39-.96c.5.38 1.03.7 1.62.94l.36 2.54c.05.24.24.41.48.41h3.84c.24 0 .44-.17.47-.41l.36-2.54c.59-.24 1.13-.56 1.62-.94l2.39.96c.22.08.47 0 .59-.22l1.92-3.32c.12-.22.07-.47-.12-.61l-2.01-1.58zM12 15.6c-1.98 0-3.6-1.62-3.6-3.6s1.62-3.6 3.6-3.6 3.6 1.62 3.6 3.6-1.62 3.6-3.6 3.6z"/></svg>
      Configurações
    </li>
    <li class="nav-item" onclick="switchNav('redes')">
      <svg viewBox="0 0 24 24"><path d="M12 4C7.31 4 3.07 5.9 0 8.98L12 21 24 8.98C20.93 5.9 16.69 4 12 4zm0 3.5c3.55 0 6.78 1.41 9.15 3.7L12 19.3 2.85 11.2C5.22 8.91 8.45 7.5 12 7.5z"/></svg>
      Redes
    </li>
    <li class="nav-item" onclick="switchNav('ajuda')">
      <svg viewBox="0 0 24 24"><path d="M12 2C6.48 2 2 6.48 2 12s4.48 10 10 10 10-4.48 10-10S17.52 2 12 2zm1 16h-2v-2h2v2zm1.07-7.75l-.9.92C12.45 11.9 12 12.5 12 14h-2v-.5c0-1.1.45-2.1 1.17-2.83l1.24-1.26c.37-.36.59-.86.59-1.41 0-1.1-.9-2-2-2s-2 .9-2 2H7c0-2.76 2.24-5 5-5s5 2.24 5 5c0 1.04-.42 1.99-1.07 2.75z"/></svg>
      Ajuda
    </li>
  </ul>
</div>

<!-- MAIN WRAPPER -->
<div class="main-wrapper">
  <div class="top-header">
    <div>
      <div class="subtitle">SMART DASHBOARD v2.1</div>
      <div class="page-title">Configuração</div>
    </div>
    <div class="user-badge">
      <div class="user-avatar">✓</div>
      <span id="ipHeader">ESP32 Conectado</span>
    </div>
  </div>

  <div class="content-grid">
    <!-- VISÃO GERAL DO STATUS -->
    <div class="card col-6">
      <div class="card-header">
        <div class="card-title">Visão Geral do Status</div>
      </div>
      <div class="status-line">Status de conexão atual: <span class="active" id="liveWifi">Conexão Ativa</span></div>
      <div class="status-line">Uptime de: <b id="liveUptime">-- mins</b></div>
      <div class="status-line">Firmware versão: <b id="liveVersion">v2.1.2</b></div>

      <div style="margin-top: 10px; display: flex; flex-direction: column; gap: 8px;">
        <button class="btn-primary" onclick="triggerOta()" id="btnOta">🚀 Atualizar Firmware (GitHub OTA)</button>
        <div class="progress-bar-bg" id="otaProgBg"><div class="progress-bar-fill" id="otaProgFill"></div></div>
        <div style="font-size:11px; color:var(--text-muted);" id="otaMsg"></div>
      </div>
    </div>

    <!-- CONFIGURAÇÃO DE CIDADE E CLIMA -->
    <div class="card col-6">
      <div class="card-header">
        <div class="card-title">Configuração de Cidade e Clima</div>
      </div>
      <div class="row-inputs">
        <div class="form-group">
          <label class="form-label">Cidade</label>
          <input type="text" id="city" class="form-input" placeholder="Ex: Lavras, MG">
        </div>
        <div class="form-group">
          <label class="form-label">Região</label>
          <select id="uf" class="form-select">
            <option value="MG">Minas Gerais (MG)</option>
            <option value="SP">São Paulo (SP)</option>
            <option value="RJ">Rio de Janeiro (RJ)</option>
            <option value="PR">Paraná (PR)</option>
            <option value="SC">Santa Catarina (SC)</option>
            <option value="RS">Rio Grande do Sul (RS)</option>
            <option value="DF">Distrito Federal (DF)</option>
            <option value="BA">Bahia (BA)</option>
            <option value="GO">Goiás (GO)</option>
            <option value="PE">Pernambuco (PE)</option>
          </select>
        </div>
      </div>
      <button class="btn-primary" onclick="saveLocation()">Salvar Localização</button>
    </div>

    <!-- CONFIGURAÇÃO DE REDES (WIFI) -->
    <div class="card col-6">
      <div class="card-header">
        <div class="card-title">Configuração de Redes (WiFi)</div>
        <button class="btn-primary btn-small" onclick="scanWifi()">Buscar</button>
      </div>
      <div class="wifi-list" id="wifiList">
        <div class="wifi-row" onclick="selectWifi('Casa_Net')">
          <span>Rede Atual: <b>Casa_Net</b></span>
          <span class="wifi-signal">📶</span>
        </div>
      </div>
      <div class="form-group">
        <label class="form-label">SSID</label>
        <input type="text" id="ssid" class="form-input" placeholder="Nome da Rede">
      </div>
      <div class="form-group">
        <label class="form-label">Senha</label>
        <input type="password" id="pass" class="form-input" placeholder="Senha do Wi-Fi">
      </div>
      <button class="btn-primary" onclick="saveWifi()">Salvar Configuração</button>
    </div>

    <!-- CONFIGURAÇÃO DE COTAÇÃO DE MOEDAS -->
    <div class="card col-6">
      <div class="card-header">
        <div class="card-title">Configuração de Cotação de Moedas</div>
        <button class="btn-primary btn-small" onclick="addCurrencyPrompt()">Adicionar Moeda</button>
      </div>
      <table class="currency-table">
        <thead>
          <tr>
            <th>Código 1</th>
            <th>Código 2</th>
            <th>Exibir Nome</th>
            <th>Taxa/Fonte</th>
            <th>Ações</th>
          </tr>
        </thead>
        <tbody id="currencyBody">
        </tbody>
      </table>
      <div class="form-group" style="margin-top: 10px;">
        <label class="form-label">API Key / Fonte de Dados</label>
        <input type="text" class="form-input" value="AwesomeAPI (Dados em Tempo Real)" readonly>
      </div>
    </div>

    <!-- AJUSTES DE EXIBIÇÃO & UPLOAD MANUAL -->
    <div class="card col-12">
      <div class="card-header">
        <div class="card-title">Ajustes de Exibição & Upload Local</div>
      </div>
      <div class="row-inputs">
        <div class="form-group" style="flex: 2;">
          <label class="form-label">Brilho do Display: <span id="brightVal">180</span></label>
          <input type="range" min="10" max="255" id="bright" style="width: 100%; margin-top: 8px;" oninput="updateBright(this.value)">
        </div>
        <div class="form-group" style="flex: 1;">
          <label class="form-label">Tema</label>
          <button class="btn-primary" onclick="toggleTheme()" id="themeBtn">🌙 Modo Escuro</button>
        </div>
        <div class="form-group" style="flex: 2;">
          <label class="form-label">Upload Direto de firmware.bin (Sem Internet)</label>
          <div style="display:flex;gap:6px;margin-top:4px;">
            <input type="file" id="binFile" accept=".bin" class="form-input" style="padding:4px;">
            <button class="btn-primary btn-small" onclick="uploadLocalBin()">Enviar</button>
          </div>
        </div>
      </div>
    </div>
  </div>
</div>

<div class="toast" id="toast">Configuração salva com sucesso!</div>

<script>
let state = {};
let currencies = [
  { c1: 'USD-BRL', c2: 'USD', name: 'Dólar', rate: 'R$ 4,92', flag: '🇺🇸' },
  { c1: 'EUR-BRL', c2: 'EUR', name: 'Euro', rate: 'R$ 5,21', flag: '🇪🇺' },
  { c1: 'BTC-BRL', c2: 'BTC/BRL', name: 'Bitcoin', rate: 'R$ 171.450', flag: '₿' }
];

function toast(msg) {
  let t = document.getElementById('toast');
  t.textContent = msg;
  t.style.display = 'block';
  setTimeout(() => t.style.display = 'none', 3000);
}

function switchNav(tab) {
  document.querySelectorAll('.nav-item').forEach(el => el.classList.remove('active'));
  event.currentTarget.classList.add('active');
  if (tab === 'ajuda' || tab === 'config') {
    toast('Navegando para ' + tab);
  }
}

function renderCurrencies() {
  let tbody = document.getElementById('currencyBody');
  tbody.innerHTML = currencies.map((c, i) => `
    <tr>
      <td><div class="currency-item"><span>${c.flag||'💰'}</span> <b>${c.c1}</b></div></td>
      <td>${c.c2}</td>
      <td><input type="text" class="form-input" style="padding:4px 8px;font-size:12px;" value="${c.name}" onchange="currencies[${i}].name=this.value"></td>
      <td><span style="color:var(--accent-blue);font-weight:700;">${c.rate}</span></td>
      <td>
        <button class="btn-action" onclick="editCurrency(${i})">✏️</button>
        <button class="btn-action" onclick="deleteCurrency(${i})">🗑️</button>
      </td>
    </tr>
  `).join('');
}

function addCurrencyPrompt() {
  let pair = prompt('Digite o par da moeda (ex: ETH-BRL, GBP-BRL, CAD-BRL):');
  if (pair && pair.includes('-')) {
    currencies.push({ c1: pair.toUpperCase(), c2: pair.split('-')[0].toUpperCase(), name: pair.split('-')[0], rate: 'R$ --', flag: '💰' });
    renderCurrencies();
    saveCurrencies();
  }
}

function editCurrency(i) {
  let pair = prompt('Editar par de moedas:', currencies[i].c1);
  if (pair) {
    currencies[i].c1 = pair.toUpperCase();
    currencies[i].c2 = pair.split('-')[0].toUpperCase();
    renderCurrencies();
    saveCurrencies();
  }
}

function deleteCurrency(i) {
  if (confirm('Remover ' + currencies[i].c1 + '?')) {
    currencies.splice(i, 1);
    renderCurrencies();
    saveCurrencies();
  }
}

async function saveCurrencies() {
  let body = {};
  for (let i = 1; i <= 6; i++) {
    if (i <= currencies.length) {
      body['c' + i] = currencies[i - 1].c1;
      body['c' + i + 'en'] = true;
    } else {
      body['c' + i] = '';
      body['c' + i + 'en'] = false;
    }
  }
  await fetch('/api/config', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body) });
  toast('Moedas atualizadas!');
}

async function saveLocation() {
  let city = document.getElementById('city').value;
  let r = await fetch(`https://nominatim.openstreetmap.org/search?format=json&q=${encodeURIComponent(city + ', Brasil')}`);
  let j = await r.json();
  let lat = -21.2461, lon = -44.9992;
  if (j && j.length > 0) {
    lat = parseFloat(j[0].lat);
    lon = parseFloat(j[0].lon);
  }
  await fetch('/api/config', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ city, lat, lon }) });
  toast('Localização salva com sucesso!');
}

async function saveWifi() {
  let ssid = document.getElementById('ssid').value;
  let pass = document.getElementById('pass').value;
  await fetch('/api/config', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ ssid, pass }) });
  toast('Wi-Fi salvo! O ESP32 irá conectar.');
}

async function scanWifi() {
  let list = document.getElementById('wifiList');
  list.innerHTML = '<div style="padding:10px;font-size:12px;color:var(--text-muted);">Buscando redes...</div>';
  try {
    let r = await fetch('/api/scan');
    let j = await r.json();
    list.innerHTML = j.map(n => `
      <div class="wifi-row" onclick="selectWifi('${n.ssid}')">
        <span>${n.ssid || '(Oculta)'}</span>
        <span class="wifi-signal">📶 ${n.rssi}dBm</span>
      </div>
    `).join('');
  } catch (e) {
    list.innerHTML = '<div style="padding:10px;font-size:12px;color:var(--text-muted);">Erro ao escanear.</div>';
  }
}

function selectWifi(ssid) {
  document.getElementById('ssid').value = ssid;
  document.getElementById('pass').focus();
  toast('Rede ' + ssid + ' selecionada');
}

function updateBright(v) {
  document.getElementById('brightVal').textContent = v;
  fetch('/api/config', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ bright: parseInt(v) }) });
}

function toggleTheme() {
  let cur = document.documentElement.getAttribute('data-theme');
  let next = cur === 'light' ? 'dark' : 'light';
  if (next === 'light') document.documentElement.setAttribute('data-theme', 'light');
  else document.documentElement.removeAttribute('data-theme');
  document.getElementById('themeBtn').textContent = next === 'light' ? '☀️ Modo Claro' : '🌙 Modo Escuro';
}

async function triggerOta() {
  let btn = document.getElementById('btnOta');
  let msg = document.getElementById('otaMsg');
  let pBg = document.getElementById('otaProgBg');
  let pFill = document.getElementById('otaProgFill');
  btn.disabled = true;
  btn.textContent = 'Verificando GitHub...';
  msg.textContent = 'Consultando últimas releases...';
  pBg.style.display = 'block';

  try {
    let r = await fetch('/api/ota/check', { method: 'POST' });
    let j = await r.json();
    if (j.hasUpdate) {
      msg.textContent = 'Baixando e gravando versão ' + j.latest + '...';
      await fetch('/api/ota/update', { method: 'POST' });
      let iv = setInterval(async () => {
        let vr = await fetch('/api/version');
        let vj = await vr.json();
        pFill.style.width = (vj.progress || 0) + '%';
        msg.textContent = `Progresso: ${vj.progress}% - ${vj.error || 'Gravando flash'}`;
        if (vj.state === 3 || vj.progress === 100) {
          clearInterval(iv);
          msg.textContent = 'Atualizado com sucesso! Reiniciando em instantes...';
          setTimeout(() => location.reload(), 6000);
        }
      }, 1000);
    } else {
      msg.textContent = j.msg || 'O ESP32 já está na versão mais recente!';
      btn.disabled = false;
      btn.textContent = '🚀 Atualizar Firmware (GitHub OTA)';
    }
  } catch (e) {
    msg.textContent = 'Erro ao consultar OTA.';
    btn.disabled = false;
    btn.textContent = '🚀 Atualizar Firmware (GitHub OTA)';
  }
}

async function uploadLocalBin() {
  let fileInput = document.getElementById('binFile');
  if (!fileInput.files.length) {
    toast('Selecione um arquivo .bin primeiro!');
    return;
  }
  let file = fileInput.files[0];
  let formData = new FormData();
  formData.append('firmware', file);
  toast('Enviando firmware localmente...');
  try {
    let r = await fetch('/api/ota/upload', { method: 'POST', body: formData });
    let j = await r.json();
    toast(j.msg || 'Upload concluído! Reiniciando...');
    setTimeout(() => location.reload(), 5000);
  } catch (e) {
    toast('Erro no upload local.');
  }
}

async function loadData() {
  try {
    let r = await fetch('/api/data');
    let j = await r.json();
    document.getElementById('liveWifi').textContent = j.wifi === 'Conectado' ? 'Conexão Ativa (' + j.ip + ')' : 'Modo AP';
    document.getElementById('liveUptime').textContent = Math.floor(j.uptime / 60) + ' mins';
    document.getElementById('ipHeader').textContent = 'IP: ' + j.ip;
  } catch (e) {}
}

async function loadConfig() {
  try {
    let r = await fetch('/api/config');
    let j = await r.json();
    document.getElementById('city').value = j.city || 'Lavras, MG';
    document.getElementById('ssid').value = j.ssid || '';
    document.getElementById('bright').value = j.bright || 180;
    document.getElementById('brightVal').textContent = j.bright || 180;

    currencies = [];
    let flags = { 'USD-BRL': '🇺🇸', 'EUR-BRL': '🇪🇺', 'BTC-BRL': '₿', 'ETH-BRL': 'Ξ' };
    let names = { 'USD-BRL': 'Dólar', 'EUR-BRL': 'Euro', 'BTC-BRL': 'Bitcoin', 'ETH-BRL': 'Ethereum' };
    for (let i = 1; i <= 6; i++) {
      if (j['c' + i] && j['c' + i + 'en']) {
        let pair = j['c' + i];
        currencies.push({
          c1: pair,
          c2: pair.split('-')[0],
          name: names[pair] || pair.split('-')[0],
          rate: i === 1 ? (j.dolar || 'R$ --') : 'R$ --',
          flag: flags[pair] || '💰'
        });
      }
    }
    if (currencies.length === 0) {
      currencies = [
        { c1: 'USD-BRL', c2: 'USD', name: 'Dólar', rate: 'R$ 4,92', flag: '🇺🇸' },
        { c1: 'EUR-BRL', c2: 'EUR', name: 'Euro', rate: 'R$ 5,21', flag: '🇪🇺' },
        { c1: 'BTC-BRL', c2: 'BTC/BRL', name: 'Bitcoin', rate: 'R$ 171.450', flag: '₿' }
      ];
    }
    renderCurrencies();
  } catch (e) {}
}

renderCurrencies();
loadConfig();
loadData();
setInterval(loadData, 5000);
</script>
</body>
</html>
)rawliteral";

void handleRoot() {
  Serial.printf("[Web] GET %s (page %d bytes)\n", webServer.uri().c_str(), (int)strlen_P(HTML_PAGE));
  webServer.send_P(200, "text/html; charset=UTF-8", HTML_PAGE);
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
  webServer.send(200, "application/json; charset=UTF-8", out);
}

void handlePostConfig() {
  Serial.printf("[Web] POST /api/config args=%d hasPlain=%d body=%dB uri='%s'\n",
                webServer.args(), webServer.hasArg("plain")?1:0,
                (int)webServer.arg("plain").length(), webServer.uri().c_str());
  if (!webServer.hasArg("plain")) {
    webServer.send(400, "application/json; charset=UTF-8", "{\"msg\":\"sem body\"}");
    return;
  }
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, webServer.arg("plain"));
  if (err) {
    webServer.send(400, "application/json; charset=UTF-8", "{\"msg\":\"JSON invalido\"}");
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
    webServer.send(200, "application/json; charset=UTF-8", "{\"msg\":\"WiFi alterado, reconectando...\"}");
    delay(500);
    WiFi.begin(gConfig.wifi_ssid, gConfig.wifi_pass);
  } else {
    webServer.send(200, "application/json; charset=UTF-8", "{\"msg\":\"Salvo com sucesso! Painel atualizado\"}");
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
  webServer.send(200, "application/json; charset=UTF-8", out);
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
  webServer.send(200, "application/json; charset=UTF-8", out);
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
  webServer.send(200, "application/json; charset=UTF-8", out);
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
  webServer.send(200, "application/json; charset=UTF-8", out);
}

void handleOtaUpdate() {
  otaRequestUpdate();
  webServer.send(200, "application/json; charset=UTF-8", "{\"msg\":\"iniciando OTA em background, aguarde...\",\"ok\":true}");
}

void handleRestart() {
  Serial.println("[Web] POST /api/restart -> reiniciando");
  webServer.send(200, "application/json; charset=UTF-8", "{\"msg\":\"reiniciando...\"}");
  delay(500);
  ESP.restart();
}

void handleNotFound() {
  if (isApMode()) {
    webServer.sendHeader("Location", String("http://") + WiFi.softAPIP().toString() + "/", true);
    webServer.send(302, "text/plain; charset=UTF-8", "");
    Serial.printf("[Web] captive redirect para %s -> /\n", webServer.hostHeader().c_str());
  } else {
    webServer.send(404, "text/plain; charset=UTF-8", "Não encontrado");
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

  // Upload direto de firmware .bin via navegador
  webServer.on("/api/ota/upload", HTTP_POST, [](){
    webServer.send(200, "application/json; charset=UTF-8", "{\"msg\":\"Upload concluido! Reiniciando...\"}");
    delay(800);
    ESP.restart();
  }, [](){
    HTTPUpload& upload = webServer.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.printf("[OTA-Web] Iniciando upload: %s\n", upload.filename.c_str());
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) {
        Serial.printf("[OTA-Web] Sucesso: %u bytes gravados\n", upload.totalSize);
      } else {
        Update.printError(Serial);
      }
    }
  });

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
