// Smart Classroom dashboard - reads the NETPIE shadow (live), feed (history)
// and sends commands to the gateway over the NETPIE REST API.
// Credentials are kept in this browser only (localStorage or config.local.js).
'use strict';

const API = 'https://api.netpie.io/v2/device';
const FEED = 'https://feed.netpie.io/api/v1/datapoints/query';
const POLL_MS = 5000;
const STALE_S = 45;          // gateway publishes every 15 s
const PM_BAD = 37.5;         // same thresholds as gateway/include/config.h
const CO2_WARN = 1000;
const CFG_KEY = 'netpie_cfg';

const METRICS = {
  temp:  { unit: '°C',    series: [['in_temp', 'ในห้อง'], ['out_temp', 'นอกห้อง']] },
  co2:   { unit: 'ppm',   series: [['co2', 'CO₂']] },
  pm:    { unit: 'µg/m³', series: [['out_pm25', 'PM2.5'], ['out_pm10', 'PM10']] },
  smoke: { unit: 'raw',   series: [['out_smoke', 'MQ-2']] },
};
const SRC_TH = { button: 'ปุ่ม', dashboard: 'เว็บ', ai: 'AI', rule: 'อัตโนมัติ', auto: 'อัตโนมัติ', estop: 'E-stop', safety: 'ความปลอดภัย', none: '–' };

const $ = (id) => document.getElementById(id);
let cfg = loadCfg();
let last = null;
let chart = null;
let metric = 'temp';
let rangeH = 1;
let pollTimer = null;
let histTimer = null;

// ---------- settings ----------
function loadCfg() {
  if (window.NETPIE_CONFIG && window.NETPIE_CONFIG.clientId) return window.NETPIE_CONFIG;
  try { return JSON.parse(localStorage.getItem(CFG_KEY)) || null; } catch { return null; }
}
function saveCfg(c) {
  cfg = c;
  try { localStorage.setItem(CFG_KEY, JSON.stringify(c)); } catch { /* private mode */ }
}
function authHeader() { return `Device ${cfg.clientId}:${cfg.token}`; }

function openSettings() {
  const f = $('cfg-form');
  f.clientId.value = cfg?.clientId || '';
  f.token.value = cfg?.token || '';
  $('dlg').showModal();
}
$('btn-settings').onclick = openSettings;
$('dlg').addEventListener('close', () => {
  if ($('dlg').returnValue !== 'save') return;
  const f = $('cfg-form');
  saveCfg({ clientId: f.clientId.value.trim(), token: f.token.value.trim() });
  start();
});

// ---------- helpers ----------
const num = (v, d = 1) => (typeof v === 'number' && isFinite(v) ? v.toFixed(d) : '–');
const has = (v) => typeof v === 'number' && isFinite(v);
function setText(id, t) { $(id).textContent = t; }
function tileState(id, cls, hint, hintId) {
  const el = $(id);
  el.classList.remove('good', 'warn', 'bad');
  if (cls) el.classList.add(cls);
  if (hintId) setText(hintId, hint || '');
}
function toast(msg, ms = 2500) {
  const t = $('toast');
  t.textContent = msg; t.hidden = false;
  clearTimeout(toast._t); toast._t = setTimeout(() => (t.hidden = true), ms);
}
function fmtAge(s) {
  if (!has(s)) return '–';
  if (s < 60) return `${Math.round(s)} วิ`;
  if (s < 3600) return `${Math.round(s / 60)} นาที`;
  return `${(s / 3600).toFixed(1)} ชม.`;
}
function fmtUptime(s) {
  if (!has(s)) return '–';
  const d = Math.floor(s / 86400), h = Math.floor((s % 86400) / 3600), m = Math.floor((s % 3600) / 60);
  return (d ? `${d}d ` : '') + `${h}h ${m}m`;
}
function setStatus(text, cls) { const s = $('status'); s.textContent = text; s.className = `pill pill-${cls}`; }

// ---------- live values ----------
async function poll() {
  try {
    const r = await fetch(`${API}/shadow/data`, { headers: { Authorization: authHeader() } });
    if (r.status === 401 || r.status === 403) { setStatus('Key ไม่ถูกต้อง', 'bad'); return; }
    if (!r.ok) throw new Error(`HTTP ${r.status}`);
    const j = await r.json();
    last = j.data || {};
    render(last);
  } catch (e) {
    setStatus('ติดต่อ NETPIE ไม่ได้', 'bad');
    console.warn('shadow', e);
  }
}

