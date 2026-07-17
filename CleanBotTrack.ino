#include <WiFi.h>
#include <WebSocketsServer.h>

// WebSockets Server on Port 81
WebSocketsServer webSocket = WebSocketsServer(81);

// Wifi Configuration (Change these to connect to your local router)
const char* ssid = "Aug-4G";
const char* password = "Yegova@123";

// Fallback Access Point Configuration
const char* ap_ssid = "CleanBot_AP";
const char* ap_password = "12345678";

// Motor Pins
#define ENA 33
#define ENB 32
#define IN1 26
#define IN2 27
#define IN3 14
#define IN4 12

// Relay / Vacuum Pin
#define VACUUM 25

// Sensor Pins
#define TRIG 5
#define ECHO 18
#define IR_FRONT_LEFT 34
#define IR_FRONT_RIGHT 35

// Status LED (Onboard LED)
#define LED_PIN 2

// Telemetry & Sensor variables
long distance = 999;
int irLeftState = HIGH;
int irRightState = HIGH;

int motorSpeedL = 200; // Left motor speed (0-255)
int motorSpeedR = 200; // Right motor speed (0-255)
int baseSpeed = 200;   // Default speed limit

// State tracking & Safety thresholds
String currentMode = "S";    // "S"=Stop, "F"=Forward, "B"=Backward, "L"=Left, "R"=Right
const int SAFE_DIST_CM = 15; // Safe distance threshold for ultrasonic sensor (rear)
bool isAutoMode = false;
bool vacuumState = false;

// Consecutive reading counts to filter ultrasonic noise ("Anti-Scare" filter)
int consecutiveObstacleCount = 0;
const int CONSECUTIVE_LIMIT = 3; 

// Autonomous (AI) state machine variables
enum AutoState {
  AUTO_FORWARD,
  AUTO_DECIDE,
  AUTO_REVERSE,
  AUTO_SPIN
};
AutoState autoState = AUTO_FORWARD;
unsigned long stateTimer = 0;
unsigned long lastTelemetryTime = 0;
bool spinDirectionLeft = true; // True for left, False for right

// Forward declarations
void stopRobot();
void moveForward();
void moveBackward();
void turnLeft();
void turnRight();
void vacuumOn();
void vacuumOff();
void broadcastTelemetry();

// ---------------- ULTRASONIC READ FUNCTION ----------------
long readUltrasonic() {
  digitalWrite(TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG, LOW);

  long duration = pulseIn(ECHO, HIGH, 25000); // 25ms timeout (~4.2m max)
  if (duration == 0) return 999; // Assume clear if no echo

  long d = duration * 0.034 / 2;
  if (d <= 2 || d > 400) return 999; // Filter out ground bounces or spikes
  return d;
}

// ---------------- MOTOR CONTROLS ----------------
void applySpeeds() {
  ledcWrite(ENA, motorSpeedL);
  ledcWrite(ENB, motorSpeedR);
}

void moveForward() {
  currentMode = "F";
  applySpeeds();
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
}

void moveBackward() {
  currentMode = "B";
  applySpeeds();
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
}

void turnLeft() {
  currentMode = "L";
  applySpeeds();
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
}

void turnRight() {
  currentMode = "R";
  applySpeeds();
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
}

void stopRobot() {
  currentMode = "S";
  ledcWrite(ENA, 0);
  ledcWrite(ENB, 0);
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
}

void vacuumOn() {
  digitalWrite(VACUUM, HIGH);
  vacuumState = true;
}

void vacuumOff() {
  digitalWrite(VACUUM, LOW);
  vacuumState = false;
}

