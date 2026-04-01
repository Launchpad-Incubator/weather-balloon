#include <Wire.h>
#include <DHT.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>
#include <WiFi.h>
#include "time.h"
#include <TinyGPSPlus.h>      
#include <SoftwareSerial.h>
#include <math.h>

#define DHT22_PIN D2
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
  double latitude;  
  double longitude;
  float wind_speed;
  float wind_dir;
};

const char* ssid = "Bill Clinternet";
const char* password = "UsmC2336";

const char* ntpServer = "pool.ntp.org";

DHT dht(DHT22_PIN, DHTTYPE);
Adafruit_BMP280 bmp;
SoftwareSerial ss(7, 8);    
TinyGPSPlus gps;

void setBuiltInLED(SystemState state) {
  switch (state) {
    case BOOTING:
      neopixelWrite(LED_PIN, 0, 0, RGB_BRIGHTNESS);  // Blue
      break;
    case SYSTEM_OK:
      neopixelWrite(LED_PIN, 0, RGB_BRIGHTNESS, 0);  // Green
      break;
    case READ_ERROR:
      neopixelWrite(LED_PIN, RGB_BRIGHTNESS, RGB_BRIGHTNESS / 2, 0);  // Orange
      break;
    case HARDWARE_FIX:
      neopixelWrite(LED_PIN, RGB_BRIGHTNESS, 0, 0);  // Red
      break;
  }
}

SensorReadings readSensors() {
  float dhtTemp = dht.readTemperature();
  float bmpTemp = bmp.readTemperature();
  float humidity = dht.readHumidity();
  float pressure = bmp.readPressure();
  
  if (isnan(dhtTemp) || isnan(bmpTemp) || isnan(humidity) || isnan(pressure)) {
    currentState = READ_ERROR;

    SensorReadings errResult = {ERR_VAL, ERR_VAL, ERR_VAL, ERR_VAL, ERR_VAL, ERR_VAL, ERR_VAL, ERR_VAL};
    return errResult;
  }

  currentState = SYSTEM_OK;
  float hectoPascals = (pressure / 100.0); // HPA

  SensorReadings result;
  result.DHT_temp = dhtTemp;
  result.BMP_temp = bmpTemp;
  result.humidity = humidity;
  result.pressure = hectoPascals;

  if (gps.location.isValid()) {
    result.latitude = gps.location.lat();
    result.longitude = gps.location.lng();
    result.wind_speed = gps.speed.mps();
    result.wind_dir = fmod(gps.course.deg() + 180, 360);
  } else {
    result.latitude = ERR_VAL; 
    result.longitude = ERR_VAL;
    result.wind_speed = 0;
    result.wind_dir = 0;
  }

  return result;
}

void DHT_Startup() {
  delay(3000);
  dht.begin();
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  if (isnan(h) || isnan(t)) {
    Serial.println("dht22 no connecto :(");
    currentState = READ_ERROR;
  } else {
    Serial.println("DHT22 Connected and Sending Data.");
  }
}

bool timeSynced = false;

void setup() {

  Serial.begin(115200);
  unsigned long start = millis();
  while (!Serial && millis() - start < 3000)
    ;

  // No pinMode needed for neopixelWrite
  setBuiltInLED(BOOTING);

  Serial.println("--- ESP32-S3 Weather Station ---");

  Wire.begin();


  if (!bmp.begin(0x76) && !bmp.begin(0x77)) {
    Serial
      .println("BMP not found!");
    currentState = HARDWARE_FIX;
    setBuiltInLED(HARDWARE_FIX);
    while (1) delay(10);
  }
  ss.begin(9600);
  DHT_Startup();

  bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,
                  Adafruit_BMP280::SAMPLING_X2,
                  Adafruit_BMP280::SAMPLING_X16,
                  Adafruit_BMP280::FILTER_X16,  // Corrected type
                  Adafruit_BMP280::STANDBY_MS_500);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Trying to get on le internet");
  int retryCount = 0;
  while (WiFi.status() != WL_CONNECTED && retryCount < 20) {
    delay(500);
    Serial
      .print(".");
    retryCount++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial
      .println(" Bluetooth connected");
    // Set for UTC: Offset 0, Daylight 0
    configTime(0, 0, ntpServer);
    timeSynced = true;
  } else {
    Serial
      .println("Nooo mi hotspot");
  }


  Serial.println("Sensors Initialized.");
}
#include <WiFi.h>
#include "time.h"

int WindGust = 0;

void printInternalTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial
      .println("Time not set yet (sync with NTP first)");
    return;
  }
  Serial.print(&timeinfo,  "@%d%H%Mz");

  // Or access individual components:
  int hour = timeinfo.tm_hour;
  int min = timeinfo.tm_min;
}
void loop() {
  while (ss.available() > 0) {
    gps.encode(ss.read());
  }


  if (WiFi.status() != WL_CONNECTED && (millis() % 60000 < 1000) && timeSynced == false) {
    WiFi.begin(ssid, password);
  }

  SensorReadings data = readSensors();
  setBuiltInLED(currentState);

  if (data.wind_speed > WindGust) {
    WindGust = data.wind_speed;
  }

  if (currentState == SYSTEM_OK || 0 == 0) {
    Serial
      .println("---------------------------");
    printInternalTime();
    Serial.print("/t");
    Serial.print(data.BMP_temp * 1.8 + 32,0);
    Serial.print("h");
    Serial.print(data.humidity, 0);
    Serial.print("b");
    Serial.print(data.pressure / 10, 0);
    Serial.print("w");
    Serial.println("RSW");
    if (gps.location.isValid()) {
      Serial.print("Lat: "); Serial.println(data.latitude, 6);
      Serial.print("Lng: "); Serial.println(data.longitude, 6);
    } else {
      Serial.println("gps is looking for satelites :O");
    }
  } else {
    Serial.println("Ruh roh raggy, smth is wrong D:");
  }

  delay(1000);  // Standard delay for testing
}
