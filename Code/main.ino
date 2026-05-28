#include <Wire.h>
#include <DHT.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>
#include <WiFi.h>
#include "time.h"
#include <TinyGPSPlus.h>
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

const char* ntpServer = "pool.ntp.org";

float last_v = 0;
unsigned long last_time = 0;

DHT dht(DHT22_PIN, DHTTYPE);
Adafruit_BMP280 bmp;
TinyGPSPlus gps;

void setBuiltInLED(SystemState state) {
  switch (state) {
    case BOOTING:
      neopixelWrite(LED_PIN, 0, 0, RGB_BRIGHTNESS);
      break;
    case SYSTEM_OK:
      neopixelWrite(LED_PIN, 0, RGB_BRIGHTNESS, 0);
      break;
    case READ_ERROR:
      neopixelWrite(LED_PIN, RGB_BRIGHTNESS, RGB_BRIGHTNESS / 2, 0);
      break;
    case HARDWARE_FIX:
      neopixelWrite(LED_PIN, RGB_BRIGHTNESS, 0, 0);
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
    SensorReadings errResult = { ERR_VAL, ERR_VAL, ERR_VAL, ERR_VAL, ERR_VAL, ERR_VAL, ERR_VAL, ERR_VAL };
    return errResult;
  }

  float hectoPascals = (pressure / 100.0);

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
    currentState = SYSTEM_OK;
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

const char* networks[][2] = {
  { "Launchpad Internal", "L@unchP@d!nc" },
  { "Bill Clinternet", "UsmC2336" },
};

void connectWithTimeout() {
  for (int i = 0; i < 2; i++) {
    Serial.printf("Trying %s", networks[i][0]);
    WiFi.begin(networks[i][0], networks[i][1]);

    unsigned long startAttemptTime = millis();

    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 20000) {
      delay(500);
      Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nBluetooth Connected");
      configTime(0, 0, ntpServer);
      return;
    } else {
      Serial.println("\nNooo mi hotspot");
    }
  }
  Serial.println("I have no bars 😭");
}

void WiFiReconnectorTask(void* pvParameters) {
  for (;;) {
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("[WiFi Task] Connection lost. Retrying...");

      for (int i = 0; i < 2; i++) {
        WiFi.begin(networks[i][0], networks[i][1]);
        unsigned long start = millis();

        while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
          vTaskDelay(500 / portTICK_PERIOD_MS);
        }

        if (WiFi.status() == WL_CONNECTED) {
          Serial.println("[WiFi Task] Connected!");
          configTime(0, 0, ntpServer);
          break;
        }
      }
    }
    vTaskDelay(30000 / portTICK_PERIOD_MS);
  }
}

void setup() {
  Serial.begin(115200);
  unsigned long start = millis();
  while (!Serial && millis() - start < 3000)
    ;

  setBuiltInLED(BOOTING);
  Serial.println("--- ESP32-S3 Weather Station ---");

  Wire.begin();

  if (!bmp.begin(0x76) && !bmp.begin(0x77)) {
    Serial.println("BMP not found!");
    currentState = HARDWARE_FIX;
    setBuiltInLED(HARDWARE_FIX);
    while (1) delay(10);
  }

  Serial1.setRxBufferSize(2048);
  Serial1.begin(115200, SERIAL_8N1, D7, D8, false);

  DHT_Startup();

  bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,
                  Adafruit_BMP280::SAMPLING_X2,
                  Adafruit_BMP280::SAMPLING_X16,
                  Adafruit_BMP280::FILTER_X16,
                  Adafruit_BMP280::STANDBY_MS_500);

  WiFi.mode(WIFI_STA);
  connectWithTimeout();

  Serial.println("Sensors Initialized.");

  xTaskCreatePinnedToCore(
    WiFiReconnectorTask,
    "WiFiTask",
    4096,
    NULL,
    1,
    NULL,
    0);
}

int WindGust = 0;

void printInternalTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Time not set yet (sync with NTP first)");
    return;
  }
  Serial.print(&timeinfo, "%d%H%Mz");
}

String formatTemp(float tempCelsius) {
  int tempRounded = (int)round(tempCelsius * 1.8 + 32);
  char buffer[5];
  sprintf(buffer, "%03d", tempRounded % 1000);
  return String(buffer);
}

String formatPressure(float pressure) {
  int pressureRounded = (int)round(pressure * 10);
  char buffer[7];
  sprintf(buffer, "%05d", pressureRounded % 100000);
  return String(buffer);
}

String formatHumidity(float humidity) {
  int humidityRounded = (int)round(humidity);
  char buffer[4];
  sprintf(buffer, "%02d", humidityRounded % 100);
  return String(buffer);
}

unsigned long lastTime = 0;
const unsigned long interval = 1000;

bool currentInversion = false;
long currentBaud = 115200;

void loop() {

  int bytesProcessed = 0;
  while (Serial1.available() > 0 && bytesProcessed < 128) {
    char c = Serial1.read();
    bytesProcessed++;

    if ((c >= 32 && c <= 126) || c == '\r' || c == '\n') {
        Serial.write(c);
        gps.encode(c);
    }
  }

  if (millis() - lastTime >= interval) {
    lastTime = millis();
    SensorReadings data = readSensors();
    setBuiltInLED(currentState);

    if (data.wind_speed > WindGust) {
      WindGust = data.wind_speed;
    }

    if (currentState == SYSTEM_OK || 1 == 1) {
      Serial.println("\n---------------------------");
      Serial.print("@");
      if (gps.location.isValid()) {
        int latDeg = (int)data.latitude;
        double latMin = (data.latitude - latDeg) * 60.0;
        char latStr[9];
        sprintf(latStr, "%02d%05.2f%c", abs(latDeg), latMin, (latDeg >= 0) ? 'N' : 'S');

        int lngDeg = (int)data.longitude;
        double lngMin = (data.longitude - lngDeg) * 60.0;
        char lngStr[10];
        sprintf(lngStr, "%03d%05.2f%c", abs(lngDeg), lngMin, (lngDeg >= 0) ? 'E' : 'W');

        Serial.print(latStr);
        Serial.print("/");
        Serial.print(lngStr);
        Serial.print("_");
      } else {
        Serial.print("0000.00N/00000.00W_");
      }
      printInternalTime();
      Serial.print("/t");
      Serial.print(formatTemp(data.BMP_temp));
      Serial.print("h");
      Serial.print(formatHumidity(data.humidity));
      Serial.print("b");
      Serial.println(formatPressure(data.pressure));

      if (gps.location.isValid()) {
      } else {
        Serial.println("gps is looking for satelites :O (" + String(gps.satellites.value()) + " satellites found)");
        currentState = READ_ERROR;
      }
    } else {
      Serial.println("Ruh roh raggy, something is wrong D:");
      currentState = READ_ERROR;
    }
  }
}
