#include <Arduino.h>
#include <M5Tough.h>
#include <Adafruit_MLX90614.h>
#include <SensirionI2CSfa3x.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <NTPClient.h>
#include <TimeLib.h>
#include <Timezone.h>
#include "FreeSansBold36pt7b.h"
#include "FreeSans8pt7b.h"
#include "logo.h"

#define WIFI_SSID "XXXXXXXXX"
#define WIFI_PASS "XXXXXXXXX"
#define WIFI_WAIT_SECS  10
#define WIFI_RETRY_SECS 30

#define READING_INTERVAL_SEC 3
#define TEMP_THRESHOLD_RED 30.0
#define TEMP_THRESHOLD_ORANGE 25.0
#define TEMP_THRESHOLD_GREEN 20.0
#define TEMP_PUBLISH_THRESHOLD 0.2

#define MQTT_PUBLISH_INTERVAL_SECS 20
#define MQTT_BROKER "192.168.10.1"
#define MQTT_TOPIC  "mlx90614/state"
#define MQTT_RETRY_SECS 10
//#define MQTT_USER "username"
//#define MQTT_PASS "password"

#define NTP_ADDRESS "de.pool.ntp.org"
#define NTP_UPDATE  1800  // interval in seconds between updates

#define SKETCH_NAME "MLX90614Thermometer_MQTT"
#define SKETCH_VER  "1.0b3"

// IR Sensor
Adafruit_MLX90614 mlx = Adafruit_MLX90614();

// SFA30 (Formaldehyd)
SensirionI2CSfa3x SFA30;
uint16_t sfa30Error;
char sfa30ErrorMsg[256];

// setup the ntp udp client
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, NTP_ADDRESS, 0, (NTP_UPDATE * 1000));

// TimeZone Settings Details https://github.com/JChristensen/Timezone
TimeChangeRule CEST = {"CEST", Last, Sun, Mar, 2, 120};
TimeChangeRule CET = {"CET ", Last, Sun, Oct, 3, 60};
Timezone CE(CEST, CET);  // Frankfurt, Paris

// setup wifi and mqtt client
WiFiClient espClient;
PubSubClient mqtt(espClient);

// fake ° symbol
void printDegree(uint16_t color) {
  M5.Lcd.fillCircle(M5.Lcd.getCursorX()+14, M5.Lcd.getCursorY()-42, 8, WHITE);
  M5.Lcd.fillCircle(M5.Lcd.getCursorX()+14, M5.Lcd.getCursorY()-42, 4, color);
}


char* getTimeString(time_t epoch) {
    time_t t = CE.toLocal(epoch);
    static char strTime[9]; // HH:MM:SS
    
    if (epoch > 1000) { 
      sprintf(strTime, "%.2d:%.2d:%.2d", hour(t), minute(t), second(t));
    } else {
      strncpy(strTime, "--:--:--", 8); 
    }
    return strTime;
}


// returns ptr to array with current date
char* getDateString(time_t epoch) {
    time_t t = CE.toLocal(epoch);
    static char strDate[11]; // DD.MM.YYYY

    if (epoch > 1000) {
      sprintf(strDate, "%.2d.%.2d.%4d", day(t), month(t), year(t));
    } else {
      strncpy(strDate, "--.--.----", 10); 
    }
    return strDate;
}


// returns hardware system id (last 3 bytes of mac address)
String getSystemID() {
    uint8_t mac[8];
    char sysid[8];
    
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    sprintf(sysid, "%02X%02X%02X", mac[3], mac[4], mac[5]);
    return String(sysid);
}


