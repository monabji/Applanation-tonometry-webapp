#include <WiFi.h>
#include <WebServer.h>

const char* ssid = "YOUR_WIFI_NAME";
const char* password = "YOUR_WIFI_PASSWORD";

const int fsrPin = 33;       // FSR voltage-divider junction
const int buzzerPin = 25;

const float GAMMA = 2.2;     // Higher = less sensitive at light pressure
const unsigned long FILTER_INTERVAL_MS = 50;
const float FILTER_ALPHA = 0.15;   // Lower = slower/smoother response
const unsigned long SEND_INTERVAL_MS = 100;

const int TARGET = 2000;
const int TOLERANCE = 500;

float smoothed = 0;
int baseline = 0;
bool wasAligned = false;

WebServer server(80);
WiFiClient sseClient;
bool sseActive = false;

// Manav Bhavin Soni (CSE Core) VIT Vellore
const char htmlPage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>IOP Tonometer Trainer</title>
  <style>
    @import url('https://fonts.googleapis.com/css2?family=IBM+Plex+Mono:wght@400;500;600&display=swap');

    * { box-sizing: border-box; }
    body {
      font-family: "IBM Plex Mono", monospace;
      text-align: center;
      background: #0a0a0a;
      color: #f2f2f2;
      margin: 0;
      padding: 36px 20px;
    }
    h2 {
      margin: 12px 0 22px;
      font-family: "IBM Plex Mono", monospace;
      font-size: 1.55rem;
      font-weight: 600;
      letter-spacing: -0.035em;
    }
    .connectionStatus {
      display: inline-flex;
      align-items: center;
      gap: 8px;
      color: #aebbd0;
      font-size: 0.78rem;
      font-weight: 500;
      letter-spacing: 0.02em;
    }
    .connectionIcon { width: 18px; height: 18px; flex: 0 0 auto; }
    .connectionStatus.connected { color: #7ff5aa; }
    .connectionStatus.connected .connectionIcon {
      filter: drop-shadow(0 0 5px rgba(93, 255, 139, 0.7));
      animation: signalPulse 1.8s ease-in-out infinite;
    }
    .connectionStatus.disconnected { color: #ff9a9f; }
    #miresBox {
      width: 360px;
      height: auto;
      aspect-ratio: 1;
      margin: 0 auto 22px;
      border-radius: 50%;
      filter: drop-shadow(0 18px 30px rgba(0, 72, 255, 0.24));
    }
    svg { display: block; width: 100%; height: 100%; overflow: visible; }
    .live { animation: liveSignal 2.8s ease-in-out infinite; }
    .mire {
      fill: none;
      stroke: #73ff00;
      stroke-width: 5;
      stroke-linecap: round;
      filter: drop-shadow(0 0 5px rgba(115, 255, 0, 0.45));
      transition: transform 0.35s ease-out;
    }
    #statusPanel {
      width: min(500px, 92vw);
      margin: 0 auto;
      padding: 20px 22px;
      border: 1px solid rgba(255, 255, 255, 0.16);
      border-radius: 8px;
      background: #202020;
      box-shadow: 0 12px 30px rgba(0, 0, 0, 0.2);
    }
    #status { font-size: 1.35rem; margin-top: 0; font-weight: 600; letter-spacing: 0.01em; }
    #val {
      font-size: 0.95rem;
      margin: 0;
      color: #bdbdbd;
      font-weight: 400;
      font-family: "IBM Plex Mono", monospace;
      font-variant-numeric: tabular-nums;
    }
    .metrics { display: flex; justify-content: center; gap: 12px; margin-top: 14px; }
    .metric { min-width: 150px; padding: 12px 16px; border-radius: 6px; background: rgba(255, 255, 255, 0.06); }
    .metricLabel { display: block; margin-bottom: 4px; color: #888; font-size: 0.7rem; font-weight: 500; letter-spacing: 0.08em; text-transform: uppercase; }
    .metricValue { display: block; color: #f2f2f2; font-size: 0.95rem; font-family: "IBM Plex Mono", monospace; font-variant-numeric: tabular-nums; font-weight: 500; }
    .metricHint { display: block; margin-top: 7px; color: #8d8d8d; font-size: 0.68rem; font-weight: 400; line-height: 1.4; }
    .aligned, .low, .high { color: #f5f5f5; }
    #miresBox.aligned { filter: drop-shadow(0 18px 34px rgba(0, 255, 126, 0.28)); }
    #miresBox.aligned .mire { filter: drop-shadow(0 0 8px rgba(93, 255, 139, 0.92)); }
    #barContainer {
      position: relative;
      width: 80%;
      max-width: 500px;
      height: 14px;
      padding: 3px;
      background: #172033;
      border: 1px solid rgba(255, 255, 255, 0.1);
      margin: 20px auto 0;
      border-radius: 4px;
      overflow: hidden;
      box-shadow: inset 0 2px 5px rgba(0, 0, 0, 0.45), 0 6px 18px rgba(0, 0, 0, 0.22);
    }
    #bar {
      height: 100%;
      width: 0%;
      border-radius: 4px;
      background: linear-gradient(90deg, #20d8ff 0%, #43f59c 46%, #ffe45c 72%, #ff5f68 100%);
      box-shadow: 0 0 12px rgba(67, 245, 156, 0.42);
      transition: width 0.25s ease-out;
    }
    .barLabels { display: flex; justify-content: space-between; width: 80%; max-width: 500px; margin: 7px auto 0; color: #71809a; font-size: 0.68rem; font-weight: 500; }
    #infoBox {
      width: min(500px, 92vw);
      margin: 16px auto 0;
      padding: 14px 18px;
      border: 1px solid rgba(255, 255, 255, 0.12);
      border-radius: 6px;
      background: #151515;
      text-align: left;
    }
    .infoTitle { display: block; margin-bottom: 7px; color: #f2f2f2; font-size: 0.78rem; font-weight: 600; letter-spacing: 0.08em; text-transform: uppercase; }
    .infoText { margin: 0; color: #a5a5a5; font-size: 0.72rem; font-weight: 400; line-height: 1.55; }
    #watermark { position: fixed; left: 12px; bottom: 8px; z-index: 9; color: rgba(255, 255, 255, 0.34); font-size: 0.58rem; font-weight: 400; pointer-events: none; }
    #demoControl {
      position: fixed;
      right: 20px;
      bottom: 20px;
      z-index: 10;
      width: min(270px, calc(100vw - 28px));
      padding: 12px 14px;
      border: 1px solid rgba(255, 255, 255, 0.16);
      border-radius: 6px;
      background: #202020;
      box-shadow: 0 10px 24px rgba(0, 0, 0, 0.28);
      text-align: left;
    }
    .demoHeader { display: flex; justify-content: space-between; align-items: baseline; gap: 12px; margin-bottom: 8px; }
    .demoLabel { color: #bdbdbd; font-size: 0.72rem; font-weight: 500; letter-spacing: 0.06em; text-transform: uppercase; }
    #demoValue { color: #f2f2f2; font-size: 0.85rem; font-weight: 500; font-variant-numeric: tabular-nums; }
    #demoSlider { display: block; width: 100%; margin: 0; accent-color: #43f59c; cursor: pointer; }
    @keyframes signalPulse {
      0%, 100% { transform: scale(1); opacity: 0.8; }
      50% { transform: scale(1.35); opacity: 1; }
    }
    @keyframes liveSignal {
      0%, 100% { transform: scale(1); }
      50% { transform: scale(1.012); }
    }
    @media (max-width: 520px) {
      body { padding: 24px 14px; }
      #miresBox { width: min(360px, 82vw); }
      .metrics { gap: 8px; }
      .metric { min-width: 0; flex: 1; padding: 10px 8px; }
      #demoControl { right: 14px; bottom: 14px; }
    }
  </style>
</head>
<body>
  <div id="connectionStatus" class="connectionStatus">
    <svg class="connectionIcon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true">
      <path d="M5 13a10 10 0 0 1 14 0" />
      <path d="M8.5 16.5a5 5 0 0 1 7 0" />
      <path d="M2 8.82a15 15 0 0 1 20 0" />
      <line x1="12" y1="20" x2="12.01" y2="20" />
    </svg>
    <span id="connectionText">Connecting to tonometer...</span>
  </div>
  <h2>Goldmann Mires: Applanation Trainer</h2>

  <div id="miresBox">
    <svg viewBox="0 0 360 360" aria-label="Goldmann applanation mires">
      <circle cx="180" cy="180" r="145" fill="#162457" fill-opacity="0.34" stroke="#8cecff" stroke-width="6" opacity="0.9" />
      <path id="topMire" class="mire" d="M 92 180 A 44 44 0 0 1 180 180" />
      <path id="bottomMire" class="mire" d="M 180 180 A 44 44 0 0 0 268 180" />
    </svg>
  </div>

  <div id="statusPanel">
    <div id="status">Waiting for tonometer input</div>
    <div class="metrics">
      <div class="metric">
        <span class="metricLabel">Current pressure</span>
        <span id="val" class="metricValue">0.0 mmHg</span>
        <span class="metricHint">Normal eye pressure: 10 to 21 mmHg</span>
      </div>
    </div>
  </div>

  <div id="barContainer"><div id="bar"></div></div>
  <div class="barLabels" aria-hidden="true"><span>0</span><span>1000</span><span>2000</span><span>3000</span><span>4095</span></div>
  <div id="infoBox">
    <span class="infoTitle">Information</span>
    <p class="infoText">The Demo ADC slider at the bottom simulates the hardware. Once the hardware is connected, the status will show “Hardware connected,” and you can operate the web app using the hardware.</p>
  </div>
  <div id="demoControl">
    <div class="demoHeader">
      <span class="demoLabel">Demo ADC</span>
      <span id="demoValue">2000</span>
    </div>
    <input id="demoSlider" type="range" min="0" max="4095" value="2000" step="1" aria-label="Demo ADC value">
  </div>
  <!-- Manav Bhavin Soni (CSE Core) VIT Vellore -->
  <div id="watermark">Manav Bhavin Soni (CSE Core) VIT Vellore</div>

  <script>
    const topMire = document.getElementById("topMire");
    const bottomMire = document.getElementById("bottomMire");
    const miresBox = document.getElementById("miresBox");
    const status = document.getElementById("status");
    const valueDisplay = document.getElementById("val");
    const bar = document.getElementById("bar");
    const connectionStatus = document.getElementById("connectionStatus");
    const connectionText = document.getElementById("connectionText");
    const demoSlider = document.getElementById("demoSlider");
    const demoValue = document.getElementById("demoValue");

    const adcPerMmHg = 100;
    const normalMinMmHg = 10;
    const normalMaxMmHg = 21;
    const minAlignedAdc = normalMinMmHg * adcPerMmHg;
    const maxAlignedAdc = normalMaxMmHg * adcPerMmHg;
    const target = (minAlignedAdc + maxAlignedAdc) / 2;
    const tolerance = (maxAlignedAdc - minAlignedAdc) / 2;
    const maxDiff = Math.max(target, 4095 - target);
    const maxOffset = 40;

    function adcToMmHg(adc) {
      return adc / adcPerMmHg;
    }

    function computeOffset(value) {
      const diff = value - target;
      const absDiff = Math.abs(diff);
      const direction = diff === 0 ? 0 : (diff > 0 ? 1 : -1);
      const excess = Math.max(0, absDiff - tolerance);
      const t = Math.min(excess / (maxDiff - tolerance), 1);
      const eased = t * t * (3 - 2 * t);
      const movement = eased * maxOffset;

      return {
        topX: direction * movement,
        bottomX: -direction * movement
      };
    }

    const evtSource = new EventSource("/events");
    evtSource.onopen = function() {
      connectionStatus.className = "connectionStatus connected";
      connectionText.textContent = "Hardware connected";
    };
    evtSource.onerror = function() {
      connectionStatus.className = "connectionStatus disconnected";
      connectionText.textContent = evtSource.readyState === EventSource.CONNECTING ? "Connecting to tonometer..." : "Connection lost";
    };

    function renderValue(value) {
      valueDisplay.textContent = adcToMmHg(value).toFixed(1) + " mmHg";
      bar.style.width =
        Math.min(100, Math.max(0, value / 4095 * 100)) + "%";

      const pos = computeOffset(value);
      topMire.style.transform = `translateX(${pos.topX}px)`;
      bottomMire.style.transform = `translateX(${pos.bottomX}px)`;
      miresBox.classList.add("live");

      if (value >= minAlignedAdc &&
          value <= maxAlignedAdc) {
        status.textContent = "Fig. 3: ALIGNED / CORRECT";
        status.className = "aligned";
        miresBox.classList.add("aligned");
      } else if (value < target) {
        status.textContent = "Fig. 2: LOW PRESSURE";
        status.className = "low";
        miresBox.classList.remove("aligned");
      } else {
        status.textContent = "Fig. 1: HIGH PRESSURE";
        status.className = "high";
        miresBox.classList.remove("aligned");
      }
    }
    evtSource.onmessage = function(event) {
      renderValue(parseInt(event.data, 10));
    };
    demoSlider.addEventListener("input", function() {
      const value = parseInt(demoSlider.value, 10);
      demoValue.textContent = value;
      renderValue(value);
    });
  </script>
</body>
</html>
)rawliteral";

void handleRoot() {
  server.send_P(200, "text/html", htmlPage);
}

void handleEvents() {
  WiFiClient client = server.client();

  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/event-stream");
  client.println("Cache-Control: no-cache");
  client.println("Connection: keep-alive");
  client.println();

  sseClient = client;
  sseActive = true;
}

void setup() {
  Serial.begin(115200);

  pinMode(fsrPin, INPUT);
  pinMode(buzzerPin, OUTPUT);

  analogReadResolution(12);
  analogSetPinAttenuation(fsrPin, ADC_11db);

  WiFi.begin(ssid, password);

  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi connected.");
  Serial.print("Open this address in a browser: http://");
  Serial.println(WiFi.localIP());

  // Keep the FSR untouched during this calibration.
  Serial.println("Calibrating baseline. Do not touch the FSR...");
  long total = 0;
  const int samples = 50;

  for (int i = 0; i < samples; i++) {
    total += analogRead(fsrPin);
    delay(10);
  }

  baseline = total / samples;
  smoothed = baseline;

  Serial.print("Baseline = ");
  Serial.println(baseline);

  server.on("/", handleRoot);
  server.on("/events", handleEvents);
  server.begin();
}

void loop() {
  server.handleClient();

  static unsigned long lastFilter = 0;
  static unsigned long lastSend = 0;
  static unsigned long lastSerial = 0;

  unsigned long now = millis();

  // Read and smooth at a fixed interval, making ring motion controllably slow.
  if (now - lastFilter >= FILTER_INTERVAL_MS) {
    lastFilter = now;

    int raw = analogRead(fsrPin);

    int delta = raw - baseline;
    if (delta < 0) delta = 0;

    float normalized = (float)delta / (4095.0 - baseline);
    normalized = constrain(normalized, 0.0f, 1.0f);

    // Makes gentle presses less sensitive but retains full-scale reach.
    float curved = pow(normalized, GAMMA);

    int scaledRaw = baseline + (int)(curved * (4095 - baseline));
    scaledRaw = constrain(scaledRaw, 0, 4095);

    smoothed += FILTER_ALPHA * (scaledRaw - smoothed);
  }

  int value = (int)smoothed;

  bool isAligned =
    value >= TARGET - TOLERANCE &&
    value <= TARGET + TOLERANCE;

  // One beep when entering the correct range.
  if (isAligned && !wasAligned) {
    tone(buzzerPin, 2500, 150);
  }
  wasAligned = isAligned;

  if (now - lastSerial >= 100) {
    lastSerial = now;
    Serial.print("Processed ADC = ");
    Serial.println(value);
  }

  if (sseActive &&
      sseClient.connected() &&
      now - lastSend >= SEND_INTERVAL_MS) {
    lastSend = now;
    sseClient.printf("data: %d\n\n", value);
  }

  if (sseActive && !sseClient.connected()) {
    sseActive = false;
  }
}
