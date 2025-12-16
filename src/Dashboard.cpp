#include <Arduino.h>

const char DASHBOARD_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<title>ESP DNS Dashboard</title>
<style>
body {
  font-family: Arial, sans-serif;
  background: #1e1e1e;
  color: #eee;
  margin: 0;
  padding: 20px;
}
.card {
  background: #2b2b2b;
  padding: 16px;
  border-radius: 8px;
  margin-bottom: 16px;
}
.grid {
  display: grid;
  grid-template-columns: repeat(3, 1fr);
  gap: 12px;
}
h1 { margin-bottom: 10px; }
.stat {
  font-size: 28px;
  font-weight: bold;
}
canvas {
  width: 100%;
  height: 200px;
}
</style>
</head>
<body>
<h1>ESP_Hole Dashboard</h1>

<div class="grid">
  <div class="card">
    <div>Total Queries</div>
    <div class="stat" id="total">0</div>
  </div>
  <div class="card">
    <div>Blocked</div>
    <div class="stat" id="blocked">0</div>
  </div>
  <div class="card">
    <div>Blocked %</div>
    <div class="stat" id="percent">0%</div>
  </div>
</div>

<div class="card">
  <h3>Last 24 Hours</h3>
  <canvas id="chart"></canvas>
</div>

<div class="card">
  <h3>Top Queried Domains</h3>
  <ul id="topq"></ul>
</div>

<div class="card">
  <h3>Top Blocked Domains</h3>
  <ul id="topb"></ul>
</div>

<div class="card">
  <h3>Memory</h3>
  <div>Free Heap: <span id="heapfree">0</span> bytes</div>
  <div>Min Heap: <span id="heapmin">0</span> bytes</div>
</div>

<script>
  async function loadStats() {
    const r = await fetch('/stats');
    const s = await r.json();

    document.getElementById('total').textContent = s.total;
    document.getElementById('blocked').textContent = s.blocked;
    document.getElementById('percent').textContent =
      s.total ? ((s.blocked / s.total) * 100).toFixed(1) + '%' : '0%';
      
    document.getElementById('heapfree').textContent = s.heap.free;
    document.getElementById('heapmin').textContent = s.heap.min;


    drawChart(s.hours);
    renderTop('topq', s.top.queried);
    renderTop('topb', s.top.blocked);
  }

  function drawChart(data) {
    const c = document.getElementById('chart');
    const ctx = c.getContext('2d');

    c.width = c.clientWidth;
    c.height = c.clientHeight;

    ctx.clearRect(0, 0, c.width, c.height);

    const max = Math.max(...data.map(h => h.q), 1);
    const barWidth = c.width / data.length;

    data.forEach((h, i) => {
      const qh = (h.q / max) * c.height;
      const bh = (h.b / max) * c.height;

      ctx.fillStyle = '#555';
      ctx.fillRect(i * barWidth, c.height - qh, barWidth - 2, qh);

      ctx.fillStyle = '#e74c3c';
      ctx.fillRect(i * barWidth, c.height - bh, barWidth - 2, bh);
    });
  }

  function renderTop(id, list) {
    const ul = document.getElementById(id);
    ul.innerHTML = '';
    list.forEach(e => {
      const li = document.createElement('li');
      li.textContent = `${e.d} (${e.c})`;
      ul.appendChild(li);
    });
  }

  loadStats();
  setInterval(loadStats, 60_000);
</script>
</body>
</html>
)rawliteral";
