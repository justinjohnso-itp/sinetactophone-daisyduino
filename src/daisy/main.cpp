#include <Arduino.h>
#include <Wire.h> // optional
#include "DaisyDuino.h"
#include <string.h> // optional
#include <math.h>   // For mtof and mapping

// ----- Serial Communication ----- //
uint32_t rx3 = 30, tx3 = 29;
HardwareSerial NanoSerial(rx3, tx3); 
#define SERIAL_BAUD_RATE 115200
#define VOLUME_KNOB_PIN A0   // <-- Add volume knob pin definition

// ----- Sensor Configuration ----- //
#define NUM_SENSORS 16
#define MAX_DETECTION_RANGE 200.0f // mm
int sensorValues[NUM_SENSORS]; // Store latest readings

// ----- Scale & Octave Settings ----- //
int currentOctave = 4;     // Base octave (C4)
int currentScale = 0;      // Default to Major (Ionian)

// Structure to hold scale information
struct Scale {
    const char* name;      // Name of the scale
    int notes[12];         // Notes in the scale (up to 12)
    int length;            // Number of notes in the scale
};

// ----- Scale Definitions ----- //
const int MAX_SCALE_LENGTH = 12;
// Indices: 0:Major, 1:Minor, 9:Blues (matching example)
const int NUM_SCALES = 11; // Keep all scales from example for potential future use

const Scale SCALES[NUM_SCALES] = {
    {"Ionian (Major)",    {0, 2, 4, 5, 7, 9, 11, 0, 0, 0, 0, 0}, 7}, // Index 0
    {"Aeolian (Minor)",   {0, 2, 3, 5, 7, 8, 10, 0, 0, 0, 0, 0}, 7}, // Index 1
    {"Dorian",            {0, 2, 3, 5, 7, 9, 10, 0, 0, 0, 0, 0}, 7},
    {"Phrygian",          {0, 1, 3, 5, 7, 8, 10, 0, 0, 0, 0, 0}, 7},
    {"Lydian",            {0, 2, 4, 6, 7, 9, 11, 0, 0, 0, 0, 0}, 7},
    {"Mixolydian",        {0, 2, 4, 5, 7, 9, 10, 0, 0, 0, 0, 0}, 7},
    {"Locrian",           {0, 1, 3, 5, 6, 8, 10, 0, 0, 0, 0, 0}, 7},
    {"Major Pentatonic",  {0, 2, 4, 7, 9, 0, 0, 0, 0, 0, 0, 0}, 5},
    {"Minor Pentatonic",  {0, 3, 5, 7, 10, 0, 0, 0, 0, 0, 0, 0}, 5},
    {"Blues",             {0, 3, 5, 6, 7, 10, 0, 0, 0, 0, 0, 0}, 6}, // Index 9
    {"Chromatic",         {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}, 12}
};

// ----- Synthesis Engine ----- //
DaisyHardware     hw;
size_t            num_channels;
Oscillator        osc[2]; // Two oscillators for the two halves
float             volumeLevel; // <-- Add global volume variable

// Helper function to map sensor distance to a 0.0-1.0 range (inverted)
float mapSensorDistance(int distance) {
    float clamped_dist = max(0.0f, min((float)distance, MAX_DETECTION_RANGE));
    // Invert: Closer (0mm) -> 1.0, Farther (200mm) -> 0.0
    return 1.0f - (clamped_dist / MAX_DETECTION_RANGE); 
}

// Helper function to calculate MIDI note based on sensor input (0.0-1.0)
int calculateMidiNote(float controlValue) { // <-- Renamed and changed return type
    int scaleLength = SCALES[currentScale].length;
    int totalSteps = scaleLength * 2; // 2 octaves

    // Quantize control value (0.0-1.0) to a discrete index
    int noteIndex = round(controlValue * totalSteps);
    if (noteIndex >= totalSteps) noteIndex = totalSteps - 1; // Clamp to max index

    int noteOffset;
    if (noteIndex < scaleLength) { // First octave
        noteOffset = SCALES[currentScale].notes[noteIndex];
    } else if (noteIndex < totalSteps) { // Second octave (handle top note carefully)
         // Access the note within the scale for the second octave
        int scaleNoteIndex = noteIndex - scaleLength; 
        // Ensure we don't go out of bounds for scales with fewer than 12 notes
        scaleNoteIndex = scaleNoteIndex % scaleLength; 
        noteOffset = 12 + SCALES[currentScale].notes[scaleNoteIndex];
    } else { // Should not happen due to clamping, but safety first
        noteOffset = 24; // Top note is 2 octaves above root
    }

    // Calculate the MIDI note number.
    int midiNote = (currentOctave * 12) + noteOffset;

    return midiNote; // <-- Return MIDI note
}


void AudioCallback(float **in, float **out, size_t size) {
  // Process both oscillators
  for (size_t i = 0; i < size; i++) {
    float sig1 = osc[0].Process();
    float sig2 = osc[1].Process();

    // Simple mix
    float mixed_sig = (sig1 + sig2) * 0.5f; 

    // Apply overall volume control
    mixed_sig *= volumeLevel; // <-- Apply volumeLevel here

    for (size_t chn = 0; chn < num_channels; chn++) {
      out[chn][i] = mixed_sig;
    }
  }
}

