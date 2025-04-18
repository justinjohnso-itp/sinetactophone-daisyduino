// I2C Scanner Sketch
// Scans the I2C bus for devices and reports their addresses.

#include <Arduino.h>
#include <Wire.h>
#include "SparkFun_VL53L5CX_Library.h" // VL53L5CX library
#include "TCA9548.h"                   // Multiplexer library (Rob Tillaart)

// --- Configuration ---
const int NUM_CHANNELS_TO_CHECK = 8; // Check all 8 channels of the TCA9548A
const int MAX_SENSORS = 8; // Maximum possible sensors we might configure arrays for
const int SENSOR_RESOLUTION = 16; // 4x4 = 16 zones
// const int SENSOR_RESOLUTION = 64; // 8x8 = 64 zones
const int IMAGE_WIDTH = (SENSOR_RESOLUTION == 16) ? 4 : 8;
const int RANGING_FREQUENCY_HZ = 15; // Max 15Hz for 8x8, 60Hz for 4x4

// Define the multiplexer channels to check.
// We'll iterate through these indices. If you only have 6 sensors,
// the code will skip channels where no sensor is found.
// You can customize this array later if you skip specific hardware ports.
const uint8_t sensorChannels[NUM_CHANNELS_TO_CHECK] = {0, 1, 2, 3, 4, 5, 6, 7};

// Define new unique I2C addresses to potentially assign for each channel slot
const byte NEW_ADDRESS_0 = 0x30;
const byte NEW_ADDRESS_1 = 0x31;
const byte NEW_ADDRESS_2 = 0x32;
const byte NEW_ADDRESS_3 = 0x33;
const byte NEW_ADDRESS_4 = 0x34;
const byte NEW_ADDRESS_5 = 0x35;
const byte NEW_ADDRESS_6 = 0x36; // Address for channel 6 slot
const byte NEW_ADDRESS_7 = 0x37; // Address for channel 7 slot
const byte newAddresses[MAX_SENSORS] = {
    NEW_ADDRESS_0, NEW_ADDRESS_1, NEW_ADDRESS_2, NEW_ADDRESS_3,
    NEW_ADDRESS_4, NEW_ADDRESS_5, NEW_ADDRESS_6, NEW_ADDRESS_7
};
const byte DEFAULT_ADDRESS = 0x29; // Default address of the VL53L5CX
const byte TCA_ADDRESS = 0x70; // Default address for TCA9548A

// --- Global Objects ---
TCA9548 tca(TCA_ADDRESS);             // Multiplexer object (Rob Tillaart)
// Size arrays for the maximum possible sensors
SparkFun_VL53L5CX myImagers[MAX_SENSORS];
VL53L5CX_ResultsData measurementData;        // Structure for sensor results (reused)
bool sensorConfigured[MAX_SENSORS] = {false}; // Track configuration status for each channel slot

// Helper function to select TCA channel
// Note: Rob Tillaart's selectChannel returns void. Error checking is less direct.
void selectChannel(uint8_t channel) {
  if (channel >= 8) {
    Serial.print("Error: Invalid TCA channel ");
    Serial.println(channel);
    return; // Exit if channel is invalid
  }
  // Serial.print("Selecting TCA channel "); // Uncomment for verbose debugging
  // Serial.println(channel);
  tca.selectChannel(channel);
  // We can't directly check for success here like with the Adafruit library.
  // Failure will likely manifest as subsequent sensor communication errors.
  delay(10); // Increased delay after switching channel
}

