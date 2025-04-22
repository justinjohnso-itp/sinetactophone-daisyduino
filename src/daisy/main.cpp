#include <Arduino.h>
#include <Wire.h> // optional
#include "DaisyDuino.h"
#include <string.h> // optional

uint32_t rx3 = 30, tx3 = 29;
HardwareSerial NanoSerial(rx3, tx3); 
#define SERIAL_BAUD_RATE 115200
#define VOLUME_KNOB_PIN A0   // ← define volume knob pin

DaisyHardware     hw;
size_t            num_channels;
static Oscillator osc;
float             volumeLevel; // ← new global for volume

void MyCallback(float **in, float **out, size_t size) {
  // update osc parameters each callback
  osc.SetFreq(300.0f);
  osc.SetAmp(volumeLevel);  // ← use volumeLevel here
  for (size_t i = 0; i < size; i++) {
    float sig = osc.Process();
    for (size_t chn = 0; chn < num_channels; chn++) {
      out[chn][i] = sig;
    }
  }
}

void setup(){
  Serial.begin(SERIAL_BAUD_RATE);
  Serial.println("--- Daisy Serial Relay ---");
  Serial.println("USB Monitor Initialized.");

  NanoSerial.begin(SERIAL_BAUD_RATE);
  Serial.println("Hardware NanoSerial (Nano RX/TX) Initialized.");
  Serial.print("Listening for Nano on NanoSerial (Pins D13/D14) at ");
  Serial.print(SERIAL_BAUD_RATE);
  Serial.println(" baud...");

  // audio init
  hw = DAISY.init(DAISY_SEED, AUDIO_SR_48K);
  num_channels = hw.num_channels;
  osc.Init(DAISY.get_samplerate());
  osc.SetWaveform(Oscillator::WAVE_TRI);

  // configure knobs
  pinMode(VOLUME_KNOB_PIN, INPUT_ANALOG);

  // read initial knob values
  volumeLevel = analogRead(VOLUME_KNOB_PIN) / 1023.0f;  // ← init volume

  DAISY.begin(MyCallback);
}

void loop(){
  // Check if data is available from the Nano on NanoSerial
  if(NanoSerial.available() > 0){
    String incomingData = NanoSerial.readStringUntil('\n');
    incomingData.trim();

    Serial.print("Nano->Daisy: ");
    Serial.println(incomingData);

    // parse lines like:
    //   >S6:{"zones":[1566,1522,1492,1462,393,1471,1460,1435,357,364,1428,1401,344,335,1399,1371]}
    if(incomingData.startsWith(">S")){
      int colonIdx = incomingData.indexOf(':');
      int sensorId = incomingData.substring(2, colonIdx).toInt();

      int lb = incomingData.indexOf('[');
      int rb = incomingData.indexOf(']');
      String list = incomingData.substring(lb + 1, rb);

      const int MAX_ZONES = 16;         // match Daisy side capability
      int zones[MAX_ZONES];
      int zoneCount = 0;

      while(list.length() && zoneCount < MAX_ZONES){
        int comma = list.indexOf(',');
        String token;
        if(comma == -1){
          token = list;
          list = "";
        } else {
          token = list.substring(0, comma);
          list = list.substring(comma + 1);
        }
        zones[zoneCount++] = token.toInt();
      }

      // Now you have:
      //  sensorId  (0–7)
      //  zones[0..zoneCount-1]
      //  zoneCount

      // Example: print first and last
      Serial.print("Parsed Sensor "); Serial.print(sensorId);
      Serial.print(" zones: count="); Serial.print(zoneCount);
      Serial.print(" first="); Serial.print(zones[0]);
      Serial.print(" last=");  Serial.println(zones[zoneCount-1]);

      // TODO: store zones[] into your application state
    }
  }

  // update knobs every loop
  volumeLevel = analogRead(VOLUME_KNOB_PIN) / 1023.0f;
}