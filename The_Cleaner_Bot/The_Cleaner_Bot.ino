#include "BluetoothSerial.h"

// Initialize Bluetooth
BluetoothSerial SerialBT;

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
#define IR_BACK_LEFT 34
#define IR_BACK_RIGHT 35

// Variables
long distance = 0;
int irLeftState = HIGH;
int irRightState = HIGH;

int motorSpeed = 200;   // 0-255

// State tracking & Safety thresholds
String currentMode = "S";    // Tracks what the robot is currently doing
const int SAFE_DIST_CM = 15; // Stop if front obstacle is closer than 15cm

long readUltrasonic() {
  digitalWrite(TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG, LOW);

  long duration = pulseIn(ECHO, HIGH, 30000); // Added timeout to prevent lag
  if (duration == 0) return 999; // If no echo, assume path is clear

  return duration * 0.034 / 2;
}

void moveBackward() {
  ledcWrite(ENA, motorSpeed);
  ledcWrite(ENB, motorSpeed);
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
}

void moveForward() {
  ledcWrite(ENA, motorSpeed);
  ledcWrite(ENB, motorSpeed);
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
}

void turnLeft() {
  ledcWrite(ENA, motorSpeed);
  ledcWrite(ENB, motorSpeed);
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
}

void turnRight() {
  ledcWrite(ENA, motorSpeed);
  ledcWrite(ENB, motorSpeed);
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
}

void stopRobot() {
  ledcWrite(ENA, 0);
  ledcWrite(ENB, 0);
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
}

void vacuumOn() {
  digitalWrite(VACUUM, HIGH);
}

void vacuumOff() {
  digitalWrite(VACUUM, LOW);
}

void processCommand(String cmd) {
  if (cmd == "F") { moveForward(); currentMode = "F"; }
  else if (cmd == "B") { moveBackward(); currentMode = "B"; }
  else if (cmd == "L") { turnLeft(); currentMode = "L"; }
  else if (cmd == "R") { turnRight(); currentMode = "R"; }
  else if (cmd == "S") { stopRobot(); currentMode = "S"; }
  else if (cmd == "V1") { vacuumOn(); SerialBT.println("Vacuum ON"); }
  else if (cmd == "V0") { vacuumOff(); SerialBT.println("Vacuum OFF"); }
}

void setup() {
  Serial.begin(115200);
  
  // Start Bluetooth with the device name "Cleaner_Robot"
  SerialBT.begin("Cleaner_Robot"); 
  Serial.println("Bluetooth Started! Ready to pair.");

  // Motor output setup
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  pinMode(VACUUM, OUTPUT);

  // Sensor pin setup
  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);
  pinMode(IR_BACK_LEFT, INPUT);
  pinMode(IR_BACK_RIGHT, INPUT);

  /* PWM setup (ESP32 Core v3.x API) */
  ledcAttach(ENA, 1000, 8);
  ledcAttach(ENB, 1000, 8);

  stopRobot();
}

void loop() {
  // 1. Read all sensors
  distance = readUltrasonic();
  irLeftState = digitalRead(IR_BACK_LEFT);
  irRightState = digitalRead(IR_BACK_RIGHT);

  // 2. Listen for Bluetooth Commands
  if (SerialBT.available()) {
    String cmd = SerialBT.readStringUntil('\n');
    cmd.trim(); // Remove invisible characters like \r
    
    if (cmd.length() > 0) {
      SerialBT.print("Received: ");
      SerialBT.println(cmd);
      processCommand(cmd);
    }
  }

  // 3. The "Reflexes" (Safety Overrides)
  
  // Safety: Stop if moving forward and ultrasonic detects something close
  if (currentMode == "F" && distance < SAFE_DIST_CM) {
    stopRobot();
    currentMode = "S"; // Update state so it doesn't get stuck in a loop
    SerialBT.println("ALERT: Front obstacle! Brakes applied.");
  }

  // Safety: Stop if moving backward and either rear IR detects an obstacle
  // (Assuming your IR module outputs LOW when an object is detected)
  if (currentMode == "B" && (irLeftState == LOW || irRightState == LOW)) {
    stopRobot();
    currentMode = "S";
    SerialBT.println("ALERT: Rear obstacle! Brakes applied.");
  }

  delay(50); // Short delay for stability
}