void wifiConnect() {
  uint8_t waitSecs = 0;
  
  M5.Lcd.fillScreen(BLUE);
  M5.Lcd.setFreeFont(&FreeSans12pt7b);
  M5.Lcd.setCursor(20,40);
  M5.Lcd.print("Connecting to WiFi..");
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  while (waitSecs < WIFI_WAIT_SECS) {
    if (WiFi.status() != WL_CONNECTED) {
      M5.Lcd.print(".");
      delay(1000);
      waitSecs++;
    } else {
      M5.Lcd.print("OK");
      M5.Lcd.setCursor(20,100);
      M5.Lcd.print("SSID: ");
      M5.Lcd.println(WiFi.SSID());
      M5.Lcd.setCursor(20,130);
      M5.Lcd.print("RSSI: ");
      M5.Lcd.print(WiFi.RSSI());
      M5.Lcd.println(" dBm");
      M5.Lcd.setCursor(20,160);
      M5.lcd.print("IP: ");
      M5.lcd.println(WiFi.localIP());
      delay(2000);
      break;
    }
  }

  if (WiFi.status() != WL_CONNECTED) {
      M5.Lcd.fillScreen(RED);
      M5.Lcd.setCursor(30,70);
      M5.Lcd.print("Failed to connect to SSID");
      M5.Lcd.setTextDatum(MC_DATUM);
      M5.Lcd.drawString(WIFI_SSID, 160, 120, 4);
      delay(5000);
  }
}


// connect to mqtt broker with random client id
bool mqttConnect(bool startup) {
  static char clientid[16];
  static uint8_t mqtt_error = 0;
  
  if (!mqtt.connected()) {
    // generate pseudo random client id
    snprintf(clientid, sizeof(clientid), "M5Tough_%x", random(0xffff));
    Serial.printf("Connecting to MQTT Broker %s as %s...", MQTT_BROKER, clientid); 

#ifdef MQTT_USER
    if (mqtt.connect(clientid, MQTT_USER, MQTT_PASS)) {
#else
    if (mqtt.connect(clientid)) {
#endif
      Serial.println(F("OK."));
      return true;

    } else {
      Serial.printf("failed (error %d)\n", mqtt.state());
      if (startup) {
        M5.Lcd.fillScreen(RED);
        M5.Lcd.setCursor(20,70);
        M5.Lcd.print("Failed to connect to MQTT");
        M5.Lcd.setTextDatum(MC_DATUM);
        M5.Lcd.drawString(MQTT_BROKER, 160, 120, 4);
        delay(5000);
      }
      return false;
    }
  }

  return true;
}



// rollover safe comparison for given timestamp with millis()
int32_t tsDiff(uint32_t tsMillis) {
  int32_t diff = millis() - tsMillis;
  if (diff < 0)
    return abs(diff);
  else
    return diff;
}


// returns runtim in minutes
uint32_t getRuntimeMinutes() {
    static uint32_t lastMillis = 0;
    static uint32_t seconds = 0;

    seconds += tsDiff(lastMillis) / 1000;
    lastMillis = millis();
    return seconds/60;
}


// try to publish sensor reedings with given timeout 
// will implicitly call mqtt_init()
bool mqttSend(double objectTemp, double ambientTemp, int16_t hcho) {
    StaticJsonDocument<256> JSON;
    static char topic[64], buf[192];

    JSON.clear();
    JSON["systemId"] = getSystemID();;
    JSON["objectTemp"] = objectTemp;
    JSON["ambientTemp"] = ambientTemp;
    JSON["formaldehyde"] = hcho / 5.0;
    JSON["rssi"] = WiFi.RSSI();
    JSON["runtime"] = getRuntimeMinutes();
    JSON["version"] = SKETCH_VER;

    memset(buf, 0, sizeof(buf));
    size_t s = serializeJson(JSON, buf);
    if (mqttConnect(false)) {
        snprintf(topic, sizeof(topic)-1, "%s", MQTT_TOPIC);
        if (mqtt.publish(topic, buf, s)) {
            Serial.printf("MQTT: published %d bytes to %s on %s\n", s, MQTT_TOPIC, MQTT_BROKER);
            return true;
        }
    }
    Serial.printf("MQTT: failed to publish to %s on %s (error %d)\n", MQTT_TOPIC, MQTT_BROKER, mqtt.state());
    return false;
}


void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.printf("\nStarting Sketch %s v%s\n",SKETCH_NAME, SKETCH_VER);
  Serial.printf("Compiled on %s, %s\n", __DATE__, __TIME__);
  
  M5.begin();
  M5.Lcd.fillScreen(WHITE);
  M5.Lcd.pushImage(5, 70, logoWidth, logoHeight, logo);
  delay(2000);

  M5.Lcd.fillScreen(BLUE);
  M5.Lcd.setFreeFont(&FreeSans24pt7b);
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.setCursor(45,90);
  M5.Lcd.print("MLX90614");
  M5.Lcd.setCursor(20,130);
  M5.Lcd.print("Thermometer");
  M5.Lcd.setFreeFont(&FreeSans8pt7b);
  M5.Lcd.setCursor(110,160);
  M5.Lcd.printf("Firmware %s", SKETCH_VER);

  if (!mlx.begin()) {
    M5.Lcd.fillRect(0, 205, 320, 40, RED);
    M5.Lcd.setFreeFont(&FreeSans12pt7b);
    M5.Lcd.setCursor(55, 230);
    M5.Lcd.print("No sensor detected");
    Serial.println("ERROR: Failed to detect sensor MLX90614");
    while (1);
  } else {
    M5.Lcd.fillRect(0, 205, 320, 40, DARKGREEN);
    M5.Lcd.setFreeFont(&FreeSans12pt7b);
    M5.Lcd.setCursor(80   , 230);
    M5.Lcd.print("Emissivity: ");
    M5.Lcd.print(mlx.readEmissivity());
    Serial.print("Sensor MLX90614 ready, emissivity ");
    Serial.println(mlx.readEmissivity());
    delay(2000);
  }

  SFA30.begin(Wire);
  sfa30Error = SFA30.startContinuousMeasurement();
  if (sfa30Error) {
    errorToString(sfa30Error, sfa30ErrorMsg, 256);
    Serial.printf("ERROR: Failed to detect sensor SFA30 (%s)\n", sfa30ErrorMsg);
  } else {
    Serial.println("Sensor SFA30 ready, starting continuous measurement");
    delay(2000);
  }
  
  wifiConnect();
  
  timeClient.begin();
  if (WiFi.status() == WL_CONNECTED) {
    M5.Lcd.fillScreen(BLUE);
    M5.Lcd.setFreeFont(&FreeSans12pt7b);
    M5.Lcd.setCursor(20,40);
    M5.Lcd.print("Syncing time...");
    timeClient.forceUpdate();
    M5.Lcd.print("OK");
    delay(2000);
  }
  
  mqtt.setServer(MQTT_BROKER, 1883);
  if (WiFi.status() == WL_CONNECTED) {
    M5.Lcd.fillScreen(BLUE);
    M5.Lcd.setFreeFont(&FreeSans12pt7b);
    M5.Lcd.setCursor(20,40);
    M5.Lcd.print("Connecting to MQTT...");
    if (mqttConnect(true)) {
        M5.Lcd.print("OK");
        delay(1000);
    }
  }
}


