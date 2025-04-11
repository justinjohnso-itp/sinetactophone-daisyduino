#include <Arduino.h>
#include "DaisyDuino.h"
#include <math.h>

// Forward declarations
void UpdateControls();
void UpdateEncoder();
void UpdateButtons();
void UpdateKnobs();
void UpdateLeds();
void DebugPrint();

// ----- Scale & Octave Settings ----- //
int currentOctave = 4;     // Base octave; e.g., if currentOctave=3 then the base note is 3*12 = 36 (i.e. C2)
int currentScale = 0;      // Scale selection index (0 to NUM_SCALES-1)

// Structure to hold scale information
struct Scale {
    const char* name;      // Name of the scale
    int notes[12];         // Notes in the scale (up to 12)
    int length;            // Number of notes in the scale
    uint8_t colorR;        // Red component (0-255)
    uint8_t colorG;        // Green component (0-255)
    uint8_t colorB;        // Blue component (0-255)
};

// ----- Scale Definitions ----- //
const int MAX_SCALE_LENGTH = 12;
const int NUM_SCALES = 11;  // Updated to include all modes

// Define all scales with their names, note patterns, and distinct LED colors
const Scale SCALES[NUM_SCALES] = {
    {"Ionian (Major)",    {0, 2, 4, 5, 7, 9, 11, 0, 0, 0, 0, 0}, 7,  255,   0,   0}, // Red
    {"Aeolian (Minor)",   {0, 2, 3, 5, 7, 8, 10, 0, 0, 0, 0, 0}, 7,    0, 255,   0}, // Green
    {"Dorian",            {0, 2, 3, 5, 7, 9, 10, 0, 0, 0, 0, 0}, 7,  255, 255,   0}, // Yellow
    {"Phrygian",          {0, 1, 3, 5, 7, 8, 10, 0, 0, 0, 0, 0}, 7,    0,   0, 255}, // Blue
    {"Lydian",            {0, 2, 4, 6, 7, 9, 11, 0, 0, 0, 0, 0}, 7,  255,   0, 255}, // Magenta
    {"Mixolydian",        {0, 2, 4, 5, 7, 9, 10, 0, 0, 0, 0, 0}, 7,    0, 255, 255}, // Cyan
    {"Locrian",           {0, 1, 3, 5, 6, 8, 10, 0, 0, 0, 0, 0}, 7,  255, 128,   0}, // Orange
    {"Major Pentatonic",  {0, 2, 4, 7, 9, 0, 0, 0, 0, 0, 0, 0}, 5,  128,   0, 255}, // Purple
    {"Minor Pentatonic",  {0, 3, 5, 7, 10, 0, 0, 0, 0, 0, 0, 0}, 5,    0, 128,  64}, // Teal
    {"Blues",             {0, 3, 5, 6, 7, 10, 0, 0, 0, 0, 0, 0}, 6,  255, 255, 128}, // Light Yellow
    {"Chromatic",         {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}, 12, 255, 255, 255}  // White
};

DaisyHardware hw;
Oscillator osc[2];

// Control mapping:
// Button 1 and Pot 1 → Oscillator 1 (index 0)
// Button 2 and Pot 2 → Oscillator 2 (index 1)
bool oscillatorActive[2] = {false, false};
// Default frequency for both oscillators
float oscillatorFreq[2] = {440.0f, 440.0f};

bool usingSineWave = true;  // toggles between sine and triangle wave

// Track previous pot positions for movement detection
float prevKnob[2] = {0.5f, 0.5f};

static void AudioCallback(float **in, float **out, size_t size)
{
    UpdateControls();
    for (size_t i = 0; i < size; i++)
    {
        float sig1 = oscillatorActive[0] ? osc[0].Process() : 0.0f;
        float sig2 = oscillatorActive[1] ? osc[1].Process() : 0.0f;
        
        // Simple pan:
        // Oscillator 1 is panned to the left: more signal in left channel.
        // Oscillator 2 is panned to the right: more signal in right channel.
        float left  = sig1 * 0.6f + sig2 * 0.4f;
        float right = sig1 * 0.4f + sig2 * 0.6f;
        
        out[0][i] = left;
        out[1][i] = right;
    }
}

