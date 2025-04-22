<!-- filepath: /Users/justin/Library/CloudStorage/Dropbox/NYU/Semester 2 ('25 Spring)/Sinetactophone/daisyduino/Documentation/Posts/2025-04-22-refactoring-scales-and-cleanup.md -->
To get started with the synth project, I decided to base my initial code on the `ChordMachine` example provided within the DaisyDuino library (`examples/Pod/ChordMachine/`). I'd already spent some time understanding how that example worked, particularly its use of oscillators, controls, and the `AudioCallback` function.

I copied the `ChordMachine.ino` file into my project's `src/` directory (renaming it to something like `main.cpp` or `daisy_synth.cpp`). Looking at this copied code, I realized it had some features I didn't need and lacked others I wanted. Specifically, it used a system based on predefined chord types (`chord[10][3]` array, `InitChords()` function, `chordNum` variable) selected by the encoder. My goal was different; I wanted to select musical *scales* (modes) and then perhaps derive chords or notes from those scales.

This meant the first major task was refactoring the example code to remove the chord logic and replace it with a scale-based system.

I started by identifying all the parts related to the old chord system:
- The global `int chord[10][3]` array holding chord intervals.
- The global `int chordNum` variable tracking the selected chord.
- The `InitChords()` function that populated the `chord` array.
- The parts of `UpdateEncoder()` that incremented/decremented/reset `chordNum`.
- The note calculation logic within `UpdateKnobs()` that used `freq + chord[chordNum][...]`.
- The LED color logic in `UpdateLeds()` based on `chordNum`.

I systematically removed these elements:
1.  Deleted the `chord` array and `chordNum` variable declarations.
2.  Deleted the entire `InitChords()` function definition.
3.  Removed the call to `InitChords()` from `setup()`.
4.  Modified `UpdateEncoder()` to remove the `chordNum` manipulation. I'd need to replace this later with logic to select scales.
5.  Rewrote the note calculation part of `UpdateKnobs()` to remove references to `chord[chordNum]`. This would also need replacement logic based on scales.
6.  Cleared out the `UpdateLeds()` function, as its logic was now irrelevant.

This cleanup left the code in a state where it would compile but wouldn't make much sense musically yet. It removed the core chord generation but hadn't replaced it.

Next, I needed to implement the scale system. I started with a simple array holding the intervals for a couple of scales, similar to how the `chord` array worked but for 7-note scales:

```cpp
// Initial scale idea (later replaced)
int scales[][7] = {
    {0, 2, 4, 5, 7, 9, 11}, // Major (Ionian)
    {0, 2, 3, 5, 7, 8, 10}, // Natural Minor (Aeolian)
};
int scaleNum = 0; // To select the scale
```

This felt a bit basic. I realized I wanted:
- More scales: Specifically, all seven standard diatonic modes (Ionian, Dorian, Phrygian, Lydian, Mixolydian, Aeolian, Locrian).
- Clearer organization: Just having arrays of intervals wasn't very descriptive. It would be better to associate a name with each scale.
- Integrated feedback: I wanted the LEDs to reflect the selected scale, so the color should be part of the scale definition.

This led me to define a `struct` to hold all the information for a single scale:

```cpp
struct ScaleInfo {
    const char* name;
    int intervals[7];
    float color[3]; // R, G, B values (0.0 to 1.0)
};
```

Using this struct, I defined the seven modes, assigning a unique color to each. I tried to pick colors that were distinct:

```cpp
const int NUM_SCALES = 7;
ScaleInfo scales[NUM_SCALES] = {
    {"Ionian",     {0, 2, 4, 5, 7, 9, 11}, {1.0f, 1.0f, 1.0f}},    // White
    {"Dorian",     {0, 2, 3, 5, 7, 9, 10}, {0.0f, 0.0f, 1.0f}},    // Blue
    {"Phrygian",   {0, 1, 3, 5, 7, 8, 10}, {1.0f, 0.0f, 0.0f}},    // Red
    {"Lydian",     {0, 2, 4, 6, 7, 9, 11}, {1.0f, 1.0f, 0.0f}},    // Yellow
    {"Mixolydian", {0, 2, 4, 5, 7, 9, 10}, {1.0f, 0.5f, 0.0f}}, // Orange
    {"Aeolian",    {0, 2, 3, 5, 7, 8, 10}, {0.5f, 0.0f, 0.5f}},   // Purple
    {"Locrian",    {0, 1, 3, 5, 6, 8, 10}, {0.0f, 1.0f, 0.0f}}     // Green
};
int scaleNum = 0; // Index for the scales array
```

With the scale data structure defined, I went back to the functions I'd previously gutted:

In `UpdateEncoder()`, I added logic to increment/decrement `scaleNum` based on encoder turns, making sure it wrapped around using `NUM_SCALES`. I also kept the button press logic to reset `scaleNum` back to 0 (Ionian).

```cpp
// Inside UpdateEncoder
void UpdateEncoder() {
    // ... read encoder increment ...
    if (increment != 0) {
        scaleNum = (scaleNum + increment + NUM_SCALES) % NUM_SCALES;
    }
    if (hw.encoder.RisingEdge()) { // Check button press
        scaleNum = 0; // Reset to Ionian
    }
}
```

In `UpdateKnobs()`, I needed to implement the logic to calculate the `notes` array based on the selected scale. This involved reading the root note from one knob (like the original `freq`), and then calculating the four oscillator notes using intervals from `scales[scaleNum].intervals`. I also needed to consider how the second knob (originally `inversion`) would function – perhaps selecting different degrees from the scale or controlling octave shifts. The exact implementation here needed careful thought about how the four oscillators would map to the 7-note scale.

```cpp
// Inside UpdateKnobs (conceptual)
void UpdateKnobs() {
    int rootNote = 127 * analogRead(PIN_POD_POT_1) / 1023; // Read root note
    // Read second knob for degree selection/voicing/octave?
    int knob2Value = analogRead(PIN_POD_POT_2);

    for (int i = 0; i < 4; i++) { // For each oscillator
        // Determine which scale degree 'i' maps to (e.g., 0, 2, 4, 6?)
        int degreeIndex = (i * 2) % 7; // Example: Play 1st, 3rd, 5th, 7th degrees
        
        int interval = scales[scaleNum].intervals[degreeIndex];
        int octaveOffset = (degreeIndex / 7) * 12; // Handle octave if degree > 6 (not needed here)
        
        // Add logic based on knob2Value for inversions or octave shifts?
        // ... (e.g., similar to original inversion logic but based on scale degrees)

        notes[i] = rootNote + interval + octaveOffset; 
    }
}
```
*(Note: The exact mapping from the four oscillators to the seven scale degrees and the role of the second knob required further refinement beyond this basic structure.)*

Finally, I rewrote `UpdateLeds()` to use the color stored in the selected `ScaleInfo` struct:

```cpp
void UpdateLeds() {
    // Get color for the current scale
    float r = scales[scaleNum].color[0];
    float g = scales[scaleNum].color[1];
    float b = scales[scaleNum].color[2];

    // Set the hardware LEDs 
    hw.led.Set(r, g, b); 
}
```

This refactoring process transformed the original `ChordMachine` example into a foundation more suited for my project. It removed the unnecessary chord logic and established a clear, extensible system for handling musical scales (modes) with associated names and visual feedback. The next steps would involve refining the note selection logic in `UpdateKnobs` and ensuring the audio output matched the intended scale-based harmony.
