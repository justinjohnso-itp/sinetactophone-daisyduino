#include <DaisyDuino.h>
#include <string.h>

DaisyHardware hardware;
size_t numChannels;
float sampleRate;

// Create audio objects
static void AudioCallback(float **in, float **out, size_t size) {
    for (size_t i = 0; i < size; i++) {
        // Process audio here
        out[0][i] = in[0][i];
        out[1][i] = in[1][i];
    }
}

const int MAX_SENSORS = 8;
const int SENSOR_RESOLUTION = 16;
int sensorData[MAX_SENSORS][SENSOR_RESOLUTION];

void setup() {
    // Initialize Daisy hardware
    sampleRate = DAISY.get_samplerate();
    numChannels = 2; // Stereo

    hardware = DAISY.init(DAISY_SEED, AUDIO_SR_48K);
    DAISY.begin(AudioCallback);

    // Initialize Serial for sensor data
    Serial.begin(115200);
    while (!Serial) delay(10);
    Serial.println("Daisy Receiver Ready");
    
    // Initialize sensor data array
    for (int i = 0; i < MAX_SENSORS; i++) {
        for (int j = 0; j < SENSOR_RESOLUTION; j++) {
            sensorData[i][j] = 0;
        }
    }
}

void parseData(String data) {
  // Expected format: "S{sensor_index}:{value1},{value2},...E"
  if (data.length() < 4) return; // Minimum valid length
  
  if (data.charAt(0) != 'S' || data.charAt(data.length()-1) != 'E') return;
  
  // Extract sensor index
  int colonPos = data.indexOf(':');
  if (colonPos == -1) return;
  
  int sensorIndex = data.substring(1, colonPos).toInt();
  if (sensorIndex >= MAX_SENSORS) return;
  
  // Extract values
  String values = data.substring(colonPos + 1, data.length() - 1);
  int valueIndex = 0;
  int lastComma = -1;
  
  // Parse comma-separated values
  for (int i = 0; i < values.length(); i++) {
    if (values.charAt(i) == ',' || i == values.length() - 1) {
      int endPos = (i == values.length() - 1) ? i + 1 : i;
      String valueStr = values.substring(lastComma + 1, endPos);
      if (valueIndex < SENSOR_RESOLUTION) {
        sensorData[sensorIndex][valueIndex] = valueStr.toInt();
      }
      valueIndex++;
      lastComma = i;
    }
  }
}

void loop() {
  if (Serial.available() > 0) {
    String incomingData = Serial.readStringUntil('\n');
    if (incomingData.startsWith("S") && incomingData.endsWith("E")) {
      parseData(incomingData);
      
      // Debug: Print received data
      Serial.print("Received data for sensor ");
      Serial.print(incomingData.substring(1, incomingData.indexOf(':')));
      Serial.println();
    }
  }

  // Process the sensor data here
  // Example: Access data using sensorData[sensor_index][zone_index]
  
  delay(1);
}
