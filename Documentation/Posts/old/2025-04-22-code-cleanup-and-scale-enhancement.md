<!-- filepath: /Users/justin/Library/CloudStorage/Dropbox/NYU/Semester 2 ('25 Spring)/Sinetactophone/daisyduino/Documentation/Posts/2025-04-22-code-cleanup-and-scale-enhancement.md -->
# 2025-04-22: Code Cleanup and Scale Enhancement

The `main.cpp` code felt a bit messy, with both "chords" and "scales" structures floating around from earlier experiments. I wasn't sure which one was actually driving the note quantization anymore. I asked Copilot to figure out which one was actively being used and remove the other, along with any other duplicate or deprecated code it could find.

Copilot analyzed the code and confirmed my suspicion: the "scales" system was the one being used in `UpdateKnobs`, `UpdateEncoder`, and `UpdateLeds`, while the "chords" structure seemed like leftovers.

It proceeded with the cleanup:

1.  Removed the `Chord` struct definition.
2.  Removed the global `chords` array.
3.  Removed the `currentChord` variable.
4.  Removed the forward declaration for `getClosestChordNote()`.

```cpp
// ----- Scale & Octave Settings ----- //
int currentOctave = 4;     // Base octave; e.g., if currentOctave=3 then the base note is 3*12 = 36 (i.e. C2)
int currentScale = 0;      // Scale selection index (0 to NUM_SCALES-1)

// ----- Existing Declarations ----- //
// ... (SCALES, SCALE_LENGTHS arrays remain)
```

5.  Fixed some comments that contradicted the code's behavior, like one saying the encoder press didn't toggle the waveform (it did) and another saying LED color was based on chords (it was based on scales).

```cpp
// Default frequency for both oscillators
float oscillatorFreq[2] = {440.0f, 440.0f};

bool usingSineWave = true;  // toggles between sine and triangle wave // <-- Fixed comment

// ...

//
// UpdateLeds: sets each LED only when its associated oscillator is on.
// The LED color is determined by the current scale. // <-- Fixed comment
//
void UpdateLeds()
{
    // ... (rest of function)
}
```

6.  Updated the `DebugPrint` function to show the `currentScale` instead of the non-existent `currentChord`.

```cpp
void DebugPrint()
{
    // ... (other prints)
    Serial.print("Current Octave: "); Serial.println(currentOctave);
    Serial.print("Current Scale: "); Serial.println(currentScale); // <-- Changed from chord name
    // ... (rest of function)
}
```

Copilot also pointed out that the implementation for `getClosestChordNote()` was still present at the end of the file, referencing the now-deleted `chords` array, making it unusable and redundant. It also noted the `getScaleNote()` function was marked as "retained for legacy" but wasn't called anywhere. I confirmed that both could be removed.

Copilot then removed the implementations for both `getClosestChordNote()` and `getScaleNote()`, as well as the forward declaration for `getScaleNote()`.

```cpp
// Forward declarations
void UpdateControls();
void UpdateEncoder();
void UpdateButtons();
void UpdateKnobs();
void UpdateLeds();
// float getScaleNote(...); // <-- Removed
void DebugPrint();

// ... (rest of code)

// void UpdateControls() { ... } // <-- Last function in the file now

/* Removed functions:
float getClosestChordNote(...) { ... }
float getScaleNote(...) { ... }
*/
```

With the code cleaned up and focused solely on the scale system, I wanted to enhance it. The current setup just had numbered scales (0-5). I asked Copilot to adjust the scale definition to include the name of each scale (like "Major", "Minor Pentatonic") and to add the standard musical modes (Dorian, Phrygian, Lydian, Mixolydian, Locrian) that weren't already included.
