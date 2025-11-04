// ---------------- PIN CONFIGURATION ----------------
const int trigPin = 9;
const int echoPin = 8;

// Analog IR sensors
const int irLeft = A0;
const int irRight = A1;

// ---------------- NEURO-FUZZY PARAMETERS ----------------
float wNear = 0.7;
float wMedium = 0.4;
float wFar = 0.1;

// Initial dynamic weights for side sensors
float wLeft = 0.5;
float wRight = 0.5;

// ---------------- SETUP ----------------
void setup() {
  Serial.begin(9600);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(irLeft, INPUT);
  pinMode(irRight, INPUT);
  Serial.println("=== Neuro-Fuzzy Obstacle Avoidance (Dynamic Weight + Analog IR) ===");
}

// ---------------- ULTRASONIC READ FUNCTION ----------------
long readUltrasonic() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long duration = pulseIn(echoPin, HIGH);
  long distance = duration * 0.034 / 2;
  return distance;
}

// ---------------- FUZZY MEMBERSHIP FUNCTIONS (DISTANCE) ----------------
float fuzzyNear(long d) {
  if (d <= 10) return 1.0;
  else if (d <= 20) return (20.0 - d) / 10.0;
  else return 0.0;
}

float fuzzyMedium(long d) {
  if (d <= 10 || d >= 40) return 0.0;
  else if (d <= 25) return (d - 10.0) / 15.0;
  else return (40.0 - d) / 15.0;
}

float fuzzyFar(long d) {
  if (d <= 25) return 0.0;
  else if (d <= 50) return (d - 25.0) / 25.0;
  else return 1.0;
}

// ---------------- FUZZY MEMBERSHIP FUNCTIONS (IR INTENSITY) ----------------
float fuzzyLow(int val) {
  if (val <= 200) return 1.0;
  else if (val <= 400) return (400.0 - val) / 200.0;
  else return 0.0;
}

float fuzzyMediumIR(int val) {
  if (val <= 300 || val >= 700) return 0.0;
  else if (val <= 500) return (val - 300.0) / 200.0;
  else return (700.0 - val) / 200.0;
}

float fuzzyHigh(int val) {
  if (val <= 600) return 0.0;
  else if (val <= 900) return (val - 600.0) / 300.0;
  else return 1.0;
}

// ---------------- DYNAMIC WEIGHT UPDATE ----------------
void updateWeights(int leftVal, int rightVal) {
  // Higher IR value (strong reflection) means closer obstacle → lower its weight
  float leftIntensity = constrain(leftVal / 1023.0, 0.0, 1.0);
  float rightIntensity = constrain(rightVal / 1023.0, 0.0, 1.0);

  // Dynamic weight allocation (less intensity = more weight)
  wLeft = 1.0 - leftIntensity;
  wRight = 1.0 - rightIntensity;

  // Normalize to maintain balance
  float sum = wLeft + wRight;
  wLeft /= sum;
  wRight /= sum;
}

// ---------------- MAIN LOOP ----------------
void loop() {
  long distance = readUltrasonic();
  int leftVal = analogRead(irLeft);
  int rightVal = analogRead(irRight);

  // Update dynamic weights based on IR readings
  updateWeights(leftVal, rightVal);

  // Compute fuzzy memberships
  float nearVal = fuzzyNear(distance);
  float medVal = fuzzyMedium(distance);
  float farVal = fuzzyFar(distance);

  float leftLow = fuzzyLow(leftVal);
  float leftMed = fuzzyMediumIR(leftVal);
  float leftHigh = fuzzyHigh(leftVal);

  float rightLow = fuzzyLow(rightVal);
  float rightMed = fuzzyMediumIR(rightVal);
  float rightHigh = fuzzyHigh(rightVal);

  // Compute activations (weighted fuzzy outputs)
  float actLeft = (leftMed * 0.5 + leftHigh * 1.0) * wLeft;
  float actRight = (rightMed * 0.5 + rightHigh * 1.0) * wRight;

  // Debug print
  Serial.print("Dist: "); Serial.print(distance);
  Serial.print(" cm | L_IR: "); Serial.print(leftVal);
  Serial.print(" | R_IR: "); Serial.print(rightVal);
  Serial.print(" | wL: "); Serial.print(wLeft, 2);
  Serial.print(" | wR: "); Serial.print(wRight, 2);
  Serial.print(" | Near: "); Serial.print(nearVal, 2);
  Serial.print(" | Med: "); Serial.print(medVal, 2);
  Serial.print(" | Far: "); Serial.print(farVal, 2);
  Serial.print(" --> ");

  // ------------------- FUZZY DECISION RULES -------------------

  // Case 1: Both side sensors detect strong reflections (blocked)
  if (actLeft > 0.6 && actRight > 0.6 && nearVal < 0.4) {
    Serial.println("Rule: Blocked on both sides → Move BACK slightly");
  }

  // Case 2: Left side obstacle detected → Turn RIGHT
  else if (actLeft > 0.6 && actRight < 0.4) {
    Serial.println("Rule: Strong LEFT obstacle → Turn RIGHT");
  }

  // Case 3: Right side obstacle detected → Turn LEFT
  else if (actRight > 0.6 && actLeft < 0.4) {
    Serial.println("Rule: Strong RIGHT obstacle → Turn LEFT");
  }

  // Case 4: Front obstacle (near) detected → Back & turn toward lower weight side
  else if (nearVal > 0.6) {
    if (wLeft > wRight) {
      Serial.println("Rule: Front obstacle → BACK + Turn LEFT (right side blocked)");
    } else {
      Serial.println("Rule: Front obstacle → BACK + Turn RIGHT (left side blocked)");
    }
  }

  // Case 5: Medium obstacles → slight adjust
  else if (medVal > 0.3) {
    if (actLeft > actRight)
      Serial.println("Rule: Medium + Slight Left obstacle → Slight RIGHT adjust");
    else if (actRight > actLeft)
      Serial.println("Rule: Medium + Slight Right obstacle → Slight LEFT adjust");
    else
      Serial.println("Rule: Medium + Balanced → Forward cautiously");
  }

  // Case 6: Clear path → Move Forward
  else if (farVal > 0.3 && actLeft < 0.3 && actRight < 0.3) {
    Serial.println("Rule: Far + Clear → Move FORWARD");
  }

  // Default uncertain case
  else {
    Serial.println("Rule: Uncertain → Scan or slow forward");
  }

  delay(600);AC
}