void loop() {
  static time_t lastReading = 0, lastTimeUpdate = 0, mqttRetry = 0, lastMqttPublish = 0;
  static time_t wifiReconnect = millis() + (WIFI_RETRY_SECS * 1000);
  time_t epochTime = 0;
  static double lastObjectTemp = 0.0, lastAmbientTemp = 0.0;
  double objectTemp, ambientTemp;
  uint16_t color, warncolor;
  int16_t sfa30_hcho, sfa30_hum, sfa30_temp;

  if (millis() - lastReading > (READING_INTERVAL_SEC*1000)) {
    lastReading = millis(); 
    objectTemp = int(mlx.readObjectTempC() * 10) / 10.0;
    ambientTemp = int(mlx.readAmbientTempC() * 10) / 10.0;

    if (abs(objectTemp-lastObjectTemp) >= TEMP_PUBLISH_THRESHOLD || 
          abs(ambientTemp-lastAmbientTemp) >= TEMP_PUBLISH_THRESHOLD ||
          (millis() - lastMqttPublish) > (MQTT_PUBLISH_INTERVAL_SECS*1000)) {
      if (objectTemp <= TEMP_THRESHOLD_GREEN)
        color = BLUE;
      else if (objectTemp >= TEMP_THRESHOLD_RED)
        color = RED;
      else if (objectTemp >= TEMP_THRESHOLD_ORANGE)
        color = ORANGE;
      else if (objectTemp >= TEMP_THRESHOLD_GREEN)
        color = DARKGREEN;
  
      M5.Lcd.fillScreen(color);
      M5.Lcd.setTextColor(WHITE);
      M5.Lcd.setFreeFont(&FreeSansBold36pt7b);
      M5.Lcd.setCursor(65,110);
      M5.Lcd.print(objectTemp, 1);
      printDegree(color);
      M5.Lcd.print(" C");
      lastObjectTemp = objectTemp;
  
      M5.Lcd.setFreeFont(&FreeSans12pt7b);
      M5.Lcd.setCursor(90, 160);
      M5.Lcd.print("Ambiant: ");
      M5.Lcd.print(ambientTemp, 1);
      lastAmbientTemp = ambientTemp;
      Serial.print("MLX90614: objectTemperature(");
      Serial.print(objectTemp);
      Serial.print(" C), ambientTemperature(");
      Serial.print(ambientTemp);
      Serial.println(" C)");

      sfa30Error = SFA30.readMeasuredValues(sfa30_hcho, sfa30_hum, sfa30_temp);
      if (sfa30Error) {
        errorToString(sfa30Error, sfa30ErrorMsg, 256);
        Serial.printf("ERROR: Failed to read from sensor SFA30 (%s)\n", sfa30ErrorMsg);
      } else {
        Serial.print("SFA30: Formaldehyde(");
        Serial.print(sfa30_hcho / 5.0);
        Serial.print(" ppb), Humidity(");
        Serial.print(sfa30_hum / 100.0);
        Serial.print(" %), Temperature(");
        Serial.print(sfa30_temp / 200.0);
        Serial.println(" C)");
      }

      warncolor = color != RED ? RED : MAGENTA;
      if (WiFi.status() != WL_CONNECTED) {
        warncolor = color != RED ? RED : CYAN;
        M5.Lcd.setTextColor(WHITE);
        M5.Lcd.fillRect(0, 205, 320, 40, warncolor);
        M5.Lcd.setFreeFont(&FreeSans12pt7b);
        M5.Lcd.setCursor(65, 230);
        M5.Lcd.print("No WiFi connection");
      } else if (!mqttRetry || millis() > mqttRetry) {
        M5.Lcd.setTextColor(BLUE);
        M5.Lcd.fillRect(0, 205, 320, 40, WHITE);
        M5.Lcd.setFreeFont(&FreeSansBold12pt7b);
        M5.Lcd.setCursor(80, 230);
        M5.Lcd.print("MQTT publish");
        if (!mqttSend(objectTemp, ambientTemp, sfa30_hcho)) {
          mqttRetry = millis() + (MQTT_RETRY_SECS * 1000);
        } else {
          lastMqttPublish = millis();
          mqttRetry = 0;
          delay(500);
        }
      }
      
      if (mqttRetry > 0 && millis() < mqttRetry) {
        M5.Lcd.setTextColor(WHITE);
        M5.Lcd.fillRect(0, 205, 320, 40, warncolor);
        M5.Lcd.setFreeFont(&FreeSans12pt7b);
        M5.Lcd.setCursor(45, 230);
        M5.Lcd.printf("MQTT failed (error %d)", mqtt.state());
      }
    }
  }

  if (millis() - lastTimeUpdate >= 1000) {
    lastTimeUpdate = millis();
    if (WiFi.status() == WL_CONNECTED && !mqttRetry) {
      timeClient.update();
      epochTime = timeClient.getEpochTime();
      M5.Lcd.setTextColor(WHITE);
      M5.Lcd.fillRect(0, 205, 320, 40, BLUE);
      M5.Lcd.setFreeFont(&FreeSans12pt7b);
      M5.Lcd.setCursor(15, 230);
      M5.Lcd.print(getDateString(epochTime));
      M5.Lcd.setCursor(215, 230);
      M5.Lcd.print(getTimeString(epochTime));
    }
  }

  if (millis() > wifiReconnect && WiFi.status() != WL_CONNECTED) {
    wifiReconnect = millis() + (WIFI_RETRY_SECS * 1000);
    wifiConnect();
  }
    
}