void InitSynth(float samplerate)
{
    for (int i = 0; i < 2; i++)
    {
        osc[i].Init(samplerate);
        osc[i].SetAmp(0.5f);
        osc[i].SetWaveform(Oscillator::WAVE_SIN);
    }
}

void setup()
{
    // Initialize Serial (non-blocking)
    Serial.begin(115200);
    float samplerate;
    hw = DAISY.init(DAISY_POD, AUDIO_SR_48K);
    samplerate = DAISY.get_samplerate();
    InitSynth(samplerate);
    DAISY.begin(AudioCallback);
}

void loop()
{
    DebugPrint();
    delay(500);  // Adjust refresh rate as desired
}

//
// DebugPrint - prints sensor values, current octave/chord, and LED output states.
//
void DebugPrint()
{
    Serial.println("===== Debug Info =====");
    
    float knob0 = analogRead(PIN_POD_POT_1) / 1023.0f;
    float knob1 = analogRead(PIN_POD_POT_2) / 1023.0f;
    Serial.print("Knob0 (Pot1): "); Serial.println(knob0, 3);
    Serial.print("Knob1 (Pot2): "); Serial.println(knob1, 3);
    
    Serial.print("Button0: ");
    Serial.println(hw.buttons[0].Pressed() ? "Pressed" : "Released");
    Serial.print("Button1: ");
    Serial.println(hw.buttons[1].Pressed() ? "Pressed" : "Released");
    
    Serial.print("Current Octave: "); Serial.println(currentOctave);
    Serial.print("Current Scale: "); Serial.println(currentScale);
    
    Serial.print("Oscillator 1: ");
    Serial.print(oscillatorActive[0] ? "ON" : "OFF");
    Serial.print(" Freq: "); Serial.println(oscillatorFreq[0], 2);
    
    Serial.print("Oscillator 2: ");
    Serial.print(oscillatorActive[1] ? "ON" : "OFF");
    Serial.print(" Freq: "); Serial.println(oscillatorFreq[1], 2);
    
    // Use scale index to set LED color (for example purposes)
    uint32_t colorR = 0, colorG = 0, colorB = 0;
    switch(currentScale)
    {
        case 0: colorR = 255; break;      // Scale 0 → Red
        case 1: colorG = 255; break;      // Scale 1 → Green
        case 2: colorR = 255, colorG = 255; break; // Scale 2 → Yellow
        case 3: colorB = 255; break;      // Scale 3 → Blue
        case 4: colorR = 255, colorB = 255; break; // Scale 4 → Magenta
        case 5: colorG = 255, colorB = 255; break; // Scale 5 → Cyan
    }
    
    Serial.print("LED0 (Oscillator 1): ");
    Serial.print(oscillatorActive[0] ? "ON" : "OFF");
    Serial.print(" Color: (");
    Serial.print((float)colorR / 255.0f, 2); Serial.print(", ");
    Serial.print((float)colorG / 255.0f, 2); Serial.print(", ");
    Serial.print((float)colorB / 255.0f, 2); Serial.println(")");
    
    Serial.print("LED1 (Oscillator 2): ");
    Serial.print(oscillatorActive[1] ? "ON" : "OFF");
    Serial.print(" Color: (");
    Serial.print((float)colorR / 255.0f, 2); Serial.print(", ");
    Serial.print((float)colorG / 255.0f, 2); Serial.print(", ");
    Serial.print((float)colorB / 255.0f, 2); Serial.println(")");
    
    Serial.println("======================");
}

//
// UpdateEncoder - turning cycles scales, and pressing toggles waveform (sine/triangle).
//
void UpdateEncoder()
{
    int delta = hw.encoder.Increment();
    if (delta != 0)
    {
        // Turning now cycles scales.
        currentScale += delta;
        currentScale = (currentScale % NUM_SCALES + NUM_SCALES) % NUM_SCALES;
    }
    if (hw.encoder.RisingEdge())
    {
        // Pressing now toggles waveform.
        usingSineWave = !usingSineWave;
        for (int i = 0; i < 2; i++)
            osc[i].SetWaveform(usingSineWave ? Oscillator::WAVE_SIN : Oscillator::WAVE_TRI);
    }
}

//
// UpdateButtons: toggles each oscillator (independently).
//
void UpdateButtons()
{
    if (hw.buttons[0].RisingEdge())
        oscillatorActive[0] = !oscillatorActive[0];

    if (hw.buttons[1].RisingEdge())
        oscillatorActive[1] = !oscillatorActive[1];
}

