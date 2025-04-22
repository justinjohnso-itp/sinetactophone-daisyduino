I spent a fair bit of time trying to get the DaisyDuino library code to run from the external QSPI flash memory on the Daisy Seed. The idea was to free up the internal flash, which seemed like a good practice given the library's size. I went through the process of configuring PlatformIO build flags, linker sections, and adding QSPI initialization code, as documented in [my previous post on configuring QSPI](./2025-04-22-configuring-qspi-for-daisyduino.md).

Ultimately, while I got it *building* after some LDF mode adjustments ([documented here](./2025-04-22-debugging-daisy-build-ldf-mode.md)), I wasn't entirely confident it was working correctly or if it was worth the complexity right now. It felt like a potential source of subtle bugs down the line, and I wanted to focus on the core functionality: audio synthesis based on sensor input.

So, I decided to pivot. Instead of trying to cram everything onto the Daisy and optimize memory placement, I'd offload the sensor reading part to a separate microcontroller. I had an Arduino Nano 33 IoT handy, which has plenty of analog inputs and is easy to work with.

The new plan: The Nano 33 IoT will read the analog sensor(s) and send the values over a serial connection to the Daisy Seed. The Daisy will then focus purely on receiving these values and using them to control the audio synthesis.

This immediately brought up a configuration challenge: how to manage code for two different microcontrollers within a single PlatformIO project. I needed separate source files and distinct build configurations for each board.

First, I decided to organize the source code. I created two new files in the `src/` directory:
- `src/nano_33_iot_sender.cpp`
- `src/daisy_receiver.cpp`

Next, I had to modify the `platformio.ini` file to tell PlatformIO about these two separate targets. I added a new environment block for the Nano 33 IoT (`[env:nano33iot]`) alongside the existing one for the Daisy (`[env:electrosmith_daisy]`). The crucial part was using the `build_src_filter` option within each environment. This lets you specify exactly which source files should be included or excluded when building for that specific environment. This prevents code intended for one board from being compiled for the other.

My `platformio.ini` looked something like this:

```ini
; PlatformIO Project Configuration File

[env:electrosmith_daisy]
platform = ststm32
board = electrosmith_daisy
framework = arduino
lib_deps = 
	Wire
	# Using GitHub source for latest DaisyDuino
	https://github.com/electro-smith/DaisyDuino.git 
lib_ldf_mode = deep+ # Kept from previous debugging
build_flags = 
	-D HAL_SDRAM_MODULE_ENABLED
	-D USBD_USE_CDC
	-D USBCON
upload_protocol = dfu
; Specify the source file for the Daisy, exclude others
build_src_filter = +<src/daisy_receiver.cpp> -<src/nano_33_iot_sender.cpp> -<src/main.cpp>

[env:nano33iot]
platform = atmelsam
board = nano_33_iot
framework = arduino
; Specify the source file for the Nano 33 IoT, exclude others
build_src_filter = +<src/nano_33_iot_sender.cpp> -<src/daisy_receiver.cpp> -<src/main.cpp>
```
I kept the `lib_ldf_mode = deep+` for the Daisy environment, as that seemed necessary from the previous build debugging. I also made sure the DaisyDuino library was still being pulled from GitHub.

With the configuration set up, I wrote the code for each board.

For the Nano 33 IoT (`src/nano_33_iot_sender.cpp`):

```cpp
#include <Arduino.h>

// Define the analog pin
const int ANALOG_PIN = A0;

void setup() {
  // Initialize Serial1 (hardware serial on pins TX/RX) for communication with Daisy
  Serial1.begin(115200);
  // Initialize Serial (USB serial) for debugging if needed
  Serial.begin(115200);
  pinMode(ANALOG_PIN, INPUT);
  Serial.println("Nano 33 IoT Sender Ready");
}

void loop() {
  // Read the analog value (0-1023)
  int sensorValue = analogRead(ANALOG_PIN);

  // Send the value over Serial1
  Serial1.println(sensorValue);

  // Optional: Print to USB serial for debugging
  // Serial.print("Sent: ");
  // Serial.println(sensorValue);

  // Wait a bit before sending the next value
  delay(100);
}
```
This code is straightforward: it initializes `Serial1` (the hardware serial pins TX/RX on the Nano 33 IoT) and reads the value from `A0` in the loop, sending it out using `Serial1.println()`. I set the baud rate to 115200.

For the Daisy Seed (`src/daisy_receiver.cpp`):

```cpp
#include <DaisyDuino.h>

DaisyHardware hw;

void setup() {
  hw.Init(DAISY_SEED, AUDIO_SR_48K); // Basic Daisy Seed init

  // Initialize Serial (USB) for monitoring output
  Serial.begin(115200);

  // Initialize Serial1 (Hardware Serial on D13/D14 - RX/TX) for communication with Nano
  Serial1.begin(115200);
  Serial.println("Daisy Receiver Ready");
}

void loop() {
  // Check if data is available on Serial1
  if (Serial1.available() > 0) {
    // Read the incoming string until newline
    String receivedValue = Serial1.readStringUntil('\n');

    // Print the received value to the USB Serial Monitor
    Serial.print("Received: ");
    Serial.println(receivedValue);
  }
  
  // Small delay to prevent spamming checks, though Serial1.available is non-blocking
  delay(10); 
}
```
On the Daisy side, I needed to initialize the Daisy hardware (`hw.Init`). Then, I set up `Serial` (for USB output to my computer's serial monitor) and `Serial1`. On the Daisy Seed using DaisyDuino, `Serial1` corresponds to the hardware serial pins D13 (RX) and D14 (TX). I made sure to connect the Nano's TX pin to Daisy's D13 (RX) and the Nano's RX pin to Daisy's D14 (TX), and also connected their grounds. The code checks if data is available on `Serial1`, reads it as a string until the newline character (since the Nano sends using `println`), and then prints it to the main `Serial` output just to verify the connection.

To actually compile and upload the code to the specific boards, I needed to use PlatformIO's environment flag (`-e`). The commands would be:

```sh
# To compile and upload to the Nano 33 IoT
pio run -e nano33iot -t upload

# To compile and upload to the Daisy Seed
pio run -e electrosmith_daisy -t upload
```

This setup felt much cleaner. The build process worked for both environments without the strange errors I'd encountered before. Separating the concerns – sensor reading on the Nano, audio on the Daisy – simplified the code for each board and avoided the complexities of advanced memory management on the Daisy for now. The next step is to actually use the `receivedValue` on the Daisy to control synthesis parameters.
