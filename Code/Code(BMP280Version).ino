// Arduino Weather Station for Nano ESP32
// Original by Glen Popiel - KW5GP | Adapted for ESP32 (Serial Only)

#include <Wire.h>
#include <DHT.h>           // Use "DHT sensor library" by Adafruit
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>

// Pin Definitions for Nano ESP32
#define DHT22_PIN 2      // Physical Label D2 (GPIO 5)
#define DHTTYPE DHT22

#define BMP085_ADDRESS 0x77 

DHT dht(DHT22_PIN, DHTTYPE);
Adafruit_BMP280 bmp;

// BMP085 Calibration variables
int ac1, ac2, ac3, b1, b2, mb, mc, md;
unsigned int ac4, ac5, ac6;
long b5;
const unsigned char OSS = 0; 

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
  float humidity = dht.readHumidity();
  float dhtTempC = dht.readTemperature(); // Temp from DHT22
  
  if (isnan(humidity) || isnan(dhtTempC)) {
    Serial.println("Failed to read from DHT sensor!");
  } else {
    // Read BMP280 Data using library functions
    float bmpTempC = bmp.readTemperature();
    float pressurePa = bmp.readPressure();
    float inHg = (pressurePa / 3386.39); // Correct conversion to inHg

    // Output to Serial Monitor
    Serial.println("---------------------------");
    Serial.print("Humidity: "); Serial.print(humidity); Serial.println(" %");
    Serial.print("DHT22 Temp: "); Serial.print((dhtTempC * 1.8) + 32); Serial.println(" F");
    Serial.print("Pressure: "); Serial.print(inHg); Serial.println(" inHg");
    Serial.print("BMP280 Temp: "); Serial.print((bmpTempC * 1.8) + 32); Serial.println(" F");
  }

  delay(5000);
}