// ---------------- WEBSOCKET COMMAND PARSER ----------------
void processCommand(String cmd) {
  if (cmd == "PING") {
    // Keepalive ping, just reply with telemetry immediately
    broadcastTelemetry();
    return;
  }

  Serial.print("Executing Command: ");
  Serial.println(cmd);

  if (cmd == "F") {
    isAutoMode = false;
    moveForward();
  }
  else if (cmd == "B") {
    isAutoMode = false;
    moveBackward();
  }
  else if (cmd == "L") {
    isAutoMode = false;
    turnLeft();
  }
  else if (cmd == "R") {
    isAutoMode = false;
    turnRight();
  }
  else if (cmd == "S") {
    isAutoMode = false;
    stopRobot();
  }
  else if (cmd == "V1") {
    vacuumOn();
  }
  else if (cmd == "V0") {
    vacuumOff();
  }
  else if (cmd == "AUTO") {
    isAutoMode = true;
    autoState = AUTO_FORWARD;
    Serial.println("Autonomous cleaning mode started.");
  }
  else if (cmd == "MANUAL") {
    isAutoMode = false;
    stopRobot();
    Serial.println("Returned to Manual Mode.");
  }
  else if (cmd.startsWith("SPDL:")) {
    motorSpeedL = cmd.substring(5).toInt();
    motorSpeedL = constrain(motorSpeedL, 0, 255);
    applySpeeds();
  }
  else if (cmd.startsWith("SPDR:")) {
    motorSpeedR = cmd.substring(5).toInt();
    motorSpeedR = constrain(motorSpeedR, 0, 255);
    applySpeeds();
  }

  broadcastTelemetry();
}

// ---------------- BROADCAST TELEMETRY ----------------
void broadcastTelemetry() {
  // Construct JSON status string manually (saves memory & compiles out-of-the-box)
  String json = "{";
  json += "\"mode\":\"" + currentMode + "\",";
  json += "\"auto\":" + String(isAutoMode ? 1 : 0) + ",";
  json += "\"dist\":" + String(distance) + ",";
  json += "\"irL\":" + String(irLeftState) + ",";
  json += "\"irR\":" + String(irRightState) + ",";
  json += "\"spdL\":" + String(motorSpeedL) + ",";
  json += "\"spdR\":" + String(motorSpeedR) + ",";
  json += "\"vac\":" + String(vacuumState ? 1 : 0);
  json += "}";

  webSocket.broadcastTXT(json);
}

// ---------------- WEBSOCKET EVENTS ----------------
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.printf("[%u] Client Disconnected\n", num);
      if (webSocket.connectedClients() == 0) {
        digitalWrite(LED_PIN, LOW); // Turn off status LED
        // Stop robot for safety if no client is connected
        if (!isAutoMode) {
          stopRobot();
        }
      }
      break;
      
    case WStype_CONNECTED: {
      IPAddress ip = webSocket.remoteIP(num);
      Serial.printf("[%u] Connected from %d.%d.%d.%d\n", num, ip[0], ip[1], ip[2], ip[3]);
      digitalWrite(LED_PIN, HIGH); // Turn on status LED to indicate active client
      broadcastTelemetry(); // Send initial state immediately
      break;
    }
    
    case WStype_TEXT: {
      String text = String((char*)payload);
      text.trim();
      processCommand(text);
      break;
    }
    
    default:
      break;
  }
}

