#include <WiFi.h>
#include <WebServer.h>

// ==========================================
// ⚠️ ENTER YOUR WIFI DETAILS HERE ⚠️
// ==========================================
const char* ssid = "YOUR_WIFI_NAME";
const char* password = "YOUR_WIFI_PASSWORD";

WebServer server(80);

// Motor Pins
#define ENA 33
#define ENB 32
#define IN1 26
#define IN2 27
#define IN3 14
#define IN4 12
#define VACUUM 25

// Sensor Pins
#define TRIG 5
#define ECHO 18
#define IR_FRONT_LEFT 34
#define IR_FRONT_RIGHT 35

// Variables
long distanceRear = 0;
int irLeftState = HIGH;
int irRightState = HIGH;
int motorSpeed = 180; // Adjusted for better control
String currentMode = "IDLE";
bool autoMode = false;

// Auto AI State Machine
unsigned long autoActionTime = 0;
int autoState = 0; // 0=Fwd, 1=Rev, 2=Turn

// ==========================================
// THE WEB APP UI (HTML/CSS/JS)
// ==========================================
const char htmlPage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1, user-scalable=0">
<title>CleanBot AI Dashboard</title>
<style>
  body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background: #0f172a; color: #f8fafc; text-align: center; margin:0; padding:15px; user-select: none; }
  .panel { background: #1e293b; padding: 25px; border-radius: 20px; box-shadow: 0 10px 25px rgba(0,0,0,0.5); max-width: 400px; margin: 0 auto; }
  h2 { margin-top: 0; color: #38bdf8; font-weight: 800; text-transform: uppercase; letter-spacing: 1px; }
  .btn { background: #3b82f6; color: white; border: none; padding: 20px; font-size: 24px; border-radius: 12px; margin: 5px; cursor: pointer; width: 85px; transition: transform 0.1s; box-shadow: 0 4px 6px rgba(0,0,0,0.3); }
  .btn:active { transform: scale(0.90); background: #2563eb; }
  .btn-wide { width: 100%; background: #10b981; margin-top: 15px; font-weight: bold; font-size: 16px; padding: 15px; }
  .btn-vac { background: #ef4444; }
  .btn-stop { background: #475569; }
  .status-box { background: #0f172a; padding: 15px; border-radius: 12px; margin: 20px 0; font-family: monospace; font-size: 16px; text-align: left; border: 1px solid #334155; }
  .status-box div { margin: 8px 0; }
  .alert { color: #f43f5e; font-weight: bold; animation: blink 1s infinite; }
  @keyframes blink { 50% { opacity: 0.5; } }
  .grid { display: grid; grid-template-columns: 1fr 1fr 1fr; gap: 8px; justify-items: center; margin-bottom: 25px; }
</style>
</head>
<body>
<div class="panel">
  <h2>🤖 CleanBot AI Panel</h2>
  
  <!-- SENSOR TELEMETRY DISPLAY -->
  <div class="status-box">
    <div>State: <span id="mode" style="color:#10b981; font-weight:bold;">IDLE</span></div>
    <div>Rear Dist (Sonic): <span id="dist">--</span> cm</div>
    <div>Front L (IR): <span id="irl">--</span></div>
    <div>Front R (IR): <span id="irr">--</span></div>
  </div>

  <!-- MANUAL CONTROLS -->
  <div class="grid">
    <div></div><button class="btn" onmousedown="snd('F')" onmouseup="snd('S')" ontouchstart="snd('F')" ontouchend="snd('S')">▲</button><div></div>
    <button class="btn" onmousedown="snd('L')" onmouseup="snd('S')" ontouchstart="snd('L')" ontouchend="snd('S')">◀</button>
    <button class="btn btn-stop" onclick="snd('S')">■</button>
    <button class="btn" onmousedown="snd('R')" onmouseup="snd('S')" ontouchstart="snd('R')" ontouchend="snd('S')">▶</button>
    <div></div><button class="btn" onmousedown="snd('B')" onmouseup="snd('S')" ontouchstart="snd('B')" ontouchend="snd('S')">▼</button><div></div>
  </div>

  <!-- AI AND VACUUM CONTROLS -->
  <button class="btn btn-wide" onclick="snd('AUTO')">⚡ START AI AUTO-CLEAN</button>
  <div style="display:flex; gap:10px;">
    <button class="btn btn-wide btn-vac" onclick="snd('V1')">🌪️ VAC ON</button>
    <button class="btn btn-wide btn-stop" onclick="snd('V0')">VAC OFF</button>
  </div>
</div>

<script>
  function snd(cmd) { fetch('/cmd?c=' + cmd); }
  
  // Polling telemetry data every 400ms
  setInterval(() => {
    fetch('/telemetry').then(r => r.json()).then(d => {
      document.getElementById('dist').innerText = d.dist;
      document.getElementById('mode').innerText = d.mode;
      
      let lState = d.irL == 0 ? '<span class="alert">OBSTACLE!</span>' : 'CLEAR';
      let rState = d.irR == 0 ? '<span class="alert">OBSTACLE!</span>' : 'CLEAR';
      
      document.getElementById('irl').innerHTML = lState;
      document.getElementById('irr').innerHTML = rState;
    }).catch(e => console.log(e));
  }, 400);
</script>
</body>
</html>
)rawliteral";

// ==========================================
// HARDWARE FUNCTIONS
// ==========================================
long readUltrasonic() {
  digitalWrite(TRIG, LOW); delayMicroseconds(2);
  digitalWrite(TRIG, HIGH); delayMicroseconds(10);
  digitalWrite(TRIG, LOW);
  long duration = pulseIn(ECHO, HIGH, 30000);
  if (duration == 0) return 999;
  return duration * 0.034 / 2;
}

void moveForward() { ledcWrite(ENA, motorSpeed); ledcWrite(ENB, motorSpeed); digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH); digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH); }
void moveBackward() { ledcWrite(ENA, motorSpeed); ledcWrite(ENB, motorSpeed); digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW); digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW); }
void turnLeft() { ledcWrite(ENA, motorSpeed); ledcWrite(ENB, motorSpeed); digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH); digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW); }
void turnRight() { ledcWrite(ENA, motorSpeed); ledcWrite(ENB, motorSpeed); digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW); digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH); }
void stopRobot() { ledcWrite(ENA, 0); ledcWrite(ENB, 0); digitalWrite(IN1, LOW); digitalWrite(IN2, LOW); digitalWrite(IN3, LOW); digitalWrite(IN4, LOW); }

