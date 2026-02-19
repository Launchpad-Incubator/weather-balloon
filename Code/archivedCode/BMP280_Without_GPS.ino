#include <Wire.h>
#include <DHT.h>            
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>
#include <WiFi.h>
#include "time.h"

#define DHT22_PIN 2
#define DHTTYPE DHT22

#define RGB_BRIGHTNESS 64
#if defined(RGB_BUILTIN)
  #define LED_PIN RGB_BUILTIN
#else
  #define LED_PIN 48
#endif

enum SystemState {
  BOOTING,      
  SYSTEM_OK,    
  READ_ERROR,   
  HARDWARE_FIX  
};

SystemState currentState = BOOTING;

struct SensorReadings {
  float DHT_temp;
  float BMP_temp;
  float humidity;
  float pressure;
};

const char* ssid = "Bill Clinternet"; 
const char* password = "UsmC2336";

const char* ntpServer = "pool.ntp.org";
const float ERR_VAL = -9999.0;

DHT dht(DHT22_PIN, DHTTYPE);
Adafruit_BMP280 bmp;         

void setBuiltInLED(SystemState state) {
  switch (state) {
    case BOOTING:      
      neopixelWrite(LED_PIN, 0, 0, RGB_BRIGHTNESS);  // Blue
      break;
    case SYSTEM_OK:    
      neopixelWrite(LED_PIN, 0, RGB_BRIGHTNESS, 0);  // Green
      break;
    case READ_ERROR:  
      neopixelWrite(LED_PIN, RGB_BRIGHTNESS, RGB_BRIGHTNESS/2, 0); // Orange
      break;
    case HARDWARE_FIX: 
      neopixelWrite(LED_PIN, RGB_BRIGHTNESS, 0, 0);  // Red
      break;
  }
}

// 3. FIXED STRUCT RETURN
SensorReadings readSensors() {
  float dhtTemp = dht.readTemperature();
  float bmpTemp = bmp.readTemperature();
  float humidity = dht.readHumidity();
  float pressure = bmp.readPressure();

  if (isnan(dhtTemp) || isnan(bmpTemp) || isnan(humidity) || isnan(pressure)) {
    currentState = READ_ERROR;
    // Fix: Explicitly create the struct to avoid conversion errors
    SensorReadings errResult;
    errResult.DHT_temp = ERR_VAL;
    errResult.BMP_temp = ERR_VAL;
    errResult.humidity = ERR_VAL;
    errResult.pressure = ERR_VAL;
    return errResult;
  }

  currentState = SYSTEM_OK;
  float pressureInHg = (pressure / 3386.39);
  
  SensorReadings result;
  result.DHT_temp = dhtTemp;
  result.BMP_temp = bmpTemp;
  result.humidity = humidity;
  result.pressure = pressureInHg;
  return result;
}

void setup() {

  Serial0.begin(115200);
  // On ESP32-S3, wait for Serial0 but with a timeout so it runs without a PC
  unsigned long start = millis();
  while (!Serial0 && millis() - start < 3000); 

  // No pinMode needed for neopixelWrite
  setBuiltInLED(BOOTING);

  Serial0.println("--- ESP32-S3 Weather Station ---");

  Wire.begin();
  dht.begin();

  if (!bmp.begin(0x76) && !bmp.begin(0x77)) {
    Serial0.println("BMP not found!");
    currentState = HARDWARE_FIX;
    setBuiltInLED(HARDWARE_FIX);
    while (1) delay(10);
  }

  bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,
                  Adafruit_BMP280::SAMPLING_X2,
                  Adafruit_BMP280::SAMPLING_X16,
                  Adafruit_BMP280::FILTER_X16,    // Corrected type
                  Adafruit_BMP280::STANDBY_MS_500);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial0.print("Connecting to WiFi");
  int retryCount = 0;
  while (WiFi.status() != WL_CONNECTED && retryCount < 20) {
    delay(500);
    Serial0.print(".");
    retryCount++;
  }

  if(WiFi.status() == WL_CONNECTED) {
    Serial0.println(" Connected!");
    // Set for UTC: Offset 0, Daylight 0
    configTime(0, 0, ntpServer);
  } else {
    Serial0.println(" Failed! (Running offline)");
  }


  Serial0.println("Sensors Initialized.");
}
#include <WiFi.h>
#include "time.h"

void printInternalTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial0.println("Time not set yet (sync with NTP first)");
    return;
  }
  // Print formatted time: e.g., "Tuesday, February 18 2026 14:30:05"
  Serial0.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
  
  // Or access individual components:
  int hour = timeinfo.tm_hour;
  int min  = timeinfo.tm_min;
}
void loop() {
  SensorReadings data = readSensors();
  setBuiltInLED(currentState);

  if (currentState == SYSTEM_OK) {
    Serial0.println("---------------------------");
    Serial0.print("Time: "); printInternalTime();
    Serial0.print("Humidity: ");    Serial0.print(data.humidity); Serial0.println(" %");
    Serial0.print("DHT22 Temp: ");  Serial0.print((data.DHT_temp * 1.8) + 32); Serial0.println(" F");
    Serial0.print("BMP280 Temp: "); Serial0.print((data.BMP_temp * 1.8) + 32); Serial0.println(" F");
    Serial0.print("Pressure: ");    Serial0.print(data.pressure); Serial0.println(" inHg");
  } else {
    Serial0.println("System Error! Checking components...");
  }
  
  delay(1000); // Standard delay for testing
}
