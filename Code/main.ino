#include <Wire.h>
#include <DHT.h>           
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>

// --- Definitions ---
#define DHT22_PIN D2
#define DHTTYPE DHT22

// --- Enum for System State ---
enum SystemState {
  BOOTING,      // Yellow/Orange
  SYSTEM_OK,    // Green
  READ_ERROR,   // Red
  HARDWARE_FIX  // Solid Red (Critical)
};

SystemState currentState = BOOTING;

// --- Data Structures ---
struct SensorReadings {
  float DHT_temp;
  float BMP_temp;
  float humidity;
  float pressure;
};

const float ERR_VAL = -9999.0;

// Initialize Sensors
DHT dht(DHT22_PIN, DHTTYPE);
Adafruit_BMP280 bmp;

// --- LED Control Function (Inverted Logic for Built-in LED) ---
void setBuiltInLED(SystemState state) {
  // On Nano ESP32 built-in LED: 0 = Full Brightness, 255 = OFF
  switch (state) {
    case BOOTING:     
      analogWrite(LED_RED, 0);    // Red ON
      analogWrite(LED_GREEN, 50); // Green Half-ON (creates Orange/Yellow)
      analogWrite(LED_BLUE, 255);  // Blue OFF
      break;
    case SYSTEM_OK:   
      analogWrite(LED_RED, 255);   // Red OFF
      analogWrite(LED_GREEN, 0);   // Green ON
      analogWrite(LED_BLUE, 255);  // Blue OFF
      break;
    case READ_ERROR:  
      analogWrite(LED_RED, 0);    // Red ON
      analogWrite(LED_GREEN, 150); // Green Half-ON (creates Orange/Yellow)
      analogWrite(LED_BLUE, 255);  // Blue OFF
      break;
    case HARDWARE_FIX: 
      analogWrite(LED_RED, 0);     // Red ON
      analogWrite(LED_GREEN, 255); // Green OFF
      analogWrite(LED_BLUE, 255);  // Blue OFF
      break;
  }
}

// --- Sensor Reading Logic ---
SensorReadings readSensors() {
  float dhtTemp = dht.readTemperature();
  float bmpTemp = bmp.readTemperature();
  float humidity = dht.readHumidity();
  float pressure = bmp.readPressure();
  
  // Check for NaN (Not a Number) failures
  if (isnan(dhtTemp) || isnan(bmpTemp) || isnan(humidity) || isnan(pressure)) {
    currentState = READ_ERROR;
    return {ERR_VAL, ERR_VAL, ERR_VAL, ERR_VAL};
  }

  currentState = SYSTEM_OK;
  float pressureInHg = (pressure / 3386.39);
  return {dhtTemp, bmpTemp, humidity, pressureInHg};
}

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  // Initialize Built-in LED Pins
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);
  setBuiltInLED(BOOTING);

  Serial.println("--- Nano ESP32 Weather Station ---");
  
  Wire.begin();
  dht.begin();

  // Try to find the BMP280
  if (!bmp.begin(0x76) && !bmp.begin(0x77)) {
    Serial.println("BMP not found D:");
    currentState = HARDWARE_FIX;
    setBuiltInLED(HARDWARE_FIX);
    while (1);
  }

  // BMP280 fine-tuning
  bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,
                  Adafruit_BMP280::SAMPLING_X2,
                  Adafruit_BMP280::SAMPLING_X16,
                  Adafruit_BMP280::FILTER_X16,
                  Adafruit_BMP280::STANDBY_MS_500);

  Serial.println("Sensors Initialized. Yipee :P");
}

void loop() {
  SensorReadings data = readSensors();
  setBuiltInLED(currentState);

  if (currentState == SYSTEM_OK) {
    Serial.println("---------------------------");
    Serial.print("Humidity: ");    Serial.print(data.humidity); Serial.println(" %");
    Serial.print("DHT22 Temp: ");  Serial.print((data.DHT_temp * 1.8) + 32); Serial.println(" F");
    Serial.print("BMP280 Temp: "); Serial.print((data.BMP_temp * 1.8) + 32); Serial.println(" F");
    Serial.print("Pressure: ");    Serial.print(data.pressure); Serial.println(" inHg");
  } else {
    Serial.println("Failed to read sensors! Check wiring. :C");
  }
  
  delay(5000);
}
