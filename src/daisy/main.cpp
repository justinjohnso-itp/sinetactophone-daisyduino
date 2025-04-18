#include <DaisyDuino.h>

DaisyHardware patch;

void setup() {
  // Explicitly start Serial communication at 9600 baud rate
  // Daisy Seed uses 'Serial' for USB CDC communication
  Serial.begin(9600);

  // Wait for Serial port to connect. Important for native USB devices.
  // Might not be strictly necessary depending on how you monitor, but good practice.
  // while (!Serial); // This can hang if no serial monitor is connected at startup

  Serial.println("Daisy Receiver Ready");
}

void loop() {
  // Check if there is data available to read from Serial
  if (Serial.available() > 0) {
    // Read the incoming line (expects data terminated by newline from the sender)
    String incomingValue = Serial.readStringUntil('\\n');

    // Print the received value to the Serial Monitor
    Serial.print("Received: ");
    Serial.println(incomingValue);
  }
  
  // Add a small delay to prevent overwhelming the Serial buffer or CPU
  // Adjust as needed based on the sender's rate
  delay(10); 
}
