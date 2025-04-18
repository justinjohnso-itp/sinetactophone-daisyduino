// I2C Scanner Sketch
// Scans the I2C bus for devices and reports their addresses.

#include <Arduino.h>
#include <Wire.h>
#include "SparkFun_VL53L5CX_Library.h" // VL53L5CX library
#include "TCA9548.h"                   // Multiplexer library (Rob Tillaart)

// --- Configuration ---
const int NUM_SENSORS = 6; // Number of VL53L5CX sensors connected
const int SENSOR_RESOLUTION = 16; // 4x4 = 16 zones
// const int SENSOR_RESOLUTION = 64; // 8x8 = 64 zones
const int IMAGE_WIDTH = (SENSOR_RESOLUTION == 16) ? 4 : 8;
const int RANGING_FREQUENCY_HZ = 15; // Max 15Hz for 8x8, 60Hz for 4x4

// Define the multiplexer channels each sensor is connected to
const uint8_t sensorChannels[NUM_SENSORS] = {0, 1, 2, 3, 4, 5}; // Sensor 0 on Ch 0, ..., Sensor 5 on Ch 5

// Define new unique I2C addresses to assign
const byte NEW_ADDRESS_0 = 0x30;
const byte NEW_ADDRESS_1 = 0x31;
const byte NEW_ADDRESS_2 = 0x32;
const byte NEW_ADDRESS_3 = 0x33; // Added for sensor 3
const byte NEW_ADDRESS_4 = 0x34; // Added for sensor 4
const byte NEW_ADDRESS_5 = 0x35; // Added for sensor 5
const byte newAddresses[NUM_SENSORS] = {NEW_ADDRESS_0, NEW_ADDRESS_1, NEW_ADDRESS_2, NEW_ADDRESS_3, NEW_ADDRESS_4, NEW_ADDRESS_5};
const byte DEFAULT_ADDRESS = 0x29; // Default address of the VL53L5CX
const byte TCA_ADDRESS = 0x70; // Default address for TCA9548A

// --- Global Objects ---
TCA9548 tca(TCA_ADDRESS);             // Multiplexer object (Rob Tillaart)
SparkFun_VL53L5CX myImagers[NUM_SENSORS];    // Array for sensor objects
VL53L5CX_ResultsData measurementData;        // Structure for sensor results (reused)

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
  delay(1); // Small delay after switching might help stability
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
  for (int i = 0; i < NUM_SENSORS; i++) {
    uint8_t channel = sensorChannels[i];
    byte newAddr = newAddresses[i];

    Serial.print("Checking/Setting Sensor on Channel ");
    Serial.print(channel);
    Serial.print(" to Address 0x");
    Serial.println(newAddr, HEX);

    selectChannel(channel); // Select the channel

    // Temporarily use one of the imager objects to check/set address
    // Check if sensor is ALREADY at the new address
    Wire.beginTransmission(newAddr);
    byte error = Wire.endTransmission();

    if (error == 0) {
        Serial.println("  Sensor already at target address.");
    } else {
        // Sensor not at new address, try initializing at default address
        Serial.print("  Attempting begin() at default address 0x");
        Serial.println(DEFAULT_ADDRESS, HEX);
        // Use a temporary object or re-use myImagers[i] for this setup phase
        if (myImagers[i].begin(DEFAULT_ADDRESS) == false) {
          Serial.print("  Sensor not found at default address 0x");
          Serial.print(DEFAULT_ADDRESS, HEX);
          Serial.println(" on this channel. Check wiring. Freezing...");
          while (1);
        }
        Serial.println("  Sensor found at default address.");

        // Set the new address using the correct function name
        Serial.print("  Setting new address to 0x");
        Serial.println(newAddr, HEX);
        // *** Use setAddress as suggested by the compiler ***
        if (myImagers[i].setAddress(newAddr) == false) {
            Serial.println("  Failed to set new I2C address. Freezing...");
            while(1);
        }
        Serial.println("  New address set successfully.");
        delay(50); // Give sensor time to process address change
    }
  }
  Serial.println("Address setting phase complete.");


  // --- Initialize Sensors with NEW Addresses ---
  Serial.println("\nInitializing sensors with their assigned addresses...");
  for (int i = 0; i < NUM_SENSORS; i++) {
    uint8_t channel = sensorChannels[i];
    byte currentAddr = newAddresses[i]; // Use the NEW address

    Serial.print("Initializing Sensor "); Serial.print(i);
    Serial.print(" on Channel "); Serial.print(channel);
    Serial.print(" at Address 0x"); Serial.println(currentAddr, HEX);

    selectChannel(channel); // Select the channel

    // Initialize the persistent sensor object using its NEW address
    if (myImagers[i].begin(currentAddr) == false) {
        Serial.print("  Failed to initialize Sensor "); Serial.print(i);
        Serial.print(" at address 0x"); Serial.println(currentAddr, HEX);
        Serial.println("  Check wiring/address setting. Freezing...");
        while(1);
    }
    Serial.println("  Sensor initialized.");

    // Configure and start ranging
    myImagers[i].setResolution(SENSOR_RESOLUTION);
    myImagers[i].setRangingFrequency(RANGING_FREQUENCY_HZ);
    myImagers[i].startRanging();
    Serial.print("  Sensor "); Serial.print(i); Serial.println(" ranging started.");
  }

  Serial.println("\n--- Setup Complete ---");
}

void loop() {
  for (int i = 0; i < NUM_SENSORS; i++) {
    uint8_t channel = sensorChannels[i];
    byte currentAddr = newAddresses[i]; // Sensor's assigned address

    // Select the channel for the sensor we want to check
    selectChannel(channel);

    // Check if data is ready for the current sensor
    // The object myImagers[i] is already configured for its unique address
    if (myImagers[i].isDataReady()) {
      // Get the data
      if (myImagers[i].getRangingData(&measurementData)) {
        Serial.print("\n--- Sensor ");
        Serial.print(i);
        Serial.print(" (Ch:"); Serial.print(channel);
        Serial.print("/Addr:0x"); Serial.print(currentAddr, HEX);
        Serial.println(") ---");
        Serial.println("Distance (mm):");

        // Iterate through each zone (pixel)
        for (int j = 0; j < SENSOR_RESOLUTION; j++) {
          Serial.print(measurementData.distance_mm[j]);
          Serial.print("\t"); // Tab spacing

          // Newline after each row
          if ((j + 1) % IMAGE_WIDTH == 0) {
            Serial.println();
          }
        }
      } else {
          Serial.print("Error getting data from Sensor ");
          Serial.println(i);
      }
    }
  }
  // Add a small delay outside the sensor loop if readings are too fast
  delay(50); // Read all sensors approx every 50ms + processing time
}
