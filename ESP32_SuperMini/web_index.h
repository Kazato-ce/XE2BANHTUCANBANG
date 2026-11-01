/**
 * @file web_index.h
 * @brief Giao diện Web Dashboard điều khiển và giám sát Robot tự cân bằng
 *        Tự chứa 100% (HTML5 + CSS + JS thuần), không cần internet.
 */
#ifndef WEB_INDEX_H
#define WEB_INDEX_H

const char PAGE_MAIN[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="vi">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Robot Tự Cân Bằng - Dashboard & PID</title>
  <style>
    :root {
      --bg: #0f172a;
      --card-bg: #1e293b;
      --card-border: #334155;
      --accent: #38bdf8;
      --accent-hover: #0284c7;
      --text: #f8fafc;
      --text-muted: #94a3b8;
      --green: #22c55e;
      --yellow: #eab308;
      --red: #ef4444;
      --blue: #3b82f6;
    }
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body {
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
      background-color: var(--bg);
      color: var(--text);
      padding: 16px;
      display: flex;
      flex-direction: column;
      align-items: center;
      min-height: 100vh;
    }
    .container {
      width: 100%;
      max-width: 900px;
      display: flex;
      flex-direction: column;
      gap: 16px;
    }
    header {
      background: var(--card-bg);
      border: 1px solid var(--card-border);
      border-radius: 12px;
      padding: 16px 20px;
      display: flex;
      flex-wrap: wrap;
      justify-content: space-between;
      align-items: center;
      gap: 10px;
    }
    h1 { font-size: 1.25rem; font-weight: 700; color: var(--accent); }
    .badge-group { display: flex; gap: 8px; align-items: center; }
    .badge {
      font-size: 0.8rem;
      font-weight: 600;
      padding: 4px 10px;
      border-radius: 9999px;
      background: #334155;
      text-transform: uppercase;
      letter-spacing: 0.5px;
    }
    .badge.connected { background: rgba(34, 197, 94, 0.2); color: var(--green); border: 1px solid var(--green); }
    .badge.disconnected { background: rgba(239, 68, 68, 0.2); color: var(--red); border: 1px solid var(--red); }
    .badge.state-balance { background: rgba(34, 197, 94, 0.25); color: var(--green); border: 1px solid var(--green); }
    .badge.state-fallen { background: rgba(234, 179, 8, 0.25); color: var(--yellow); border: 1px solid var(--yellow); }
    .badge.state-fault { background: rgba(239, 68, 68, 0.25); color: var(--red); border: 1px solid var(--red); }
    .badge.state-idle { background: rgba(59, 130, 246, 0.25); color: var(--blue); border: 1px solid var(--blue); }

    .grid-metrics {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(130px, 1fr));
      gap: 12px;
    }
    .metric-card {
      background: var(--card-bg);
      border: 1px solid var(--card-border);
      border-radius: 10px;
      padding: 12px 14px;
      text-align: center;
    }
    .metric-title { font-size: 0.75rem; color: var(--text-muted); text-transform: uppercase; margin-bottom: 4px; }
    .metric-value { font-size: 1.5rem; font-weight: 700; color: #fff; font-variant-numeric: tabular-nums; }
    .metric-unit { font-size: 0.75rem; color: var(--text-muted); font-weight: normal; }

    .chart-card {
      background: var(--card-bg);
      border: 1px solid var(--card-border);
      border-radius: 12px;
      padding: 16px;
    }
    .chart-header { display: flex; justify-content: space-between; margin-bottom: 10px; font-size: 0.9rem; font-weight: 600; }
    canvas { width: 100%; height: 180px; display: block; border-radius: 6px; background: #0b1120; }

    .control-card {
      background: var(--card-bg);
      border: 1px solid var(--card-border);
      border-radius: 12px;
      padding: 18px;
    }
    .card-title { font-size: 1rem; font-weight: 700; margin-bottom: 14px; color: var(--accent); display: flex; justify-content: space-between; align-items: center; }

    .pid-row {
      display: grid;
      grid-template-columns: 80px 1fr 90px;
      align-items: center;
      gap: 12px;
      margin-bottom: 14px;
    }
    .pid-label { font-size: 0.9rem; font-weight: 600; }
    input[type=range] {
      width: 100%;
      accent-color: var(--accent);
      cursor: pointer;
    }
    input[type=number] {
      background: #0f172a;
      border: 1px solid var(--card-border);
      border-radius: 6px;
      color: #fff;
      padding: 6px 8px;
      font-size: 0.95rem;
      font-weight: 600;
      text-align: right;
      width: 100%;
    }
    .btn-row {
      display: flex;
      flex-wrap: wrap;
      gap: 10px;
      margin-top: 16px;
    }
    button {
      flex: 1;
      min-width: 120px;
      padding: 10px 16px;
      border: none;
      border-radius: 8px;
      font-size: 0.9rem;
      font-weight: 600;
      cursor: pointer;
      transition: background 0.15s ease;
    }
    .btn-primary { background: var(--accent); color: #0f172a; }
    .btn-primary:hover { background: var(--accent-hover); }
    .btn-preset { background: #334155; color: #fff; }
    .btn-preset:hover { background: #475569; }
    .btn-danger { background: var(--red); color: #fff; }
    .btn-danger:hover { background: #dc2626; }

    .toast {
      position: fixed;
      bottom: 20px;
      right: 20px;
      background: #047857;
      color: #fff;
      padding: 10px 16px;
      border-radius: 8px;
      font-size: 0.85rem;
      font-weight: 600;
      opacity: 0;
      transition: opacity 0.3s;
      pointer-events: none;
      z-index: 100;
    }
    .toast.show { opacity: 1; }
  </style>
</head>
<body>
  <div class="container">
    <header>
      <div>
        <h1>Xe Tự Cân Bằng 2 Bánh</h1>
        <small style="color:var(--text-muted)">STM32F412 &times; ESP32-S3 Telemetry</small>
      </div>
      <div class="badge-group">
        <span id="badge-link" class="badge disconnected">Chưa kết nối STM32</span>
        <span id="badge-state" class="badge state-idle">STATE: --</span>
      </div>
    </header>

    <!-- Metrics MPU6050 & Motion -->
    <div class="grid-metrics">
      <div class="metric-card">
        <div class="metric-title">Góc Pitch (&theta;)</div>
        <div class="metric-value" id="val-angle">0.0<span class="metric-unit">&deg;</span></div>
      </div>
      <div class="metric-card">
        <div class="metric-title">Tốc độ Gyro</div>
        <div class="metric-value" id="val-rate">0.0<span class="metric-unit">&deg;/s</span></div>
      </div>
      <div class="metric-card">
        <div class="metric-title">Góc Accel</div>
        <div class="metric-value" id="val-acc">0.0<span class="metric-unit">&deg;</span></div>
      </div>
      <div class="metric-card">
        <div class="metric-title">Tốc độ (v_cmd)</div>
        <div class="metric-value" id="val-vcmd">0<span class="metric-unit"> sps</span></div>
      </div>
      <div class="metric-card">
        <div class="metric-title">Gia tốc (a_cmd)</div>
        <div class="metric-value" id="val-acmd">0<span class="metric-unit"> s&sup2;</span></div>
      </div>
      <div class="metric-card">
        <div class="metric-title">Chu kỳ Loop</div>
        <div class="metric-value" id="val-dt">0<span class="metric-unit">&mu;s</span></div>
      </div>
    </div>

    <!-- Real-time Chart -->
    <div class="chart-card">
      <div class="chart-header">
        <span>Đồ thị Góc Nghiêng Thời Gian Thực (Pitch vs Setpoint)</span>
        <span style="color:var(--accent); font-size:0.8rem">&bull; Xanh: Angle &nbsp; &bull; Xám: Setpoint</span>
      </div>
      <canvas id="chartCanvas" width="800" height="180"></canvas>
    </div>

    <!-- PID Tuning Controls -->
    <div class="control-card">
      <div class="card-title">
        <span>Điều Chỉnh Bộ Điều Khiển PID</span>
        <label style="font-size:0.8rem; font-weight:normal; cursor:pointer; color:var(--text-muted)">
          <input type="checkbox" id="chk-autosend" checked> Tự gửi khi chỉnh slider
        </label>
      </div>

      <div class="pid-row">
        <div class="pid-label">Kp:</div>
        <input type="range" id="slider-kp" min="0" max="3000" step="10" value="600">
        <input type="number" id="num-kp" min="0" max="5000" step="1" value="600">
      </div>

      <div class="pid-row">
        <div class="pid-label">Ki:</div>
        <input type="range" id="slider-ki" min="0" max="50" step="0.1" value="0.0">
        <input type="number" id="num-ki" min="0" max="100" step="0.1" value="0.0">
      </div>

      <div class="pid-row">
        <div class="pid-label">Kd:</div>
        <input type="range" id="slider-kd" min="0" max="150" step="0.5" value="20.0">
        <input type="number" id="num-kd" min="0" max="200" step="0.1" value="20.0">
      </div>

      <div class="pid-row">
        <div class="pid-label">Trim (&deg;):</div>
        <input type="range" id="slider-trim" min="-5.0" max="5.0" step="0.05" value="0.0">
        <input type="number" id="num-trim" min="-10.0" max="10.0" step="0.05" value="0.0">
      </div>

      <div class="btn-row">
        <button class="btn-primary" onclick="sendPID()">GỬI THÔNG SỐ PID</button>
        <button class="btn-preset" onclick="applyPreset(600, 0, 20, 0)">Preset Chuẩn</button>
        <button class="btn-preset" onclick="applyPreset(400, 0, 15, 0)">Preset Êm</button>
        <button class="btn-preset" onclick="applyPreset(800, 0.5, 28, 0)">Preset Cứng</button>
        <button class="btn-danger" onclick="stopRobot()">DỪNG KHẨN CẤP</button>
      </div>
    </div>
  </div>

  <div id="toast" class="toast">Đã cập nhật PID thành công!</div>

  <script>
    const STATE_NAMES = ["INIT", "CALIBRATE", "IDLE", "BALANCE", "FALLEN", "FAULT"];
    const STATE_CLASSES = ["state-idle", "state-idle", "state-idle", "state-balance", "state-fallen", "state-fault"];

    // Canvas chart data buffer
    const MAX_POINTS = 160;
    const historyData = [];
    const canvas = document.getElementById("chartCanvas");
    const ctx = canvas.getContext("2d");

    let autoSendTimeout = null;
    let lastValidTime = 0;

    function syncPairs(sliderId, numId) {
      const s = document.getElementById(sliderId);
      const n = document.getElementById(numId);
      s.addEventListener("input", () => {
        n.value = s.value;
        onParamChanged();
      });
      n.addEventListener("input", () => {
        s.value = n.value;
        onParamChanged();
      });
    }
    syncPairs("slider-kp", "num-kp");
    syncPairs("slider-ki", "num-ki");
    syncPairs("slider-kd", "num-kd");
    syncPairs("slider-trim", "num-trim");

    function onParamChanged() {
      if (document.getElementById("chk-autosend").checked) {
        clearTimeout(autoSendTimeout);
        autoSendTimeout = setTimeout(sendPID, 250);
      }
    }

    function applyPreset(kp, ki, kd, trim) {
      document.getElementById("slider-kp").value = kp; document.getElementById("num-kp").value = kp;
      document.getElementById("slider-ki").value = ki; document.getElementById("num-ki").value = ki;
      document.getElementById("slider-kd").value = kd; document.getElementById("num-kd").value = kd;
      document.getElementById("slider-trim").value = trim; document.getElementById("num-trim").value = trim;
      sendPID();
    }

    function showToast(msg, bg="#047857") {
      const t = document.getElementById("toast");
      t.innerText = msg;
      t.style.background = bg;
      t.classList.add("show");
      setTimeout(() => t.classList.remove("show"), 2000);
    }

    function sendPID() {
      const kp = parseFloat(document.getElementById("num-kp").value) || 0;
      const ki = parseFloat(document.getElementById("num-ki").value) || 0;
      const kd = parseFloat(document.getElementById("num-kd").value) || 0;
      const trim = parseFloat(document.getElementById("num-trim").value) || 0;

      fetch("/api/pid", {
        method: "POST",
        headers: { "Content-Type": "application/x-www-form-urlencoded" },
        body: `kp=${kp}&ki=${ki}&kd=${kd}&trim=${trim}`
      })
      .then(r => r.text())
      .then(txt => showToast("Đã gửi PID: Kp=" + kp + " Kd=" + kd))
      .catch(err => showToast("Lỗi gửi PID: " + err, "#dc2626"));
    }

    function stopRobot() {
      // Đặt Kp=0, Ki=0, Kd=0 để ngắt lực cân bằng ngay
      applyPreset(0, 0, 0, 0);
      showToast("ĐÃ GỬI LỆNH DỪNG ĐỘNG CƠ!", "#dc2626");
    }

    function drawChart(currentAngle, setpoint) {
      historyData.push({ angle: currentAngle, sp: setpoint });
      if (historyData.length > MAX_POINTS) historyData.shift();

      const w = canvas.width;
      const h = canvas.height;
      ctx.clearRect(0, 0, w, h);

      // Gridlines: 0 deg (center), +20 deg, -20 deg
      const midY = h / 2;
      const scaleY = h / 50; // +-25 deg range

      ctx.strokeStyle = "#1e293b";
      ctx.lineWidth = 1;
      ctx.beginPath();
      ctx.moveTo(0, midY); ctx.lineTo(w, midY); // 0 line
      ctx.moveTo(0, midY - 20 * scaleY); ctx.lineTo(w, midY - 20 * scaleY);
      ctx.moveTo(0, midY + 20 * scaleY); ctx.lineTo(w, midY + 20 * scaleY);
      ctx.stroke();

      // Setpoint line
      ctx.strokeStyle = "#64748b";
      ctx.setLineDash([4, 4]);
      ctx.beginPath();
      for (let i = 0; i < historyData.length; i++) {
        const x = (i / (MAX_POINTS - 1)) * w;
        const y = midY - (historyData[i].sp * scaleY);
        if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
      }
      ctx.stroke();
      ctx.setLineDash([]);

      // Angle line
      ctx.strokeStyle = "#38bdf8";
      ctx.lineWidth = 2;
      ctx.beginPath();
      for (let i = 0; i < historyData.length; i++) {
        const x = (i / (MAX_POINTS - 1)) * w;
        const y = midY - (historyData[i].angle * scaleY);
        if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
      }
      ctx.stroke();
    }

    // Fetch telemetry data loop
    let firstLoad = true;
    function pollTelemetry() {
      fetch("/api/data")
        .then(r => r.json())
        .then(d => {
          lastValidTime = Date.now();
          const badgeLink = document.getElementById("badge-link");
          badgeLink.innerText = "STM32: OK (20Hz)";
          badgeLink.className = "badge connected";

          // Update State
          const badgeState = document.getElementById("badge-state");
          const stName = STATE_NAMES[d.state] || "UNKNOWN";
          badgeState.innerText = "STATE: " + stName;
          badgeState.className = "badge " + (STATE_CLASSES[d.state] || "state-idle");

          // Update metrics
          document.getElementById("val-angle").innerHTML = d.angle.toFixed(1) + '<span class="metric-unit">&deg;</span>';
          document.getElementById("val-rate").innerHTML = d.rate.toFixed(1) + '<span class="metric-unit">&deg;/s</span>';
          document.getElementById("val-acc").innerHTML = d.acc_angle.toFixed(1) + '<span class="metric-unit">&deg;</span>';
          document.getElementById("val-vcmd").innerHTML = Math.round(d.v_cmd) + '<span class="metric-unit"> sps</span>';
          document.getElementById("val-acmd").innerHTML = Math.round(d.a_cmd) + '<span class="metric-unit"> s&sup2;</span>';
          document.getElementById("val-dt").innerHTML = d.dt_us + '<span class="metric-unit">&mu;s</span>';

          // Initialize PID inputs from STM32 once on first connection
          if (firstLoad && d.kp > 0) {
            document.getElementById("slider-kp").value = d.kp; document.getElementById("num-kp").value = d.kp;
            document.getElementById("slider-ki").value = d.ki; document.getElementById("num-ki").value = d.ki;
            document.getElementById("slider-kd").value = d.kd; document.getElementById("num-kd").value = d.kd;
            document.getElementById("slider-trim").value = d.trim; document.getElementById("num-trim").value = d.trim;
            firstLoad = false;
          }

          drawChart(d.angle, d.trim || 0);
        })
        .catch(e => {
          if (Date.now() - lastValidTime > 1500) {
            const badgeLink = document.getElementById("badge-link");
            badgeLink.innerText = "Mất kết nối STM32";
            badgeLink.className = "badge disconnected";
          }
        });
    }

    // Poll every 80ms (~12.5 FPS)
    setInterval(pollTelemetry, 80);
  </script>
</body>
</html>
)rawliteral";

#endif // WEB_INDEX_H

