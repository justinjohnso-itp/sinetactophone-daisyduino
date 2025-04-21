#include <Arduino.h>
#include <Wire.h>
#include "SparkFun_VL53L5CX_Library.h"
#include "Adafruit_NeoPixel.h"
#include <SparkFun_I2C_Mux_Arduino_Library.h>

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
const byte TCA_ADDRESS = 0x70; // Default address for Qwiic Mux

// --- Global Objects ---
QWIICMUX qwiicMux;
SparkFun_VL53L5CX myImagers[MAX_SENSORS];
VL53L5CX_ResultsData measurementData;
bool sensorConfigured[MAX_SENSORS] = {false};

// Helper function to select Mux channel/port
void selectChannel(uint8_t channel) {
  if (channel >= 8) {
    Serial.print("Error: Invalid Mux channel "); // Log to USB Serial
    Serial.println(channel);
    Serial1.print("Error: Invalid Mux channel "); // Log to Hardware Serial1
    Serial1.println(channel);
    return;
  }

  // Use setPort() as shown in basic library examples
  if (!qwiicMux.setPort(channel)) {
      Serial.print("Error: Failed to set Mux port to "); // Log to USB Serial
      Serial.println(channel);
      Serial1.print("Error: Failed to set Mux port to "); // Log to Hardware Serial1
      Serial1.println(channel);
      // Consider adding error handling here if needed
  }
  delay(10); // Keep delay for stability after switching ports
}