// ---------------- SETUP ----------------
void setup() {
  Serial.begin(115200);

  // Status LED
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // Motor outputs
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  pinMode(VACUUM, OUTPUT);

  // Sensor setup
  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);
  pinMode(IR_FRONT_LEFT, INPUT);
  pinMode(IR_FRONT_RIGHT, INPUT);

  // PWM speed control setup (ESP32 core 3.x API)
  ledcAttach(ENA, 1000, 8);
  ledcAttach(ENB, 1000, 8);

  stopRobot();
  vacuumOff();

  // Try Wi-Fi router connection
  WiFi.begin(ssid, password);
  Serial.print("Connecting to Wi-Fi: ");
  Serial.println(ssid);

  int timeoutCounter = 0;
  while (WiFi.status() != WL_CONNECTED && timeoutCounter < 20) {
    delay(500);
    Serial.print(".");
    timeoutCounter++;
  }

  // Fallback to AP Mode if router connection failed
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\nWi-Fi connection failed. Starting Access Point...");
    WiFi.disconnect();
    WiFi.softAP(ap_ssid, ap_password);
    IPAddress apIP = WiFi.softAPIP();
    Serial.print("AP Started! Network SSID: ");
    Serial.println(ap_ssid);
    Serial.print("IP Address: ");
    Serial.println(apIP);
  } else {
    Serial.println("\nWi-Fi connected successfully!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  }

  // Start WebSocket Server
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  Serial.println("WebSocket server started on port 81");
}

// ---------------- MAIN LOOP ----------------
void loop() {
  webSocket.loop();

  // 1. Read Sensors
  long rawDist = readUltrasonic();
  irLeftState = digitalRead(IR_FRONT_LEFT);
  irRightState = digitalRead(IR_FRONT_RIGHT);

  // 2. Ultrasonic Noise Filtering ("Anti-Scare" Logic)
  // Glitch filters: ignore extreme or error values, require multiple samples to trigger warning
  if (rawDist < SAFE_DIST_CM) {
    consecutiveObstacleCount++;
  } else {
    consecutiveObstacleCount = 0;
  }
  
  // Decide filtered distance: only count obstacle if triggered consecutively
  distance = rawDist;
  bool backObstacleDetected = (consecutiveObstacleCount >= CONSECUTIVE_LIMIT);

  // 3. Safety Reflexes (Safety Overrides for manual and auto modes)
  
  // If moving forward and front IR sensor detects an obstacle
  if (!isAutoMode && currentMode == "F" && (irLeftState == LOW || irRightState == LOW)) {
    stopRobot();
    Serial.println("SAFETY WARNING: Front obstacle! Stopped.");
    broadcastTelemetry();
  }

  // If moving backward and rear ultrasonic detects an obstacle
  if (!isAutoMode && currentMode == "B" && backObstacleDetected) {
    stopRobot();
    Serial.println("SAFETY WARNING: Rear obstacle! Stopped.");
    broadcastTelemetry();
  }

  // 4. Autonomous (AI) Maneuvering State Machine
  if (isAutoMode) {
    unsigned long currentMillis = millis();

    switch (autoState) {
      case AUTO_FORWARD:
        // Default action: move forward
        moveForward();
        // Check front IR sensors for obstacles
        if (irLeftState == LOW || irRightState == LOW) {
          stopRobot();
          // Decide which way to turn: spin away from the obstacle
          if (irLeftState == LOW && irRightState == HIGH) {
            spinDirectionLeft = false; // Turn right
          } else if (irRightState == LOW && irLeftState == HIGH) {
            spinDirectionLeft = true;  // Turn left
          } else {
            spinDirectionLeft = (random(0, 2) == 0); // Both blocked: random turn
          }
          
          Serial.println("Auto-Nav: Front obstacle detected! Transition to DECIDE.");
          autoState = AUTO_DECIDE;
          stateTimer = currentMillis;
        }
        break;

      case AUTO_DECIDE:
        // Check if rear is clear
        if (!backObstacleDetected && distance > 25) {
          // Rear is clear, back up a bit to get space
          moveBackward();
          Serial.println("Auto-Nav: Rear clear. Reversing to gain maneuver space.");
          autoState = AUTO_REVERSE;
          stateTimer = currentMillis;
        } else {
          // Rear is blocked, must spin in place
          if (spinDirectionLeft) {
            turnLeft();
          } else {
            turnRight();
          }
          Serial.println("Auto-Nav: Rear blocked. Spinning in place.");
          autoState = AUTO_SPIN;
          stateTimer = currentMillis;
        }
        break;

      case AUTO_REVERSE:
        // Reverse for 700ms or until rear obstacle detected
        if (backObstacleDetected || (currentMillis - stateTimer >= 700)) {
          stopRobot();
          // After reversing (or if blocked), spin to change heading
          if (spinDirectionLeft) {
            turnLeft();
          } else {
            turnRight();
          }
          Serial.println("Auto-Nav: Reverse complete. Commencing turn.");
          autoState = AUTO_SPIN;
          stateTimer = currentMillis;
        }
        break;

      case AUTO_SPIN:
        // Spin for at least 500ms and until front path is clear
        if (currentMillis - stateTimer >= 500) {
          if (irLeftState == HIGH && irRightState == HIGH) {
            // Path is clear! Move forward again
            moveForward();
            Serial.println("Auto-Nav: Front clear! Moving forward.");
            autoState = AUTO_FORWARD;
          }
        }
        // Timeout safeguard: if spinning for > 2.5s, reverse again
        if (currentMillis - stateTimer > 2500) {
          stopRobot();
          autoState = AUTO_DECIDE;
        }
        break;
    }
  }

  // 5. Periodic Telemetry Broadcast (every 150ms to keep client updated)
  if (millis() - lastTelemetryTime >= 150) {
    broadcastTelemetry();
    lastTelemetryTime = millis();
  }

  delay(20); // Small loop stabilization delay
}
