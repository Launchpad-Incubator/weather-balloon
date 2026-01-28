// Arduino Weather Station for Nano ESP32
// Original by Glen Popiel - KW5GP | Adapted for ESP32 (Serial Only)

#include <Wire.h>
#include <DHT.h>           // Use "DHT sensor library" by Adafruit
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>

// Pin Definitions for Nano ESP32
#define DHT22_PIN D2
#define DHTTYPE DHT22

#define BMP085_ADDRESS 0x77 

DHT dht(DHT22_PIN, DHTTYPE);
Adafruit_BMP280 bmp;

// BMP085 Calibration variables
int ac1, ac2, ac3, b1, b2, mb, mc, md;
unsigned int ac4, ac5, ac6;
long b5;
const unsigned char OSS = 0; 

struct SensorReadings {
  float DHT_temp;
  float BMP_temp;
  float humidity;
  float pressure;
};

SensorReadings failedSensorReadings = {-200000, -200000, -200000, -200000};

SensorReadings readSensors() {
  float dhtTemp = dht.readTemperature(); // Read DHT temperature
  float bmpTemp = bmp.readTemperature(); // Read BMP temperature
  float humidity = dht.readHumidity(); // Read DHT humidity
  float pressure = bmp.readPressure();
  if (isnan(pressure)) {
    return failedSensorReadings
  }
  float pressureInHg = (pressure / 3386.39);
  if (isnan(dhtTemp) || isnan(bmpTemp) || isnan(humidity)) {
    Serial.println("DHT sensor read failed :(");
    return failedSensorReadings // If it fails to read, it will return an error value that the script can catch
  } else {
  return {dhtTemp, bmpTemp, humidity, pressureInHg};
  }
}

void setup() {
  // ESP32 works best at higher baud rates
  Serial.begin(115200); 
  while (!Serial) delay(10); // Wait for Serial Monitor to open
  
  Serial.println("--- Nano ESP32 Weather Station ---");
  
  Wire.begin(); // Defaults to A4 (SDA) and A5 (SCL)
  dht.begin();

  if (!bmp.begin(0x76)) {
    Serial.println("Could not find a valid BMP280 sensor at 0x76, trying 0x77...");
    if (!bmp.begin(0x77)) {
      Serial.println("Check wiring! Could not find BMP280.");
      while (1);
    }
  }

  bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,     /* Operating Mode. */
                  Adafruit_BMP280::SAMPLING_X2,     /* Temp. oversampling */
                  Adafruit_BMP280::SAMPLING_X16,    /* Pressure oversampling */
                  Adafruit_BMP280::FILTER_X16,      /* Filtering. */
                  Adafruit_BMP280::STANDBY_MS_500); /* Standby time. */

  Serial.println("Sensors Initialized.");
}

void loop() {
  SensorReadings data = readSensors();
  if (data.DHT_temp != -200000 && data.BMP_temp != -200000 && data.humidity != -200000 && data.pressure != -200000) { // if anything here is recieving our error value, then stop
    Serial.print("Failed to read sensors :C");
  } else {
    // Output to Serial Monitor
    Serial.println("---------------------------");
    Serial.print("Humidity: "); Serial.print(data.humidity); Serial.println(" %");
    Serial.print("DHT22 Temp: "); Serial.print((data.DHT_temp * 1.8) + 32); Serial.println(" F");
    Serial.print("Pressure: "); Serial.print(data.pressure); Serial.println(" inHg");
    Serial.print("BMP280 Temp: "); Serial.print((data.BMP_temp * 1.8) + 32); Serial.println(" F");
  }
  delay(5000);
}
