#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_VL53L0X.h>
#include <Adafruit_NeoPixel.h>

// Forward declarations for animation functions
void updateSensorAnimation(int sensorIndex, int distance);
void breatheAnimation(Adafruit_NeoPixel &strip, uint16_t hue, uint8_t sat);
void waveAnimation(Adafruit_NeoPixel &strip, uint16_t hue, uint8_t sat, uint16_t phase, int speed);
void setupSensor(Adafruit_VL53L0X &sensor, int xshutPin, uint8_t address);

// ---------- Hardware Definitions ----------

// ToF sensors (Half A and Half B)
Adafruit_VL53L0X tofA1, tofA2, tofB1, tofB2;
#define XSHUT_A1 17
#define XSHUT_A2 16
#define XSHUT_B1 20
#define XSHUT_B2 21

// LEDs for each half
// Half A LEDs
#define LED_PIN_POT_A      6
#define LED_PIN_SENSOR_A1  4  // ToF sensor A1
#define LED_PIN_SENSOR_A2  5  // ToF sensor A2
// Half B LEDs
#define LED_PIN_POT_B      7
#define LED_PIN_SENSOR_B1  2  // ToF sensor B1
#define LED_PIN_SENSOR_B2  3  // ToF sensor B2

#define LED_COUNT_RING 24
#define LED_COUNT_DOT  7

