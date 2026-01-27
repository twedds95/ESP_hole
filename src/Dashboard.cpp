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

      #header {
        display: flex;
        align-items: center;
        justify-content: space-between;
        padding: 12px 16px;
      }

      #header h1 {
        margin: 0;
        font-size: 1.4rem;
        white-space: nowrap;
      }

      .stat {
        font-size: 28px;
        font-weight: bold;
      }

      canvas {
        width: 100%;
        height: 200px;
      }

      .top-list li {
        display: flex;
        align-items: center;
        gap: 6px;
      }

      .top-list .domain {
        flex: 1;
        min-width: 0;
        overflow: hidden;
        text-overflow: ellipsis;
        white-space: nowrap;
      }

      .top-list .count {
        flex-shrink: 0;
        color: #aaa;
      }

      .action-btn {
        flex-shrink: 0;
        border: none;
        border-radius: 6px;
        padding: 4px 6px;
        font-size: 13px;
        cursor: pointer;
        opacity: 0.85;
      }

      .action-btn:hover {
        opacity: 1;
      }

      .action-btn.allow {
        background: #2ecc71;
        color: #000;
      }

      .action-btn.block {
        background: #e74c3c;
        color: #000;
      }

      /* Mobile tap safety */
      @media (max-width: 600px) {
        .action-btn {
          padding: 6px 8px;
          font-size: 14px;
        }
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

      .menu {
        position: relative;
      }

      #menuBtn {
        font-size: 20px;
        background: none;
        border: none;
        cursor: pointer;
        color: #eee;
      }

      .menu-content {
        display: none;
        position: absolute;
        top: 100%;
        right: 0;
        margin-top: 6px;
        background: #1e1e1e;
        border: 1px solid #444;
        border-radius: 6px;
        min-width: 160px;
        z-index: 1000;
      }

      .menu-item {
        padding: 12px 16px;
        cursor: pointer;
        font-size: 15px;
        color: #eee;
        white-space: nowrap;
      }

      .menu-item:hover {
        background: #333;
      }

      .menu-divider {
        height: 1px;
        background: #444;
        margin: 6px 0;
      }

      .overlay {
        display: none;
        position: fixed;
        inset: 0;
        background: rgba(0, 0, 0, 0.6);
        z-index: 2000;
      }

      .editor {
        background: #111;
        border-radius: 8px;
        width: min(800px, 90%);
        margin: 10vh auto;
        padding: 12px;
      }

      .editor textarea {
        width: 100%;
        height: 240px;
        background: #000;
        color: #eee;
        border: 1px solid #444;
        resize: vertical;
      }

      .editorButtons {
        display: flex;
        justify-content: flex-end;
        gap: 8px;
        margin-top: 8px;
      }
    </style>
  </head>

  <body>
    <div id="header">
      <h1>ESP_hole Dashboard</h1>
      <button id="menuBtn" title="Edit Lists">☰</button>
    </div>
    <div class="menu">
      <div id="menu" class="menu-content">
        <div class="menu-item" onclick="openEditor('rewrite')">✏️ Edit Rewrite</div>
        <div class="menu-item" onclick="openEditor('blocklist')">⛔ Edit Block List</div>
        <div class="menu-item" onclick="openEditor('whitelist')">✅ Edit Whitelist</div>
        <div class="menu-divider"></div>
        <div class="menu-item logs" onclick="openLogs()">📜 View Logs</div>
      </div>
    </div>

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
          <div>Avg Added Process Time</div>
          <div class="stat" id="processTime">0</div>
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
        <ul id="topq" class="top-list queried"></ul>
      </div>

      <div class="card">
        <h3>Top Blocked Domains</h3>
        <ul id="topb" class="top-list blocked"></ul>
      </div>
    </div>

    <div class="card">
      <h3>Memory</h3>
      <div>Free Heap: <span id="heapfree">0</span> bytes</div>
      <div>Min Heap: <span id="heapmin">0</span> bytes</div>

      <div id="editorOverlay" class="overlay">
        <div class="editor">
          <h3 id="editorTitle"></h3>
          <textarea id="editorArea"></textarea>

          <div class="editorButtons">
            <button id="editorOk" onclick="applyEditor()">OK</button>
            <button id="editorCancel" onclick="closeEditor()">Cancel</button>
          </div>
        </div>
      </div>

    </div>

    <script>
      let lastHours = null;
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
            Math.round(s.responseTime / s.total).toFixed(2) + ' ms';
          document.getElementById('processTime').textContent =
            Math.round(s.processTime / s.total).toFixed(2) + ' ms';
          document.getElementById('blocked').textContent = s.blocked;
          document.getElementById('percent').textContent =
            (s.blocked / s.total * 100).toFixed(1) + '%';

          drawChart(s.hours);
          lastHours = s.hours;
          renderTop('topq', s.top.queried, "allow");
          renderTop('topb', s.top.blocked, "block");
        } catch (e) {
          console.warn("Stats fetch failed:", e);
          document.getElementById("offlineState").style.display = "block";
          document.getElementById("dashboard").style.display = "none";
          document.getElementById("emptyState").style.display = "none";
        }
      }

      const canvas = document.getElementById("chart");
      const ctx = canvas.getContext("2d");
      const pad = { l: 36, r: 36, t: 20, b: 30 };
      let canvasSize = { width: 0, height: 0 };
      let hoverHour = -1;
      let lastHover = -1;

      function handlePointer(e) {
        const rect = canvas.getBoundingClientRect();
        const x = e.clientX - rect.left;

        const plotW = canvasSize.width - pad.l - pad.r;
        const hour = Math.floor(
          (x - pad.l) / (plotW / 24)
        );

        hoverHour = (hour >= 0 && hour < 24) ? hour : -1;

        if (hoverHour !== lastHover) {
          lastHover = hoverHour;
          drawChart(lastHours);
        }
      }

      canvas.addEventListener("mousemove", handlePointer);
      canvas.addEventListener("mouseleave", () => {
        hoverHour = -1;
        drawChart(lastHours);
      });

      canvas.addEventListener("touchmove", e => {
        handlePointer(e.touches[0]);
        e.preventDefault();
      }, { passive: false });

      function resizeCanvas() {
        const dpr = window.devicePixelRatio || 1;
        const rect = canvas.getBoundingClientRect();

        canvas.width = Math.round(rect.width * dpr);
        canvas.height = Math.round(rect.height * dpr);

        ctx.setTransform(1, 0, 0, 1, 0, 0);

        ctx.scale(dpr, dpr);

        canvasSize = { width: rect.width, height: rect.height };
      }

      let resizeTimer;
      window.addEventListener("resize", () => {
        clearTimeout(resizeTimer);
        resizeTimer = setTimeout(() => {
          resizeCanvas();
          if (lastHours) drawChart(lastHours);
        }, 200);
      });

      function drawChart(hours) {
        const width = canvasSize.width;
        const height = canvasSize.height;
        ctx.clearRect(0, 0, width, height);

        if (!hours || hours.length === 0) return;

        const plotW = width - pad.l - pad.r;
        const plotH = height - pad.t - pad.b;
        const stepX = plotW / 24;
        const barW = stepX * 0.7;

        const queries = hours.map(h => h.q || 0);
        const avgMs = hours.map(h => (h.q > 0) ? h.t / (h.q) : 0);

        const maxQ = Math.max(1, ...queries);
        const maxMs = Math.max(1, ...avgMs);

        drawAxes(width, height, plotH, maxQ, maxMs);
        drawBars(hours, plotH, stepX, barW, maxQ);
        drawAvgLine(avgMs, plotH, stepX, maxMs);
        drawHourLabels(hours, plotH, stepX);
        if (hoverHour >= 0) {
          drawTooltip(hours, hoverHour, plotH, plotW);
        }
      }

      function drawAxes(width, height, plotH, maxQ, maxMs) {
        ctx.strokeStyle = "#aaa";
        ctx.lineWidth = 1;

        ctx.beginPath();
        ctx.moveTo(pad.l, pad.t);
        ctx.lineTo(pad.l, pad.t + plotH);
        ctx.stroke();

        ctx.beginPath();
        ctx.moveTo(width - pad.r, pad.t);
        ctx.lineTo(width - pad.r, pad.t + plotH);
        ctx.stroke();

        ctx.beginPath();
        ctx.moveTo(pad.l, pad.t + plotH);
        ctx.lineTo(width - pad.r, pad.t + plotH);
        ctx.stroke();

        ctx.fillStyle = "#888";
        ctx.font = "11px system-ui";
        ctx.textBaseline = "middle";

        const numSteps = 5;
        const stepY = (plotH - 6) / numSteps;

        const xL = pad.l - 6;
        const xR = width - pad.r + 6;
        for (let i = 0; i <= numSteps; i++) {
          const y = pad.t + plotH - (i * stepY);
          ctx.textAlign = "right";
          const valQ = Math.round(i * (maxQ / numSteps))
          const labelQ = `${valQ}`;
          ctx.fillText(labelQ, xL, y);
          ctx.textAlign = "left";
          const valMS = Math.round(i * (maxMs / numSteps))
          const labelMs = `${valMS} ms`;
          ctx.fillText(labelMs, xR, y);
        }
      }

      function drawBars(hours, plotH, stepX, barW, maxQ) {
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

      function drawAvgLine(avgMs, plotH, stepX, maxMs) {
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
          !previousHasValue ? ctx.moveTo(x, y) : ctx.lineTo(x, y);
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

      function drawHourLabels(hours, plotH, stepX) {
        ctx.fillStyle = "#888";
        ctx.font = "11px system-ui, sans-serif";
        ctx.textAlign = "center";
        ctx.textBaseline = "top";

        // every 3 hours
        const start = hours.length - 1
        for (let i = start; i >= 0; i -= 3) {
          const label = (i >= start) ? "Now" : `-${start - i}h`;

          const x = pad.l + i * stepX + stepX / 2;
          const y = pad.t + plotH + 6;

          ctx.fillText(label, x, y);
        }
      }

      function drawTooltip(hours, i, plotH, plotW) {
        const h = hours[i];
        if (!h || !h.q || h.q === 0) return;

        const avgMs = h.q ? (h.t / h.q).toFixed(1) : "0";

        const lines = [
          `Queries: ${h.q}`,
          `Blocked: ${h.b}`,
          `Avg ms: ${avgMs}`
        ];

        const x = pad.l + (i + 0.5) * (plotW / 24);
        const y = pad.t + 10;

        ctx.font = "12px system-ui, sans-serif";
        const padding = 6;
        const lineH = 14;

        const w = Math.max(...lines.map(l => ctx.measureText(l).width)) + padding * 2;
        const hgt = lines.length * lineH + padding * 2;

        let tx = x - w / 2;
        let ty = y;

        // Keep on canvas
        if (tx < 4) tx = 4;
        if (tx + w > canvas.width) tx = canvas.width - w - 4;

        // Box
        ctx.fillStyle = "rgba(30,30,30,0.9)";
        ctx.fillRect(tx, ty, w, hgt);

        // Text
        ctx.fillStyle = "#fff";
        lines.forEach((l, n) => {
          ctx.fillText(l, tx + padding, ty + padding + (n + 1) * lineH - 4);
        });
      }

      function renderTop(id, list, mode) {
        const ul = document.getElementById(id);
        ul.innerHTML = '';
        list.sort((a, b) => b.c - a.c);

        list.forEach(e => {
          const li = document.createElement('li');
          const dom = document.createElement('span');
          dom.className = 'domain';
          dom.textContent = e.d;
          dom.title = e.d;

          const cnt = document.createElement('span');
          cnt.className = 'count';
          cnt.textContent = `(${e.c})`;

          const btn = document.createElement('button');
          btn.className = 'action-btn ' + (mode === 'block' ? 'allow' : 'block');
          btn.textContent = mode === 'block' ? '✅' : '⛔';
          btn.title = mode === 'block' ? 'Whitelist' : 'Block';

          btn.onclick = async () => {
            await fetch(
              mode === 'block' ? '/whitelist/add' : '/blocklist/add',
              {
                method: 'POST',
                headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
                body: 'domain=' + encodeURIComponent(e.d)
              }
            );
            li.remove();
          };

          if ((!e.u && mode === "block") || (e.u && mode != "block")) {
            li.append(dom, btn, cnt);
          } else {
            li.append(dom, cnt);
          }
          ul.appendChild(li);
        });
      }

      menuBtn.onclick = () => {
        menu.style.display = menu.style.display === 'block' ? 'none' : 'block';
      };

      document.addEventListener('click', e => {
        if (!menu.contains(e.target) && e.target !== menuBtn)
          menu.style.display = 'none';
      });

      let currentEdit = null;
      let cachedLists = {};

      async function openLogs() {
        currentEdit = "logs";
        editorOverlay.style.display = 'block';

        editorTitle.textContent = "ESP Logs";
        editorArea.readOnly = true;
        editorArea.value = "Loading logs...";

        document.getElementById("editorOk").style.display = "none";
        document.getElementById("editorCancel").textContent = "Close";

        try {
          const res = await fetch("/logs", { cache: "no-store" });
          editorArea.value = res.ok
            ? await res.text()
            : "Failed to load logs";
        } catch (e) {
          editorArea.value = "Error loading logs";
        }

        editorArea.scrollTop = editorArea.scrollHeight;
      }

      async function openEditor(type) {
        currentEdit = type;
        menu.style.display = 'none';

        const res = await fetch(`/list/${type}`);
        const data = await res.text();

        editorTitle.textContent =
          type === 'rewrite' ? 'Edit Rewrite Rules'
            : type === 'blocklist' ? 'Edit Block List'
              : 'Edit Whitelist';

        editorArea.readOnly = false;
        editorArea.value = data.trim();
        editorOverlay.style.display = 'block';

        const others = ['rewrite', 'blocklist', 'whitelist'].filter(x => x !== type);
        for (const o of others) {
          cachedLists[o] = (await fetch(`/list/${o}`).then(r => r.text()))
            .split('\n')
            .map(x => x.trim())
            .filter(Boolean);
        }
        document.getElementById("editorOk").style.display = "inline-block";
      }

      async function applyEditor() {
        const lines = editorArea.value
          .split('\n')
          .map(l => l.trim())
          .filter(Boolean);

        if (lines.length === 0) {
          if (!confirm(`Clear ${currentEdit} list?`)) return;
        }

        if (lines.length === 0) {
          const clearRes = await fetch(`/list/${currentEdit}`, {
            method: 'POST', headers: {
              "Content-Type": "text/plain"
            },
            body: "\0"
          });

          if (!clearRes.ok) {
            alert("Failed to clear list");
            return;
          }

          cachedLists[currentEdit] = [];
          closeEditor();
          loadStats();
          return;
        }

        if (currentEdit === "blocklist" && lines.length > 50) {
          alert("This block list is getting pretty long, consider re-running the preprocessing scripts to include these domains.");
        }

        const domains = lines.map(l => l.split(',')[0]);
        for (const [list, values] of Object.entries(cachedLists)) {
          if (list === currentEdit) continue;
          for (const d of domains) {
            if (values.some(v => v.startsWith(d + ',') || v === d)) {
              alert(`Conflict: "${d}" exists in ${list}`);
              return;
            }
            if (currentEdit === "rewrite" && values.some(v => !v.startsWith(d + ','))) {
              alert("Rewrite list should be in the form: 'mywebsitedomain.com,10.0.0.99' for each entry.");
              return;
            }
          }
        }

        const res = await fetch(`/list/${currentEdit}`, {
          method: 'POST',
          body: lines.join('\n')
        });

        if (!res.ok) {
          alert("Update failed");
          return;
        }

        cachedLists[currentEdit] = lines;
        closeEditor();
        loadStats();
      }

      function closeEditor() {
        editorOverlay.style.display = 'none';
        editorArea.readOnly = false;
        editorArea.value = "";
        document.getElementById("editorOk").style.display = "inline-block";
        document.getElementById("editorCancel").textContent = "Cancel";
        currentEdit = null;
      }

      resizeCanvas();
      loadStats();
      setInterval(loadStats, 30000); //30s
    </script>
  </body>

  </html>
  )rawliteral";