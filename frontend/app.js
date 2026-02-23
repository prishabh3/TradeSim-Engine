/* ─────────────────────────────────────────────────────────────────────────
   app.js — TradeSim Trading Terminal
   ───────────────────────────────────────────────────────────────────────── */

const API = 'http://localhost:8080/api';
const POLL_MS = 500;
const MAX_HIST = 120;

// ── State ──────────────────────────────────────────────────────────────────
let symbols = [];
let activeSym = null;
let bookSym = null;
let history = {};     // { SYM: [{x, y}, ...] }
let chart = null;
let orderSide = 'BUY';
let seenTradeIds = new Set();
let allFills = [];

// ── DOM refs ───────────────────────────────────────────────────────────────
const $nav = document.getElementById('v-nav');
const $cash = document.getElementById('v-cash');
const $pnl = document.getElementById('v-pnl');
const $upnl = document.getElementById('v-upnl');
const $pill = document.getElementById('conn-pill');
const $clock = document.getElementById('clock');
const $symTabs = document.getElementById('sym-tabs');
const $oSym = document.getElementById('o-symbol');
const $bookSym = document.getElementById('book-sym');
const $preview = document.getElementById('order-preview');
const $submitBtn = document.getElementById('submit-btn');
const $oMsg = document.getElementById('order-msg');
const $chartPx = document.getElementById('chart-price');
const $chartChg = document.getElementById('chart-chg');

// ── Formatters ─────────────────────────────────────────────────────────────
const f$ = v => v == null ? '—' : '$' + Math.abs(v).toFixed(2).replace(/\B(?=(\d{3})+(?!\d))/g, ',');
const fpnl = v => v == null ? '—' : (v >= 0 ? '+' : '') + f$(v).replace('$-', '$');
const f4 = v => v == null ? '—' : v.toFixed(4);
const pCls = v => v > 0.0001 ? 'pos' : v < -0.0001 ? 'neg' : '';
const tsNow = () => new Date().toLocaleTimeString('en-US', { hour12: false });

// ── Clock ──────────────────────────────────────────────────────────────────
setInterval(() => { $clock.textContent = tsNow(); }, 1000);
$clock.textContent = tsNow();

// ── Connection indicator ───────────────────────────────────────────────────
function setLive(ok) {
  $pill.textContent = ok ? '● LIVE' : '● OFFLINE';
  $pill.className = 'topbar__pill ' + (ok ? 'live' : 'err');
}

// ── Chart ──────────────────────────────────────────────────────────────────
function initChart() {
  const ctx = document.getElementById('priceChart').getContext('2d');
  chart = new Chart(ctx, {
    type: 'line',
    data: { labels: [], datasets: [{ label: '', data: [], borderColor: '#2196f3', borderWidth: 1.5, pointRadius: 0, tension: 0.2, fill: 'start', backgroundColor: 'rgba(33,150,243,.05)' }] },
    options: {
      responsive: true, maintainAspectRatio: false, animation: false,
      plugins: {
        legend: { display: false },
        tooltip: {
          mode: 'index', intersect: false,
          backgroundColor: '#131722', borderColor: '#252d3d', borderWidth: 1,
          titleColor: '#5a6a85', bodyColor: '#d1d9e6',
          callbacks: { label: c => f$(c.parsed.y) }
        }
      },
      scales: {
        x: {
          ticks: { color: '#2e3a50', maxTicksLimit: 5, font: { family: 'IBM Plex Mono', size: 9 } },
          grid: { color: 'rgba(37,45,61,.6)' }, border: { color: '#252d3d' }
        },
        y: {
          position: 'right',
          ticks: { color: '#5a6a85', font: { family: 'IBM Plex Mono', size: 10 }, callback: v => f$(v) },
          grid: { color: 'rgba(37,45,61,.6)' }, border: { color: '#252d3d' }
        }
      }
    }
  });
}

function pushPrice(sym, price) {
  if (!history[sym]) history[sym] = [];
  history[sym].push({ x: tsNow(), y: price });
  if (history[sym].length > MAX_HIST) history[sym].shift();
}

function refreshChart() {
  if (!chart || !activeSym || !history[activeSym]?.length) return;
  const pts = history[activeSym];
  const first = pts[0].y, last = pts[pts.length - 1].y;
  const up = last >= first;
  const color = up ? '#26a69a' : '#ef5350';

  chart.data.labels = pts.map(p => p.x);
  chart.data.datasets[0].data = pts.map(p => p.y);
  chart.data.datasets[0].borderColor = color;
  chart.data.datasets[0].backgroundColor = up ? 'rgba(38,166,154,.05)' : 'rgba(239,83,80,.05)';
  chart.update('none');

  const chg = ((last - first) / first * 100).toFixed(2);
  $chartPx.textContent = f$(last);
  $chartChg.textContent = (up ? '▲ +' : '▼ ') + chg + '%';
  $chartChg.className = 'chart-chg ' + (up ? 'pos' : 'neg');
}

