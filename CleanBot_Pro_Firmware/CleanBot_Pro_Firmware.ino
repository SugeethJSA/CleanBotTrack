#include <WiFi.h>
#include <WebSocketsServer.h>

// ==========================================
// ⚠️ ENTER YOUR WIFI DETAILS HERE ⚠️
// ==========================================
const char* ssid = "Aug-4G";
const char* password = "Yegova@123";

// Start a WebSocket server on port 81 (standard port for WebSocketsServer)
WebSocketsServer webSocket = WebSocketsServer(81);

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
int motorSpeedLeft = 200;  // Increased from 120 (120 is often too low to overcome stall torque)
int motorSpeedRight = 200; // Increased from 120
String currentMode = "IDLE";
bool autoMode = false;

// Auto AI State Machine
unsigned long autoActionTime = 0;
int autoState = 0; // 0=Fwd, 1=Rev, 2=Turn

// Dead-Man's Switch / Safety
unsigned long lastPingTime = 0;
bool clientConnected = false;
unsigned long lastTelemetryTime = 0;
unsigned long lastSensorTime = 0;

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

void moveForward() { ledcWrite(ENA, motorSpeedLeft); ledcWrite(ENB, motorSpeedRight); digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH); digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH); }
void moveBackward() { ledcWrite(ENA, motorSpeedLeft); ledcWrite(ENB, motorSpeedRight); digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW); digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW); }
void turnLeft() { ledcWrite(ENA, motorSpeedLeft); ledcWrite(ENB, motorSpeedRight); digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH); digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW); }
void turnRight() { ledcWrite(ENA, motorSpeedLeft); ledcWrite(ENB, motorSpeedRight); digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW); digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH); }
void stopRobot() { ledcWrite(ENA, 0); ledcWrite(ENB, 0); digitalWrite(IN1, LOW); digitalWrite(IN2, LOW); digitalWrite(IN3, LOW); digitalWrite(IN4, LOW); }

// ==========================================
// WEBSOCKET HANDLER
// ==========================================
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.printf("[%u] Disconnected!\n", num);
      clientConnected = false;
      stopRobot();
      currentMode = "DISCONNECTED";
      autoMode = false;
      break;
      
    case WStype_CONNECTED: {
      IPAddress ip = webSocket.remoteIP(num);
      Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);
      clientConnected = true;
      lastPingTime = millis(); // Reset dead-man switch
      currentMode = "IDLE";
      break;
    }
      
    case WStype_TEXT: {
      String cmd = String((char*)payload);
      lastPingTime = millis(); // Any command counts as a heartbeat
      
      if (cmd == "PING") {
        // Just a keepalive, do nothing else.
      }
      else if (cmd.startsWith("SPDL:")) {
        motorSpeedLeft = cmd.substring(5).toInt();
      }
      else if (cmd.startsWith("SPDR:")) {
        motorSpeedRight = cmd.substring(5).toInt();
      }
      else if (cmd == "F") { autoMode = false; moveForward(); currentMode = "MANUAL: FWD"; }
      else if (cmd == "B") { autoMode = false; moveBackward(); currentMode = "MANUAL: REV"; }
      else if (cmd == "L") { autoMode = false; turnLeft(); currentMode = "MANUAL: LEFT"; }
      else if (cmd == "R") { autoMode = false; turnRight(); currentMode = "MANUAL: RIGHT"; }
      else if (cmd == "S") { autoMode = false; stopRobot(); currentMode = "IDLE"; }
      else if (cmd == "V1") { digitalWrite(VACUUM, HIGH); }
      else if (cmd == "V0") { digitalWrite(VACUUM, LOW); }
      else if (cmd == "AUTO") { autoMode = true; currentMode = "AI AUTO-CLEAN"; autoState = 0; }
      break;
    }
  }
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

  // Start WebSocket server
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  Serial.println("WebSocket server started on port 81");
}

// ==========================================
// MAIN LOOP
// ==========================================
void loop() {
  webSocket.loop(); // Must be called frequently

  if (millis() - lastSensorTime > 60) {
    distanceRear = readUltrasonic();
    irLeftState = digitalRead(IR_FRONT_LEFT);
    irRightState = digitalRead(IR_FRONT_RIGHT);
    lastSensorTime = millis();
  }

  // 🕒 1. DEAD-MAN'S SWITCH (Safety Failsafe)
  if (clientConnected && (millis() - lastPingTime > 1500)) {
    // If no ping received for 1.5 seconds, assume app crashed or WiFi dropped
    stopRobot();
    autoMode = false;
    currentMode = "FAILSAFE HALT";
    Serial.println("Connection lost! Failsafe engaged.");
    clientConnected = false;
  }

  // 🚀 2. AI REFLEXES
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
    // 🛡️ 3. MANUAL SAFETY REFLEXES
    // NOTE: If your robot refuses to move forward or backward but can turn left/right, 
    // it's likely because your sensors are disconnected or seeing the floor as an obstacle!
    if (currentMode == "MANUAL: FWD" && (irLeftState == LOW || irRightState == LOW)) { 
      stopRobot(); currentMode = "IDLE (FRONT BLOCKED)"; 
    }
    if (currentMode == "MANUAL: REV" && distanceRear < 15) { 
      stopRobot(); currentMode = "IDLE (REAR BLOCKED)"; 
    }
  }
  
  // 📡 4. TELEMETRY BROADCAST
  // Send state to all connected WebSocket clients every 150ms
  if (millis() - lastTelemetryTime > 150) {
    String json = "{\"dist\":" + String(distanceRear) + 
                  ",\"irL\":" + String(irLeftState) + 
                  ",\"irR\":" + String(irRightState) + 
                  ",\"mode\":\"" + currentMode + "\"}";
    webSocket.broadcastTXT(json);
    lastTelemetryTime = millis();
  }
  
  delay(10); // Short delay for stability, but keeps loop extremely fast
}
