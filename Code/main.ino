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
  int rawWind = analogRead(A7);

  SensorReadings result;

  if (isnan(dhtTemp)) {
    currentState = READ_ERROR;
    result.DHT_temp = ERR_VAL;
  } else {
    result.DHT_temp = dhtTemp;
  }

  if (isnan(bmpTemp)) {
    currentState = READ_ERROR;
    result.BMP_temp = ERR_VAL;
  } else {
    result.BMP_temp = bmpTemp;
  }

  if (isnan(humidity)) {
    currentState = READ_ERROR;
    result.humidity = ERR_VAL;
  } else {
    result.humidity = humidity;
  }

  if (isnan(pressure)) {
    currentState = READ_ERROR;
    result.pressure = ERR_VAL;
  } else {
    float hectoPascals = (pressure / 100.0);
    result.pressure = hectoPascals;
  }

  if (isnan(rawWind)) { 
    currentState = READ_ERROR; 
    result.wind_speed = ERR_VAL; 
  } else { 
    float windVolts = ((float)rawWind * 3.3) / 4095.0; 
    float zeroWind_V = 1.3692; 
    float windMPH = 0.0; 
    
    float activeTempCelsius = ERR_VAL;

    if (result.BMP_temp != ERR_VAL) {
      activeTempCelsius = result.BMP_temp;
    } else if (result.DHT_temp != ERR_VAL) {
      activeTempCelsius = result.DHT_temp;
    }

    if (activeTempCelsius != ERR_VAL) {
      float tempKelvin = activeTempCelsius + 273.15; 
      
      if (tempKelvin > 0.0) { 
        windMPH = pow((((windVolts - zeroWind_V) / (3.038517 * pow(tempKelvin, 0.115157))) / 0.087288), 3.009364); 
      }
    } else {
      windMPH = ERR_VAL;
      Serial.println("[Wind Error] Both BMP280 and DHT22 are dead. Wind calculation impossible.");
    }
    
    if (windVolts <= (zeroWind_V + 0.05) || isnan(windMPH) || windMPH < 0.0) { 
      windMPH = 0.0; 
    }
    result.wind_speed = windMPH; 
  }

  if (gps.location.isValid()) {
    result.latitude = gps.location.lat();
    result.longitude = gps.location.lng();
    result.wind_dir = fmod(gps.course.deg() + 180, 360);
    currentState = SYSTEM_OK;
  } else {
    result.latitude = ERR_VAL;
    result.longitude = ERR_VAL;
    result.wind_dir = 0;
    currentState = READ_ERROR;
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
  }

  Serial1.setRxBufferSize(1024);
  Serial1.begin(9600, SERIAL_8N1, D7, D8, false);

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

    analogReadResolution(12);
}

int WindGust = 0;

void printInternalTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Time not set yet (sync with NTP first or wait for GPS lock)");
    return;
  }
  Serial.print(&timeinfo, "%d%H%Mz");
}

String giveInternalTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "Time not set yet (sync with NTP first)";
  }
  
  char buffer[16];
  if (strftime(buffer, sizeof(buffer), "%d%H%Mz", &timeinfo) > 0) {
    return String(buffer);
  } else {
    return "Formatting Error";
  }
}

String formatTemp(float tempCelsius) {
  if (tempCelsius == ERR_VAL) return "999";

  int tempRounded = (int)round(tempCelsius * 1.8 + 32); 
  char buffer[16];
  sprintf(buffer, "%03d", tempRounded % 1000); 
  return String(buffer); 
} 

String formatPressure(float pressure) { 
  if (pressure == ERR_VAL) return "99999";

  int pressureRounded = (int)round(pressure * 10); 
  char buffer[16];
  sprintf(buffer, "%05d", pressureRounded % 100000); 
  return String(buffer); 
} 

