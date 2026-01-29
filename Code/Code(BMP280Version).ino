// Arduino Weather Station for Nano ESP32
// Original by Glen Popiel - KW5GP | Adapted for ESP32 (Serial Only)

#include <Wire.h>
#include <DHT.h>           // Use "DHT sensor library" by Adafruit
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>

// Pin Definitions for Nano ESP32
#define DHT22_PIN D2
#define DHTTYPE DHT22

DHT dht(DHT22_PIN, DHTTYPE);
Adafruit_BMP280 bmp;

struct SensorReadings {
  float DHT_temp;
  float BMP_temp;
  float humidity;
  float pressure;
};

const float ERR_VAL = -9999.0;
SensorReadings failedSensorReadings = {ERR_VAL, ERR_VAL, ERR_VAL, ERR_VAL}; // Values for error catching that shouldn't ever show up in real cases

SensorReadings readSensors() { // we state here that it returns a SensorReadings structure
  float dhtTemp = dht.readTemperature(); // Read DHT temperature
  float bmpTemp = bmp.readTemperature(); // Read BMP temperature
  float humidity = dht.readHumidity(); // Read DHT humidity
  float pressure = bmp.readPressure(); // Read BMP pressure
  float pressureInHg = -200000;

  if (isnan(dhtTemp) || isnan(bmpTemp) || isnan(humidity) || isnan(pressure)) {
    Serial.println("Roh no, I can't find the sensor readings ˙◠˙");
    return failedSensorReadings;
  }
  
  pressureInHg = (pressure / 3386.39); // avoid running this if the value doesn't get returned, as math on a null value would cause the program to crash during runtime
  return {dhtTemp, bmpTemp, humidity, pressureInHg}; // Sensor Readings tuple with values in the order of: tempFromDHT, tempFromBMP, humidityFromDHT, pressureInHgFromBMP

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
    // Output to Serial Monitor
    Serial.println("---------------------------");
    Serial.print("Humidity: "); Serial.print(data.humidity); Serial.println(" %"); // since it's using our SensorReadings structure, you can just use value.humidity
    Serial.print("DHT22 Temp: "); Serial.print((data.DHT_temp * 1.8) + 32); Serial.println(" F");
    Serial.print("Pressure: "); Serial.print(data.pressure); Serial.println(" inHg");
    Serial.print("BMP280 Temp: "); Serial.print((data.BMP_temp * 1.8) + 32); Serial.println(" F");
  } else {
    Serial.print("Failed to read sensors :C");
  }
  delay(5000);
}
