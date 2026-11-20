#pragma once
#include <Arduino.h>

static const char WEB_PAGE[] PROGMEM = R"WEB(<!doctype html><html lang="th"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><meta name="color-scheme" content="dark"><title>ExamBarier · สภาพแวดล้อมห้องสอบ</title><style>
:root{
  --bg:#111413;--surface:#191d1b;--surface-2:#202522;--line:#303733;
  --text:#efeee8;--muted:#a7aaa1;--accent:#a9b69b;
  --good:#a9b69b;--warn:#d2b77e;--bad:#d98f82;
}
*{box-sizing:border-box}
html,body{margin:0}
body{
  background:radial-gradient(900px 460px at 12% -8%,rgba(149,158,135,.09),transparent 65%),linear-gradient(90deg,rgba(197,202,190,.035) 1px,transparent 1px),linear-gradient(rgba(197,202,190,.035) 1px,transparent 1px),var(--bg);
  background-size:auto,38px 38px,38px 38px,auto;
  color:var(--text);font:15px/1.55 system-ui,"Noto Sans Thai",sans-serif;
  -webkit-font-smoothing:antialiased;
}
.page{max-width:1360px;margin:0 auto;padding:24px 28px 44px}
.muted{color:var(--muted)}
.small{font-size:12.5px}
.mono,.field-id,code,#updated,.system dd,.tooltip strong,.tooltip span{font-family:ui-monospace,SFMono-Regular,Menlo,Monaco,Consolas,monospace;font-variant-numeric:tabular-nums}
.sr-only{position:absolute;width:1px;height:1px;overflow:hidden;clip:rect(0 0 0 0)}
button{font:inherit;cursor:pointer}
button:focus-visible,select:focus-visible{outline:2px solid rgba(95,227,189,.5);outline-offset:2px}
.top{display:flex;justify-content:space-between;align-items:center;gap:18px;flex-wrap:wrap;padding-bottom:16px;border-bottom:1px solid rgba(167,170,161,.16)}
.eyebrow{font-size:10.5px;font-weight:700;letter-spacing:.2em;color:var(--accent)}
.brand h1{font-size:25px;line-height:1.2;margin:4px 0 0;letter-spacing:-.025em}
.brand .sub{margin:5px 0 0;color:var(--muted);font-size:12.5px}
.brand .sub .mono{color:#d0c8b4;font-size:11px;letter-spacing:.04em}
.controls{display:flex;max-width:100%;min-width:0;align-items:center;gap:10px;flex-wrap:wrap}
.badge{display:inline-flex;align-items:center;gap:8px;border:1px solid var(--line);border-radius:999px;padding:8px 13px;font-size:12px;color:var(--muted);background:rgba(25,29,27,.82);white-space:nowrap}
.badge:before{content:'';width:7px;height:7px;border-radius:50%;background:currentColor;opacity:.65}
.badge.ok{color:var(--good);border-color:rgba(169,182,155,.35)}
.badge.warn{color:var(--warn);border-color:rgba(210,183,126,.35)}
.select{display:inline-flex;align-items:center;position:relative;height:42px;background:var(--surface);border:1px solid var(--line);border-radius:999px;padding:0 30px 0 14px}
.select>span{font-size:12.5px;color:var(--muted);padding-right:10px;margin-right:8px;border-right:1px solid var(--line)}
.select select{appearance:none;background:transparent;border:0;color:var(--text);font:inherit;font-size:13.5px;height:40px;padding:0 0 0 2px;cursor:pointer;outline:none}
.select:after{content:'⌄';position:absolute;right:13px;pointer-events:none;color:var(--muted);font-size:12px}
.select select option{background:#202522;color:var(--text)}
#refresh{height:42px;padding:0 16px;border-radius:999px;border:1px solid var(--line);background:var(--surface);color:var(--text);font-size:13.5px}
#refresh:hover{border-color:rgba(169,182,155,.5)}
#refresh:disabled{opacity:.55;cursor:wait}
.verdict{display:flex;justify-content:space-between;align-items:center;gap:18px;flex-wrap:wrap;margin-top:18px;padding:16px 18px;border:1px solid var(--line);border-left:4px solid var(--good);border-radius:18px;background:linear-gradient(180deg,rgba(22,32,51,.92),rgba(18,26,41,.92))}
.verdict.warn{border-left-color:var(--warn)}
.verdict.bad{border-left-color:var(--bad)}
.verdict-main{display:flex;align-items:center;gap:12px}
.verdict-dot{width:10px;height:10px;border-radius:50%;background:var(--good);box-shadow:0 0 0 4px rgba(95,227,189,.12);flex:0 0 auto}
.verdict.warn .verdict-dot{background:var(--warn);box-shadow:0 0 0 4px rgba(255,199,107,.12)}
.verdict.bad .verdict-dot{background:var(--bad);box-shadow:0 0 0 4px rgba(255,143,143,.12)}
.verdict-main strong{display:block;font-size:21px;line-height:1.25;letter-spacing:-.01em}
.verdict-main .small{display:block;margin-top:2px}
.chips{display:flex;max-width:100%;min-width:0;gap:8px;flex-wrap:wrap;list-style:none;margin:0;padding:0}
.chip{display:inline-flex;align-items:center;gap:6px;border-radius:999px;padding:5px 11px;font-size:12px;line-height:1.4;border:1px solid var(--line);color:var(--muted);background:rgba(10,15,26,.35);white-space:nowrap}
.chip .dot{width:6px;height:6px;border-radius:50%;background:currentColor;flex:0 0 auto}
.chip-good{color:var(--good);border-color:rgba(95,227,189,.28)}
.chip-warn{color:var(--warn);border-color:rgba(255,199,107,.28)}
.chip-bad{color:var(--bad);border-color:rgba(255,143,143,.3)}
.chip-unknown{color:var(--muted)}
.chip-stale{color:var(--muted);border-style:dashed}
.flame{display:flex;flex-wrap:wrap;align-items:center;gap:12px;margin-top:12px;padding:10px 16px;border:1px solid var(--line);border-radius:14px;background:var(--surface);font-size:13px;color:var(--muted)}
.flame-copy{display:flex;align-items:baseline;gap:8px;color:var(--text)}
.flame-copy strong{font-size:13px;font-weight:600}
.flame-text{font-size:15px;font-weight:600}
.flame .spacer{flex:1}
.flame.good #flame-text,.flame.good .flame-icon{color:var(--good)}
.flame.bad{border-color:rgba(255,143,143,.5);background:rgba(255,143,143,.07)}
.flame.bad .flame-icon,.flame.bad #flame-text{color:var(--bad)}
.flame-icon{display:flex;width:20px;height:20px;color:var(--muted)}
.message{margin:10px 0 0;color:var(--warn);font-size:12.5px}
.message:empty{display:none}
.layout{display:grid;min-width:0;grid-template-columns:minmax(0,2.05fr) minmax(320px,1fr);gap:16px;align-items:start;margin-top:16px}
.sensor-grid{display:grid;min-width:0;grid-template-columns:repeat(2,minmax(0,1fr));gap:14px;min-width:0}
.side{display:grid;min-width:0;gap:14px;min-width:0}
.card{display:flex;min-width:0;flex-direction:column;gap:8px;min-width:0;padding:16px;border:1px solid var(--line);border-radius:18px;background:linear-gradient(180deg,#162033,#121a29)}
.card-head{display:grid;min-width:0;grid-template-columns:auto 1fr auto auto;align-items:center;gap:8px}
.card-icon{display:flex;width:18px;height:18px;color:var(--muted)}
.card-icon svg{width:18px;height:18px}
.card-title{font-size:14.5px;font-weight:600;margin:0;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.card .value{font-size:40px;font-weight:600;line-height:1.1;letter-spacing:-.03em;font-variant-numeric:tabular-nums}
.card .value.stale{opacity:.45}
.card .unit{font-size:12.5px;color:var(--muted);margin-left:6px;letter-spacing:0;font-weight:400}
.card .graph{position:relative;margin-top:2px}
.graph svg{display:block;width:100%;max-width:100%;min-width:0;height:104px;touch-action:pan-y;cursor:crosshair}
.graph svg:focus-visible{outline:2px solid rgba(95,227,189,.5);outline-offset:2px}
.band{display:none}
.area{stroke:none}
.crosshair{position:absolute;border-left:1px dashed var(--muted);pointer-events:none;opacity:.6}
.point{position:absolute;width:9px;height:9px;transform:translate(-50%,-50%);background:var(--accent);border:2px solid var(--bg);border-radius:50%;pointer-events:none}
.tooltip{position:absolute;z-index:2;padding:7px 10px;border:1px solid rgba(95,227,189,.4);border-radius:10px;background:#0b1220;box-shadow:0 8px 24px rgba(0,0,0,.35);pointer-events:none;font-size:12px;white-space:nowrap}
.tooltip strong{display:block;color:var(--accent);font-size:14px;font-variant-numeric:tabular-nums}
.card-meta{display:flex;justify-content:space-between;gap:8px;font-size:11.5px;color:var(--muted)}.calibration-row{display:flex;align-items:center;justify-content:space-between;gap:8px;font-size:10.5px;color:var(--muted)}.calibration-row button{padding:5px 9px;border:1px solid var(--line);border-radius:8px;background:var(--surface-2);color:var(--text);font:inherit;cursor:pointer}.calibration-row button:disabled{opacity:.45;cursor:not-allowed}
.card-meta span{overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.expand{display:grid;place-items:center;width:30px;height:30px;padding:0;border:1px solid var(--line);border-radius:9px;background:transparent;color:var(--muted)}
.expand:hover{color:var(--text);border-color:rgba(95,227,189,.4)}
.expand svg{width:14px;height:14px}
.card.bad{border-color:rgba(255,143,143,.45)}
.card.bad .value{color:var(--bad)}
.card-detail .row{display:flex;justify-content:space-between;gap:8px}
.card-detail{display:none;margin-top:4px;padding-top:10px;border-top:1px solid var(--line)}
.card.expanded{grid-column:1/-1}
.card.expanded .graph svg{height:260px}
.card.expanded .card-detail{display:block}
.chart-footer{display:flex;justify-content:flex-end;align-items:center;gap:12px;margin-top:10px}
.chart-footer button{min-height:40px;padding:0 14px;border-radius:10px;border:1px solid var(--line);background:transparent;color:var(--text);font-size:12.5px}
.chart-footer button:disabled{opacity:.45;cursor:wait}
.panel{min-width:0;padding:16px;border:1px solid var(--line);border-radius:18px;background:linear-gradient(180deg,#162033,#121a29)}
.panel-head{display:flex;justify-content:space-between;align-items:center;gap:10px}
.panel h2{font-size:14.5px;margin:0;font-weight:600}
.fan-main{display:flex;align-items:baseline;gap:8px;margin-top:10px}
.fan-value{font-size:32px;font-weight:650;letter-spacing:-.02em}
.ai-stack{display:flex;height:9px;border-radius:999px;overflow:hidden;background:var(--line);margin-top:12px}
.ai-stack i{display:block;height:100%;width:0;transition:width .3s}
.ai-stack .more{background:var(--accent)}
.ai-stack .same{background:#6f8ab8}
.ai-stack .less{background:#3f4b60}
.ai-legend{display:flex;gap:12px;flex-wrap:wrap;margin-top:8px;font-size:11.5px;color:var(--muted)}
.ai-legend span{display:inline-flex;align-items:center;gap:5px}
.ai-legend b{color:var(--text);font-weight:600;font-variant-numeric:tabular-nums}
.sw{display:inline-block;width:7px;height:7px;border-radius:2px}
.sw.more{background:var(--accent)}.sw.same{background:#6f8ab8}.sw.less{background:#3f4b60}
.panel p{margin:8px 0 0;font-size:12px}
.alerts-list{list-style:none;margin:10px 0 0;padding:0;display:grid;gap:9px}
.alert-row{display:grid;grid-template-columns:auto 1fr;gap:9px;align-items:start}
.alert-dot{width:7px;height:7px;border-radius:50%;margin-top:6px;background:var(--muted);flex:0 0 auto}
.alert-row.warn .alert-dot{background:var(--warn)}
.alert-row.bad .alert-dot{background:var(--bad)}
.alert-copy{display:flex;flex-direction:column;gap:1px;font-size:12.5px;min-width:0}
.alert-copy strong{font-weight:500;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.alert-copy span{color:var(--muted);font-size:11.5px}
.system-block{grid-column:1/-1;margin-top:2px;padding:0 16px;border:1px solid var(--line);border-radius:16px;background:var(--surface)}
.system-block summary{padding:13px 0;font-size:13px;font-weight:600;color:var(--muted);cursor:pointer}
.system-block[open] summary{color:var(--text);border-bottom:1px solid var(--line)}
.system{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:20px;margin:16px 0}
.system h2{font-size:13px;margin:0;color:var(--muted);font-weight:600}
dl{margin:8px 0 0}
dl div{display:flex;justify-content:space-between;gap:16px;padding:7px 0;border-bottom:1px solid rgba(34,49,72,.7);font-size:12.5px}
dl div:last-child{border:0}
dt{color:var(--muted)}
dd{margin:0;text-align:right;overflow-wrap:anywhere}
footer{display:flex;justify-content:space-between;gap:12px;flex-wrap:wrap;margin-top:16px;font-size:11.5px;color:var(--muted)}
@media(max-width:1100px){.layout{grid-template-columns:1fr}.side{grid-template-columns:repeat(2,minmax(0,1fr))}.system-block{grid-column:auto}}
@media(max-width:760px){.page{padding:16px}.brand h1{font-size:21px}.sensor-grid{grid-template-columns:1fr}.side{grid-template-columns:1fr}.verdict{padding:14px}.verdict-main strong{font-size:19px}.card .value{font-size:36px}.graph svg{height:96px}.system{grid-template-columns:1fr}.select{height:40px}#refresh{height:40px}}
.sensor-grid{grid-template-columns:repeat(6,minmax(0,1fr));gap:12px}
.field-id{display:flex;align-items:center;gap:7px;color:var(--muted);font-size:9px;letter-spacing:.07em;text-transform:uppercase}
.field-id code{color:#d0c8b4;font-size:10.5px;letter-spacing:0;text-transform:none}
.verdict{background:linear-gradient(110deg,rgba(169,182,155,.055),transparent 48%),var(--surface);border-left-width:3px;border-radius:15px;box-shadow:0 10px 26px rgba(0,0,0,.11)}
.card{gap:8px;padding:15px;border-radius:15px;background:linear-gradient(165deg,rgba(255,255,255,.018),transparent 48%),var(--surface);box-shadow:0 10px 28px rgba(0,0,0,.12)}
.card#card-temperature{grid-column:1/-1;--chart:#d3c59e;background:linear-gradient(110deg,rgba(211,197,158,.055),transparent 38%),var(--surface)}
.card#card-humidity,.card#card-light{grid-column:span 3}.card#card-humidity{--chart:#93b8b2}.card#card-light{--chart:#cdb989}.card#card-sound{grid-column:span 3;--chart:#aab9ba}.card#card-mq135{grid-column:span 3;--chart:#aab99c}.card#card-flame{grid-column:span 3;--chart:#c99e91}
.card#card-temperature .graph svg{height:132px}.card:not(#card-temperature) .graph svg{height:94px}
.card-icon{color:var(--chart,var(--muted))}.card .value>span:first-child{font-family:ui-monospace,SFMono-Regular,Menlo,Monaco,Consolas,monospace}.card#card-temperature .value{font-size:47px}
.card-meta span:last-child{color:#c5b995}.avg-point{fill:var(--chart,var(--accent));stroke:var(--surface);stroke-width:1.6;vector-effect:non-scaling-stroke}.point{background:var(--chart,var(--accent));border-color:var(--surface);box-shadow:0 0 0 2px rgba(239,238,232,.08)}.tooltip{background:#202321;border-color:var(--chart,var(--accent));box-shadow:0 8px 24px rgba(0,0,0,.34)}.tooltip strong{color:var(--chart,var(--accent));font-size:13px}.tooltip span{font-size:10.5px;color:var(--muted)}
.panel{padding:16px;border-radius:15px;background:linear-gradient(165deg,rgba(255,255,255,.018),transparent 48%),var(--surface);box-shadow:0 10px 28px rgba(0,0,0,.12)}
.panel-head{flex-wrap:wrap}.model-state{display:inline-flex;align-items:center;gap:6px;color:var(--muted);font:9px ui-monospace,SFMono-Regular,Menlo,monospace;letter-spacing:.06em}.model-state:before{content:'';width:6px;height:6px;border-radius:50%;background:currentColor}.model-state.active{color:var(--good)}.model-state.paused{color:var(--warn)}
.ai-stack .same{background:#929d9c}.ai-stack .less{background:#666d68}.sw.same{background:#929d9c}.sw.less{background:#666d68}
.alerts-list{position:relative;gap:12px;margin-top:12px}.alert-row{position:relative;grid-template-columns:9px minmax(0,1fr);gap:10px}.alert-row:not(:last-child):before{content:'';position:absolute;left:3px;top:10px;bottom:-14px;width:1px;background:var(--line)}.alert-dot{box-shadow:0 0 0 3px var(--surface)}.alert-copy span{font-size:10px}
.system-block{border-radius:13px;background:var(--surface)}.system-block summary{font-size:12px}.system h2{font-size:11px;letter-spacing:.04em}dl div{border-bottom-color:rgba(167,170,161,.12);font-size:11.5px}
footer{padding-top:12px;border-top:1px solid rgba(167,170,161,.14);font-size:10.5px}
@media(max-width:760px){.page{padding:17px 16px 32px}.controls{width:100%}.select{flex:1;justify-content:space-between;min-width:0}.select select{min-width:0}.sensor-grid{grid-template-columns:repeat(6,minmax(0,1fr));gap:10px}.card#card-temperature{grid-column:1/-1}.card#card-humidity,.card#card-light,.card#card-sound,.card#card-mq135,.card#card-flame{grid-column:span 3}.card#card-temperature .graph svg{height:112px}.card:not(#card-temperature) .graph svg{height:90px}.side{grid-template-columns:1fr}.card{padding:13px}.card#card-temperature .value{font-size:40px}}
@media(max-width:480px){.top{align-items:flex-start}.controls{gap:8px}.badge{font-size:11px;padding:7px 10px}#refresh{height:40px;padding:0 13px}.chips{gap:6px}.chip{padding:4px 8px;font-size:10.5px}.sensor-grid{grid-template-columns:minmax(0,1fr)}.card#card-temperature,.card#card-humidity,.card#card-light,.card#card-sound,.card#card-mq135,.card#card-flame{grid-column:1}.card:not(#card-temperature) .graph svg{height:104px}.system-block{padding:0 12px}footer{font-size:10px}}
@media(prefers-reduced-motion:reduce){*,*:before,*:after{scroll-behavior:auto!important;animation-duration:.01ms!important;animation-iteration-count:1!important;transition-duration:.01ms!important}}
</style></head><body>
<div class="page">
<header class="top">
  <div class="brand">
    <div class="eyebrow">EXAMBARIER · ENVIRONMENTAL MONITOR</div>
    <h1>ห้องสอบ 1</h1>
    <p class="sub"><span class="mono">NODE OUT-01</span> · Gateway · NETPIE</p>
  </div>
  <div class="controls">
    <span id="connection" class="badge" role="status">Gateway กำลังเชื่อมต่อ…</span><span id="netpie-connection" class="badge" role="status">NETPIE รอข้อมูล</span><span id="node-connection" class="badge" role="status">Node รอข้อมูล</span>
    <label class="select"><span>ช่วงเวลา</span><select id="time-range" aria-label="ช่วงเวลาย้อนหลังของทุกกราฟ"><option value="0.08333333333333333">5 นาที</option><option value="0.25" selected>15 นาที</option><option value="1">1 ชม.</option><option value="72">3 วัน</option><option value="168">7 วัน</option><option value="360">15 วัน</option><option value="720">30 วัน</option></select></label>
    <button id="refresh" type="button">รีเฟรช</button>
  </div>
</header>

<section class="verdict" id="verdict" aria-live="polite">
  <div class="verdict-main"><span class="verdict-dot"></span><div><strong id="verdict-text">กำลังอ่านข้อมูล…</strong><span id="verdict-sub" class="small">รอข้อมูลจาก NETPIE</span></div></div>
  <ul id="verdict-chips" class="chips"></ul>
</section>

<p id="message" class="message" role="status"></p>

<main class="layout">
  <div id="sensor-charts" class="sensor-grid"></div>
  <aside class="side">
    <section class="panel fan" aria-labelledby="fan-title">
      <div class="panel-head"><h2 id="fan-title">AI Fan Control</h2><span id="model-state" class="model-state">MODEL · WAIT</span><span id="fan-state" class="chip chip-unknown">รอข้อมูล</span></div>
      <div class="fan-main"><span id="fan-value" class="fan-value">—</span><span id="fan-src" class="small"></span></div>
      <div class="ai-stack" aria-hidden="true"><i id="ai-more" class="more"></i><i id="ai-same" class="same"></i><i id="ai-less" class="less"></i></div>
      <div class="ai-legend"><span><i class="sw more"></i>แนะนำเปิด <b id="ai-more-p">—</b></span><span><i class="sw same"></i>เกณฑ์ 40%</span><span><i class="sw less"></i>แนะนำปิด <b id="ai-less-p">—</b></span></div>
      <p id="fan-meta" class="muted small">รอข้อมูลพัดลมและ AI จาก NETPIE</p>
    </section>
    <section class="panel alerts" aria-labelledby="alerts-title">
      <div class="panel-head"><h2 id="alerts-title">เหตุการณ์ล่าสุด</h2><span id="discord-state" class="chip chip-unknown">รอข้อมูล</span></div>
      <ul id="alerts-list" class="alerts-list"><li class="muted small">ยังไม่มีเหตุการณ์</li></ul>
      <p id="alerts-meta" class="muted small">Discord webhook · cooldown</p>
    </section>
  </aside>
  <details class="system-block"><summary>สถานะระบบ</summary><div class="system">
    <section><h2>การเชื่อมต่อ</h2><dl><div><dt>NETPIE REST API</dt><dd id="rest">—</dd></div><div><dt>MQTT ส่งขึ้น NETPIE</dt><dd id="mqtt">—</dd></div><div><dt>IP address</dt><dd id="ip">—</dd></div><div><dt>สัญญาณ Wi-Fi</dt><dd id="rssi">—</dd></div><div><dt>โหนดออนไลน์</dt><dd id="nodes">—</dd></div><div><dt>ทำงานต่อเนื่อง</dt><dd id="uptime">—</dd></div></dl></section>
    <section><h2>การรับและส่งข้อมูล</h2><dl><div><dt>Node ต้นทาง</dt><dd id="nodeid">—</dd></div><div><dt>รับแล้ว / ขาดช่วง</dt><dd id="packets">—</dd></div><div><dt>ส่ง NETPIE สำเร็จ</dt><dd id="published">—</dd></div><div><dt>ส่งไม่สำเร็จ</dt><dd id="failed">—</dd></div><div><dt>รอส่งในบัฟเฟอร์</dt><dd id="buffered">—</dd></div></dl></section>
  </div></details>
</main>

<footer><span id="updated">ยังไม่มีข้อมูล</span><span>ค่าจากเซนเซอร์ยังไม่สอบเทียบ · ข้อมูลจาก NETPIE · ESP32</span></footer>
</div>
<script>
const ICONS={
 temperature:'<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round"><path d="M14 14.76V5a2 2 0 0 0-4 0v9.76a4 4 0 1 0 4 0z"/><path d="M12 9v6"/></svg>',
 humidity:'<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round"><path d="M12 3c-3.2 4.2-6 7.3-6 11a6 6 0 0 0 12 0c0-3.7-2.8-6.8-6-11z"/><path d="M9 15a3 3 0 0 0 3 3"/></svg>',
 light:'<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round"><circle cx="12" cy="12" r="4"/><path d="M12 2v2M12 20v2M4.9 4.9l1.4 1.4M17.7 17.7l1.4 1.4M2 12h2M20 12h2M4.9 19.1l1.4-1.4M17.7 6.3l1.4-1.4"/></svg>',
 sound:'<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round"><path d="M11 5 6 9H3v6h3l5 4z"/><path d="M15.5 8.5a5 5 0 0 1 0 7"/><path d="M18.5 5.5a9 9 0 0 1 0 13"/></svg>',
 mq135:'<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round"><path d="M3 8h11a3 3 0 1 0-3-3"/><path d="M3 12h15a3 3 0 1 1-3 3"/><path d="M3 16h7"/></svg>',
 flame:'<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linejoin="round"><path d="M12 3c.8 3.2 3.8 4.6 3.8 8.2A3.8 3.8 0 0 1 12 15a3.8 3.8 0 0 1-3.8-3.8c0-1.8.9-2.9 1.9-3.9.1 1.6.9 2.5 1.9 2.5 0-2.8-.9-4.8 0-6.8z"/></svg>'
};
const METRICS={
 temperature:{field:'temperature_c',title:'อุณหภูมิ',unit:'°C',valueUnit:'°C',min:-50,max:150,autoScale:true,digits:1,band:[24,26],target:'ต่ำสุด 24 °C · สูงสุด 26 °C',rules:{type:'range',good:[24,26],warn:[22,28]}},
 humidity:{field:'mq135_rh',title:'ความชื้น',unit:'%RH',valueUnit:'%RH',min:0,max:100,autoScale:true,digits:1},
 light:{field:'light_lux',title:'ความเข้มแสง',unit:'lx',valueUnit:'lux',min:0,max:1000000,autoScale:true,band:[300,500],target:'ต่ำสุด 300 lx · สูงสุด 500 lx',rules:{type:'range',good:[300,500],warn:[150,800]}},
 sound:{field:'sound_raw',title:'ระดับเสียง',unit:'ADC',valueUnit:'ADC / 4095',min:0,max:4095,autoScale:true,target:'ไม่เกิน 1,800 ADC',rules:{type:'max',warn:1800,bad:2600}},
 mq135:{field:'mq135_raw',title:'Gas Response (Rs/R0)',unit:'Rs/R0',valueUnit:'Rs/R0',min:0,max:20,rawMin:0,rawMax:4095,autoScale:true,digits:2,target:'อัตราส่วนไม่มีหน่วย · ไม่ใช่ ppm'},
 flame:{field:'flame_raw',title:'เปลวไฟ',unit:'ADC',valueUnit:'ADC / 4095',min:0,max:4095,autoScale:true,target:'ตรวจพบเมื่อค่าต่ำกว่า 1,500 ADC',rules:{type:'min',warn:2000,bad:1500}}
};
const stateLabel={good:'ปกติ',warn:'ควรตรวจ',bad:'ผิดปกติ',unknown:'รอข้อมูล',stale:'ข้อมูลเก่า'};
const METRIC_LABEL={temperature:'อุณหภูมิ',humidity:'ความชื้น',light:'แสง',sound:'เสียง',mq135:'ก๊าซ',flame:'เปลวไฟ'};
function metricState(id,value,age){
 if(value==null||!Number.isFinite(value))return 'unknown';
 if(age!=null&&age>45)return 'stale';
 const r=METRICS[id].rules;if(!r)return 'good';
 if(r.type==='range'){if(value>=r.good[0]&&value<=r.good[1])return 'good';if(value>=r.warn[0]&&value<=r.warn[1])return 'warn';return 'bad';}
 if(r.type==='min'){if(value<r.bad)return 'bad';if(value<r.warn)return 'warn';return 'good';}
 if(r.type==='max'){if(value>r.bad)return 'bad';if(value>r.warn)return 'warn';return 'good';}
 return 'good';
}
const el=id=>document.getElementById(id);
const STATUS_POLL_MS=10000,HISTORY_REFRESH_MS=10000,EMPTY_HISTORY_POLL_MS=60000,HISTORY_OVERLAP_MS=20000;
const MAX_GAP_MS=90000;
let selectedHours=0.25,lastStatus=null,statusBusy=false,statusTimer;
const fmt=(v,d=0)=>Number(v).toLocaleString('th-TH',{maximumFractionDigits:d});
const stamp=t=>new Date(t).toLocaleString('th-TH',{day:'numeric',month:'short',year:'numeric',hour:'2-digit',minute:'2-digit',second:'2-digit'});
function ageText(seconds){if(!Number.isFinite(seconds)||seconds<0)return 'ไม่ทราบ';if(seconds<60)return Math.round(seconds)+' วินาที';if(seconds<3600)return Math.floor(seconds/60)+' นาที';if(seconds<86400)return Math.floor(seconds/3600)+' ชั่วโมง';const days=Math.floor(seconds/86400),hours=Math.floor(seconds%86400/3600);return days+' วัน'+(hours?' '+hours+' ชั่วโมง':'');}
function mq135GasResponse(raw,r0Factor){if(!Number.isFinite(raw)||!Number.isFinite(r0Factor)||r0Factor<=0)return null;const vOut=raw*3.3/4095*1.5;if(vOut<=0||vOut>=5)return null;const value=((5/vOut)-1)/r0Factor;return Number.isFinite(value)&&value>=0?value:null;}
function averageIntervalFor(hours){if(hours<=1)return 60000;if(hours<=72)return 10800000;if(hours<=168)return 21600000;if(hours<=360)return 43200000;return 86400000;}
function rawForDisplay(points,gapLimit){return points.map((p,index)=>({t:p[0],value:p[1],start:p[0],end:p[0],count:1,breakBefore:index===0||p[0]-points[index-1][0]>gapLimit}));}
function intervalLabel(ms){const hours=ms/3600000;return hours>=1?(Number.isInteger(hours)?hours:hours.toFixed(1))+' ชม.':Math.round(ms/60000)+' นาที';}
function averageForDisplay(points,bucketMs,gapLimit){
 const plotted=[];let bucket=[],activeBucket=null,breakBefore=true,lastTime=null;
 const flush=()=>{if(!bucket.length)return;const sum=bucket.reduce((total,p)=>total+p[1],0);plotted.push({t:(bucket[0][0]+bucket[bucket.length-1][0])/2,value:sum/bucket.length,start:bucket[0][0],end:bucket[bucket.length-1][0],count:bucket.length,breakBefore});bucket=[];};
 for(const point of points){const date=new Date(point[0]);date.setHours(0,0,0,0);const bucketKey=date.getTime()+Math.floor((point[0]-date.getTime())/bucketMs)*bucketMs,gap=lastTime!==null&&point[0]-lastTime>gapLimit;if(bucket.length&&(bucketKey!==activeBucket||gap)){flush();breakBefore=gap;}if(!bucket.length)activeBucket=bucketKey;bucket.push(point);lastTime=point[0];}
 flush();return plotted;
}
function markup(id,m){return `<section class="card" id="card-${id}" aria-labelledby="${id}-title">
<div class="card-head"><span class="card-icon">${ICONS[id]||''}</span><h2 id="${id}-title" class="card-title" title="${m.field}">${m.title}</h2><span id="${id}-status" class="chip chip-unknown">รอข้อมูล</span><button id="${id}-expand" class="expand" type="button" aria-expanded="false" aria-label="ขยายกราฟ ${m.title}"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round"><path d="M9 4H4v5M15 4h5v5M9 20H4v-5M15 20h5v-5"/></svg></button></div>
<div class="field-id"><span>NETPIE DATATAG</span><code>${m.field}</code></div>
<div id="${id}-reading" class="value stale"><span id="${id}-value">—</span><span class="unit">${m.valueUnit}</span></div>
<div id="${id}-graph" class="graph" aria-busy="true"><svg id="${id}-svg" viewBox="0 0 600 180" preserveAspectRatio="none" role="img" tabindex="0" aria-label="กราฟข้อมูลย้อนหลัง ${m.title}"><rect id="${id}-band" class="band" x="0" y="180" width="600" height="0"/><defs><linearGradient id="g-${id}" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="var(--chart, #a9b69b)" stop-opacity=".16"/><stop offset="1" stop-color="var(--chart, #a9b69b)" stop-opacity="0"/></linearGradient></defs><path id="${id}-area" class="area" d="" fill="url(#g-${id})"/><path d="M0 1H600 M0 90H600 M0 179H600" fill="none" stroke="var(--line)" stroke-dasharray="2 6"/><path id="${id}-line" d="" fill="none" stroke="var(--chart, var(--accent))" stroke-width="2.2" vector-effect="non-scaling-stroke"/><g id="${id}-averages" aria-hidden="true"></g><circle id="${id}-single" r="3" fill="var(--chart, var(--accent))" visibility="hidden"/></svg><div id="${id}-crosshair" class="crosshair" hidden></div><div id="${id}-point" class="point" hidden></div><div id="${id}-tooltip" class="tooltip" role="status" hidden><strong id="${id}-hover-value"></strong><span id="${id}-hover-time"></span></div></div>
<div class="card-meta"><span id="${id}-age">รอข้อมูล</span><span>${m.target||''}</span></div>
${id==='mq135'?'<div class="calibration-row"><span id="mq135-calibration" role="status">ยังไม่ตั้ง R0</span><button id="mq135-calibrate" type="button" title="วาง Node ในอากาศสะอาดและรอเซนเซอร์อุ่นก่อนตั้ง R0">ตั้ง R0</button></div>':''}
<div id="${id}-detail" class="card-detail"><div class="row muted small"><span id="${id}-start">—</span><span>→</span><span id="${id}-end">—</span></div><p id="${id}-scale" class="muted small"></p><p id="${id}-summary" class="muted small">รอข้อมูลย้อนหลัง</p><p id="${id}-note" class="muted small" role="status"></p><div class="chart-footer"><button id="${id}-csv" type="button" disabled>ดาวน์โหลด CSV</button></div></div>
</section>`;}
const CHART_IDS=Object.keys(METRICS);
el('sensor-charts').innerHTML=CHART_IDS.map(id=>markup(id,METRICS[id])).join('');

class SensorChart{
 constructor(id,metric){this.id=id;this.metric=metric;this.r0Factor=null;this.generation=0;this.points=new Map();this.chart=null;this.index=-1;this.loading=false;this.complete=false;this.coveredUntil=0;this.lastHistoryTry=0;const svg=this.e('svg');for(const event of ['pointermove','pointerdown'])svg.addEventListener(event,e=>this.pointer(e));for(const event of ['pointerleave','pointercancel','blur'])svg.addEventListener(event,()=>this.hide());svg.addEventListener('keydown',e=>this.key(e));this.e('csv').addEventListener('click',()=>this.download());}
 e(part){return el(this.id+'-'+part);}
 live(data){if(this.id==='mq135'){const nextFactor=data?.mq135_r0_ready?data.mq135_r0_factor:null;if(this.r0Factor!==nextFactor){this.r0Factor=nextFactor;if(this.points.size)this.render();}}
  const graphPoints=this.chart?.points,graphLatest=graphPoints?.length?graphPoints[graphPoints.length-1]:null,raw=data?.measurements?.[this.id]??null,statusReading=this.id==='mq135'?mq135GasResponse(raw,this.r0Factor):raw,reading=graphLatest?graphLatest[1]:statusReading,age=graphLatest?Math.max(0,Math.floor((Date.now()-graphLatest[0])/1000)):data?.measurement_age_s?.[this.id];
  this.e('value').textContent=reading===null?'—':fmt(reading,this.metric.digits||0);
  this.e('reading').classList.toggle('stale',reading===null);
  this.e('age').textContent=reading===null?(this.id==='mq135'&&!this.r0Factor?'ตั้ง R0 ก่อนแสดงค่า':'รอข้อมูล '+this.metric.field):'ล่าสุด '+ageText(age)+' ที่แล้ว';
  const state=metricState(this.id,reading,age);
  const label=this.id==='flame'?{good:'ไม่พบเปลวไฟ',warn:'ใกล้เกณฑ์',bad:'ตรวจพบเปลวไฟ',unknown:'รอข้อมูล',stale:'ข้อมูลเก่า'}[state]:this.id==='mq135'?(reading===null?'ตั้ง R0':state==='stale'?'ข้อมูลเก่า':'วัดได้'):stateLabel[state];
  this.e('status').textContent=label;
  this.e('status').className='chip chip-'+state;
  el('card-'+this.id).classList.toggle('bad',state==='bad');}
 hide(){for(const part of ['crosshair','point','tooltip'])this.e(part).hidden=true;}
 reset(end,hours){this.incremental=false;this.complete=false;this.coveredUntil=0;this.lastHistoryTry=Date.now();this.generation++;this.controller?.abort();clearTimeout(this.timer);this.end=end;this.hours=hours;this.start=end-hours*3600000;this.cursor=this.start;this.points.clear();this.chart=null;this.index=-1;this.attempts=0;this.loading=true;this.hide();this.e('csv').disabled=true;this.e('line').setAttribute('d','');this.e('area').setAttribute('d','');this.e('averages').replaceChildren();this.e('band').setAttribute('height','0');this.e('single').setAttribute('visibility','hidden');this.e('summary').textContent='';this.e('scale').textContent='';this.e('start').textContent=stamp(this.start);this.e('end').textContent=stamp(this.end);this.load(this.generation);}
 extend(end){if(this.loading)return;if(!this.complete){this.reset(end,this.hours||selectedHours);return;}const previousEnd=this.coveredUntil;this.generation++;this.controller?.abort();clearTimeout(this.timer);this.incremental=true;this.lastHistoryTry=Date.now();this.end=end;this.start=end-this.hours*3600000;this.cursor=Math.max(this.start,previousEnd-HISTORY_OVERLAP_MS);for(const [timestamp] of this.points)if(timestamp<this.start)this.points.delete(timestamp);this.attempts=0;this.loading=true;this.e('csv').disabled=true;this.e('start').textContent=stamp(this.start);this.e('end').textContent=stamp(end);this.render();this.load(this.generation);}
 render(){
  const m=this.metric,rawValid=[...this.points.values()].filter(p=>Array.isArray(p)&&p.length===2&&Number.isFinite(p[0])&&Number.isFinite(p[1])&&p[0]>=this.start&&p[0]<=this.end&&p[1]>=(m.rawMin??m.min)&&p[1]<=(m.rawMax??m.max)).sort((a,b)=>a[0]-b[0]),rawByTime=new Map(rawValid.map(p=>[p[0],p[1]])),valid=this.id==='mq135'?(this.r0Factor?rawValid.map(p=>[p[0],mq135GasResponse(p[1],this.r0Factor)]).filter(p=>Number.isFinite(p[1])):[]):rawValid;
  let min=Infinity,max=-Infinity,sum=0;for(const p of valid){min=Math.min(min,p[1]);max=Math.max(max,p[1]);sum+=p[1];}
  let lo=m.min,hi=m.max;if(m.autoScale){if(valid.length){const pad=Math.max((max-min)*.1,this.id==='temperature'?1:this.id==='mq135'?0.1:10);lo=Math.max(m.min,min-pad);hi=Math.min(m.max,max+pad);}else{lo=0;hi=this.id==='temperature'?50:this.id==='mq135'?2:1000;}}
  const intervals=[];for(let i=1;i<valid.length;i++){const delta=valid[i][0]-valid[i-1][0];if(delta>0)intervals.push(delta);}intervals.sort((a,b)=>a-b);const expectedInterval=intervals.length?intervals[Math.floor(intervals.length/2)]:0,rangeHours=this.hours||selectedHours,rawOnly=rangeHours<=0.25,intervalMs=averageIntervalFor(rangeHours),gapLimit=Math.max(MAX_GAP_MS,expectedInterval*2.5,rawOnly?0:intervalMs*2.5),displayed=rawOnly?rawForDisplay(valid,gapLimit):averageForDisplay(valid,intervalMs,gapLimit);
  this.chart={points:valid,rawByTime,displayPoints:displayed,gapLimit,intervalMs,lo,hi};
  if(m.band){const b0=m.band[0],b1=m.band[1],y1=179-(b1-lo)/(hi-lo)*178,y2=179-(b0-lo)/(hi-lo)*178;this.e('band').setAttribute('y',Math.max(0,Math.min(y1,y2)).toFixed(1));this.e('band').setAttribute('height',Math.max(0,Math.abs(y2-y1)).toFixed(1));}else this.e('band').setAttribute('height','0');
  this.index=-1;this.hide();const segments=[];for(const p of displayed){if(!segments.length||p.breakBefore)segments.push([]);segments[segments.length-1].push(p);}
  const coords=p=>({x:(p.t-this.start)/(this.end-this.start)*600,y:179-(p.value-lo)/(hi-lo)*178});
  const path=segments.map(segment=>{if(segment.length===1){const p=segment[0],point=coords(p);let x1=(p.start-this.start)/(this.end-this.start)*600,x2=(p.end-this.start)/(this.end-this.start)*600;if(x2-x1<4){x1=point.x-2;x2=point.x+2;}return`M${x1.toFixed(1)},${point.y.toFixed(1)} L${x2.toFixed(1)},${point.y.toFixed(1)}`;}return segment.map((p,index)=>{const point=coords(p);return(index?'L':'M')+point.x.toFixed(1)+','+point.y.toFixed(1);}).join(' ');}).join(' ');this.e('line').setAttribute('d',path);
  const areaPath=segments.filter(segment=>segment.length>1).map(segment=>{const first=coords(segment[0]),last=coords(segment[segment.length-1]),line=segment.map((p,index)=>{const point=coords(p);return(index?'L':'M')+point.x.toFixed(1)+','+point.y.toFixed(1);}).join(' ');return line+` L${last.x.toFixed(1)},179 L${first.x.toFixed(1)},179 Z`;}).join(' ');this.e('area').setAttribute('d',areaPath);
  this.e('averages').replaceChildren();this.e('single').setAttribute('visibility','hidden');
  const averaged=displayed.length<valid.length;this.e('scale').textContent='สเกล '+fmt(lo,m.digits||0)+'–'+fmt(hi,m.digits||0)+' '+m.unit+(averaged?' · เฉลี่ยทุก '+intervalLabel(intervalMs)+' · วาด '+displayed.length.toLocaleString('th-TH')+' จุด':' · แสดง '+valid.length.toLocaleString('th-TH')+' จุด');
  this.e('summary').textContent=valid.length?valid.length.toLocaleString('th-TH')+' จุดย้อนหลัง · ต่ำสุด '+fmt(min,m.digits||0)+' · สูงสุด '+fmt(max,m.digits||0)+' · เฉลี่ย '+fmt(sum/valid.length,m.digits||0)+' '+m.unit:this.id==='mq135'&&!this.r0Factor?'ตั้ง R0 จากอากาศสะอาดก่อนแสดงค่า':'ไม่มีข้อมูลในช่วงเวลานี้';return valid.length;
 }
 inspect(index){if(!this.chart?.displayPoints.length)return;this.index=Math.max(0,Math.min(this.chart.displayPoints.length-1,index));const p=this.chart.displayPoints[this.index],svg=this.e('svg').getBoundingClientRect(),box=this.e('graph').getBoundingClientRect();if(!svg.width||!svg.height)return;const x=(p.t-this.start)/(this.end-this.start)*svg.width+svg.left-box.left,y=(179-(p.value-this.chart.lo)/(this.chart.hi-this.chart.lo)*178)/180*svg.height+svg.top-box.top;const guide=this.e('crosshair'),dot=this.e('point'),tip=this.e('tooltip');guide.style.left=x+'px';guide.style.top=(svg.top-box.top)+'px';guide.style.height=svg.height+'px';dot.style.left=x+'px';dot.style.top=y+'px';this.e('hover-value').textContent=(p.count>1?'เฉลี่ย ':'')+fmt(p.value,p.count>1?1:(this.metric.digits??0))+' '+this.metric.unit;this.e('hover-time').textContent=p.count>1?stamp(p.start)+' – '+stamp(p.end)+' · '+p.count.toLocaleString('th-TH')+' จุด':stamp(p.t);guide.hidden=dot.hidden=tip.hidden=false;tip.style.left=Math.max(8,Math.min(x+16,box.width-tip.offsetWidth-8))+'px';tip.style.top=Math.max(0,Math.min(y-tip.offsetHeight-14,box.height-tip.offsetHeight))+'px';}
 pointer(event){if(!this.chart?.displayPoints.length)return;const box=this.e('svg').getBoundingClientRect();if(!box.width)return;const fraction=Math.max(0,Math.min(1,(event.clientX-box.left)/box.width)),target=this.start+fraction*(this.end-this.start),points=this.chart.displayPoints;let lo=0,hi=points.length;while(lo<hi){const mid=(lo+hi)>>1;if(points[mid].t<target)lo=mid+1;else hi=mid;}let index=Math.min(lo,points.length-1);if(index>0&&Math.abs(points[index-1].t-target)<=Math.abs(points[index].t-target))index--;this.inspect(index);}
 key(event){if(!this.chart?.displayPoints.length)return;let i=this.index<0?this.chart.displayPoints.length-1:this.index;if(event.key==='ArrowLeft')i--;else if(event.key==='ArrowRight')i++;else if(event.key==='Home')i=0;else if(event.key==='End')i=this.chart.displayPoints.length-1;else if(event.key==='Escape'){this.hide();return;}else return;event.preventDefault();this.inspect(i);}
 async load(generation){if(generation!==this.generation)return;this.controller=new AbortController();const controller=this.controller,timeout=setTimeout(()=>controller.abort(),8000);this.e('graph').setAttribute('aria-busy','true');this.e('note').textContent=this.incremental?'กำลังโหลด…':'กำลังโหลด…';try{const response=await fetch('/api/history?hours='+this.hours+'&metric='+this.id+'&after='+this.cursor+'&until='+this.end,{cache:'no-store',signal:controller.signal});if(!response.ok)throw Error('HTTP '+response.status);const data=await response.json();if(generation!==this.generation)return;if(data.state==='loading'){if(++this.attempts>60)throw Error('รอ NETPIE นานเกินไป');this.timer=setTimeout(()=>this.load(generation),1000);return;}this.attempts=0;if(data.metric!==this.id)throw Error('ข้อมูลไม่ตรงกับเซนเซอร์');if(data.state!=='ready')throw Error(data.http===401||data.http===403?'NETPIE ปฏิเสธ key':'NETPIE Feed HTTP '+data.http);if(data.aggregation!=='raw')throw Error('รูปแบบข้อมูลไม่ถูกต้อง');for(const point of data.points||[])if(Array.isArray(point))this.points.set(point[0],point);const count=this.render();if(data.next_after!=null){if(!Number.isFinite(data.next_after)||data.next_after<=this.cursor||data.next_after>=this.end)throw Error('ช่วงข้อมูลถัดไปไม่ถูกต้อง');this.cursor=data.next_after;this.e('note').textContent='กำลังโหลด '+count.toLocaleString('th-TH')+' จุด';this.timer=setTimeout(()=>this.load(generation),250);return;}const rawCount=this.chart?.rawByTime?.size||count;this.e('note').textContent=count?'':this.id==='mq135'&&!this.r0Factor&&rawCount?'ตั้ง R0 จากอากาศสะอาดก่อนแสดง Gas Response':'ยังไม่มีข้อมูลย้อนหลัง';this.e('csv').disabled=!rawCount;this.loading=false;this.complete=true;this.coveredUntil=this.end;this.e('graph').setAttribute('aria-busy','false');}catch(error){if(generation!==this.generation)return;this.loading=false;this.hide();if(!this.incremental){this.chart=null;this.e('line').setAttribute('d','');this.e('single').setAttribute('visibility','hidden');this.e('summary').textContent='กดรีเฟรชเพื่อลองใหม่';}this.e('note').textContent=(this.incremental?'เติมข้อมูลไม่สำเร็จ: ':'โหลดไม่ได้: ')+error.message;this.e('csv').disabled=!this.chart?.points.length;this.e('graph').setAttribute('aria-busy','false');}finally{clearTimeout(timeout);}}
 csv(){const mq=this.id==='mq135';if(mq){const raw=[...(this.chart?.rawByTime||new Map())].sort((a,b)=>a[0]-b[0]);return 'timestamp_ms,timestamp_utc,mq135_raw,rs_r0_ratio\r\n'+raw.map(([t,value])=>t+','+new Date(t).toISOString()+','+value+','+(this.r0Factor?mq135GasResponse(value,this.r0Factor):'')).join('\r\n');}return 'timestamp_ms,timestamp_utc,'+this.metric.field+'\r\n'+(this.chart?.points||[]).map(p=>p[0]+','+new Date(p[0]).toISOString()+','+p[1]).join('\r\n');}
 download(){const rangeName=this.hours<0.1?'5m':this.hours===0.25?'15m':this.hours===1?'1h':this.hours===72?'3d':this.hours===168?'7d':this.hours===360?'15d':'30d',url=URL.createObjectURL(new Blob(['\uFEFF'+this.csv()],{type:'text/csv;charset=utf-8'})),link=document.createElement('a');link.href=url;link.download=this.metric.field+'_'+rangeName+'.csv';document.body.appendChild(link);link.click();link.remove();setTimeout(()=>URL.revokeObjectURL(url),1000);}
}

const charts=CHART_IDS.map(id=>new SensorChart(id,METRICS[id]));
const FAN_SRC={ai:'AI',rule:'กฎ',manual:'มือ',safety:'ความปลอดภัย',none:'—'};
function setBar(name,v){const p=Math.round((v||0)*100),bar=el('ai-'+name),label=el('ai-'+name+'-p');if(bar)bar.style.width=p+'%';if(label)label.textContent=v==null?'—':p+'%';}
function renderFan(d){
 const f=d?.fan,state=el('fan-state');
 if(d?.gas_fan_model_ready){const probability=Number(d.gas_fan_probability)||0,shouldOpen=!!d.gas_fan_should_open,outputOn=!!d.gas_fan_output_on;el('model-state').textContent='DATA MODEL · ACTIVE';el('model-state').className='model-state active';el('fan-value').textContent=outputOn?'พัดลมทำงาน':'พัดลมหยุด';el('fan-src').textContent='สั่งโดย AI · Relay GPIO18 · '+(shouldOpen?'แนะนำเปิด':'แนะนำปิด');state.textContent='โอกาสเปิด '+fmt(probability*100)+'%';state.className='chip chip-'+(shouldOpen?'warn':'good');setBar('more',probability);setBar('same',0);setBar('less',1-probability);el('fan-meta').textContent='อุณหภูมิ '+fmt(d.gas_fan_temperature_c,1)+'°C · ความชื้น '+fmt(d.gas_fan_humidity_rh,1)+'%RH · Rs/R0 '+fmt(d.gas_fan_rs_r0,2)+' · เกณฑ์ '+fmt(d.gas_fan_threshold*100)+'% · หยุดฉุกเฉิน GPIO25';return;}
 if(d&&!d.gas_fan_model_ready&&d.gas_fan_output_on===false){el('model-state').textContent='MODEL · WAIT';el('model-state').className='model-state';el('fan-value').textContent='พัดลมหยุด';el('fan-src').textContent='ปิดเพื่อความปลอดภัย';state.textContent='รอข้อมูลเซนเซอร์';state.className='chip chip-unknown';setBar('more',null);setBar('same',0);setBar('less',null);el('fan-meta').textContent='ต้องมีอุณหภูมิ ความชื้น และ Rs/R0 ล่าสุด · ปุ่มหยุดฉุกเฉิน GPIO25';return;}
 if(!f){state.textContent='รอข้อมูล';state.className='chip chip-unknown';el('model-state').textContent='MODEL · WAIT';el('model-state').className='model-state';el('fan-value').textContent='—';el('fan-src').textContent='';setBar('more',null);setBar('same',null);setBar('less',null);el('fan-meta').textContent='ยังไม่มีฟิลด์ fan/AI ใน shadow';return;}
 const on=!!f.fan_on,suspended=!!f.ai_suspended;
 el('model-state').textContent=f.ai_valid?(suspended?'MODEL · PAUSED':'MODEL · ACTIVE'):'MODEL · WARMING';
 el('model-state').className='model-state '+(suspended?'paused':f.ai_valid?'active':'');
 el('fan-value').textContent=on?'เปิด':'ปิด';
 el('fan-src').textContent='สั่งโดย '+(FAN_SRC[f.fan_src]||f.fan_src||'—');
 state.textContent=suspended?'AI ระงับ':on?'ทำงาน':'หยุด';
 state.className='chip chip-'+(suspended?'bad':on?'good':'unknown');
 const p=f.ai_probs||{};setBar('more',p.more);setBar('same',p.same);setBar('less',p.less);
 el('fan-meta').textContent='อุณหภูมิอินพุต '+fmt(f.ai_temp,1)+'°C · เกณฑ์เปิด '+fmt(f.ai_threshold_on,1)+'°C · '+(f.ai_valid?'AI พร้อม':'AI ยังไม่พร้อม');
}
const ALERT_KIND={flame:'bad',gas:'warn',node:'warn'};
function renderAlerts(d){
 const list=el('alerts-list');list.innerHTML='';
 const items=d?.alerts||[];
 if(!items.length){const li=document.createElement('li');li.className='muted small';li.textContent='ยังไม่มีเหตุการณ์';list.appendChild(li);}
 else for(const a of items.slice(0,3)){
  const li=document.createElement('li');li.className='alert-row '+(ALERT_KIND[a.kind]||'');
  const dot=document.createElement('span');dot.className='alert-dot';
  const copy=document.createElement('div');copy.className='alert-copy';
  const strong=document.createElement('strong');strong.textContent=a.message;
  const meta=document.createElement('span');meta.textContent=new Date(a.at*1000).toLocaleString('th-TH',{day:'numeric',month:'short',hour:'2-digit',minute:'2-digit'})+(a.sent?' · ส่ง Discord แล้ว':' · ส่งไม่สำเร็จ');
  copy.append(strong,meta);li.append(dot,copy);list.appendChild(li);
 }
 const ok=d?.discord_ok;
 el('discord-state').textContent=ok==null?'รอข้อมูล':ok?'พร้อม':'ผิดพลาด';
 el('discord-state').className='chip chip-'+(ok==null?'unknown':ok?'good':'bad');
 el('alerts-meta').textContent=d?.discord_cooldown_s?('cooldown '+d.discord_cooldown_s+' วินาที'):'Discord webhook';
}
const VERDICT_ORDER=['bad','warn','stale','unknown','good'];
function renderVerdict(d){
 const chips=el('verdict-chips');chips.innerHTML='';
 let overall='good',bad=0,warn=0;
 const flameSt=metricState('flame',d?.measurements?.flame,d?.measurement_age_s?.flame);
 if(flameSt==='bad')bad++;else if(flameSt==='warn')warn++;
 if(VERDICT_ORDER.indexOf(flameSt)<VERDICT_ORDER.indexOf(overall))overall=flameSt;
 for(const id of Object.keys(METRICS)){
  if(id==='flame')continue;
  const rawV=d?.measurements?.[id]??null,v=id==='mq135'?(d?.mq135_r0_ready?mq135GasResponse(rawV,d.mq135_r0_factor):null):rawV,age=d?.measurement_age_s?.[id],st=metricState(id,v,age);
  if(st==='bad')bad++;else if(st==='warn')warn++;
  if(VERDICT_ORDER.indexOf(st)<VERDICT_ORDER.indexOf(overall))overall=st;
  const li=document.createElement('li');li.className='chip chip-'+st;
  const dot=document.createElement('span');dot.className='dot';
  li.append(dot,document.createTextNode(METRIC_LABEL[id]+' '+(v==null?'—':fmt(v,METRICS[id].digits||0)+(id==='temperature'?'°':id==='humidity'?'%':id==='light'?' lx':''))));
  chips.appendChild(li);
 }
 el('verdict').className='verdict'+(overall==='bad'||overall==='warn'?' '+overall:'');
 let text='ห้องพร้อมสอบ';
 if(d==null)text='กำลังอ่านข้อมูล…';
 else if(flameSt==='bad')text='ตรวจพบเปลวไฟ';
 else if(overall==='bad')text='ห้องผิดปกติ';
 else if(overall==='warn')text='ควรตรวจสอบ';
 else if(overall==='stale')text='ข้อมูลเซนเซอร์เก่า';
 else if(overall==='unknown')text='รอข้อมูลเซนเซอร์';
 el('verdict-text').textContent=text;
 const ages=Object.values(d?.measurement_age_s||{}).filter(x=>x!=null);
 const age=ages.length?Math.max(...ages):null;
 el('verdict-sub').textContent=d==null?'รอข้อมูลจาก NETPIE':`อัปเดตล่าสุด ${ageText(age)} ที่แล้ว · ${bad?'มีค่าผิดปกติ':warn?'มีค่าที่ควรตรวจ':'ทุกค่าอยู่ในเกณฑ์'}`;
}
for(const chart of charts){chart.e('expand').addEventListener('click',()=>{const card=el('card-'+chart.id),open=card.classList.toggle('expanded');chart.e('expand').setAttribute('aria-expanded',open);chart.hide();if(chart.chart)chart.render();});}
function refreshHistory(){const end=Date.now();for(const chart of charts)chart.reset(end,selectedHours);}
async function update(){
 if(statusBusy)return;
 statusBusy=true;clearTimeout(statusTimer);
 el('refresh').disabled=true;
 const controller=new AbortController(),timeout=setTimeout(()=>controller.abort(),4000);
 try{
  const response=await fetch('/api/status',{cache:'no-store',signal:controller.signal});
  if(!response.ok)throw Error('HTTP '+response.status);
  const d=await response.json();lastStatus=d;
  renderVerdict(d);renderFan(d);renderAlerts(d);
  el('connection').textContent='Gateway ออนไลน์';el('connection').className='badge ok';el('netpie-connection').textContent=d.netpie_connected?'NETPIE เชื่อมต่อแล้ว':'NETPIE ขาดการเชื่อมต่อ';el('netpie-connection').className='badge '+(d.netpie_connected?'ok':'warn');el('node-connection').textContent=d.node_online?'Node เชื่อมต่อแล้ว':'Node ขาดการเชื่อมต่อ';el('node-connection').className='badge '+(d.node_online?'ok':'warn');
  el('message').textContent=d.netpie_connected?'':(d.netpie_http===401||d.netpie_http===403?'NETPIE ปฏิเสธ key กรุณาตรวจสอบ Client ID และ Token':'อ่าน NETPIE ไม่ได้ ตรวจสอบอินเทอร์เน็ต');
  for(const chart of charts)chart.live(d);
  el('mq135-calibration').textContent=d.mq135_r0_ready?'ตั้ง R0 แล้ว · ค่าอ้างอิง ADC '+fmt(d.mq135_r0_adc):d.node_online&&d.warmed_up?'วางในอากาศสะอาดแล้วกดตั้ง R0':'รอ Node ออนไลน์และเซนเซอร์อุ่น';
  el('mq135-calibrate').disabled=!d.node_online||!d.warmed_up;
  el('rest').textContent=d.netpie_connected?'อ่านสำเร็จ':'อ่านไม่สำเร็จ (HTTP '+d.netpie_http+')';
  el('rest').className=d.netpie_connected?'ok':'warn';
  el('mqtt').textContent=d.mqtt_connected?'เชื่อมต่อแล้ว':'ยังไม่เชื่อมต่อ';
  el('ip').textContent=d.ip;
  el('rssi').textContent=d.rssi+' dBm';
  el('nodes').textContent=d.nodes_online;
  el('uptime').textContent=Math.floor(d.uptime_s/3600)+' ชม. '+Math.floor(d.uptime_s%3600/60)+' นาที';
  el('nodeid').textContent=d.node_id;
  el('packets').textContent=d.received+' / '+d.gaps;
  el('published').textContent=d.published;
  el('failed').textContent=d.publish_failed;
  el('buffered').textContent=d.buffered;
  el('updated').textContent='อัปเดต '+new Date().toLocaleTimeString('th-TH');
 }catch(error){
  el('connection').textContent='Gateway ขาดการเชื่อมต่อ';el('connection').className='badge warn';el('netpie-connection').textContent='NETPIE ไม่ทราบสถานะ';el('netpie-connection').className='badge warn';el('node-connection').textContent='Node ไม่ทราบสถานะ';el('node-connection').className='badge warn';
  el('message').textContent='ติดต่อบอร์ดไม่ได้ ตรวจสอบ Wi-Fi และไฟเลี้ยง';
  el('mq135-calibration').textContent='รอ Gateway และ Node เชื่อมต่อ';el('mq135-calibrate').disabled=true;
  for(const chart of charts)chart.live(null);
  renderVerdict(null);renderFan(null);renderAlerts(null);
 }finally{clearTimeout(timeout);statusBusy=false;el('refresh').disabled=false;statusTimer=setTimeout(update,STATUS_POLL_MS);}
}
el('time-range').addEventListener('change',()=>{const hours=Number(el('time-range').value);if(![5/60,0.25,1,72,168,360,720].includes(hours))return;selectedHours=hours;refreshHistory();});
el('mq135-calibrate').addEventListener('click',async()=>{if(!confirm('วางเซนเซอร์ในอากาศสะอาดและรอให้อุ่นแล้วหรือยัง? ระบบจะบันทึกค่าอ้างอิง R0 สำหรับแสดงอัตราส่วน Rs/R0'))return;const button=el('mq135-calibrate'),label=el('mq135-calibration');button.disabled=true;label.textContent='กำลังตั้ง R0…';try{const response=await fetch('/api/calibrate/mq135',{method:'POST',cache:'no-store'}),result=await response.json();if(!response.ok)throw Error(result.error||'ตั้ง R0 ไม่สำเร็จ');await update();refreshHistory();label.textContent='ตั้ง R0 แล้ว · ค่าอ้างอิง ADC '+fmt(result.r0_adc);}catch(error){label.textContent=error.message;}finally{button.disabled=false;}});
el('refresh').addEventListener('click',()=>{update();refreshHistory();});
window.addEventListener('resize',()=>charts.forEach(chart=>{chart.hide();if(chart.chart)chart.render();}));
function refreshRecentHistory(){const end=Date.now();for(const chart of charts){if(chart.loading)continue;const reading=lastStatus?.measurements?.[chart.id];if(chart.points.size||reading!=null||end-chart.lastHistoryTry>=EMPTY_HISTORY_POLL_MS)chart.extend(end);}}
setInterval(refreshRecentHistory,HISTORY_REFRESH_MS);
renderVerdict(null);renderFan(null);renderAlerts(null);
update();refreshHistory();
</script></body></html>)WEB";