function render(d) {
  const age = has(d.ts) ? Date.now() / 1000 - d.ts : NaN;
  const stale = !has(age) || age > STALE_S;
  setStatus(stale ? `gateway เงียบ ${fmtAge(age)}` : 'ออนไลน์', stale ? 'warn' : 'good');
  setText('updated', has(d.ts) ? `อัปเดต ${new Date(d.ts * 1000).toLocaleTimeString('th-TH')}` : '');

  // indoor
  setText('in_temp', num(d.in_temp));
  setText('in_rh', num(d.in_rh, 0));
  setText('co2', has(d.co2) ? d.co2.toFixed(0) : '–');
  if (has(d.co2)) {
    tileState('tile-co2', d.co2 > CO2_WARN ? 'bad' : d.co2 > 800 ? 'warn' : 'good',
      d.co2 > CO2_WARN ? 'อากาศอับ ควรระบาย' : d.co2 > 800 ? 'เริ่มอับ' : 'ดี', 'co2_hint');
  } else tileState('tile-co2', null, 'ไม่มีเซ็นเซอร์', 'co2_hint');
  setText('lux', has(d.lux) ? d.lux.toFixed(0) : '–');
  setText('occupied', has(d.occupied) ? (d.occupied ? 'มีคน' : 'ว่าง') : '–');

  // outdoor
  const nodes = d.nodes || {};
  const names = Object.keys(nodes);
  setText('node_name', names.length ? `(${names.join(', ')})` : '');
  setText('out_pm25', num(d.out_pm25, 0));
  if (has(d.out_pm25)) {
    const p = d.out_pm25;
    tileState('tile-pm', p > 75 ? 'bad' : p > PM_BAD ? 'warn' : 'good',
      p > 75 ? 'มีผลต่อสุขภาพ' : p > PM_BAD ? 'เริ่มมีผล ปิดหน้าต่าง' : 'ดี', 'pm_hint');
  } else tileState('tile-pm', null, 'ไม่มีข้อมูล', 'pm_hint');
  setText('out_temp', num(d.out_temp));
  setText('out_rh', num(d.out_rh, 0));
  setText('raining', has(d.raining) ? (d.raining ? 'ฝนตก' : 'ไม่มีฝน') : '–');
  tileState('tile-rain', d.raining ? 'warn' : null);
  setText('out_smoke', has(d.out_smoke) ? d.out_smoke.toFixed(0) : '–');
  tileState('tile-smoke', d.smoky ? 'bad' : has(d.out_smoke) ? 'good' : null,
    d.smoky ? 'ตรวจพบควัน' : has(d.out_smoke) ? 'ปกติ' : '', 'smoke_hint');

  // AC
  const acOn = !!d.ac_on;
  const acEl = $('ac_state');
  acEl.textContent = acOn ? `เปิด ${has(d.fan_pct) ? d.fan_pct + '%' : ''}` : 'ปิด';
  acEl.className = 'state' + (acOn ? ' on' : '');
  $('fan_bar').style.width = `${acOn && has(d.fan_pct) ? d.fan_pct : 0}%`;
  setText('ac_meta', `สั่งโดย: ${SRC_TH[d.ac_src] || d.ac_src || '–'}` +
    (d.ac_manual ? ' · โหมดสั่งเอง (30 นาที)' : '') +
    (has(d.ac_presses) ? ` · กดปุ่มแล้ว ${d.ac_presses} ครั้ง` : ''));
  markSeg('ac', d.ac_manual ? (acOn ? 'on' : 'off') : 'auto');

  // window
  const wEl = $('win_state');
  if (d.window_fault) { wEl.textContent = 'ติดขัด'; wEl.className = 'state bad'; }
  else { wEl.textContent = d.window_open ? 'เปิด' : 'ปิด'; wEl.className = 'state' + (d.window_open ? ' on' : ''); }
  $('win_bar').style.width = `${has(d.window_angle) ? Math.min(100, d.window_angle / 90 * 100) : 0}%`;
  setText('win_meta', `สั่งโดย: ${SRC_TH[d.window_src] || d.window_src || '–'}` +
    (has(d.window_angle) ? ` · มุม ${d.window_angle}°` : ''));
  markSeg('window', d.window_src === 'button' || d.window_src === 'dashboard' ? (d.window_open ? 'open' : 'close') : 'auto');

  // AI
  const p = has(d.ai_prob) ? d.ai_prob : 0;
  $('ai_bar').style.width = `${p * 100}%`;
  const aiEl = $('ai_state');
  aiEl.textContent = d.ai_valid ? `${(p * 100).toFixed(0)}%` : 'ยังไม่พร้อม';
  aiEl.className = 'state' + (d.ai_valid && p >= 0.7 ? ' on' : '');
  setText('ai_meta', d.ai_valid
    ? (p >= 0.7 ? 'คาดว่าจะมีคนเปิดแอร์ในอีกไม่กี่นาที เปิดล่วงหน้า' : 'โอกาสต่ำกว่าเกณฑ์ 70%')
    : 'ข้อมูลเซ็นเซอร์ในห้องยังไม่ครบ');
  $('btn-estop').hidden = !(d.estop || d.window_fault);

  renderAlerts(d, stale);
  renderNodes(nodes, d);
}