// ==========================================
// SERVER HANDLERS
// ==========================================
void handleRoot() {
  server.send(200, "text/html", htmlPage);
}

void handleCmd() {
  if (server.hasArg("c")) {
    String cmd = server.arg("c");
    if (cmd == "F") { autoMode = false; moveForward(); currentMode = "MANUAL: FWD"; }
    else if (cmd == "B") { autoMode = false; moveBackward(); currentMode = "MANUAL: REV"; }
    else if (cmd == "L") { autoMode = false; turnLeft(); currentMode = "MANUAL: LEFT"; }
    else if (cmd == "R") { autoMode = false; turnRight(); currentMode = "MANUAL: RIGHT"; }
    else if (cmd == "S") { autoMode = false; stopRobot(); currentMode = "IDLE"; }
    else if (cmd == "V1") { digitalWrite(VACUUM, HIGH); }
    else if (cmd == "V0") { digitalWrite(VACUUM, LOW); }
    else if (cmd == "AUTO") { autoMode = true; currentMode = "AI AUTO-CLEAN"; autoState = 0; }
  }
  server.send(200, "text/plain", "OK");
}

void handleTelemetry() {
  // Return JSON payload to the Web App
  String json = "{\"dist\":" + String(distanceRear) + 
                ",\"irL\":" + String(irLeftState) + 
                ",\"irR\":" + String(irRightState) + 
                ",\"mode\":\"" + currentMode + "\"}";
  server.send(200, "application/json", json);
}

// ==========================================
// SETUP
// ==========================================
void setup() {
  Serial.begin(115200);
  
  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);
  pinMode(VACUUM, OUTPUT);
  pinMode(TRIG, OUTPUT); pinMode(ECHO, INPUT);
  pinMode(IR_FRONT_LEFT, INPUT); pinMode(IR_FRONT_RIGHT, INPUT);
  
  ledcAttach(ENA, 1000, 8); ledcAttach(ENB, 1000, 8);
  stopRobot();

  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println("\nWiFi connected! IP address:");
  Serial.println(WiFi.localIP());

  // Start Server Endpoints
  server.on("/", handleRoot);
  server.on("/cmd", handleCmd);
  server.on("/telemetry", handleTelemetry);
  server.begin();
}

// ==========================================
// MAIN LOOP
// ==========================================
void loop() {
  server.handleClient(); // Process incoming web requests

  distanceRear = readUltrasonic();
  irLeftState = digitalRead(IR_FRONT_LEFT);
  irRightState = digitalRead(IR_FRONT_RIGHT);

  // 🧠 AI Autonomous Reflexes
  if (autoMode) {
    if (autoState == 0) { // State 0: Moving Forward
      moveForward();
      if (irLeftState == LOW || irRightState == LOW) { // Hit front obstacle
        autoState = 1; 
        autoActionTime = millis();
        currentMode = "AI: OBSTACLE! REV";
      }
    } 
    else if (autoState == 1) { // State 1: Reversing
      moveBackward();
      // Reverse for 800ms, OR stop reversing if ultrasonic detects something < 15cm
      if (millis() - autoActionTime > 800 || distanceRear < 15) { 
        autoState = 2; 
        autoActionTime = millis();
        currentMode = "AI: TURNING";
      }
    } 
    else if (autoState == 2) { // State 2: Turning to find a new path
      turnRight(); 
      if (millis() - autoActionTime > 600) {
        autoState = 0; // Go back to Forward
        currentMode = "AI AUTO-CLEAN";
      }
    }
  } else {
    // 🛡️ Manual Safety Reflexes
    if (currentMode == "MANUAL: FWD" && (irLeftState == LOW || irRightState == LOW)) { 
      stopRobot(); currentMode = "IDLE (FRONT BLOCKED)"; 
    }
    if (currentMode == "MANUAL: REV" && distanceRear < 15) { 
      stopRobot(); currentMode = "IDLE (REAR BLOCKED)"; 
    }
  }
  
  delay(10); // Short delay for stability
}
