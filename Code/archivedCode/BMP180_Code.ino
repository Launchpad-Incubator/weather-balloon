
// Arduino Weather Station for Nano ESP32
// Original by Glen Popiel - KW5GP | Adapted for ESP32 (Serial Only)

#include <Wire.h>
#include <DHT.h>           // Use "DHT sensor library" by Adafruit
#include <Adafruit_Sensor.h>

// Pin Definitions for Nano ESP32
#define DHT22_PIN 2      // Physical Label D2 (GPIO 5)
#define DHTTYPE DHT22

#define BMP085_ADDRESS 0x77 

DHT dht(DHT22_PIN, DHTTYPE);

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

  bmp085Calibration();
  Serial.println("Sensors Initialized.");
}

void loop() {
  // Read DHT22 (Humidity and Temp)
  float humidity = dht.readHumidity();
  float centigrade = dht.readTemperature();
  
  if (isnan(humidity) || isnan(centigrade)) {
    Serial.println("Failed to read from DHT sensor!");
  } else {
    float fahrenheit = (centigrade * 1.8) + 32;
    
    // Read BMP085 (Pressure and secondary Temp)
    float bmpTemp = bmp085GetTemperature(bmp085ReadUT());
    float pressurePa = bmp085GetPressure(bmp085ReadUP());
    float inHg = (pressurePa / 1000.0) * 0.2953;

    // Output to Serial Monitor
    Serial.println("---------------------------");
    Serial.print("Humidity: "); Serial.print(humidity); Serial.println(" %");
    Serial.print("Temperature: "); Serial.print(fahrenheit); Serial.println(" F");
    Serial.print("Pressure: "); Serial.print(inHg); Serial.println(" inHg");
    Serial.print("BMP085 Temp: "); Serial.print((bmpTemp * 1.8) + 32); Serial.println(" F");
  }

  delay(5000); // Wait 5 seconds between readings
}

// --- BMP085 Core Functions ---

void bmp085Calibration() {
  ac1 = bmp085ReadInt(0xAA);
  ac2 = bmp085ReadInt(0xAC);
  ac3 = bmp085ReadInt(0xAE);
  ac4 = bmp085ReadInt(0xB0);
  ac5 = bmp085ReadInt(0xB2);
  ac6 = bmp085ReadInt(0xB4);
  b1 = bmp085ReadInt(0xB6);
  b2 = bmp085ReadInt(0xB8);
  mb = bmp085ReadInt(0xBA);
  mc = bmp085ReadInt(0xBC);
  md = bmp085ReadInt(0xBE);
}

float bmp085GetTemperature(unsigned int ut) {
  long x1 = (((long)ut - (long)ac6)*(long)ac5) >> 15;
  long x2 = ((long)mc << 11)/(x1 + md);
  b5 = x1 + x2;
  return ((b5 + 8) >> 4) / 10.0;
}

long bmp085GetPressure(unsigned long up) {
  long x1, x2, x3, b3, b6, p;
  unsigned long b4, b7;
  b6 = b5 - 4000;
  x1 = (b2 * (b6 * b6)>>12)>>11;
  x2 = (ac2 * b6)>>11;
  x3 = x1 + x2;
  b3 = (((((long)ac1)*4 + x3)<<OSS) + 2)>>2;
  x1 = (ac3 * b6)>>13;
  x2 = (b1 * ((b6 * b6)>>12))>>16;
  x3 = ((x1 + x2) + 2)>>2;
  b4 = (ac4 * (unsigned long)(x3 + 32768))>>15;
  b7 = ((unsigned long)(up - b3) * (50000>>OSS));
  if (b7 < 0x80000000) p = (b7<<1)/b4;
  else p = (b7/b4)<<1;
  x1 = (p>>8) * (p>>8);
  x1 = (x1 * 3038)>>16;
  x2 = (-7357 * p)>>16;
  p += (x1 + x2 + 3791)>>4;
  return p;
}

int bmp085ReadInt(unsigned char address) {
  unsigned char msb, lsb;
  Wire.beginTransmission(BMP085_ADDRESS);
  Wire.write(address);
  Wire.endTransmission();
  Wire.requestFrom(BMP085_ADDRESS, 2);
  while(Wire.available()<2);
  msb = Wire.read();
  lsb = Wire.read();
  return (int) msb<<8 | lsb;
}

unsigned int bmp085ReadUT() {
  Wire.beginTransmission(BMP085_ADDRESS);
  Wire.write(0xF4);
  Wire.write(0x2E);
  Wire.endTransmission();
  delay(5);
  return bmp085ReadInt(0xF6);
}

unsigned long bmp085ReadUP() {
  unsigned char msb, lsb, xlsb;
  Wire.beginTransmission(BMP085_ADDRESS);
  Wire.write(0xF4);
  Wire.write(0x34 + (OSS<<6));
  Wire.endTransmission();
  delay(2 + (3<<OSS));
  
  Wire.beginTransmission(BMP085_ADDRESS);
  Wire.write(0xF6);
  Wire.endTransmission();
  Wire.requestFrom(BMP085_ADDRESS, 3);
  while(Wire.available() < 3);
  msb = Wire.read();
  lsb = Wire.read();
  xlsb = Wire.read();
  return (((unsigned long) msb << 16) | ((unsigned long) lsb << 8) | (unsigned long) xlsb) >> (8-OSS);
}
