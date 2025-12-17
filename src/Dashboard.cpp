#include <Arduino.h>

  const char DASHBOARD_HTML[] PROGMEM = R"rawliteral(
  <!DOCTYPE html>
  <html>

  <head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>ESP_hole Dashboard</title>
    <style>
      body {
        font-family: system-ui, sans-serif;
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
        box-shadow: 0 0 6px rgba(0, 0, 0, 0.4);
      }

      .grid {
        display: grid;
        grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
        gap: 12px;
      }

      h1 {
        margin-bottom: 10px;
      }

      .stat {
        font-size: 28px;
        font-weight: bold;
      }

      canvas {
        width: 100%;
        height: 200px;
      }

      #emptyState {
        background: #1e1e1e;
        border-radius: 8px;
        padding: 24px;
        opacity: 0.9;
      }

      #emptyState h3 {
        margin-bottom: 8px;
      }
    </style>
  </head>

  <body>
    <h1>ESP_hole Dashboard</h1>

    <div id="emptyState" class="card" style="display:none; text-align:center;">
      <h3>😴 Nothing yet</h3>
      <p>No DNS queries have been seen.</p>
      <p>Try browsing the web or connecting a device.</p>
    </div>

    <div id="offlineState" class="card" style="display:none;">
      <h3>⚠️ ESP not responding</h3>
      <p>Waiting for connection…</p>
    </div>

    <div id="dashboard">
      <div class="grid">
        <div class="card">
          <div>Total Queries</div>
          <div class="stat" id="total">0</div>
        </div>
        <div class="card">
          <div>Avg Response Time</div>
          <div class="stat" id="responseTime">0</div>
        </div>
        <div class="card">
          <div>Avg Block Time</div>
          <div class="stat" id="blockTime">0</div>
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
    </div>

    <div class="card">
      <h3>Memory</h3>
      <div>Free Heap: <span id="heapfree">0</span> bytes</div>
      <div>Min Heap: <span id="heapmin">0</span> bytes</div>
    </div>

    <script>
      async function loadStats() {
        try {
          const res = await fetch("/stats", { cache: "no-store" });
          if (!res.ok) {
            throw new Error("HTTP " + res.status);
          }

          const s = await res.json();
          if (s) {
            document.getElementById('heapfree').textContent = s.heap.free;
            document.getElementById('heapmin').textContent = s.heap.min;
          }
          if (!s || s.total === 0) {
            console.warn("No Stats yet");
            document.getElementById("emptyState").style.display = "block";
            document.getElementById("dashboard").style.display = "none";
            document.getElementById("offlineState").style.display = "none";
            return;
          }

          document.getElementById("offlineState").style.display = "none";
          document.getElementById("emptyState").style.display = "none";
          document.getElementById("dashboard").style.display = "block";

          document.getElementById('total').textContent = s.total;
          document.getElementById('responseTime').textContent =
            ((s.total - s.blocked) > 0 ?
              (Math.round(s.responseTime / (s.total - s.blocked))).toFixed(2) : '0') + ' ms';
          document.getElementById('blockTime').textContent = (Math.round(s.blockTime / s.blocked)).toFixed(2) + ' ms';
          document.getElementById('blocked').textContent = s.blocked;
          document.getElementById('percent').textContent =
            s.total ? ((s.blocked / s.total) * 100).toFixed(1) + '%' : '0%';


          drawChart(s.hours);
          renderTop('topq', s.top.queried);
          renderTop('topb', s.top.blocked);
        } catch (e) {
          console.warn("Stats fetch failed:", e);
          document.getElementById("offlineState").style.display = "block";
          document.getElementById("dashboard").style.display = "none";
          document.getElementById("emptyState").style.display = "none";
        }
      }

      const canvas = document.getElementById("chart");
      const ctx = canvas.getContext("2d");

      function resizeCanvas() {
        const dpr = window.devicePixelRatio || 1;
        const rect = canvas.getBoundingClientRect();

        canvas.width = Math.round(rect.width * dpr);
        canvas.height = Math.round(rect.height * dpr);

        ctx.setTransform(1, 0, 0, 1, 0, 0);

        ctx.scale(dpr, dpr);

        return { width: rect.width, height: rect.height };
      }

      window.addEventListener("resize", resizeCanvas);
      resizeCanvas();

      function drawChart(hours) {
        const { width, height } = resizeCanvas();
        ctx.clearRect(0, 0, width, height);

        if (!hours || hours.length === 0) return;

        const pad = { l: 36, r: 36, t: 20, b: 30 };
        const plotW = width - pad.l - pad.r;
        const plotH = height - pad.t - pad.b;
        const stepX = plotW / 24;
        const barW = stepX * 0.7;

        const queries = hours.map(h => h.q || 0);
        const avgMs = hours.map(h => (h.q - h.b > 0) ? h.t / (h.q - h.b) : 0);

        const maxQ = Math.max(1, ...queries);
        const maxMs = Math.max(1, ...avgMs);

        drawAxes(pad, width, height, plotH, maxQ, maxMs);
        drawBars(hours, pad, plotH, stepX, barW, maxQ);
        drawAvgLine(avgMs, pad, plotH, stepX, maxMs);
        drawHourLabels(hours, pad, plotH, stepX);
      }

      function drawAxes(pad, width, height, plotH, maxQ, maxMs) {
        ctx.strokeStyle = "#aaa";
        ctx.lineWidth = 1;

        // Left Y
        ctx.beginPath();
        ctx.moveTo(pad.l, pad.t);
        ctx.lineTo(pad.l, pad.t + plotH);
        ctx.stroke();

        // Right Y
        ctx.beginPath();
        ctx.moveTo(width - pad.r, pad.t);
        ctx.lineTo(width - pad.r, pad.t + plotH);
        ctx.stroke();

        // X
        ctx.beginPath();
        ctx.moveTo(pad.l, pad.t + plotH);
        ctx.lineTo(width - pad.r, pad.t + plotH);
        ctx.stroke();

        ctx.fillStyle = "#888";
        ctx.font = "11px system-ui";
        ctx.textBaseline = "middle";

        // Left labels
        ctx.textAlign = "right";
        ctx.fillText(maxQ, pad.l - 6, pad.t + 6);
        ctx.fillText("0", pad.l - 6, pad.t + plotH);

        // Right labels
        ctx.textAlign = "left";
        ctx.fillText(`${Math.round(maxMs)} ms`, width - pad.r + 6, pad.t + 6);
      }

      function drawBars(hours, pad, plotH, stepX, barW, maxQ) {
        hours.forEach((hr, i) => {
          const x = pad.l + i * stepX + (stepX - barW) / 2;

          const qH = (hr.q / maxQ) * plotH;
          const bH = (hr.b / maxQ) * plotH;

          ctx.fillStyle = "#467fcf";
          ctx.fillRect(x, pad.t + plotH - qH, barW, qH);

          ctx.fillStyle = "#d36155ff";
          ctx.fillRect(x, pad.t + plotH - bH, barW, bH);
        });
      }

      function drawAvgLine(avgMs, pad, plotH, stepX, maxMs) {
        ctx.strokeStyle = "#ffb347";
        ctx.lineWidth = 2;
        ctx.beginPath();

        let previousHasValue = false;
        avgMs.forEach((v, i) => {
          if (!v || v <= 0) {
            previousHasValue = false;
            return;
          }
          const x = pad.l + i * stepX + stepX / 2;
          const y = pad.t + plotH - (v / maxMs) * plotH;
          previousHasValue ? ctx.moveTo(x, y) : ctx.lineTo(x, y);
          previousHasValue = true;
        });

        ctx.stroke();

        ctx.fillStyle = "#ffb347";
        avgMs.forEach((v, i) => {
          if (!v || v <= 0) return;
          const x = pad.l + i * stepX + stepX / 2;
          const y = pad.t + plotH - (v / maxMs) * plotH;
          ctx.beginPath();
          ctx.arc(x, y, 2.5, 0, Math.PI * 2);
          ctx.fill();
        });
      }

      function drawHourLabels(hours, pad, plotH, stepX) {
        ctx.fillStyle = "#888";
        ctx.font = "11px system-ui, sans-serif";
        ctx.textAlign = "center";
        ctx.textBaseline = "top";

        // every 6 hours
        for (let i = hours.length - 1; i >= 0; i -= 6) {
          const label = (i >= hours.length - 1) ? "Now" : `-${24 - i}h`;

          const x = pad.l + i * stepX + stepX / 2;
          const y = pad.t + plotH + 6;

          ctx.fillText(label, x, y);
        }
      }

      function renderTop(id, list) {
        const ul = document.getElementById(id);
        ul.innerHTML = '';
        list.sort((a, b) => b.c - a.c);
        list.forEach(e => {
          const li = document.createElement('li');
          li.textContent = `${e.d} (${e.c})`;
          ul.appendChild(li);
        });
      }

      loadStats();
      setInterval(loadStats, 30000); //30s
    </script>
  </body>

  </html>
  )rawliteral";