// ── Symbol Tabs ────────────────────────────────────────────────────────────
function buildSymbolUI() {
  // Chart tabs
  $symTabs.innerHTML = symbols.map(s =>
    `<button class="sym-tab${s === activeSym ? ' active' : ''}" data-s="${s}">${s}</button>`
  ).join('');
  $symTabs.querySelectorAll('.sym-tab').forEach(b => b.addEventListener('click', () => {
    activeSym = b.dataset.s;
    buildSymbolUI(); refreshChart();
  }));

  // Order form symbol select
  const cur = $oSym.value || activeSym;
  $oSym.innerHTML = symbols.map(s => `<option value="${s}"${s === cur ? ' selected' : ''}>${s}</option>`).join('');

  // Book symbol select
  const bcur = $bookSym.value || bookSym;
  $bookSym.innerHTML = symbols.map(s => `<option value="${s}"${s === bcur ? ' selected' : ''}>${s}</option>`).join('');
  $bookSym.onchange = () => { bookSym = $bookSym.value; };
}

// ── Order Entry ────────────────────────────────────────────────────────────
function setOrderSide(side) {
  orderSide = side;
  document.getElementById('tab-buy').classList.toggle('active', side === 'BUY');
  document.getElementById('tab-sell').classList.toggle('active', side === 'SELL');
  $submitBtn.textContent = `PLACE ${side} ORDER`;
  $submitBtn.className = `submit-btn submit-btn--${side.toLowerCase()}`;
  updatePreview();
}

function onTypeChange() {
  const isMarket = document.getElementById('o-type').value === 'MARKET';
  document.getElementById('price-field').style.opacity = isMarket ? '.35' : '1';
  document.getElementById('o-price').disabled = isMarket;
  updatePreview();
}

function updatePreview() {
  const sym = $oSym.value || '—';
  const qty = document.getElementById('o-qty').value || '0';
  const type = document.getElementById('o-type').value;
  const px = document.getElementById('o-price').value;
  $preview.textContent = `${type} ${orderSide} · ${qty} × ${sym}${type === 'LIMIT' && px ? ' @ $' + parseFloat(px).toFixed(2) : ''}`;
}

// Attach live preview updates
['o-qty', 'o-price', 'o-symbol', 'o-type'].forEach(id => {
  document.getElementById(id)?.addEventListener('input', updatePreview);
  document.getElementById(id)?.addEventListener('change', updatePreview);
});

async function placeOrder() {
  const sym = $oSym.value;
  const qty = parseFloat(document.getElementById('o-qty').value);
  const type = document.getElementById('o-type').value;
  const px = parseFloat(document.getElementById('o-price').value) || 0;

  if (!sym || qty <= 0) { showMsg('Invalid order', false); return; }
  if (type === 'LIMIT' && px <= 0) { showMsg('Enter a limit price', false); return; }

  $submitBtn.disabled = true;
  try {
    const res = await fetch(API + '/order', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ symbol: sym, side: orderSide, type, quantity: qty, price: px }),
      signal: AbortSignal.timeout(2000)
    });
    const d = await res.json();
    if (d.status === 'accepted') {
      showMsg(`#${d.order_id} accepted`, true);
    } else {
      showMsg(d.error || 'Rejected', false);
    }
  } catch (e) {
    showMsg('Backend unreachable', false);
  } finally {
    $submitBtn.disabled = false;
  }
}

function showMsg(text, ok) {
  $oMsg.textContent = text;
  $oMsg.className = 'order-msg ' + (ok ? 'ok' : 'err');
  setTimeout(() => { $oMsg.textContent = ''; $oMsg.className = 'order-msg'; }, 3000);
}

// ── Portfolio ──────────────────────────────────────────────────────────────
async function pollPortfolio() {
  const d = await fetchJSON('/portfolio');
  $nav.textContent = f$(d.portfolio_value);
  $cash.textContent = f$(d.cash);
  $pnl.textContent = fpnl(d.total_pnl);
  $pnl.className = 'metric__value ' + pCls(d.total_pnl);
  $upnl.textContent = fpnl(d.unrealized_pnl);
  $upnl.className = 'metric__value ' + pCls(d.unrealized_pnl);
}

// ── Positions ──────────────────────────────────────────────────────────────
async function pollPositions() {
  const d = await fetchJSON('/positions');
  const tbody = document.getElementById('pos-body');
  const open = d.positions.filter(p => Math.abs(p.quantity) > 0.001);
  if (!open.length) {
    tbody.innerHTML = '<tr><td colspan="5" class="empty">NO POSITIONS</td></tr>';
    return;
  }
  tbody.innerHTML = open.map(p => `
    <tr>
      <td class="sym-label">${p.symbol}</td>
      <td>${p.quantity.toFixed(0)}</td>
      <td>${f4(p.avg_cost)}</td>
      <td class="${pCls(p.unrealized_pnl)}">${fpnl(p.unrealized_pnl)}</td>
      <td class="${pCls(p.realized_pnl)}">${fpnl(p.realized_pnl)}</td>
    </tr>`).join('');
}