function markSeg(target, v) {
  document.querySelectorAll(`.seg[data-target="${target}"] button`).forEach((b) =>
    b.classList.toggle('on', b.dataset.v === v));
}

function renderAlerts(d, stale) {
  const a = [];
  if (d.estop) a.push(['bad', 'Emergency Stop ทำงานอยู่ พัดลมหยุด หน้าต่างล็อก']);
  if (d.window_fault) a.push(['bad', 'หน้าต่างติดขัด (เกิน 6 วินาที) ตรวจสอบแล้วกดรีเซ็ต']);
  if (d.smoky) a.push(['bad', 'ตรวจพบควันนอกห้อง ปิดหน้าต่างแล้ว']);
  if (d.raining) a.push(['warn', 'ฝนตก ปิดหน้าต่าง']);
  if (has(d.out_pm25) && d.out_pm25 > PM_BAD) a.push(['warn', `PM2.5 นอกห้อง ${d.out_pm25.toFixed(0)} µg/m³ ปิดหน้าต่าง`]);
  if (has(d.nodes_online) && d.nodes_online === 0) a.push(['warn', 'ไม่มี sensor node ออนไลน์']);
  if (stale) a.push(['warn', 'ไม่ได้รับข้อมูลใหม่จาก gateway']);
  $('alerts').innerHTML = '';
  for (const [cls, msg] of a) {
    const div = document.createElement('div');
    div.className = `alert ${cls}`; div.textContent = msg;
    $('alerts').appendChild(div);
  }
}

function renderNodes(nodes, d) {
  const tb = $('nodes');
  tb.innerHTML = '';
  const names = Object.keys(nodes);
  if (!names.length) {
    tb.innerHTML = '<tr><td colspan="7" class="muted">ยังไม่มี node</td></tr>';
  }
  for (const n of names) {
    const x = nodes[n];
    const online = has(x.age_s) && x.age_s < 120;
    const total = (x.rx || 0) + (x.gaps || 0);
    const pdr = total ? ((x.rx / total) * 100).toFixed(1) + '%' : '–';
    const tr = document.createElement('tr');
    const cells = [
      n + (x.sim ? ' (จำลอง)' : ''),
      null,
      fmtAge(x.age_s), x.rx ?? '–', x.gaps ?? '–', x.dup ?? '–', pdr,
    ];
    cells.forEach((c, i) => {
      const td = document.createElement('td');
      if (i === 1) {
        const dot = document.createElement('span');
        dot.className = 'dot ' + (online ? 'on' : 'off');
        td.append(dot, online ? 'ออนไลน์' : 'ออฟไลน์');
      } else {
        td.textContent = c;
        if (i >= 2) td.className = 'num';
      }
      tr.appendChild(td);
    });
    tb.appendChild(tr);
  }
  setText('sys', `uptime ${fmtUptime(d.uptime_s)} · boots ${d.boots ?? '–'} · heap ${d.heap ?? '–'} B · buffered ${d.buffered ?? '–'}`);
}

// ---------- commands ----------
async function sendCmd(obj, btns) {
  btns.forEach((b) => (b.disabled = true));
  try {
    const r = await fetch(`${API}/message?topic=cmd`, {
      method: 'PUT',
      headers: { Authorization: authHeader(), 'Content-Type': 'text/plain' },
      body: JSON.stringify(obj),
    });
    if (!r.ok) throw new Error(`HTTP ${r.status}`);
    toast('ส่งคำสั่งแล้ว รอ gateway อัปเดต');
    setTimeout(poll, 1500);
    setTimeout(poll, 4000);
  } catch (e) {
    toast('ส่งคำสั่งไม่สำเร็จ: ' + e.message, 4000);
  } finally {
    btns.forEach((b) => (b.disabled = false));
  }
}
document.querySelectorAll('.seg[data-target]').forEach((seg) => {
  const btns = [...seg.querySelectorAll('button')];
  btns.forEach((b) => (b.onclick = () => sendCmd({ [seg.dataset.target]: b.dataset.v }, btns)));
});
$('btn-estop').onclick = () => sendCmd({ estop: 'reset' }, [$('btn-estop')]);