void setup(){
  // Initialize sensor values (e.g., to max range initially)
  for(int i=0; i<NUM_SENSORS; ++i) {
      sensorValues[i] = (int)MAX_DETECTION_RANGE; 
  }

  Serial.begin(SERIAL_BAUD_RATE);
  Serial.println("--- DaisyDuino Sinetactophone --- ");
  Serial.println("USB Monitor Initialized.");

  NanoSerial.begin(SERIAL_BAUD_RATE);
  Serial.println("Hardware NanoSerial (Nano RX/TX) Initialized.");
  Serial.print("Listening for Nano on NanoSerial (Pins D13/D14) at ");
  Serial.print(SERIAL_BAUD_RATE);
  Serial.println(" baud...");

  // Configure volume knob pin
  pinMode(VOLUME_KNOB_PIN, INPUT_ANALOG); // <-- Configure pin

  // Audio init
  hw = DAISY.init(DAISY_SEED, AUDIO_SR_48K);
  num_channels = hw.num_channels;
  float samplerate = DAISY.get_samplerate();

  // Initialize both oscillators
  for (int i = 0; i < 2; i++) {
      osc[i].Init(samplerate);
      osc[i].SetAmp(0.0f); // Start silent
      osc[i].SetFreq(mtof(currentOctave * 12)); // Start at base octave freq
      osc[i].SetWaveform(Oscillator::WAVE_TRI); // <-- Use Triangle waves
  }

  // Read initial volume
  volumeLevel = analogRead(VOLUME_KNOB_PIN) / 1023.0f; // <-- Read initial volume

  DAISY.begin(AudioCallback); // Use renamed callback
}

void loop(){
  // Check if data is available from the Nano on NanoSerial
  if(NanoSerial.available() > 0){
    String incomingData = NanoSerial.readStringUntil('\n');
    incomingData.trim();

    // Optional: Echo received data back to USB Serial Monitor
    // Serial.print("Nano->Daisy: ");
    // Serial.println(incomingData);

    // Parse lines like: >S6:[1566,1522,...]
    if(incomingData.startsWith(">S")){
      int colonIdx = incomingData.indexOf(':');
      // Extract sensor ID (assuming single digit for now)
      int sensorId = incomingData.substring(2, colonIdx).toInt(); 

      int lb = incomingData.indexOf('[');
      int rb = incomingData.indexOf(']');
      if (lb != -1 && rb != -1 && sensorId >= 0 && sensorId < NUM_SENSORS) {
          String list = incomingData.substring(lb + 1, rb);
          
          // For simplicity, let's assume the first value is the relevant distance
          int comma = list.indexOf(',');
          String firstValStr;
          if (comma == -1) {
              firstValStr = list;
          } else {
              firstValStr = list.substring(0, comma);
          }
          sensorValues[sensorId] = firstValStr.toInt();

          // Optional: Print parsed value
          // Serial.print("Parsed Sensor "); Serial.print(sensorId);
          // Serial.print(" value: "); Serial.println(sensorValues[sensorId]);
      }
    }
  }

  // --- Update Synthesis based on Sensor Readings ---

  // Update overall volume level from knob
  volumeLevel = analogRead(VOLUME_KNOB_PIN) / 1023.0f; // <-- Update volume each loop

  // 1. Determine Scale (Sensors 5 & 0 - Middle sensors of each half)
  // Consider a value > 0 and < MAX_DETECTION_RANGE as detection
  bool detect5 = sensorValues[5] > 0 && sensorValues[5] < MAX_DETECTION_RANGE;
  bool detect0 = sensorValues[7] > 0 && sensorValues[7] < MAX_DETECTION_RANGE; // <-- Corrected sensor index for Half B middle

  int previousScale = currentScale; // Store previous scale to check for changes

  if (detect5 && detect0) {
      currentScale = 1; // Minor scale index (Both middle sensors active)
  } else if (detect5 || detect0) {
      currentScale = 9; // Blues scale index (One middle sensor active)
  } else {
      currentScale = 0; // Major scale index (Neither middle sensor active)
  }

  // Optional: Print scale change
  if (currentScale != previousScale) {
      Serial.print("Scale changed to: ");
      Serial.println(SCALES[currentScale].name);
  }


  // 2. Update Oscillator 0 (Half A: Amp=Sensor 4, Pitch=Sensor 6) - Shifted -0.5 Octave
  float ampControl0 = mapSensorDistance(sensorValues[4]); // Left sensor for Amp
  float pitchControl0 = mapSensorDistance(sensorValues[6]); // Right sensor for Pitch
  int midiNote0 = calculateMidiNote(pitchControl0);
  midiNote0 -= 6; // Shift down half an octave (6 semitones)
  float freq0 = mtof(midiNote0); // Convert final MIDI note to frequency
  osc[0].SetAmp(ampControl0);
  osc[0].SetFreq(freq0);

  // 3. Update Oscillator 1 (Half B: Amp=Sensor 3, Pitch=Sensor 0) - Shifted -0.5 Octave
  float ampControl1 = mapSensorDistance(sensorValues[3]); // Left sensor for Amp
  float pitchControl1 = mapSensorDistance(sensorValues[0]); // Right sensor for Pitch
  int midiNote1 = calculateMidiNote(pitchControl1);
  midiNote1 -= 6; // Shift down half an octave (6 semitones)
  float freq1 = mtof(midiNote1); // Convert MIDI note to frequency
  osc[1].SetAmp(ampControl1);
  osc[1].SetFreq(freq1);

  // Optional: Add a small delay to prevent overwhelming the Serial output if debugging
  // delay(10); 
}

// Function to convert MIDI note number to frequency
// Already included in DaisyDuino.h, but good to have as reference if needed elsewhere
// float mtof(float m) {
//     return 8.175799f * powf(2.0f, m / 12.0f);
// }