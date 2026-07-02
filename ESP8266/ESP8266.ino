#include <SoftwareSerial.h>

// Pins for Traffic Lights
const int R1 = D1, Y1 = D2, G1 = D3;   // Road 1
const int R2 = D4, Y2 = D5, G2 = D6;   // Road 2
const int R3 = D7, Y3 = D8, G3 = D0;   // Road 3

// ====================== VARIABLES ======================
String lastState = "";
unsigned long lastStateTime = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  pinMode(R1, OUTPUT); pinMode(Y1, OUTPUT); pinMode(G1, OUTPUT);
  pinMode(R2, OUTPUT); pinMode(Y2, OUTPUT); pinMode(G2, OUTPUT);
  pinMode(R3, OUTPUT); pinMode(Y3, OUTPUT); pinMode(G3, OUTPUT);

  // Initial state: All Red
  allRed();
  delay(100);
  
  Serial.println("\n=====================================");
  Serial.println("   🚦 TRAFFIC LIGHT CONTROLLER 🚦");
  Serial.println("=====================================");
  Serial.println("SYSTEM_READY");
  Serial.println("=====================================\n");
}

void loop() {
  // Listen for real-time state updates from Python
  if (Serial.available() > 0) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();

    if (cmd.startsWith("SET:")) {
      String state = cmd.substring(4); // Extracts G1, Y1, G2, etc.
      
      // Only update if state changed
      if (state != lastState) {
        updateLights(state);
        lastState = state;
        lastStateTime = millis();
        
        // Log the command
        Serial.print("[ESP] → ");
        Serial.println(state);
      }
    }
  }
}

void updateLights(String state) {
  // Extract road number and determine if we're on same road
  int newRoad = getRoadNumber(state);
  int lastRoad = getRoadNumber(lastState);
  
  // If switching to a DIFFERENT road, set all red first
  if (newRoad != lastRoad) {
    allRed();
    delay(100);  // Longer delay when switching roads
  }
  
  // Now set the new state WITHOUT calling allRed()
  setRoadDirect(state);
  
  delay(30);
}

int getRoadNumber(String state) {
  // Extract road number from state (G1, Y2, R3, etc.)
  if (state.length() >= 2) {
    return state[1] - '0';  // Convert '1', '2', '3' to 1, 2, 3
  }
  return 0;  // Invalid
}

void setRoadDirect(String state) {
  // Directly set lights WITHOUT calling allRed()
  // This prevents the Red flash when transitioning Y→G on same road
  
  if (state == "R1") {
    digitalWrite(G1, LOW);
    delay(5);
    digitalWrite(Y1, LOW);
    delay(5);
    digitalWrite(R1, HIGH);
    Serial.println("[LIGHT] Road 1 → RED ❌");
  } 
  else if (state == "Y1") { 
    digitalWrite(G1, LOW);
    delay(5);
    digitalWrite(R1, LOW);
    delay(5);
    digitalWrite(Y1, HIGH);
    Serial.println("[LIGHT] Road 1 → YELLOW 🟡");
  } 
  else if (state == "G1") {
    digitalWrite(Y1, LOW);
    delay(5);
    digitalWrite(R1, LOW);
    delay(5);
    digitalWrite(G1, HIGH);
    Serial.println("[LIGHT] Road 1 → GREEN ✅");
  } 
  
  else if (state == "R2") {
    digitalWrite(G2, LOW);
    delay(5);
    digitalWrite(Y2, LOW);
    delay(5);
    digitalWrite(R2, HIGH);
    Serial.println("[LIGHT] Road 2 → RED ❌");
  }
  else if (state == "Y2") {
    digitalWrite(G2, LOW);
    delay(5);
    digitalWrite(R2, LOW);
    delay(5);
    digitalWrite(Y2, HIGH);
    Serial.println("[LIGHT] Road 2 → YELLOW 🟡");
  } 
  else if (state == "G2") {
    digitalWrite(Y2, LOW);
    delay(5);
    digitalWrite(R2, LOW);
    delay(5);
    digitalWrite(G2, HIGH);
    Serial.println("[LIGHT] Road 2 → GREEN ✅");
  } 
  
  else if (state == "R3") {
    digitalWrite(G3, LOW);
    delay(5);
    digitalWrite(Y3, LOW);
    delay(5);
    digitalWrite(R3, HIGH);
    Serial.println("[LIGHT] Road 3 → RED ❌");
  }
  else if (state == "Y3") {
    digitalWrite(G3, LOW);
    delay(5);
    digitalWrite(R3, LOW);
    delay(5);
    digitalWrite(Y3, HIGH);
    Serial.println("[LIGHT] Road 3 → YELLOW 🟡");
  } 
  else if (state == "G3") {
    digitalWrite(Y3, LOW);
    delay(5);
    digitalWrite(R3, LOW);
    delay(5);
    digitalWrite(G3, HIGH);
    Serial.println("[LIGHT] Road 3 → GREEN ✅");
  }
}

void allRed() {
  // Only called when switching between roads
  // Ensures all other roads are RED
  
  // Road 1
  digitalWrite(G1, LOW); 
  delay(5);
  digitalWrite(Y1, LOW); 
  delay(5);
  digitalWrite(R1, HIGH);
  
  // Road 2
  digitalWrite(G2, LOW); 
  delay(5);
  digitalWrite(Y2, LOW); 
  delay(5);
  digitalWrite(R2, HIGH);
  
  // Road 3
  digitalWrite(G3, LOW); 
  delay(5);
  digitalWrite(Y3, LOW); 
  delay(5);
  digitalWrite(R3, HIGH);
}