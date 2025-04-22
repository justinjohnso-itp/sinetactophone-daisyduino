I ran into a potential issue with memory usage on the Daisy Seed. The DaisyDuino library is quite large, and I wanted to free up the internal flash memory for my own application code, which might need faster access or simply more space as it grows. The Daisy Seed has external QSPI flash memory (specifically an IS25LP064A chip), which seemed like the perfect place to store the bulk of the library code.

The goal was clear: configure the PlatformIO build process so that the DaisyDuino library code and its read-only data reside in the external QSPI flash (starting at address `0x90000000`), while my `main.cpp` and any other code I write stays in the default internal flash memory.

I started digging into how QSPI works on the Daisy and how to manage memory placement with the ARM GCC linker used by PlatformIO. I looked through the [DaisyDuino QSPI utility header](https://raw.githubusercontent.com/electro-smith/DaisyDuino/refs/heads/master/src/utility/qspi.h), the [libDaisy QSPIHandle documentation](https://electro-smith.github.io/libDaisy/classdaisy_1_1_q_s_p_i_handle.html), and several forum posts ([like this one](https://forum.electro-smith.com/t/seed-qspi-example/1414) and [this one about PlatformIO issues](https://forum.electro-smith.com/t/unable-to-run-sram-qspi-software-on-bootloader-from-platformio/3775)) to understand the required steps.

It seemed like a combination of PlatformIO build flags, linker script arguments, and potentially some initialization code was needed.

First, I updated my `platformio.ini` file. I needed to tell the build system that I intended to use QSPI and that it should be memory-mapped.

```ini
# platformio.ini

[env:electrosmith_daisy]
platform = ststm32
board = electrosmith_daisy
board_build.partitions = qspi # Use QSPI partition layout
framework = arduino
lib_deps = 
	Wire
	https://github.com/electro-smith/DaisyDuino.git
build_flags = 
	-D HAL_SDRAM_MODULE_ENABLED
	-D USBD_USE_CDC
	-D USBCON
	-D USE_QSPI             # Enable QSPI code in DaisyDuino
	-D USE_MEMORY_MAPPED    # Use memory-mapped mode for QSPI access
	-D RESET_BEHAVIOR_BOOTLOADER
	-mthumb -mcpu=cortex-m7 -mfloat-abi=hard -mfpu=fpv5-d16
	# Linker flags to define QSPI memory sections
	-Wl,--section-start=.qspiflash_text=0x90000000
	-Wl,--section-start=.qspiflash_data=0x90800000 # Adjust address if needed
upload_protocol = dfu

# Add this custom script to control library placement in QSPI
extra_scripts = pre:qspi_lib_script.py
```

The key additions here are:
- `board_build.partitions = qspi`: This tells PlatformIO to use a linker script configuration suitable for QSPI, likely defining the memory regions.
- `-D USE_QSPI` and `-D USE_MEMORY_MAPPED`: These preprocessor defines enable the relevant QSPI code within the DaisyDuino library itself.
- The `-Wl,--section-start...` flags: These are passed directly to the linker (`ld`). They explicitly define the starting addresses for custom memory sections named `.qspiflash_text` (for code) and `.qspiflash_data` (for read-only data) within the QSPI memory space (`0x90000000` onwards).

However, just defining these sections isn't enough. I needed a way to tell the linker to put the *DaisyDuino library's code and data* into these specific sections, while leaving my code in the default sections (like `.text`, `.data` in internal flash/RAM).

PlatformIO doesn't have a straightforward way in the `.ini` file to apply section attributes only to specific libraries. The solution suggested in some discussions involved using an `extra_scripts` entry in `platformio.ini`. This points to a Python script that runs before the main build process and can manipulate the build environment.

So, I created a file named `qspi_lib_script.py` in the project root:

```python
# qspi_lib_script.py
import os

Import("env")

# This function modifies build flags for the DaisyDuino library
def process_lib_flags(lib_deps):
    print("Processing library build flags for QSPI placement...")
    for lib in lib_deps:
        lib_name = str(lib)
        # Check if the library name contains 'DaisyDuino'
        if "DaisyDuino" in lib_name:
            print(f"Found DaisyDuino library: {lib_name}")
            lib_path = lib.path
            
            # Add flags to potentially mark or configure the library build
            # This might involve adding attributes or specific defines
            # Option 1: Add a define that library code can use conditionally
            env.Append(CPPDEFINES=[
                ("DAISY_LIB_IN_QSPI", 1)
            ])
            
            # Option 2: Attempt to add section attributes via compiler flags
            # (This is less reliable across compilers/versions)
            # env.Append(CCFLAGS=[
            #     f"-ffunction-sections", 
            #     f"-fdata-sections",
            #     f"-Wl,--gc-sections"
            # ])
            # env.Append(LINKFLAGS=[
            #     # This part is tricky - how to apply ONLY to this lib?
            #     # Might need more advanced linker script manipulation
            # ])

            print(f"  Added DAISY_LIB_IN_QSPI define for {lib_name}")
            # Ideally, we'd directly add __attribute__((section(".qspiflash_text"))) 
            # or similar, but doing that via build flags for a specific library 
            # is complex. The linker script approach combined with careful 
            # library source modification might be needed for full control.

# Get all library builders
libras = env.GetLibBuilders()
process_lib_flags(libras)

print("Finished processing library build flags.")
```

This script hooks into the PlatformIO build environment. It iterates through the libraries being built. When it finds the DaisyDuino library, it adds a preprocessor define `DAISY_LIB_IN_QSPI`. The idea was that maybe the library source code itself could use this define with `#ifdef` guards and `__attribute__((section(...)))` directives to place its functions and data into the `.qspiflash_text` and `.qspiflash_data` sections I defined in `platformio.ini`. *Self-correction: Simply adding a define doesn't automatically move the code. The library source itself would need modification, or a more sophisticated linker script approach is required to selectively place object files from that library into the QSPI sections.* This script is more of a hook or a marker; the actual placement relies heavily on how the linker script (`*.ld` file, potentially modified by `board_build.partitions`) and the library's source attributes interact.

Finally, the QSPI flash needs to be initialized in memory-mapped mode early in the program's execution, before any code stored there is potentially accessed. I added this to the beginning of my `setup()` function in `main.cpp`:

```cpp
#include <DaisyDuino.h>
#include "utility/qspi.h" // Include the QSPI header

// ... other includes and global variables ...

DaisyHardware hw;
QSPIHandle qspi;

void setup() {
  // Initialize QSPI first!
  QSPIHandle::Config qspi_config;
  qspi_config.device = QSPIHandle::Config::Device::IS25LP064A; // Correct device for Daisy
  qspi_config.mode   = QSPIHandle::Config::Mode::MEMORY_MAPPED;
  qspi_config.pin_config.io0 = {DSY_GPIOF, 8};
  qspi_config.pin_config.io1 = {DSY_GPIOF, 9};
  qspi_config.pin_config.io2 = {DSY_GPIOF, 7};
  qspi_config.pin_config.io3 = {DSY_GPIOF, 6};
  qspi_config.pin_config.clk = {DSY_GPIOF, 10};
  qspi_config.pin_config.ncs = {DSY_GPIOG, 6};
  
  qspi.Init(qspi_config);
  // Now QSPI is memory-mapped at 0x90000000

  // Proceed with other initializations
  hw = DAISY.init(DAISY_POD, AUDIO_SR_48K);
  samplerate = DAISY.get_samplerate();

  InitSynth(samplerate);
  // InitChords(); // Removed in previous refactor

  DAISY.begin(AudioCallback);
}

// ... rest of the code (AudioCallback, UpdateControls, loop, etc.) ...
```

This code explicitly configures the QSPI peripheral with the correct pins for the Daisy platform and sets it to memory-mapped mode using the `qspi.Init(qspi_config)` call. Placing this right at the start of `setup()` is crucial.

With the `platformio.ini` flags defining the sections, the Python script attempting to mark the library (though its effectiveness might depend on library internals or further linker script work), and the explicit QSPI initialization in `main.cpp`, the setup should now be directing the linker to place the DaisyDuino library into the external flash, hopefully freeing up significant internal memory.