// ── Indicators ─────────────────────────────────────────────────────────────
async function pollIndicators() {
  const d = await fetchJSON('/indicators');
  d.indicators.forEach(ind => {
    if (ind.last_price > 0) pushPrice(ind.symbol, ind.last_price);
  });
  refreshChart();

  const tbody = document.getElementById('ind-body');
  tbody.innerHTML = d.indicators.map(ind => {
    const rsi = ind.rsi_valid ? ind.rsi : null;
    const rsiW = rsi != null ? Math.min(100, rsi).toFixed(0) : 50;
    const rsiClr = rsi == null ? '#2e3a50' : rsi < 30 ? '#26a69a' : rsi > 70 ? '#ef5350' : '#2196f3';
    const sig = ind.signal?.toUpperCase() || 'HOLD';
    const sigCls = sig === 'BUY' ? 'buy' : sig === 'SELL' ? 'sell' : 'hold';
    return `
    <tr>
      <td class="sym-label">${ind.symbol}</td>
      <td>${ind.last_price > 0 ? f4(ind.last_price) : '—'}</td>
      <td>${ind.sma_valid ? f4(ind.sma) : '—'}</td>
      <td>${ind.ema_valid ? f4(ind.ema) : '—'}</td>
      <td>
        <div class="rsi-wrap">
          <span>${rsi != null ? rsi.toFixed(1) : '—'}</span>
          <div class="rsi-bar"><div class="rsi-fill" style="width:${rsiW}%;background:${rsiClr}"></div></div>
        </div>
      </td>
      <td><span class="sig sig--${sigCls}">${sig}</span></td>
    </tr>`;
  }).join('');
}

// ── Order Book ─────────────────────────────────────────────────────────────
async function pollBook() {
  const sym = bookSym || (symbols[0] ?? 'AAPL');
  const d = await fetchJSON(`/orderbook?symbol=${sym}`);

  function buildRows(orders, cls) {
    const maxQ = Math.max(...orders.map(o => o.quantity), 1);
    let cum = 0;
    return orders.map(o => {
      cum += o.quantity;
      const pct = (o.quantity / maxQ * 100).toFixed(0);
      return `<div class="book__row">
        <span class="col-price">${f4(o.price)}</span>
        <span class="col-size">${o.quantity.toFixed(2)}</span>
        <span class="col-total">${cum.toFixed(2)}</span>
        <div class="book__row-bar" style="width:${pct}%"></div>
      </div>`;
    }).join('');
  }

  document.getElementById('asks-container').innerHTML = buildRows([...d.asks].reverse(), 'ask');
  document.getElementById('bids-container').innerHTML = buildRows(d.bids, 'bid');

  const midPx = document.getElementById('mid-price');
  if (d.bids[0] && d.asks[0]) {
    const mid = (d.bids[0].price + d.asks[0].price) / 2;
    const spread = (d.asks[0].price - d.bids[0].price).toFixed(4);
    midPx.textContent = f$(mid);
    document.getElementById('book-spread-val').textContent = `SPREAD ${spread}`;
  } else {
    midPx.textContent = '—';
  }
}

// ── Trades ─────────────────────────────────────────────────────────────────
async function pollTrades() {
  const d = await fetchJSON('/trades');
  const newFills = (d.trades || []).filter(t => !seenTradeIds.has(t.id));
  if (!newFills.length) return;
  newFills.forEach(t => seenTradeIds.add(t.id));
  allFills = [...newFills, ...allFills].slice(0, 40);

  const list = document.getElementById('trades-list');
  list.innerHTML = allFills.map(t => `
    <div class="fill-row">
      <span class="fill-sym">${t.symbol}</span>
      <span class="fill-qty">${parseFloat(t.quantity || 0).toFixed(0)}</span>
      <span class="fill-px">${f4(t.price)}</span>
      <span class="fill-val">${t.price && t.quantity ? f$(t.price * t.quantity) : '—'}</span>
    </div>`).join('');
}

// ── Fetch helper ───────────────────────────────────────────────────────────
async function fetchJSON(path) {
  const r = await fetch(API + path, { signal: AbortSignal.timeout(800) });
  if (!r.ok) throw new Error(r.status);
  return r.json();
}

// ── Poll loop ──────────────────────────────────────────────────────────────
async function poll() {
  try {
    await Promise.allSettled([pollPortfolio(), pollPositions(), pollIndicators(), pollBook(), pollTrades()]);
    setLive(true);
  } catch { setLive(false); }
}

// ── Bootstrap ──────────────────────────────────────────────────────────────
(async () => {
  initChart();

  // Retry until backend is available
  const tryConnect = async () => {
    try {
      const d = await fetchJSON('/symbols');
      symbols = d.symbols?.length ? d.symbols : ['AAPL'];
      activeSym = symbols[0];
      bookSym = symbols[0];
      symbols.forEach(s => history[s] = []);
      buildSymbolUI();
      updatePreview();
      setLive(true);
      setInterval(poll, POLL_MS);
    } catch {
      setLive(false);
      setTimeout(tryConnect, 2000);
    }
  };
  tryConnect();
})();
