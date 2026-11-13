#include <Servo.h>
#include <Wire.h> 

// --- PIN ASSIGNMENTS ---
const int enA = 3;  const int in1 = 4;  const int in2 = 5;  
const int enB = 11; const int in3 = 6;  const int in4 = 7;  

const int servoPin = 8;
const int frontTrig = 9;  const int frontEcho = 10;
const int downTrig = 12;  const int downEcho = 13;

// --- SYSTEM PARAMETERS ---
const int stopDist = 40;     
const int tooCloseDist = 25; 

// --- HIGH-SENSITIVITY ANOMALY PARAMETERS ---
const int baselineGroundDist = 15; 
const int potholeDepth = 3;        // 15cm baseline + 3cm dip = triggers at exactly 18cm or greater
const float bumpThreshold = 1.6;   

const int driveSpeed = 150;  
const int turnSpeed = 255;   
const int MPU_ADDR = 0x68;   

Servo scanServo;

void setup() {
  Serial.begin(9600);
  
  pinMode(frontTrig, OUTPUT); pinMode(frontEcho, INPUT);
  pinMode(downTrig, OUTPUT);  pinMode(downEcho, INPUT);
  
  pinMode(enA, OUTPUT); pinMode(enB, OUTPUT);
  pinMode(in1, OUTPUT); pinMode(in2, OUTPUT);
  pinMode(in3, OUTPUT); pinMode(in4, OUTPUT);
  
  scanServo.attach(servoPin);
  scanServo.write(90); 
  
  // --- INITIALIZE MPU-9250/6500 ---
  Wire.begin();
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B); 
  Wire.write(0x00); 
  Wire.endTransmission(true);
  
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x1C); 
  Wire.write(0x00); 
  Wire.endTransmission(true);
  
  haltRover(); 
  Serial.println("Phase 2 Online: Pothole Threshold set to >= 18cm.");
  delay(2000);
}

void loop() {
  // 1. Check for Speed Bumps 
  checkSurfaceImpact();

  // 2. Check for Potholes (Triggers at 18cm or greater)
  int groundDist = getDownDistance();
  
  // UPDATED: Using >= to trigger at exactly 18cm
  if (groundDist >= (baselineGroundDist + potholeDepth)) {
    
    // DEBOUNCE: Check again to filter out voltage glitches
    delay(15); 
    int confirmDist = getDownDistance();
    
    // UPDATED: Using >= here as well
    if (confirmDist >= (baselineGroundDist + potholeDepth)) {
      
      // SOFT BRAKE: Stop and let battery voltage recover
      haltRover();
      delay(400); 
      
      Serial.print("POTHOLE DETECTED! Depth reading: "); Serial.print(confirmDist); Serial.println("cm");
      Serial.println("Initiating Emergency Reverse...");
      
      reverseRover(); delay(1000); 
      turnRight(); delay(1200);    
      haltRover(); delay(500);
    }
  }

  // 3. Normal Forward Navigation
  int frontClearance = getFrontDistance();
  
  if (frontClearance > 0 && frontClearance <= stopDist) {
    haltRover();
    Serial.println("Obstacle Detected in path.");
    
    if (frontClearance <= tooCloseDist) {
      Serial.println("Too close to turn safely. Creating space...");
      
      haltRover(); delay(300);
      reverseRover(); delay(600);
      haltRover(); delay(200);
    }
    executeGreedyBypass();
  } else {
    driveForward();
  }
  
  delay(10); 
}

// --- SURFACE ANOMALY FUNCTIONS ---
void checkSurfaceImpact() {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3F); 
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR, 2, true);
  
  int16_t z_raw = (Wire.read() << 8 | Wire.read()); 
  float z_g = z_raw / 16384.0; 
  
  if (abs(z_g) > bumpThreshold) {
    Serial.print("SPEED BUMP DETECTED! G-Force: "); 
    Serial.println(abs(z_g));
    
    analogWrite(enA, 100); 
    analogWrite(enB, 100);
    delay(400); 
  }
}

int getDownDistance() {
  digitalWrite(downTrig, LOW); delayMicroseconds(2);
  digitalWrite(downTrig, HIGH); delayMicroseconds(10);
  digitalWrite(downTrig, LOW);
  
  long duration = pulseIn(downEcho, HIGH, 15000); 
  if (duration == 0) return 999; 
  return (duration / 2) / 29.1; 
}

// --- NAVIGATION & ACTUATION FUNCTIONS ---
void executeGreedyBypass() {
  int bestAngle = 150; 
  int maxClearance = 0;
  int angles[] = {30, 60, 120, 150}; 
  
  for (int i = 0; i < 4; i++) {
    scanServo.write(angles[i]);
    delay(300); 
    
    int currentDist = getFrontDistance();
    if (currentDist > maxClearance) {
      maxClearance = currentDist;
      bestAngle = angles[i];
    }
  }
  
  scanServo.write(90); 
  delay(300);
  
  if (maxClearance < tooCloseDist) {
     Serial.println("Trapped! Executing turnaround.");
     
     haltRover(); delay(300);
     reverseRover(); delay(800);
     turnRight(); delay(1500); 
     haltRover();
  } else {
    if (bestAngle == 30) { turnRight(); delay(800); } 
    else if (bestAngle == 60) { turnRight(); delay(400); } 
    else if (bestAngle == 120) { turnLeft(); delay(400); } 
    else if (bestAngle == 150) { turnLeft(); delay(800); }
  }
}

int getFrontDistance() {
  digitalWrite(frontTrig, LOW); delayMicroseconds(2);
  digitalWrite(frontTrig, HIGH); delayMicroseconds(10);
  digitalWrite(frontTrig, LOW);
  long duration = pulseIn(frontEcho, HIGH, 15000); 
  if (duration == 0) return 999; 
  return (duration / 2) / 29.1; 
}

void driveForward() {
  analogWrite(enA, driveSpeed); analogWrite(enB, driveSpeed);
  digitalWrite(in1, LOW); digitalWrite(in2, HIGH); 
  digitalWrite(in3, LOW); digitalWrite(in4, HIGH); 
}

void reverseRover() {
  analogWrite(enA, driveSpeed); analogWrite(enB, driveSpeed);
  digitalWrite(in1, HIGH); digitalWrite(in2, LOW); 
  digitalWrite(in3, HIGH); digitalWrite(in4, LOW); 
}

void turnLeft() {
  analogWrite(enA, 0); analogWrite(enB, turnSpeed); 
  digitalWrite(in1, LOW); digitalWrite(in2, LOW); 
  digitalWrite(in3, LOW); digitalWrite(in4, HIGH); 
}

void turnRight() {
  analogWrite(enA, turnSpeed); analogWrite(enB, 0); 
  digitalWrite(in1, LOW); digitalWrite(in2, HIGH); 
  digitalWrite(in3, LOW); digitalWrite(in4, LOW); 
}

void haltRover() {
  analogWrite(enA, 0); analogWrite(enB, 0);
  digitalWrite(in1, LOW); digitalWrite(in2, LOW);
  digitalWrite(in3, LOW); digitalWrite(in4, LOW);
}