void setup() {
  // Initialize USB Serial for local monitoring
  Serial.begin(115200);
  // while (!Serial); // Optional: Wait for USB monitor connection
  Serial.println("--- Nano 33 IoT Sensor Controller ---");
  Serial.println("USB Monitor Initialized.");

  // Initialize Hardware Serial1 for communication with Daisy
  Serial1.begin(115200);
  Serial.println("Hardware Serial1 (Daisy TX/RX) Initialized.");
  Serial1.println("Nano 33 IoT - SparkFun Qwiic Mux & VL53L5CX Address Setting"); // Send initial message to Daisy

  Wire.begin();
  Serial.println("I2C Initialized.");

  // Initialize LEDs
  sensorRingA1.begin();
  sensorRingA2.begin();
  sensorRingA3.begin();
  sensorRingB1.begin();
  sensorRingB2.begin();
  sensorRingB3.begin();
  Serial.println("NeoPixel Rings Initialized.");

  // Initialize Multiplexer
  Serial.println("Looking for Qwiic Mux..."); // Log to USB Serial
  Serial1.println("Looking for Qwiic Mux..."); // Log to Hardware Serial1
  if (!qwiicMux.begin()) {
    Serial.print("Qwiic Mux not found at default address 0x"); // Log to USB Serial
    Serial.println(TCA_ADDRESS, HEX);
    Serial.println("Check wiring. Freezing...");
    Serial1.print("Qwiic Mux not found at default address 0x"); // Log to Hardware Serial1
    Serial1.println(TCA_ADDRESS, HEX);
    Serial1.println("Check wiring. Freezing...");
    while (1);
  }
  if (!qwiicMux.isConnected()) {
      Serial.println("Qwiic Mux connected but isConnected() returned false. Freezing..."); // Log to USB Serial
      Serial1.println("Qwiic Mux connected but isConnected() returned false. Freezing..."); // Log to Hardware Serial1
      while(1);
  }
  Serial.println("Qwiic Mux found and connected."); // Log to USB Serial
  Serial1.println("Qwiic Mux found and connected."); // Log to Hardware Serial1

  // --- Set Unique I2C Addresses ---
  Serial.println("\nAttempting to set unique I2C addresses..."); // Log to USB Serial
  Serial1.println("\nAttempting to set unique I2C addresses..."); // Log to Hardware Serial1

  for (int i = 0; i < NUM_CHANNELS_TO_CHECK; i++) {
    uint8_t channel = sensorChannels[i];

    if (channel >= MAX_SENSORS) {
        Serial.print("Error: Channel index "); Serial.print(channel); // Log to USB Serial
        Serial.println(" is out of bounds for address array. Skipping.");
        Serial1.print("Error: Channel index "); Serial1.print(channel); // Log to Hardware Serial1
        Serial1.println(" is out of bounds for address array. Skipping.");
        continue;
    }
    byte newAddr = newAddresses[channel];

    Serial.print("Checking/Setting Sensor on Channel "); Serial.print(channel); // Log to USB Serial
    Serial.print(" to Address 0x"); Serial.println(newAddr, HEX);
    Serial1.print("Checking/Setting Sensor on Channel "); Serial1.print(channel); // Log to Hardware Serial1
    Serial1.print(" to Address 0x"); Serial1.println(newAddr, HEX);

    selectChannel(channel);
    delay(50);

    Wire.beginTransmission(newAddr);
    byte error = Wire.endTransmission();

    if (error == 0) {
        Serial.println("  Sensor already at target address."); // Log to USB Serial
        Serial1.println("  Sensor already at target address."); // Log to Hardware Serial1
        sensorConfigured[channel] = true;
    } else {
        Serial.println("  Sensor not at target address. Scanning channel..."); // Log to USB Serial
        Serial1.println("  Sensor not at target address. Scanning channel..."); // Log to Hardware Serial1
        byte foundAddr = 0;
        int devicesFound = 0;

        // Scan for devices on the current channel
        for (byte scanAddr = 1; scanAddr < 127; scanAddr++) {
            if (scanAddr == newAddr) continue; // Skip the target address itself

            Wire.beginTransmission(scanAddr);
            byte scanError = Wire.endTransmission();

            if (scanError == 0) {
                Serial.print("    Found device at address 0x"); Serial.println(scanAddr, HEX); // Log to USB Serial
                Serial1.print("    Found device at address 0x"); Serial1.println(scanAddr, HEX); // Log to Hardware Serial1
                devicesFound++;
                foundAddr = scanAddr;
                // Assuming only one sensor per channel, break after finding one
                break;
            } else if (scanError != 2) { // Error code 2 means address NACK'd (no device), other errors are notable
                Serial.print("    Scan error "); Serial.print(scanError); Serial.print(" at address 0x"); Serial.println(scanAddr, HEX); // Log to USB Serial
                Serial1.print("    Scan error "); Serial1.print(scanError); Serial1.print(" at address 0x"); Serial1.println(scanAddr, HEX); // Log to Hardware Serial1
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
                delay(5000); // Delay to allow reading the error
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
                delay(5000); // Delay to allow reading the error
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
            delay(5000); // Delay to allow reading the error
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

        delay(1); // Small delay might be needed for sensor readiness
        if (myImagers[i].isDataReady()) {
          if (myImagers[i].getRangingData(&measurementData)) {
            // Send sensor data to Daisy via Serial1 ONLY
            for (int j = 0; j < SENSOR_RESOLUTION; j++) {
              Serial1.print(">S"); // Teleplot marker + Sensor marker // Use Serial1 ONLY for data
              Serial1.print(i);    // Sensor index
              Serial1.print("Z");  // Zone marker
              Serial1.print(j);    // Zone index
              Serial1.print(":");
              Serial1.println(measurementData.distance_mm[j]); // Value and newline
            }
            // Optional: Log to USB Serial that data was sent for a sensor
            // Serial.print("Sent data for Sensor "); Serial.println(i);
          } else {
              // Optional: Log if getRangingData failed
              // Serial.print("Failed to get ranging data for Sensor "); Serial.println(i);
          }
        }
        // Optional: Log if data is not ready
        // else { Serial.print("Data not ready for Sensor "); Serial.println(i); }
    }
  }
  delay(10); // Delay between full sensor scans
}
