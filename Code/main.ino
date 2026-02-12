//LED STUFF: Green = Good, Yellow = Booting, Orange = Failed sensor read, Red = Wiring/hardware problem

#include <Wire.h>
#include <DHT.h>            
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>
#include <TinyGPS++.h>      
#include <SoftwareSerial.h>

#define DHT22_PIN D2
#define DHTTYPE DHT22

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
};

const float ERR_VAL = -9999.0;

DHT dht(DHT22_PIN, DHTTYPE);
Adafruit_BMP280 bmp;
SoftwareSerial ss(7, 8);    
TinyGPSPlus gps;            

// LED stuff
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

// Cool thingy that like not only delays, but also gets the data from the gps while in between stuff
static void smartDelay(unsigned long ms) {
  unsigned long start = millis();
  do {
    while (ss.available())
      gps.encode(ss.read());
  } while (millis() - start < ms);
}

SensorReadings readSensors() {
  float dhtTemp = dht.readTemperature();
  float bmpTemp = bmp.readTemperature();
  float humidity = dht.readHumidity();
  float pressure = bmp.readPressure();
  
  // Check if sensors are responding
  if (isnan(dhtTemp) || isnan(bmpTemp) || isnan(humidity) || isnan(pressure)) {
    currentState = READ_ERROR;
    return {ERR_VAL, ERR_VAL, ERR_VAL, ERR_VAL, 0.0, 0.0};
  }

  if (gps.charsProcessed() < 10) {
     Serial.println("D: No scrumpcious gps data yet? Check wiring pretty please :C");
     currentState = READ_ERROR; 
  } else {
     currentState = SYSTEM_OK;
  }

  float pressureInHg = (pressure / 3386.39);
  return {dhtTemp, bmpTemp, humidity, pressureInHg, gps.location.lat(), gps.location.lng()};
}

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);
  
  ss.begin(9600); 

  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);
  setBuiltInLED(BOOTING);

  Serial.println("--- Nano ESP32 Weather Station ---");
  
  Wire.begin();
  dht.begin();

  if (!bmp.begin(0x76) && !bmp.begin(0x77)) {
    Serial.println("BMP not found!");
    currentState = HARDWARE_FIX;
    setBuiltInLED(HARDWARE_FIX);
    while (1);
  }

  bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,
                  Adafruit_BMP280::SAMPLING_X2,
                  Adafruit_BMP280::SAMPLING_X16,
                  Adafruit_BMP280::SAMPLING_X16,
                  Adafruit_BMP280::STANDBY_MS_500);

  Serial.println("Sensors Initialized.");
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
    if (gps.location.isValid()) {
      Serial.print("Lat: "); Serial.println(data.latitude, 6);
      Serial.print("Lng: "); Serial.println(data.longitude, 6);
    } else {
      Serial.println("gps is looking for satelites :O");
    }

  } else {
    Serial.println("System Error! Checking components...");
  }
  smartDelay(5000); 
}
