#include <WiFi.h>
#include <esp_now.h>
#include <WebSocketsServer.h>

// ==========================================
// ⚠️ 1. ENTER YOUR WIFI DETAILS 
// ==========================================
const char* ssid = "YOUR_WIFI_NAME";
const char* password = "YOUR_WIFI_PASSWORD";

// ==========================================
// ⚠️ 2. ENTER THE OTHER ROBOT'S MAC ADDRESS
// Example: {0x24, 0x6F, 0x28, 0xAE, 0x11, 0x22}
// ==========================================
uint8_t peerAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};


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
int motorSpeedLeft = 200;  // Increased from 120
int motorSpeedRight = 200; // Increased from 120
String currentMode = "IDLE";
bool autoMode = false;

// Auto AI State Machine
unsigned long autoActionTime = 0;
int autoState = 0; // 0=Fwd, 1=Rev, 2=Turn

// Safety & Timers
unsigned long lastPingTime = 0;
bool clientConnected = false;
unsigned long lastTelemetryTime = 0;
unsigned long lastSensorTime = 0;

// Swarm RSSI Evasion
unsigned long lastEvasionTime = 0;
bool isEvadingSwarm = false;
unsigned long lastEspNowPing = 0;

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
// ESP-NOW SWARM CALLBACKS
// ==========================================
typedef struct struct_message {
    char msg[32];
} struct_message;
struct_message myData;

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  // Can be used to check if the other robot received the ping
}

// ESP32 Core v3.x Callback Signature
void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *incomingData, int len) {
  int rssi = info->rx_ctrl->rssi;
  
  // -45 dBm is a very strong signal, meaning they are likely within 1 meter of each other
  if (rssi > -45 && autoMode) {
    if (!isEvadingSwarm && millis() - lastEvasionTime > 2000) {
      Serial.printf("SWARM COLLISION IMMINENT! RSSI: %d \n", rssi);
      isEvadingSwarm = true;
      lastEvasionTime = millis();
    }
  }
}

// ==========================================
// WEBSOCKET HANDLER
// ==========================================
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      clientConnected = false;
      stopRobot();
      currentMode = "DISCONNECTED";
      autoMode = false;
      break;
      
    case WStype_CONNECTED:
      clientConnected = true;
      lastPingTime = millis(); 
      currentMode = "IDLE";
      break;
      
    case WStype_TEXT: {
      String cmd = String((char*)payload);
      lastPingTime = millis(); 
      
      if (cmd == "PING") {}
      else if (cmd.startsWith("SPDL:")) { motorSpeedLeft = cmd.substring(5).toInt(); }
      else if (cmd.startsWith("SPDR:")) { motorSpeedRight = cmd.substring(5).toInt(); }
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

  // 1. Initialize WiFi in Station Mode
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println("\nWiFi connected! IP address:");
  Serial.println(WiFi.localIP());

  // 2. Initialize ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  esp_now_register_send_cb(OnDataSent);
  esp_now_register_recv_cb(OnDataRecv);

  // Register Peer
  esp_now_peer_info_t peerInfo;
  memcpy(peerInfo.peer_addr, peerAddress, 6);
  // Important: Channel must be 0 to use the current Wi-Fi channel
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add peer");
  }

  // 3. Start WebSocket server
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
}

// ==========================================
// MAIN LOOP
// ==========================================
void loop() {
  webSocket.loop(); 

  // Sensor Polling (Non-blocking)
  if (millis() - lastSensorTime > 60) {
    distanceRear = readUltrasonic();
    irLeftState = digitalRead(IR_FRONT_LEFT);
    irRightState = digitalRead(IR_FRONT_RIGHT);
    lastSensorTime = millis();
  }

  // Broadcast ESP-NOW Swarm Ping every 200ms
  if (millis() - lastEspNowPing > 200) {
    strcpy(myData.msg, "PING");
    esp_now_send(peerAddress, (uint8_t *) &myData, sizeof(myData));
    lastEspNowPing = millis();
  }

  // Safety Failsafe
  if (clientConnected && (millis() - lastPingTime > 1500)) {
    stopRobot();
    autoMode = false;
    currentMode = "FAILSAFE HALT";
    clientConnected = false;
  }

  // 🚀 AI REFLEXES & SWARM EVASION
  if (autoMode) {
    
    // Priority 1: Evasive Maneuver from other robot
    if (isEvadingSwarm) {
      currentMode = "SWARM EVASION";
      turnRight(); // Hard right turn to escape
      if (millis() - lastEvasionTime > 1200) { // Turn for 1.2s
        isEvadingSwarm = false;
        autoState = 0; // Resume forward
      }
    } 
    // Priority 2: Standard AI Navigation
    else {
      if (autoState == 0) { // Moving Forward
        moveForward();
        if (irLeftState == LOW || irRightState == LOW) { // Hit front obstacle
          autoState = 1; 
          autoActionTime = millis();
          currentMode = "AI: OBSTACLE! REV";
        }
      } 
      else if (autoState == 1) { // Reversing
        moveBackward();
        if (millis() - autoActionTime > 800 || distanceRear < 15) { 
          autoState = 2; 
          autoActionTime = millis();
          currentMode = "AI: TURNING";
        }
      } 
      else if (autoState == 2) { // Turning
        turnRight(); 
        if (millis() - autoActionTime > 600) {
          autoState = 0; 
          currentMode = "AI AUTO-CLEAN";
        }
      }
    }
  } else {
    // Manual Safety Reflexes
    // NOTE: If your robot refuses to move forward or backward but can turn left/right, 
    // it's likely because your sensors are disconnected or seeing the floor as an obstacle!
    if (currentMode == "MANUAL: FWD" && (irLeftState == LOW || irRightState == LOW)) { 
      stopRobot(); currentMode = "IDLE (FRONT BLOCKED)"; 
    }
    if (currentMode == "MANUAL: REV" && distanceRear < 15) { 
      stopRobot(); currentMode = "IDLE (REAR BLOCKED)"; 
    }
  }
  
  // Telemetry Broadcast (150ms)
  if (millis() - lastTelemetryTime > 150) {
    String json = "{\"dist\":" + String(distanceRear) + 
                  ",\"irL\":" + String(irLeftState) + 
                  ",\"irR\":" + String(irRightState) + 
                  ",\"mode\":\"" + currentMode + "\"}";
    webSocket.broadcastTXT(json);
    lastTelemetryTime = millis();
  }
  
  delay(10);
}
