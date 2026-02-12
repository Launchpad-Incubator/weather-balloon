//LED STUFF: Green = Good, Yellow = Booting, Orange = Failed sensor read, Red = Wiring/hardware problem

#include <Wire.h>
#include <DHT.h>           
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>
#include <SoftwareSerial.h>

// --- Definitions ---
#define DHT22_PIN D2
#define DHTTYPE DHT22

// Enum for state
enum SystemState {
  BOOTING,      // Yellow (booting)
  SYSTEM_OK,    // Green (all good)
  READ_ERROR,   // Orange (failed sensor read)
  HARDWARE_FIX  // Red (missing sensors)
};

SystemState currentState = BOOTING;

// Data stuff
struct SensorReadings {
  float DHT_temp;
  float BMP_temp;
  float humidity;
  float pressure;
  byte GPS_Data;
};

const float ERR_VAL = -9999.0;

// Sensor Init
DHT dht(DHT22_PIN, DHTTYPE);
Adafruit_BMP280 bmp;
SoftwareSerial ss(7, 8);

// LED Logic--
void setBuiltInLED(SystemState state) {
  switch (state) {
    case BOOTING:     
      analogWrite(LED_RED, 0);    
      analogWrite(LED_GREEN, 0); 
      analogWrite(LED_BLUE, 255);  
      break;
    case SYSTEM_OK:   
      analogWrite(LED_RED, 255);   
      analogWrite(LED_GREEN, 0);   
      analogWrite(LED_BLUE, 255);  
      break;
    case READ_ERROR:  
      analogWrite(LED_RED, 0);    
      analogWrite(LED_GREEN, 150); 
      analogWrite(LED_BLUE, 255);  
      break;
    case HARDWARE_FIX: 
      analogWrite(LED_RED, 0);     
      analogWrite(LED_GREEN, 255); 
      analogWrite(LED_BLUE, 255);  
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
  if (isnan(dhtTemp) || isnan(bmpTemp) || isnan(humidity) || isnan(pressure) || ss.available() == 0) {
    currentState = READ_ERROR;
    return {ERR_VAL, ERR_VAL, ERR_VAL, ERR_VAL};
  }
  byte GPS_Value = ss.read();

  currentState = SYSTEM_OK;
  float pressureInHg = (pressure / 3386.39);
  return {dhtTemp, bmpTemp, humidity, pressureInHg, GPS_Value};
}

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);
  ss.begin(115200);

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
    Serial.print("GPS Data:); Serial.print(data.GPS_Data);
  } else {
    Serial.println("Failed to read sensors! Check wiring. :C");
  }
  
  delay(5000);
}
