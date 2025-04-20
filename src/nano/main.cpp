#include <Arduino.h>
#include <Wire.h>
#include "SparkFun_VL53L5CX_Library.h"
#include "Adafruit_NeoPixel.h"
#include "TCA9548.h"

// Define LEDs
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
const int RANGING_FREQUENCY_HZ = 60;

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
const byte TCA_ADDRESS = 0x70; // Default address for TCA9548A

// --- Global Objects ---
TCA9548 tca(TCA_ADDRESS);
SparkFun_VL53L5CX myImagers[MAX_SENSORS];
VL53L5CX_ResultsData measurementData;        
bool sensorConfigured[MAX_SENSORS] = {false};

// Helper function to select TCA channel
void selectChannel(uint8_t channel) {
  if (channel >= 8) {
    Serial.print("Error: Invalid TCA channel ");
    Serial.println(channel);
    return;
  }
  tca.selectChannel(channel);
  delay(10);
}

void setup() {
  Serial.begin(115200);
  while (!Serial);
  Serial.println("Nano 33 IoT - TCA9548 (Rob Tillaart) & VL53L5CX Address Setting");

  Wire.begin();

  // Initialize LEDs
  sensorRingA1.begin();
  sensorRingA2.begin();
  sensorRingA3.begin();
  sensorRingB1.begin();
  sensorRingB2.begin();
  sensorRingB3.begin();

  // Initialize Multiplexer
  Serial.println("Looking for TCA9548...");
  tca.begin();
  if (!tca.isConnected()) {
    Serial.print("TCA9548 not found at address 0x");
    Serial.println(TCA_ADDRESS, HEX);
    Serial.println("Check wiring. Freezing...");
    while (1);
  }
  Serial.println("TCA9548 found.");

  // --- Set Unique I2C Addresses ---
  Serial.println("\nAttempting to set unique I2C addresses...");

  for (int i = 0; i < NUM_CHANNELS_TO_CHECK; i++) {
    uint8_t channel = sensorChannels[i];

    if (channel >= MAX_SENSORS) {
        Serial.print("Error: Channel index "); Serial.print(channel);
        Serial.println(" is out of bounds for address array. Skipping.");
        continue;
    }
    byte newAddr = newAddresses[channel];

    Serial.print("Checking/Setting Sensor on Channel ");
    Serial.print(channel);
    Serial.print(" to Address 0x");
    Serial.println(newAddr, HEX);

    selectChannel(channel);
    delay(50);

    Wire.beginTransmission(newAddr);
    byte error = Wire.endTransmission();

    if (error == 0) {
        Serial.println("  Sensor already at target address.");
        sensorConfigured[channel] = true;
    } else {

        Serial.println("  Sensor not at target address. Scanning channel...");
        byte foundAddr = 0;
        int devicesFound = 0;

        for (byte scanAddr = 1; scanAddr < 127; scanAddr++) {
            if (scanAddr == newAddr) continue;

            Wire.beginTransmission(scanAddr);
            byte scanError = Wire.endTransmission();

            if (scanError == 0) {
                Serial.print("    Found device at address 0x");
                Serial.println(scanAddr, HEX);
                devicesFound++;
                foundAddr = scanAddr;
                break; 
            } else if (scanError != 2) {
                Serial.print("    Scan error "); Serial.print(scanError);
                Serial.print(" at address 0x"); Serial.println(scanAddr, HEX);
            }
            delay(2);
        }

        // Process scan results
        if (devicesFound == 1) {
            Serial.print("  Attempting begin() at found address 0x");
            Serial.println(foundAddr, HEX);

            if (myImagers[channel].begin(foundAddr) == false) {
                Serial.println("  Failed to initialize sensor at found address. Skipping channel.");
                delay(5000);
                continue;
            }
            Serial.println("  Sensor initialized at found address.");

            Serial.print("  Setting new address to 0x");
            Serial.println(newAddr, HEX);

            if (myImagers[channel].setAddress(newAddr) == false) {
                Serial.println("  Failed to set new I2C address. Skipping channel.");
                delay(5000);
                continue;
            }
            Serial.println("  New address set successfully.");
            sensorConfigured[channel] = true; 
            delay(50);

        } else if (devicesFound == 0) {
            Serial.println("  No sensor found on this channel during scan. Skipping channel.");
 
            delay(5000);
            continue;
        } else {
            Serial.println("  Multiple devices found on this channel! Check wiring. Skipping channel.");

            delay(5000);
            continue;
        }
    }
  }
  Serial.println("Address setting phase complete.");

  // --- Initialize Sensors with NEW Addresses ---
  Serial.println("\nInitializing sensors with their assigned addresses...");

  for (int i = 0; i < MAX_SENSORS; i++) {

    if (sensorConfigured[i]) {
        uint8_t channel = i; 
        byte currentAddr = newAddresses[i];

        Serial.print("Initializing Sensor slot "); Serial.print(i);
        Serial.print(" (Channel "); Serial.print(channel);
        Serial.print(") at Address 0x"); Serial.println(currentAddr, HEX);

        selectChannel(channel);
        delay(50);

        if (myImagers[i].begin(currentAddr) == false) {
            Serial.print("  Failed to initialize Sensor slot "); Serial.print(i);
            Serial.print(" at address 0x"); Serial.println(currentAddr, HEX);
            Serial.println("  Marking as unconfigured and skipping.");
            sensorConfigured[i] = false;
            continue;
        }
        Serial.println("  Sensor initialized.");
        myImagers[i].setResolution(SENSOR_RESOLUTION);
        myImagers[i].setRangingFrequency(RANGING_FREQUENCY_HZ);
        myImagers[i].startRanging();
        Serial.print("  Sensor slot "); Serial.print(i); Serial.println(" ranging started.");

    } else {

    }
  }

  Serial.println("\n--- Setup Complete ---");
}