// ---------- history ----------
function css(name) { return getComputedStyle(document.documentElement).getPropertyValue(name).trim(); }

async function loadHistory() {
  const m = METRICS[metric];
  const attrs = m.series.map((s) => s[0]);
  setText('chart_note', 'กำลังโหลด…');
  try {
    const r = await fetch(FEED, {
      method: 'POST',
      headers: { Authorization: authHeader(), 'Content-Type': 'application/json' },
      body: JSON.stringify({
        start_relative: { value: rangeH, unit: 'hours' },
        metrics: [{
          name: cfg.clientId,
          tags: { attr: attrs },
          limit: 5000,
          group_by: [{ name: 'tag', tags: ['attr'] }],
        }],
      }),
    });
    if (!r.ok) throw new Error(`HTTP ${r.status}`);
    const j = await r.json();
    const byAttr = {};
    for (const res of j.queries?.[0]?.results || []) {
      const g = res.group_by?.find((x) => x.name === 'tag');
      const attr = g?.group?.attr || res.tags?.attr?.[0];
      if (attr) byAttr[attr] = res.values || [];
    }
    drawChart(m, byAttr);
    const n = Object.values(byAttr).reduce((s, v) => s + v.length, 0);
    setText('chart_note', n ? `${n} จุด จาก NETPIE Feed` :
      'ยังไม่มีข้อมูลใน Feed: เปิด Feed ของ device ใน NETPIE portal และเพิ่ม field ใน Schema (ดู docs/cloud_setup.md)');
  } catch (e) {
    drawChart(m, {});
    setText('chart_note', 'โหลดประวัติไม่ได้: ' + e.message);
  }
}

function drawChart(m, byAttr) {
  const colors = [css('--accent'), css('--warn')];
  const datasets = m.series.map(([attr, label], i) => ({
    label,
    data: (byAttr[attr] || []).map(([t, v]) => ({ x: t, y: v })),
    borderColor: colors[i], backgroundColor: colors[i],
    borderWidth: 2, pointRadius: 0, tension: 0.25,
  }));
  const grid = css('--grid'), muted = css('--muted');
  const opts = {
    responsive: true, maintainAspectRatio: false, animation: false, parsing: false,
    interaction: { mode: 'index', intersect: false },
    scales: {
      x: {
        type: 'linear', min: Date.now() - rangeH * 3600e3, max: Date.now(),
        grid: { color: grid }, ticks: { color: muted, maxTicksLimit: 7,
          callback: (v) => new Date(v).toLocaleTimeString('th-TH', { hour: '2-digit', minute: '2-digit' }) },
      },
      y: { grid: { color: grid }, ticks: { color: muted }, title: { display: true, text: m.unit, color: muted } },
    },
    plugins: {
      legend: { display: datasets.length > 1, labels: { color: muted, boxWidth: 12 } },
      tooltip: { callbacks: { title: (it) => new Date(it[0].parsed.x).toLocaleString('th-TH') } },
    },
  };
  if (chart) chart.destroy();
  chart = new Chart($('chart'), { type: 'line', data: { datasets }, options: opts });
}

function segPick(id, key, fn) {
  const btns = [...document.querySelectorAll(`#${id} button`)];
  btns.forEach((b) => (b.onclick = () => {
    btns.forEach((x) => x.classList.toggle('on', x === b));
    fn(b.dataset[key]);
    loadHistory();
  }));
}
segPick('metric-seg', 'm', (v) => (metric = v));
segPick('range-seg', 'r', (v) => (rangeH = Number(v)));

// ---------- start ----------
function start() {
  clearInterval(pollTimer);
  clearInterval(histTimer);
  if (!cfg || !cfg.clientId || !cfg.token) { setStatus('ยังไม่ได้ตั้งค่า', 'muted'); openSettings(); return; }
  poll();
  pollTimer = setInterval(poll, POLL_MS);
  loadHistory();
  histTimer = setInterval(loadHistory, 60000);
}
start();
