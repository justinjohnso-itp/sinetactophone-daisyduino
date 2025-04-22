#include <Arduino.h>
#include <Wire.h>
#include "SparkFun_VL53L5CX_Library.h"
#include "Adafruit_NeoPixel.h"
#include <SparkFun_I2C_Mux_Arduino_Library.h>
#include <math.h>

// Hardware Definitions
#define LED_PIN_SENSOR_A1  2
#define LED_PIN_SENSOR_A2  3 
#define LED_PIN_SENSOR_A3  4  
#define LED_PIN_SENSOR_B1  5
#define LED_PIN_SENSOR_B2  6 
#define LED_PIN_SENSOR_B3  7 

#define LED_COUNT_RING 24

Adafruit_NeoPixel sensorRingA1(LED_COUNT_RING, LED_PIN_SENSOR_A1, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel sensorRingA2(LED_COUNT_RING, LED_PIN_SENSOR_A2, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel sensorRingA3(LED_COUNT_RING, LED_PIN_SENSOR_A3, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel sensorRingB1(LED_COUNT_RING, LED_PIN_SENSOR_B1, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel sensorRingB2(LED_COUNT_RING, LED_PIN_SENSOR_B2, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel sensorRingB3(LED_COUNT_RING, LED_PIN_SENSOR_B3, NEO_GRB + NEO_KHZ800);

const int NUM_CHANNELS_TO_CHECK = 8;
const int MAX_SENSORS = 8; 
const int SENSOR_RESOLUTION = 16;
const int IMAGE_WIDTH = (SENSOR_RESOLUTION == 16) ? 4 : 8;
const int RANGING_FREQUENCY_HZ = 20;

// Define the multiplexer channels to check.
const uint8_t sensorChannels[NUM_CHANNELS_TO_CHECK] = {0, 1, 2, 3, 4, 5, 6, 7};

// Define new unique I2C addresses to potentially assign for each channel slot
const byte NEW_ADDRESS_0 = 0x30;
const byte NEW_ADDRESS_1 = 0x31;
const byte NEW_ADDRESS_2 = 0x32;
const byte NEW_ADDRESS_3 = 0x33;
const byte NEW_ADDRESS_4 = 0x34;
const byte NEW_ADDRESS_5 = 0x35;
const byte NEW_ADDRESS_6 = 0x36; 
const byte NEW_ADDRESS_7 = 0x37;
const byte newAddresses[MAX_SENSORS] = {
    NEW_ADDRESS_0, NEW_ADDRESS_1, NEW_ADDRESS_2, NEW_ADDRESS_3,
    NEW_ADDRESS_4, NEW_ADDRESS_5, NEW_ADDRESS_6, NEW_ADDRESS_7
};
const byte DEFAULT_ADDRESS = 0x29; // Default address of the VL53L5CX
const byte TCA_ADDRESS = 0x70; // Default address for Qwiic Mux

// --- Global Objects ---
QWIICMUX qwiicMux;
SparkFun_VL53L5CX myImagers[MAX_SENSORS];
VL53L5CX_ResultsData measurementData;
bool sensorConfigured[MAX_SENSORS] = {false};

// Animation constants 
const int SENSOR_MIN_DISTANCE = 50;   // mm - minimum expected detection distance
const int SENSOR_MAX_DISTANCE = 1000; // mm - maximum expected detection distance
const int ACTIVITY_THRESHOLD = 10;     // mm change to trigger activity
const unsigned long IDLE_TIMEOUT = 500; // ms until idle
const int ANIMATION_MIN_SPEED = 20;     // Slower animation speed when close
const int ANIMATION_MID_SPEED = 100;    // Medium animation speed
const int ANIMATION_MAX_SPEED = 250;    // Faster animation speed when far
const uint8_t IDLE_BRIGHTNESS = 10;     // Minimum brightness during idle breath
const uint8_t ACTIVE_BRIGHTNESS = 20;   // Maximum brightness when active

// Animation state (one per sensor)
unsigned long lastActivityTime[MAX_SENSORS] = {0};
uint16_t animationPhase[MAX_SENSORS] = {0};
int animationSpeed[MAX_SENSORS] = {0};
bool isActive[MAX_SENSORS] = {false};
int prevDistance[MAX_SENSORS] = {0};
 
// Maps each ToF sensor index to its corresponding LED ring
Adafruit_NeoPixel* sensorRings[MAX_SENSORS] = {
  &sensorRingB3, // ToF 0 -> LED D7
  nullptr,       // ToF 1 (not used)
  nullptr,       // ToF 2 (not used)
  &sensorRingB1, // ToF 3 -> LED D5
  &sensorRingA3, // ToF 4 -> LED D4
  &sensorRingA2, // ToF 5 -> LED D3
  &sensorRingA1, // ToF 6 -> LED D2
  &sensorRingB2  // ToF 7 -> LED D6
};

// LED animation colors (HSV hue values) - based on physical layout
// D5-D7 (physical side A): Warm/earth tones
// D2-D4 (physical side B): Cool tones
const uint16_t ringColors[MAX_SENSORS] = {
  5461,   // ToF 0 -> LED D7 (warm: yellow-orange)
  0,      // ToF 1 (not used)
  0,      // ToF 2 (not used)
  0,      // ToF 3 -> LED D5 (warm: red)
  54613,  // ToF 4 -> LED D4 (cool: magenta)
  49151,  // ToF 5 -> LED D3 (cool: blue-purple)
  43690,  // ToF 6 -> LED D2 (cool: blue)
  10922   // ToF 7 -> LED D6 (warm: green)
};

// Helper function to select Mux channel/port
void selectChannel(uint8_t channel) {
  if (channel >= 8) {
    Serial.print("Error: Invalid Mux channel "); 
    Serial.println(channel);
    Serial1.print("Error: Invalid Mux channel ");
    Serial1.println(channel);
    return;
  }

  if (!qwiicMux.setPort(channel)) {
    Serial.print("Error: Failed to set Mux port to ");
    Serial.println(channel);
    Serial1.print("Error: Failed to set Mux port to ");
    Serial1.println(channel);
  }
  delay(10); // Small delay for stability after switching ports
}

// Animation constants
#define IDLE_BREATH_PERIOD 5000  // Period of breathing cycle in milliseconds

void breatheAnimation(Adafruit_NeoPixel &strip, uint8_t r, uint8_t g, uint8_t b) {
  // Compute angle for full sine wave over period
  float phase = (millis() % IDLE_BREATH_PERIOD) / (float)IDLE_BREATH_PERIOD * TWO_PI;
  float factor = (sin(phase) + 1.0f) * 0.5f;        // 0.0→1.0→0.0
  // Map factor (0.0-1.0) to brightness range (IDLE_BRIGHTNESS to ACTIVE_BRIGHTNESS)
  uint8_t bri = IDLE_BRIGHTNESS + (factor * (ACTIVE_BRIGHTNESS - IDLE_BRIGHTNESS));

  strip.setBrightness(bri);
  for (int i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, strip.Color(r, g, b));
  }
}

// HSV-based version of breatheAnimation to match the reference
void breatheAnimationHSV(Adafruit_NeoPixel &strip, uint16_t hue, uint8_t sat) {
  float breath = (sin(millis() / (float)IDLE_BREATH_PERIOD * PI) + 1.0) / 2.0;
  uint8_t brightness = IDLE_BRIGHTNESS + (breath * (ACTIVE_BRIGHTNESS - IDLE_BRIGHTNESS));
  
  for (int i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, strip.ColorHSV(hue, sat, brightness));
  }
}

// Wave animation for active sensors - creates a moving pattern around the ring
void waveAnimation(Adafruit_NeoPixel &strip, uint16_t hue, uint8_t sat, uint16_t phase, int speed) {
  int numPixels = strip.numPixels();
  
  // Map animation speed to brightness (closer = brighter)
  uint8_t maxBrightness = map(speed, ANIMATION_MIN_SPEED, ANIMATION_MAX_SPEED, 
                              ACTIVE_BRIGHTNESS / 2, ACTIVE_BRIGHTNESS);
                              
  // Map animation speed to wave frequency (closer = more waves)
  int falloff = map(speed, ANIMATION_MIN_SPEED, ANIMATION_MAX_SPEED, 2, 4);
  
  for (int i = 0; i < numPixels; i++) {
    // Calculate wave position for this pixel (normalized 0-1)
    float wavePos = (i + (phase / 100.0)) / float(numPixels);
    wavePos -= floor(wavePos);  // Wrap around to keep in 0-1 range
    
    // Generate sine wave and map to brightness
    float wave = (sin(wavePos * TWO_PI * falloff) + 1.0) / 2.0;
    uint8_t brightness = wave * maxBrightness;
    
    strip.setPixelColor(i, strip.ColorHSV(hue, sat, brightness));
  }
}

// Process a sensor reading and update its animation state
void updateSensorAnimation(int sensorIndex, int distance) {
  // If the sensor reading indicates something is present
  if (distance < SENSOR_MAX_DISTANCE) {
    // Mark as active if below max detection distance
    isActive[sensorIndex] = true;
    lastActivityTime[sensorIndex] = millis();
  } else {
    // If the sensor reads its default max value, check for idle timeout
    if (millis() - lastActivityTime[sensorIndex] > IDLE_TIMEOUT) {
      isActive[sensorIndex] = false;
    }
  }
  
  // Additionally, if there's a significant change in the sensor reading,
  // treat the sensor as active and update the last activity time
  if (abs(distance - prevDistance[sensorIndex]) > ACTIVITY_THRESHOLD) {
    isActive[sensorIndex] = true;
    lastActivityTime[sensorIndex] = millis();
  }
  
  // Map animation speed based on distance
  // A closer reading gives a slower animation, farther is faster
  animationSpeed[sensorIndex] = map(
    constrain(distance, SENSOR_MIN_DISTANCE, SENSOR_MAX_DISTANCE),
    SENSOR_MIN_DISTANCE, SENSOR_MAX_DISTANCE,
    ANIMATION_MIN_SPEED, ANIMATION_MAX_SPEED
  );
  
  // Advance the animation phase based on active state
  animationPhase[sensorIndex] += isActive[sensorIndex] ? 
    animationSpeed[sensorIndex] : (ANIMATION_MIN_SPEED / 2);
  
  // Store current distance for next comparison  
  prevDistance[sensorIndex] = distance;
}

void setup() {
  // Initialize communication interfaces
  Serial.begin(115200);  // USB Serial for debugging
  Serial1.begin(115200); // Hardware Serial for Daisy
  
  Serial.println("--- Nano 33 IoT Sensor Controller ---");
  Serial1.println("Nano 33 IoT - SparkFun Qwiic Mux & VL53L5CX Address Setting");
  
  Wire.begin(); // Initialize I2C

  // Initialize all NeoPixel LED rings
  sensorRingA1.begin();
  sensorRingA2.begin();
  sensorRingA3.begin();
  sensorRingB1.begin();
  sensorRingB2.begin();
  sensorRingB3.begin();
  Serial.println("NeoPixel Rings Initialized.");

  // Initialize Multiplexer
  Serial.println("Looking for Qwiic Mux...");
  Serial1.println("Looking for Qwiic Mux...");
  if (!qwiicMux.begin()) {
    Serial.print("Qwiic Mux not found at default address 0x");
    Serial.println(TCA_ADDRESS, HEX);
    Serial1.print("Qwiic Mux not found at default address 0x");
    Serial1.println(TCA_ADDRESS, HEX);
    Serial.println("Check wiring. Freezing...");
    Serial1.println("Check wiring. Freezing...");
    while (1);
  }
  
  if (!qwiicMux.isConnected()) {
    Serial.println("Qwiic Mux connected but isConnected() returned false. Freezing...");
    Serial1.println("Qwiic Mux connected but isConnected() returned false. Freezing...");
    while(1);
  }
  
  Serial.println("Qwiic Mux found and connected.");
  Serial1.println("Qwiic Mux found and connected.");

  // Set unique I2C addresses for sensors
  Serial.println("\nAttempting to set unique I2C addresses...");
  Serial1.println("\nAttempting to set unique I2C addresses...");

  for (int i = 0; i < NUM_CHANNELS_TO_CHECK; i++) {
    uint8_t channel = sensorChannels[i];

    if (channel >= MAX_SENSORS) {
        Serial.print("Error: Channel index "); Serial.print(channel);
        Serial.println(" is out of bounds for address array. Skipping.");
        Serial1.print("Error: Channel index "); Serial1.print(channel);
        Serial1.println(" is out of bounds for address array. Skipping.");
        continue;
    }
    byte newAddr = newAddresses[channel];

    Serial.print("Checking/Setting Sensor on Channel "); Serial.print(channel);
    Serial.print(" to Address 0x"); Serial.println(newAddr, HEX);
    Serial1.print("Checking/Setting Sensor on Channel "); Serial1.print(channel);
    Serial1.print(" to Address 0x"); Serial1.println(newAddr, HEX);

    selectChannel(channel);
    delay(50);

    Wire.beginTransmission(newAddr);
    byte error = Wire.endTransmission();

    if (error == 0) {
        Serial.println("  Sensor already at target address.");
        Serial1.println("  Sensor already at target address.");
        sensorConfigured[channel] = true;
    } else {
        Serial.println("  Sensor not at target address. Scanning channel...");
        Serial1.println("  Sensor not at target address. Scanning channel...");
        byte foundAddr = 0;
        int devicesFound = 0;

        // Scan for devices on the current channel
        for (byte scanAddr = 1; scanAddr < 127; scanAddr++) {
            if (scanAddr == newAddr) continue; // Skip the target address itself

            Wire.beginTransmission(scanAddr);
            byte scanError = Wire.endTransmission();

            if (scanError == 0) {
                Serial.print("    Found device at address 0x"); Serial.println(scanAddr, HEX);
                Serial1.print("    Found device at address 0x"); Serial1.println(scanAddr, HEX);
                devicesFound++;
                foundAddr = scanAddr;
                // Assuming only one sensor per channel, break after finding one
                break;
            } else if (scanError != 2) { // Error code 2 means address NACK'd (no device)
                Serial.print("    Scan error "); Serial.print(scanError); Serial.print(" at address 0x"); Serial.println(scanAddr, HEX);
                Serial1.print("    Scan error "); Serial1.print(scanError); Serial1.print(" at address 0x"); Serial1.println(scanAddr, HEX);
            }
            delay(2); // Small delay between scan attempts
        }

        // Process scan results
        if (devicesFound == 1) {
            Serial.print("  Attempting begin() at found address 0x"); Serial.println(foundAddr, HEX); // Log to USB Serial
            Serial1.print("  Attempting begin() at found address 0x"); Serial1.println(foundAddr, HEX); // Log to Hardware Serial1

            // Temporarily use the found address to initialize
            if (myImagers[channel].begin(foundAddr) == false) {
                Serial.println("  Failed to initialize sensor at found address. Skipping channel."); // Log to USB Serial
                Serial1.println("  Failed to initialize sensor at found address. Skipping channel."); // Log to Hardware Serial1
                delay(500); // Delay to allow reading the error
                continue;
            }
            Serial.println("  Sensor initialized at found address."); // Log to USB Serial
            Serial1.println("  Sensor initialized at found address."); // Log to Hardware Serial1

            // Now set the new address
            Serial.print("  Setting new address to 0x"); Serial.println(newAddr, HEX); // Log to USB Serial
            Serial1.print("  Setting new address to 0x"); Serial1.println(newAddr, HEX); // Log to Hardware Serial1

            if (myImagers[channel].setAddress(newAddr) == false) {
                Serial.println("  Failed to set new I2C address. Skipping channel."); // Log to USB Serial
                Serial1.println("  Failed to set new I2C address. Skipping channel."); // Log to Hardware Serial1
                delay(500); // Delay to allow reading the error
                continue;
            }
            Serial.println("  New address set successfully."); // Log to USB Serial
            Serial1.println("  New address set successfully."); // Log to Hardware Serial1
            sensorConfigured[channel] = true; // Mark as configured with the new address
            delay(50); // Allow time for address change to settle

        } else if (devicesFound == 0) {
            Serial.println("  No sensor found on this channel during scan. Skipping channel."); // Log to USB Serial
            Serial1.println("  No sensor found on this channel during scan. Skipping channel."); // Log to Hardware Serial1
            // No need for long delay here, just move to next channel
        } else { // devicesFound > 1
            Serial.println("  Multiple devices found on this channel! Check wiring. Skipping channel."); // Log to USB Serial
            Serial1.println("  Multiple devices found on this channel! Check wiring. Skipping channel."); // Log to Hardware Serial1
            delay(500); // Delay to allow reading the error
            continue;
        }
    }
  }
  Serial.println("Address setting phase complete."); // Log to USB Serial
  Serial1.println("Address setting phase complete."); // Log to Hardware Serial1

  // --- Initialize Sensors with NEW Addresses ---
  Serial.println("\nInitializing sensors with their assigned addresses..."); // Log to USB Serial
  Serial1.println("\nInitializing sensors with their assigned addresses..."); // Log to Hardware Serial1

  for (int i = 0; i < MAX_SENSORS; i++) {
    // Only attempt to initialize if it was successfully configured (found/address set)
    if (sensorConfigured[i]) {
        uint8_t channel = i; // Assuming sensor index maps directly to channel for simplicity here
        byte currentAddr = newAddresses[i];

        Serial.print("Initializing Sensor slot "); Serial.print(i); // Log to USB Serial
        Serial.print(" (Channel "); Serial.print(channel);
        Serial.print(") at Address 0x"); Serial.println(currentAddr, HEX);
        Serial1.print("Initializing Sensor slot "); Serial1.print(i); // Log to Hardware Serial1
        Serial1.print(" (Channel "); Serial1.print(channel);
        Serial1.print(") at Address 0x"); Serial1.println(currentAddr, HEX);

        selectChannel(channel); // Select the correct channel before talking to the sensor
        delay(50); // Allow mux to switch and sensor to be ready

        // Initialize the sensor object with its new address
        if (myImagers[i].begin(currentAddr) == false) {
            Serial.print("  Failed to initialize Sensor slot "); Serial.print(i); // Log to USB Serial
            Serial.print(" at address 0x"); Serial.println(currentAddr, HEX);
            Serial.println("  Marking as unconfigured and skipping.");
            Serial1.print("  Failed to initialize Sensor slot "); Serial1.print(i); // Log to Hardware Serial1
            Serial1.print(" at address 0x"); Serial1.println(currentAddr, HEX);
            Serial1.println("  Marking as unconfigured and skipping.");
            sensorConfigured[i] = false; // Mark as not configured if begin fails
            continue; // Skip to the next sensor
        }
        Serial.println("  Sensor initialized."); // Log to USB Serial
        Serial1.println("  Sensor initialized."); // Log to Hardware Serial1

        // Configure sensor settings
        myImagers[i].setResolution(SENSOR_RESOLUTION);
        myImagers[i].setRangingFrequency(RANGING_FREQUENCY_HZ);
        myImagers[i].startRanging();
        Serial.print("  Sensor slot "); Serial.print(i); Serial.println(" ranging started."); // Log to USB Serial
        Serial1.print("  Sensor slot "); Serial1.print(i); Serial1.println(" ranging started."); // Log to Hardware Serial1

    } else {
        // Optional: Log sensors that were not configured
        // Serial.print("Skipping initialization for unconfigured Sensor slot "); Serial.println(i);
        // Serial1.print("Skipping initialization for unconfigured Sensor slot "); Serial1.println(i);
    }
  }

  Serial.println("\n--- Setup Complete ---"); // Log to USB Serial
  Serial1.println("\n--- Setup Complete ---"); // Log to Hardware Serial1
}

void loop() {
  // Clear LED animation state flags for this cycle
  bool ringAnimated[MAX_SENSORS] = {false};

  // Iterate through all potential sensor slots (0 to MAX_SENSORS-1)
  for (int i = 0; i < MAX_SENSORS; i++) {
    // Skip unconfigured sensors
    if (!sensorConfigured[i]) continue;
    
    uint8_t channel = i;
    selectChannel(channel);
    delay(5); // Small delay for mux to switch
    
    if (myImagers[i].isDataReady()) {
      if (myImagers[i].getRangingData(&measurementData)) {
        // Send full zones object per sensor to Daisy
        Serial1.print(">S"); Serial1.print(i); Serial1.print(":"); 
        Serial1.print("{\"zones\":[");
        for (int z = 0; z < SENSOR_RESOLUTION; z++) {
          Serial1.print(measurementData.distance_mm[z]);
          if (z < SENSOR_RESOLUTION - 1) Serial1.print(",");
        }
        Serial1.print("]}");
        Serial1.println();
        
        // Calculate average distance for animation
        uint32_t sum = 0;
        for (int z = 0; z < SENSOR_RESOLUTION; z++) {
          sum += measurementData.distance_mm[z];
        }
        uint16_t avgDistance = sum / SENSOR_RESOLUTION;
        
        // Update animation state based on distance
        updateSensorAnimation(i, avgDistance);
        
        // Check if we have a paired LED ring for this sensor
        if (i < 6 && sensorRings[i] != nullptr) {
          // Mark that we've handled animation for this ring
          ringAnimated[i] = true;
          
          // Apply active wave animation or idle breathing based on sensor state
          if (isActive[i]) {
            waveAnimation(*sensorRings[i], ringColors[i], 255, animationPhase[i], animationSpeed[i]);
          } else {
            breatheAnimationHSV(*sensorRings[i], ringColors[i], 255);
          }
          
          // Update the LED ring immediately
          sensorRings[i]->show();
        }
      }
    }
  }
  
  // For any rings not updated by an active sensor, show breathing animation
  for (int i = 0; i < 6; i++) { // We only have 6 actual LED rings
    if (!ringAnimated[i] && sensorRings[i] != nullptr) {
      breatheAnimationHSV(*sensorRings[i], ringColors[i], 255);
      sensorRings[i]->show();
    }
  }
  
  delay(10); // Small delay between loops for stability
}