//
// UpdateKnobs - quantizes the pot values to discrete notes over two octaves.
// The top (maximum) step now correctly produces the root note one octave above the base note.
void UpdateKnobs()
{
    // Read the potentiometers (normalized)
    float knob0 = analogRead(PIN_POD_POT_1) / 1023.0f;
    float knob1 = analogRead(PIN_POD_POT_2) / 1023.0f;
    float delta0 = fabsf(knob0 - prevKnob[0]);
    float delta1 = fabsf(knob1 - prevKnob[1]);

    // Using two octaves: each octave has a number of notes defined by the scale.
    int scaleLength = SCALES[currentScale].length;
    int totalSteps = scaleLength * 2;
    // Quantize knob values to a discrete index in 0...totalSteps-1.
    int noteIndex0 = round(knob0 * totalSteps);
    if (noteIndex0 >= totalSteps) noteIndex0 = totalSteps - 1;
    int noteIndex1 = round(knob1 * totalSteps);
    if (noteIndex1 >= totalSteps) noteIndex1 = totalSteps - 1;

    int noteOffset0;
    if (noteIndex0 < scaleLength)
        noteOffset0 = SCALES[currentScale].notes[noteIndex0];
    else if (noteIndex0 < totalSteps - 1)
        noteOffset0 = 12 + SCALES[currentScale].notes[noteIndex0 - scaleLength];
    else
        // The top step should be exactly one octave above the second octave's base,
        // i.e. shift by 24 semitones instead of 12.
        noteOffset0 = 24;

    int noteOffset1;
    if (noteIndex1 < scaleLength)
        noteOffset1 = SCALES[currentScale].notes[noteIndex1];
    else if (noteIndex1 < totalSteps - 1)
        noteOffset1 = 12 + SCALES[currentScale].notes[noteIndex1 - scaleLength];
    else
        noteOffset1 = 24;

    // Calculate the MIDI note number.
    int midiNote0 = (currentOctave * 12) + noteOffset0;
    int midiNote1 = (currentOctave * 12) + noteOffset1;

    // Convert MIDI note to frequency.
    float quantFreq0 = mtof(midiNote0);
    float quantFreq1 = mtof(midiNote1);

    // Update oscillator frequencies independently.
    if (oscillatorActive[0] && delta0 > 0.001f)
    {
        oscillatorFreq[0] = quantFreq0;
        osc[0].SetFreq(oscillatorFreq[0]);
    }
    if (oscillatorActive[1] && delta1 > 0.001f)
    {
        oscillatorFreq[1] = quantFreq1;
        osc[1].SetFreq(oscillatorFreq[1]);
    }
    // Even if an oscillator is off, keep its underlying frequency updated.
    if (!oscillatorActive[0] && delta0 > 0.001f)
        oscillatorFreq[0] = quantFreq0;
    if (!oscillatorActive[1] && delta1 > 0.001f)
        oscillatorFreq[1] = quantFreq1;

    // Save current knob readings for change detection.
    prevKnob[0] = knob0;
    prevKnob[1] = knob1;
}

//
// UpdateLeds: sets each LED only when its associated oscillator is on.
// The LED color is determined by the current scale.
//
void UpdateLeds()
{
    // Get colors directly from the current scale
    uint8_t colorR = SCALES[currentScale].colorR;
    uint8_t colorG = SCALES[currentScale].colorG;
    uint8_t colorB = SCALES[currentScale].colorB;
    
    if (oscillatorActive[0])
        hw.leds[0].Set((float)colorR/255.0f, (float)colorG/255.0f, (float)colorB/255.0f);
    else
        hw.leds[0].Set(0, 0, 0);

    if (oscillatorActive[1])
        hw.leds[1].Set((float)colorR/255.0f, (float)colorG/255.0f, (float)colorB/255.0f);
    else
        hw.leds[1].Set(0, 0, 0);
}

//
// UpdateControls: debounces and then updates encoder, buttons, knobs, and LEDs.
//
void UpdateControls()
{
    hw.DebounceControls();
    UpdateEncoder();
    UpdateButtons();
    UpdateKnobs();
    UpdateLeds();
}