String formatHumidity(float humidity) {
  if (humidity == ERR_VAL) return "99";

  int humidityRounded = (int)round(humidity); 
  char buffer[16];
  sprintf(buffer, "%02d", humidityRounded % 100); 
  return String(buffer); 
} 

String formatWindSpeed(double rawSpeedMs) {
  double speedMph = rawSpeedMs * 2.23694; 

  int roundedSpeed = (int)(speedMph + 0.5);

  if (roundedSpeed < 0) roundedSpeed = 0;
  if (roundedSpeed > 999) roundedSpeed = 999;

  char buffer[6]; 

  sprintf(buffer, "%03d", roundedSpeed);

  return String(buffer);
}

String formatWindDir(double rawHeading) {
  int heading = (int)(rawHeading + 0.5);

  heading = heading % 360;
  if (heading < 0) {
    heading += 360;
  }

  char buffer[6];

  sprintf(buffer, "%03d", heading);

  return String(buffer);
}


unsigned long lastTime = 0;
const unsigned long interval = 500;

const int BATCH_SIZE = 50;
String packetBatch[BATCH_SIZE];
int packetCounter = 0;

void loop() {

  int bytesProcessed = 0;
  static String nmeaBuffer = "";

  while (Serial1.available() > 0 && bytesProcessed < 1024) {
    char c = Serial1.read();
    bytesProcessed++;

    if (c == '\n' || c == '\r') {
      if (nmeaBuffer.length() > 0) {
        
        for (int i = 0; i < nmeaBuffer.length(); i++) {
          gps.encode(nmeaBuffer[i]);
        }
        gps.encode('\r');
        gps.encode('\n');
        
        nmeaBuffer = "";
      }
    } else if (c >= 32 && c <= 126) {
      nmeaBuffer += c;
    }
  }

  if (millis() - lastTime >= interval) {
    lastTime = millis();
    SensorReadings data = readSensors();

    if (data.wind_speed > WindGust) {
      WindGust = data.wind_speed;
    }

    if (!gps.location.isValid()) {
      Serial.println("gps is looking for satelites :O (" + String(gps.satellites.value()) + " satellites found)");
      currentState = READ_ERROR;
    }


    String currentPacket = "@" + giveInternalTime();
    if (gps.location.isValid()) {
      int latDeg = (int)data.latitude;
      double latMin = (fabs(data.latitude) - abs(latDeg)) * 60.0;

      char latStr[16];
      sprintf(latStr, "%02d%05.2f%c", abs(latDeg), latMin, (data.latitude >= 0) ? 'N' : 'S');

      int lngDeg = (int)data.longitude;
      double lngMin = (fabs(data.longitude) - abs(lngDeg)) * 60.0;

      char lngStr[16];
      sprintf(lngStr, "%03d%05.2f%c", abs(lngDeg), lngMin, (data.longitude >= 0) ? 'E' : 'W');

      currentPacket += String(latStr) + "/" + String(lngStr) + "_";
    } else {
      currentPacket += "0000.00N/00000.00W_";
    }

    currentPacket += formatWindDir(data.wind_dir) + "/" + formatWindSpeed(data.wind_speed) + "t" + formatTemp(data.BMP_temp) + "h" + formatHumidity(data.humidity) + "b" + formatPressure(data.pressure) + "\n";

    Serial.print(currentPacket);

    if (packetCounter < BATCH_SIZE) {
      packetBatch[packetCounter] = currentPacket;
      packetCounter++;
    }
    if (packetCounter >= BATCH_SIZE) {
      Serial.println("\nSending batch");

      String fullBatch = "";
      for (int i = 0; i < BATCH_SIZE; i++) {
        fullBatch += packetBatch[i];
      }
      //send batch here

      packetCounter = 0;
    }
    if (currentState == HARDWARE_FIX) {
      Serial.println("check wiring and whether sensors are on");
    }
    if (currentState == READ_ERROR) {
      Serial.println("error reading sensors");
    }

    setBuiltInLED(currentState);
  }
}
