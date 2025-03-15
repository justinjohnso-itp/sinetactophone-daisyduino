# Daisy Seed + Arduino setup in VSCode with PlatformIO
## Introduction

This guide provides step-by-step instructions for setting up the Daisy Seed microcontroller with Arduino in Visual Studio Code using PlatformIO. Based on information from the Electro-Smith forum, this approach allows you to leverage the Arduino ecosystem while working with the powerful Daisy platform.

## Prerequisites

- Visual Studio Code
- PlatformIO extension for VSCode
- Daisy Seed microcontroller
- USB cable (USB-A to micro-USB)

## Setup Instructions

### 1. Install PlatformIO in VSCode

Install the PlatformIO extension from the VSCode marketplace. This provides an integrated development environment for embedded systems.

### 2. Create a New Project in PlatformIO

Create a new PlatformIO project with the following settings:
- Name: YourProjectName
- Board: Generic STM32F746ZG
- Framework: Arduino

### 3. Configure platformio.ini

Open the `platformio.ini` file and replace its contents with:

```ini
[env:electrosmith_daisy]
platform = ststm32
board = electrosmith_daisy
framework = arduino
; ----------
; Everything(?) below this needs to be added to use DaisyDuino in PlatformIO.
; Setting up a new project or importing from Arduino does not add any of this.
; ----------
lib_deps = 
    electro-smith/DaisyDuino@^1.5.2
    Wire 

build_flags = 
    ; -w                            ; optional - to suppress redundant definition warnings
    -D HAL_SDRAM_MODULE_ENABLED     ; required? build fails without this one
    ; These flags enable serial monitor over USB UART
    -D USBD_USE_CDC                 ; Define USB Communications Device Class (for serial I/O)
    -D USBCON                       ; Enable USB connection in Arduino (?)
; This is not documented on PlatformIO website but
; enables the DFU firmware upload (over USB)
upload_protocol = dfu
```

This configuration specifies the correct settings for the Daisy Seed hardware.

### 4. Install the DaisyDuino Library

Run the following command in your terminal:

```
pio pkg install --library ElectroSmith/DaisyDuino
```

Alternatively, use the PlatformIO Library Manager to search for and install "ElectroSmith/DaisyDuino".

### 5. Create a Basic Test Sketch

Create a new file `src/main.cpp` with the following code:

```cpp
#include <Arduino.h>

// the setup function runs once when you press reset or power the board
void setup() {
  // initialize digital pin LED_BUILTIN as an output.
  pinMode(LED_BUILTIN, OUTPUT);
}

// the loop function runs over and over again forever
void loop() {
  digitalWrite(LED_BUILTIN, HIGH);  // turn the LED on (HIGH is the voltage level)
  delay(1000);                      // wait for a second
  digitalWrite(LED_BUILTIN, LOW);   // turn the LED off by making the voltage LOW
  delay(1000);                      // wait for a second
}
```

### 6. Uploading to Daisy Seed

1. Connect your Daisy Seed to your computer via USB
2. Put the Daisy Seed in bootloader mode:
   - Press and hold the BOOT button
   - Press and release the RESET button
   - Release the BOOT button
3. Click the upload button in PlatformIO
Don’t worry about these “errors”
```
dfu-util: Error during download get_status
*** [upload] Error 74
```

### 7. Testing the Setup

After uploading, the Daisy Seed should reset and start running your program. You can verify the setup is working by:

1. Opening the Serial Monitor in PlatformIO (baud rate: 9600)
2. Looking for the "Serial initialized" and "Hello, Daisy Seed!" messages

### 8. Simple Oscillator Example

To further explore the capabilities of the Daisy Seed, you can create a simple oscillator. Create a new file `src/SimpleOscillator.ino` with the following code:

```cpp
#include "DaisyDuino.h"

DaisyHardware hw;
Oscillator osc;

void setup() {
  float sample_rate;

  // Initialize hardware
  hw = DAISY.init(DAISY_SEED, AUDIO_SR_48K);
  sample_rate = DAISY.get_samplerate();

  // Initialize oscillator
  osc.Init(sample_rate);
  osc.SetWaveform(Oscillator::WAVE_SIN);
  osc.SetFreq(440);

  // Start audio
  DAISY.start();
}

void loop() {
  // Nothing to do here, everything is handled in the audio callback
}

void audioCallback(float *in, float *out, size_t size) {
  for (size_t i = 0; i < size; i += 2) {
    float sig = osc.Process();
    out[i] = sig;     // left channel
    out[i + 1] = sig; // right channel
  }
}
```

### 9. Uploading the Simple Oscillator

1. Connect your Daisy Seed to your computer via USB
2. Put the Daisy Seed in bootloader mode:
   - Press and hold the BOOT button
   - Press and release the RESET button
   - Release the BOOT button
3. Click the upload button in PlatformIO

## Troubleshooting

- If upload fails, ensure your Daisy Seed is in bootloader mode
- Verify the USB connection is stable
- Check that the correct COM port is selected in PlatformIO

## References

- [Electro-Smith Forum Post](https://forum.electro-smith.com/t/daisy-seed-arduino-development-in-vscode/3639/2)
- [DaisyDuino GitHub Repository](https://github.com/electro-smith/DaisyDuino)