// LED strips for each half
Adafruit_NeoPixel potRingA(LED_COUNT_RING, LED_PIN_POT_A, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel sensorRingA1(LED_COUNT_RING, LED_PIN_SENSOR_A1, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel sensorRingA2(LED_COUNT_RING, LED_PIN_SENSOR_A2, NEO_GRB + NEO_KHZ800);

Adafruit_NeoPixel potRingB(LED_COUNT_RING, LED_PIN_POT_B, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel sensorDotB1(LED_COUNT_DOT, LED_PIN_SENSOR_B1, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel sensorDotB2(LED_COUNT_DOT, LED_PIN_SENSOR_B2, NEO_GRB + NEO_KHZ800);

// Analog inputs (potentiometers)
int potInputA = A1;
int potInputB = A0;

// ---------- Parameters & State ----------

const int SENSOR_MIN_DISTANCE = 50;
const int SENSOR_MAX_DISTANCE = 200;

const int ACTIVITY_THRESHOLD = 5;       // mm change to trigger activity
const unsigned long IDLE_TIMEOUT = 100;   // ms until idle (note off)
const int ANIMATION_MIN_SPEED = 20;
const int ANIMATION_MID_SPEED = 100;
const int ANIMATION_MAX_SPEED = 250;
const uint8_t IDLE_BRIGHTNESS = 50;
const uint8_t ACTIVE_BRIGHTNESS = 255;

// Animation state (per ToF sensor)
unsigned long lastActivityTime[4] = {0, 0, 0, 0};
uint16_t animationPhase[4] = {0, 0, 0, 0};
int animationSpeed[4] = {0, 0, 0, 0};
bool isActive[4] = {false, false, false, false};
int prevDistance[4] = {0, 0, 0, 0};

// Processed control values (mapped from ToF sensor distances)
int controlValA1 = 0;  // For tofA1 (Half A)
int controlValA2 = 0;  // For tofA2 (Half A)
int controlValB1 = 0;  // For tofB1 (Half B)
int controlValB2 = 0;  // For tofB2 (Half B)

// Last observed values for MIDI CC filtering
int lastPotValA = 0;
int lastPotValB = 0;
int lastControlValA1 = 0;
int lastControlValA2 = 0;
int lastControlValB1 = 0;
int lastControlValB2 = 0;

const int MIDI_CHANGE_THRESHOLD = 3;

// --- Add these globals near your other parameters ---
float smoothPotA = 0;
float smoothPotB = 0;
const float POT_SMOOTHING = 0.8; // Adjust between 0.0 (no update) and 1.0 (no smoothing)

// ---------- MIDI Assignments ----------

// Half A: potInputA, tofA1, tofA2; Half B: potInputB, tofB1, tofB2.
const byte NOTE_A = 60; // Note for Half A (C3)
const byte NOTE_B = 60; // Note for Half B (C2)

// MIDI CC numbers (customize as needed)
// Half A
const byte CC_POT_A = 15;
const byte CC_SENSOR_A1 = 12;
const byte CC_SENSOR_A2 = 13;
// Half B
const byte CC_POT_B = 14;
const byte CC_SENSOR_B1 = 10;
const byte CC_SENSOR_B2 = 11;

// Track current note state per half
bool noteActiveA = false;
bool noteActiveB = false;

// ---------- Utility Functions ----------

void setupSensor(Adafruit_VL53L0X &sensor, int xshutPin, uint8_t address) {
  digitalWrite(xshutPin, HIGH);
  delay(100);
  if (!sensor.begin()) {
    Serial.print("Failed to initialize sensor at pin ");
    Serial.println(xshutPin);
    while (1);
  }
  sensor.setAddress(address);
  Serial.print("Sensor initialized at 0x");
  Serial.println(address, HEX);
}

int mapSensorToMIDI(int distance) {
  distance = constrain(distance, SENSOR_MIN_DISTANCE, SENSOR_MAX_DISTANCE);
  return map(distance, SENSOR_MIN_DISTANCE, SENSOR_MAX_DISTANCE, 0, 127);
}

// --- Modified MIDI functions (send raw MIDI bytes over Serial) ---

void noteOn(byte channel, byte pitch, byte velocity) {
  Serial.write(0x90 | channel);
  Serial.write(pitch);
  Serial.write(velocity);
}

void noteOff(byte channel, byte pitch, byte velocity) {
  Serial.write(0x80 | channel);
  Serial.write(pitch);
  Serial.write(velocity);
}

void controlChange(byte channel, byte control, byte value) {
  Serial.write(0xB0 | channel);
  Serial.write(control);
  Serial.write(value);
}

// ---------- Setup ----------
void setup() {
  // For raw MIDI output, typically MIDI baud rate is 31250.
  Serial.begin(31250);
  Serial.println("Initializing...");

  // Initialize LED strips
  potRingA.begin();
  sensorRingA1.begin();
  sensorRingA2.begin();
  
  potRingB.begin();
  sensorDotB1.begin();
  sensorDotB2.begin();
  
  // Setup ToF sensor pins
  pinMode(XSHUT_A1, OUTPUT);
  pinMode(XSHUT_A2, OUTPUT);
  pinMode(XSHUT_B1, OUTPUT);
  pinMode(XSHUT_B2, OUTPUT);
  
  // Ensure sensors are off during setup
  digitalWrite(XSHUT_A1, LOW);
  digitalWrite(XSHUT_A2, LOW);
  digitalWrite(XSHUT_B1, LOW);
  digitalWrite(XSHUT_B2, LOW);
  delay(10);
  
  setupSensor(tofA1, XSHUT_A1, 0x30);
  setupSensor(tofA2, XSHUT_A2, 0x31);
  setupSensor(tofB1, XSHUT_B1, 0x32);
  setupSensor(tofB2, XSHUT_B2, 0x33);
  
  // Small startup delay
  delay(100);
}

// ---------- Main Loop ----------
void loop() {
  VL53L0X_RangingMeasurementData_t measure;
  
  // --- Read ToF sensors for Half A and Half B ---
  tofA1.rangingTest(&measure, false);
  int distanceA1 = measure.RangeMilliMeter;
  controlValA1 = mapSensorToMIDI(distanceA1);
  updateSensorAnimation(0, distanceA1);
  
  tofA2.rangingTest(&measure, false);
  int distanceA2 = measure.RangeMilliMeter;
  controlValA2 = mapSensorToMIDI(distanceA2);
  updateSensorAnimation(1, distanceA2);
  
  tofB1.rangingTest(&measure, false);
  int distanceB1 = measure.RangeMilliMeter;
  controlValB1 = mapSensorToMIDI(distanceB1);
  updateSensorAnimation(2, distanceB1);
  
  tofB2.rangingTest(&measure, false);
  int distanceB2 = measure.RangeMilliMeter;
  controlValB2 = mapSensorToMIDI(distanceB2);
  updateSensorAnimation(3, distanceB2);
  
  // --- Read Potentiometers with smoothing ---
  int rawPotA = analogRead(potInputA);
  int rawPotB = analogRead(potInputB);

  // Exponential moving average smoothing
  smoothPotA = (1.0 - POT_SMOOTHING) * smoothPotA + POT_SMOOTHING * rawPotA;
  smoothPotB = (1.0 - POT_SMOOTHING) * smoothPotB + POT_SMOOTHING * rawPotB;

  // Map the smooth values to MIDI and LED indices
  int potValA = map(smoothPotA, 0, 1023, 0, 127);
  int potValB = map(smoothPotB, 0, 1023, 0, 127);
  int potLedA = map(smoothPotA, 0, 1023, 0, LED_COUNT_RING - 1);
  int potLedB = map(smoothPotB, 0, 1023, 0, LED_COUNT_RING - 1);
  
  // --- Determine active state per half ---  
  // Half A is active if either tofA1 or tofA2 is active.
  bool activeA = isActive[0] || isActive[1];
  // Half B is active if either tofB1 or tofB2 is active.
  bool activeB = isActive[2] || isActive[3];
  
  // --- MIDI: Send Note On/Off and additional CC messages for each half ---
  // Half A (MIDI channel 0)
  if (activeA && !noteActiveA) {
    noteOn(0, NOTE_A, 127);
    noteActiveA = true;
    controlChange(0, 6, 127);  // CC 6: Note A on (always sent on channel 0)
  } else if (!activeA && noteActiveA) {
    noteOff(0, NOTE_A, 0);
    noteActiveA = false;
    controlChange(0, 7, 127);    // CC 7: Note A off (always sent on channel 0)
  }
  
  // Half B (note messages remain on MIDI channel 1,
  // but CC messages will be sent on channel 0)
  if (activeB && !noteActiveB) {
    noteOn(1, NOTE_B, 127);
    noteActiveB = true;
    controlChange(0, 8, 127);  // CC 8: Note B on (sent on channel 0)
  } else if (!activeB && noteActiveB) {
    noteOff(1, NOTE_B, 0);
    noteActiveB = false;
    controlChange(0, 9, 127);    // CC 9: Note B off (sent on channel 0)
  }
  
  // --- MIDI: Send CC messages for potentiometers (always send) ---
  if (abs(potValA - lastPotValA) > MIDI_CHANGE_THRESHOLD) {
    lastPotValA = potValA;
    controlChange(0, CC_POT_A, potValA);
  }
  if (abs(potValB - lastPotValB) > MIDI_CHANGE_THRESHOLD) {
    lastPotValB = potValB;
    controlChange(0, CC_POT_B, potValB);
  }
  
  // --- MIDI: Send CC messages for sensor values (always on channel 0) ---
  if (abs(controlValA1 - lastControlValA1) > MIDI_CHANGE_THRESHOLD) {
    lastControlValA1 = controlValA1;
    controlChange(0, CC_SENSOR_A1, controlValA1);
  }
  if (abs(controlValA2 - lastControlValA2) > MIDI_CHANGE_THRESHOLD) {
    lastControlValA2 = controlValA2;
    controlChange(0, CC_SENSOR_A2, controlValA2);
  }
  if (abs(controlValB1 - lastControlValB1) > MIDI_CHANGE_THRESHOLD) {
    lastControlValB1 = controlValB1;
    controlChange(0, CC_SENSOR_B1, controlValB1);
  }
  if (abs(controlValB2 - lastControlValB2) > MIDI_CHANGE_THRESHOLD) {
    lastControlValB2 = controlValB2;
    controlChange(0, CC_SENSOR_B2, controlValB2);
  }
  
  // --- (Optional) Flush Serial if desired ---
// Serial.flush();
  
  // --- LED Updates ---
  // Clear all LED strips
  potRingA.clear();
  sensorRingA1.clear();
  sensorRingA2.clear();
  
  potRingB.clear();
  sensorDotB1.clear();
  sensorDotB2.clear();
  
  // Potentiometer LED feedback (static style)
  potRingA.fill(potRingA.ColorHSV(1000, 255, 100));
  potRingB.fill(potRingB.ColorHSV(1000, 255, 100));
  potRingA.setPixelColor(potLedA - 1, potRingA.ColorHSV(60, 130, 80));
  potRingA.setPixelColor(potLedA,     potRingA.ColorHSV(60, 130, 255));
  potRingA.setPixelColor(potLedA + 1, potRingA.ColorHSV(60, 130, 80));
  
  potRingB.setPixelColor(potLedB - 1, potRingB.ColorHSV(60, 130, 80));
  potRingB.setPixelColor(potLedB,     potRingB.ColorHSV(60, 130, 255));
  potRingB.setPixelColor(potLedB + 1, potRingB.ColorHSV(60, 130, 80));
  
  // LED Animations (ToF sensors)
  // Half A: tofA1 controls sensorRingA1; tofA2 controls sensorRingA2.
  if (isActive[0]) { 
    waveAnimation(sensorRingA1, 0, 255, animationPhase[0], animationSpeed[0]);
  } else {
    breatheAnimation(sensorRingA1, 0, 255);
  }
  if (isActive[1]) {
    waveAnimation(sensorRingA2, 0, 255, animationPhase[1], animationSpeed[1]);
  } else {
    breatheAnimation(sensorRingA2, 0, 255);
  }
  // Half B: tofB1 controls sensorDotB1; tofB2 controls sensorDotB2.
  if (isActive[2]) {
    waveAnimation(sensorDotB1, 43690, 255, animationPhase[2], animationSpeed[2]);
  } else {
    breatheAnimation(sensorDotB1, 43690, 255);
  }
  if (isActive[3]) {
    waveAnimation(sensorDotB2, 43690, 255, animationPhase[3], animationSpeed[3]);
  } else {
    breatheAnimation(sensorDotB2, 43690, 255);
  }
  
  // Write updates to LED strips
  potRingA.show();
  sensorRingA1.show();
  sensorRingA2.show();
  
  potRingB.show();
  sensorDotB1.show();
  sensorDotB2.show();
  
  // --- Debug Output (compatible with Teleplot) ---
  /*
  static unsigned long lastDebug = 0;
  if (millis() - lastDebug > 100) {
    // Human-readable debug output (optional)
    Serial.println("\n===== THEREMIN CONTROLLER DEBUG =====");
    Serial.println("--- Half A ---");
    Serial.print("POT_A: "); Serial.print(potValA);
    Serial.print(" | RAW_TOF_A1: "); Serial.print(distanceA1);
    Serial.print(" | MIDI_TOF_A1: "); Serial.print(controlValA1);
    Serial.print(" | RAW_TOF_A2: "); Serial.print(distanceA2);
    Serial.print(" | MIDI_TOF_A2: "); Serial.println(controlValA2);
    Serial.println("--- Half B ---");
    Serial.print("POT_B: "); Serial.print(potValB);
    Serial.print(" | RAW_TOF_B1: "); Serial.print(distanceB1);
    Serial.print(" | MIDI_TOF_B1: "); Serial.print(controlValB1);
    Serial.print(" | RAW_TOF_B2: "); Serial.print(distanceB2);
    Serial.print(" | MIDI_TOF_B2: "); Serial.println(controlValB2);
    
    // Teleplot-compatible CSV output:
    // Format: teleplot,<potA>,<rawA1>,<MIDI_A1>,<rawA2>,<MIDI_A2>,<potB>,<rawB1>,<MIDI_B1>,<rawB2>,<MIDI_B2>
    Serial.print("teleplot,");
    Serial.print(potValA); Serial.print(",");
    Serial.print(distanceA1); Serial.print(",");
    Serial.print(controlValA1); Serial.print(",");
    Serial.print(distanceA2); Serial.print(",");
    Serial.print(controlValA2); Serial.print(",");
    Serial.print(potValB); Serial.print(",");
    Serial.print(distanceB1); Serial.print(",");
    Serial.print(controlValB1); Serial.print(",");
    Serial.print(distanceB2); Serial.println(",");
    Serial.println(controlValB2);
    
    lastDebug = millis();
  }
  */
  delay(1);
}

// ---------- Animation Functions ----------

void updateSensorAnimation(int sensorIndex, int distance) {
  // If the sensor reading indicates something is present
  if (distance < SENSOR_MAX_DISTANCE) {
    // Always mark as active if below max—even if there's no change.
    isActive[sensorIndex] = true;
    lastActivityTime[sensorIndex] = millis();
  } else {
    // If the sensor reads its default max value, then check for idle timeout.
    if (millis() - lastActivityTime[sensorIndex] > IDLE_TIMEOUT) {
      isActive[sensorIndex] = false;
    }
  }
  
  // Additionally, if there is a significant change in the sensor reading,
  // treat the sensor as active and update the last activity time.
  if (abs(distance - prevDistance[sensorIndex]) > ACTIVITY_THRESHOLD) {
    isActive[sensorIndex] = true;
    lastActivityTime[sensorIndex] = millis();
  }
  
  // Map the animation speed directly from the sensor reading.
  // A lower (closer) reading gives a slower animation, and a further reading gives a faster animation.
  animationSpeed[sensorIndex] = map(distance, SENSOR_MIN_DISTANCE, SENSOR_MAX_DISTANCE,
                                    ANIMATION_MIN_SPEED, ANIMATION_MAX_SPEED);
  
  // Advance the animation phase. If inactive, use a slow idle rate.
  animationPhase[sensorIndex] += isActive[sensorIndex] ? animationSpeed[sensorIndex] : (ANIMATION_MIN_SPEED / 2);
  
  prevDistance[sensorIndex] = distance;
}

void breatheAnimation(Adafruit_NeoPixel &strip, uint16_t hue, uint8_t sat) {
  float breath = (sin(millis() / 2000.0 * PI) + 1.0) / 2.0;
  uint8_t brightness = IDLE_BRIGHTNESS * breath + IDLE_BRIGHTNESS / 2;
  for (int i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, strip.ColorHSV(hue, sat, brightness));
  }
}

void waveAnimation(Adafruit_NeoPixel &strip, uint16_t hue, uint8_t sat, uint16_t phase, int speed) {
  int numPixels = strip.numPixels();
  uint8_t maxBrightness = map(speed, ANIMATION_MIN_SPEED, ANIMATION_MAX_SPEED,
                              ACTIVE_BRIGHTNESS / 2, ACTIVE_BRIGHTNESS);
  int falloff = map(speed, ANIMATION_MIN_SPEED, ANIMATION_MAX_SPEED, 2, 4);
  for (int i = 0; i < numPixels; i++) {
    float wavePos = (i + (phase / 100.0)) / float(numPixels);
    wavePos -= floor(wavePos);
    float wave = (sin(wavePos * 2 * PI * falloff) + 1.0) / 2.0;
    uint8_t brightness = wave * maxBrightness;
    strip.setPixelColor(i, strip.ColorHSV(hue, sat, brightness));
  }
}