void loop() {

  for (int i = 0; i < LED_COUNT_RING; i++) {
    sensorRingA1.setPixelColor(i, sensorRingA1.Color(255, 0, 0)); // Red
    sensorRingA2.setPixelColor(i, sensorRingA2.Color(0, 255, 0)); // Green
    sensorRingA3.setPixelColor(i, sensorRingA3.Color(0, 0, 255)); // Blue

    sensorRingB1.setPixelColor(i, sensorRingB1.Color(255, 255, 0)); // Yellow
    sensorRingB2.setPixelColor(i, sensorRingB2.Color(0, 255, 255)); // Cyan
    sensorRingB3.setPixelColor(i, sensorRingB3.Color(255, 0, 255)); // Magenta
  }

  // Write updates to LEDs
  sensorRingA1.show();
  sensorRingA2.show();
  sensorRingA3.show();
  sensorRingB1.show();
  sensorRingB2.show();
  sensorRingB3.show();

  // Iterate through all potential sensor slots (0 to MAX_SENSORS-1)
  for (int i = 0; i < MAX_SENSORS; i++) {
    // Check if the sensor is configured before trying to read data
    if (sensorConfigured[i]) {
        uint8_t channel = i;
        selectChannel(channel);

        delay(1);
        if (myImagers[i].isDataReady()) {
          if (myImagers[i].getRangingData(&measurementData)) {
            // Send sensor data to Daisy
            Serial.print("S"); // Start marker
            Serial.print(i);   // Sensor index
            Serial.print(":");
            
            // Send all zone values for this sensor
            for (int j = 0; j < SENSOR_RESOLUTION; j++) {
              Serial.print(measurementData.distance_mm[j]);
              if (j < SENSOR_RESOLUTION - 1) {
                Serial.print(",");
              }
            }
            Serial.println("E"); // End marker

            Serial.print(">s");
            Serial.print(i);
            Serial.print("_zones:");
            for (int j = 0; j < SENSOR_RESOLUTION; j++) {
              Serial.print(measurementData.distance_mm[j]);
              if (j < SENSOR_RESOLUTION - 1) {
                Serial.print(" ");
              }
            }
            Serial.println();
          }
        }
    }
  }
  delay(10);
}