void setup() {
  Serial.begin(115200);
  while (!Serial);
  Serial.println("Nano 33 IoT - TCA9548 (Rob Tillaart) & VL53L5CX Address Setting");

  Wire.begin();

  // Initialize Multiplexer
  Serial.println("Looking for TCA9548...");
  tca.begin(); // Initialize the multiplexer object
  if (!tca.isConnected()) { // Check if the multiplexer is detected
    Serial.print("TCA9548 not found at address 0x");
    Serial.println(TCA_ADDRESS, HEX);
    Serial.println("Check wiring. Freezing...");
    while (1);
  }
  Serial.println("TCA9548 found.");

  // --- Set Unique I2C Addresses ---
  Serial.println("\nAttempting to set unique I2C addresses...");
  // Iterate through all channels defined in sensorChannels
  for (int i = 0; i < NUM_CHANNELS_TO_CHECK; i++) {
    uint8_t channel = sensorChannels[i];
    // Check if channel index is valid before accessing newAddresses
    if (channel >= MAX_SENSORS) {
        Serial.print("Error: Channel index "); Serial.print(channel);
        Serial.println(" is out of bounds for address array. Skipping.");
        continue;
    }
    byte newAddr = newAddresses[channel]; // Use channel index to get target address

    Serial.print("Checking/Setting Sensor on Channel ");
    Serial.print(channel);
    Serial.print(" to Address 0x");
    Serial.println(newAddr, HEX);

    selectChannel(channel); // Select the channel
    delay(50); // Delay AFTER selecting channel

    // Check if sensor is ALREADY at the new address
    Wire.beginTransmission(newAddr);
    byte error = Wire.endTransmission();

    if (error == 0) {
        Serial.println("  Sensor already at target address.");
        sensorConfigured[channel] = true; // Mark this channel slot as configured
    } else {
        // Sensor not at target address. Scan the current channel to find it.
        Serial.println("  Sensor not at target address. Scanning channel...");
        byte foundAddr = 0;
        int devicesFound = 0;

        for (byte scanAddr = 1; scanAddr < 127; scanAddr++) {
            if (scanAddr == newAddr) continue; // Skip target address

            Wire.beginTransmission(scanAddr);
            byte scanError = Wire.endTransmission();

            if (scanError == 0) {
                Serial.print("    Found device at address 0x");
                Serial.println(scanAddr, HEX);
                devicesFound++;
                foundAddr = scanAddr;
                break; // Assume only one sensor per channel
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
            // Use channel index for myImagers array
            if (myImagers[channel].begin(foundAddr) == false) {
                Serial.println("  Failed to initialize sensor at found address. Skipping channel.");
                delay(5000);
                continue;
            }
            Serial.println("  Sensor initialized at found address.");

            Serial.print("  Setting new address to 0x");
            Serial.println(newAddr, HEX);
            // Use channel index for myImagers array
            if (myImagers[channel].setAddress(newAddr) == false) {
                Serial.println("  Failed to set new I2C address. Skipping channel.");
                delay(5000);
                continue;
            }
            Serial.println("  New address set successfully.");
            sensorConfigured[channel] = true; // Mark this channel slot as configured
            delay(50);

        } else if (devicesFound == 0) {
            Serial.println("  No sensor found on this channel during scan. Skipping channel.");
            // sensorConfigured[channel] remains false
            delay(5000); // Optional delay if no sensor found
            continue;
        } else {
            Serial.println("  Multiple devices found on this channel! Check wiring. Skipping channel.");
            // sensorConfigured[channel] remains false
            delay(5000);
            continue;
        }
    }
  }
  Serial.println("Address setting phase complete.");


  // --- Initialize Sensors with NEW Addresses ---
  Serial.println("\nInitializing sensors with their assigned addresses...");
  // Iterate through all potential sensor slots (0 to MAX_SENSORS-1)
  for (int i = 0; i < MAX_SENSORS; i++) {
    // Only proceed if this sensor slot was successfully configured
    if (sensorConfigured[i]) {
        uint8_t channel = i; // Channel index matches the loop index 'i' here
        byte currentAddr = newAddresses[i];

        Serial.print("Initializing Sensor slot "); Serial.print(i);
        Serial.print(" (Channel "); Serial.print(channel);
        Serial.print(") at Address 0x"); Serial.println(currentAddr, HEX);

        selectChannel(channel);
        delay(50);

        // Initialize the persistent sensor object using its NEW address
        if (myImagers[i].begin(currentAddr) == false) {
            Serial.print("  Failed to initialize Sensor slot "); Serial.print(i);
            Serial.print(" at address 0x"); Serial.println(currentAddr, HEX);
            Serial.println("  Marking as unconfigured and skipping.");
            sensorConfigured[i] = false; // Mark as failed
            continue;
        }
        Serial.println("  Sensor initialized.");
        myImagers[i].setResolution(SENSOR_RESOLUTION);
        myImagers[i].setRangingFrequency(RANGING_FREQUENCY_HZ);
        myImagers[i].startRanging();
        Serial.print("  Sensor slot "); Serial.print(i); Serial.println(" ranging started.");

    } else {
        // This slot wasn't configured, skip it silently or add a message if needed
        // Serial.print("Skipping initialization for Sensor slot "); Serial.print(i);
        // Serial.println(" (not configured).");
    }
  }

  Serial.println("\n--- Setup Complete ---");
}

void loop() {
  // Iterate through all potential sensor slots (0 to MAX_SENSORS-1)
  for (int i = 0; i < MAX_SENSORS; i++) {
    // Only read from sensors that were successfully configured
    if (sensorConfigured[i]) {
        uint8_t channel = i; // Channel index matches the loop index 'i'
        byte currentAddr = newAddresses[i];

        selectChannel(channel);

        if (myImagers[i].isDataReady()) {
          if (myImagers[i].getRangingData(&measurementData)) {
            Serial.print("\n--- Sensor slot ");
            Serial.print(i);
            Serial.print(" (Ch:"); Serial.print(channel);
            Serial.print("/Addr:0x"); Serial.print(currentAddr, HEX);
            Serial.println(") ---");
            Serial.println("Distance (mm):");

            for (int j = 0; j < SENSOR_RESOLUTION; j++) {
              Serial.print(measurementData.distance_mm[j]);
              Serial.print("\t");
              if ((j + 1) % IMAGE_WIDTH == 0) {
                Serial.println();
              }
            }
          } else {
              Serial.print("Error getting data from Sensor slot ");
              Serial.println(i);
          }
        }
    }
  }
  delay(50);
}
