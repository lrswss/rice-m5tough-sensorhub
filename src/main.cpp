#include <Arduino.h>
#include <M5Tough.h>
#include <Adafruit_MLX90614.h>
#include <SensirionI2CSfa3x.h>
#include <EEPROM.h>
#include <bsec.h>
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

#define READING_INTERVAL_SEC 5
#define TEMP_THRESHOLD_RED 30.0
#define TEMP_THRESHOLD_ORANGE 25.0
#define TEMP_THRESHOLD_GREEN 18.0
#define TEMP_PUBLISH_THRESHOLD 0.2
#define HCHO_PUBLISH_TRESHOLD 1.0
#define GASRESISTANCE_PUBLISH_THRESHOLD 1000

#define MQTT_PUBLISH_INTERVAL_SECS 15
#define MQTT_BROKER "192.168.10.1"
#define MQTT_TOPIC  "m5tough/state"
#define MQTT_RETRY_SECS 10
//#define MQTT_USER "username"
//#define MQTT_PASS "password"

#define BME680_STATE_SAVE_PERIOD  UINT32_C(240 * 60 * 1000)  // every 4 hours

#define NTP_ADDRESS "de.pool.ntp.org"
#define NTP_UPDATE  1800  // interval in seconds between updates

#define SKETCH_NAME "TEP-RICE-M5Tough-SensorHub"
#define SKETCH_VER  "1.0b4"

// IR temparature sensor
Adafruit_MLX90614 mlx = Adafruit_MLX90614();
bool mlx90614_ready = false;

// SFA30 (Formaldehyd)
SensirionI2CSfa3x SFA30;
uint16_t sfa30Error;
char sfa30ErrorMsg[256];
bool sfa30_ready = false;

// BME680 (Temp, Hum, Pres, eCO2, VOC)
Bsec bme680;
static const std::string iaq_accurracy_verbose[4] = {
    "stabilizing",
    "uncertain",
    "calibrating",
    "calibrated"
};
static bsec_virtual_sensor_t sensorList[7] = {
    BSEC_OUTPUT_IAQ,
    BSEC_OUTPUT_CO2_EQUIVALENT,
    BSEC_OUTPUT_BREATH_VOC_EQUIVALENT,
    BSEC_OUTPUT_RAW_GAS,
    BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE,
    BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY,
    BSEC_OUTPUT_RUN_IN_STATUS
};
/* Configure the BSEC library with information about the sensor
    18v/33v = Voltage at Vdd. 1.8V or 3.3V
    3s/300s = BSEC operating mode, BSEC_SAMPLE_RATE_LP or BSEC_SAMPLE_RATE_ULP
    4d/28d = Operating age of the sensor in days
    generic_18v_3s_4d
    generic_18v_3s_28d
    generic_18v_300s_4d
    generic_18v_300s_28d
    generic_33v_3s_4d
    generic_33v_3s_28d
    generic_33v_300s_4d
    generic_33v_300s_28d
*/
const uint8_t bsec_config_iaq[] = {
#include "config/generic_33v_3s_4d/bsec_iaq.txt"  // LP sample rate, 4 days calibration backlog
};

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
      bme680.run();
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
      Serial.printf("WiFi: connected to %s with IP ", WiFi.SSID().c_str());
      Serial.println(WiFi.localIP());
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
      Serial.printf("WiFi: connection to SSID %s failed!\n", WIFI_SSID);
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
time_t tsDiff(time_t tsMillis) {
  if ((millis() - tsMillis) < 0)
    return (LONG_MAX - tsMillis + 1);
  else
    return (millis() - tsMillis);
}


// returns runtime in minutes
uint32_t getRuntimeMinutes() {
    static time_t lastMillis = 0;
    static uint32_t seconds = 0;

    seconds += tsDiff(lastMillis) / 1000;
    lastMillis = millis();
    return seconds/60;
}


bool bme680_status() {
  if (bme680.bsecStatus < BSEC_OK) {
    Serial.printf("ERROR: BSEC library returns code (%d)\n", bme680.bsecStatus);
    return false;
  } else if (bme680.bsecStatus > BSEC_OK) {
    Serial.printf("WARNING: BSEC library returns code (%d)\n", bme680.bsecStatus);
  }

  if (bme680.bme68xStatus < BME68X_OK) {
    Serial.printf("ERROR: sensor BME680 returns code (%d)\n", bme680.bme68xStatus);
    return false;
  } else if (bme680.bme68xStatus > BME68X_OK) {
      Serial.printf("WARNING: sensor BME68X returns code (%d)\n", bme680.bme68xStatus);
  }

  return true;
}


// try to publish sensor reedings with given timeout 
// will implicitly call mqtt_init()
bool mqttSend(double objectTemp, double ambientTemp, int hcho, int bme680Status,
        int iaq, int iaq_accuracy, int co2, float voc, int gas_resistance, int humidity) {

    StaticJsonDocument<256> JSON;
    static char topic[64], buf[208];

    JSON.clear();
    JSON["systemId"] = getSystemID();
    if (mlx90614_ready) {
        JSON["objectTemp"] = objectTemp;
        JSON["ambientTemp"] = ambientTemp;
    }
    if (!sfa30Error)
        JSON["hcho"] = hcho / 5.0;
    if (bme680_status()) {
        JSON["humidity"] = humidity; // 0-100%
        JSON["gasResistance"] = (gas_resistance/1000); // kOhms
        JSON["iaqAccuracy"] = iaq_accuracy; // 0-3
        if (iaq_accuracy >= 1) {
            JSON["iaq"] = iaq; // 0-500
            JSON["VOC"] = int(voc*10)/10; // ppm
            JSON["eCO2"] = co2; // ppm
        }
    }
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


bool bme680_loadState() {
  uint8_t bsecState[BSEC_MAX_STATE_BLOB_SIZE] = { 0 };

  if (EEPROM.read(0) == BSEC_MAX_STATE_BLOB_SIZE) {
    Serial.print("BME680: restore BSEC state from EEPROM (");
    for (uint8_t i = 0; i < BSEC_MAX_STATE_BLOB_SIZE; i++) {
      bsecState[i] = EEPROM.read(i + 1);
      Serial.print(bsecState[i], HEX);
    }
    Serial.print(")...");
    bme680.setState(bsecState);
    if (bme680_status()) {
        Serial.println("OK");
        return true;
    } else {
        Serial.println("failed!");
        return false;
    }
  } else {
    Serial.println("BME680: no valid BSEC state, erasing EEPROM");
    for (uint8_t i = 0; i < BSEC_MAX_STATE_BLOB_SIZE + 1; i++)
      EEPROM.write(i, 0);
    return EEPROM.commit();
  }
}

// Save current BSEC state to EEPROM if IAQ accuracy
// reaches 3 for the first time or peridically if
// BME680_STATE_SAVE_PERIOD has passed
bool bme680_updateState() {
  static time_t lastStateUpdate = 0;
  uint8_t bsecState[BSEC_MAX_STATE_BLOB_SIZE] = { 0 };

  if ((lastStateUpdate == 0 && bme680.iaqAccuracy >= 3) ||
        tsDiff(lastStateUpdate) >= BME680_STATE_SAVE_PERIOD) {

    bme680.getState(bsecState);
    if (bme680_status()) {
        Serial.print("BME680: writing BSEC state to EEPROM (");
        for (uint8_t i = 0; i < BSEC_MAX_STATE_BLOB_SIZE ; i++) {
            EEPROM.write(i + 1, bsecState[i]);
            Serial.print(bsecState[i], HEX);
        }
        Serial.print(")...");
        EEPROM.write(0, BSEC_MAX_STATE_BLOB_SIZE);
        if (EEPROM.commit()) {
            Serial.println("OK");
            lastStateUpdate = millis();
            return true;
        } else {
            Serial.println("failed!");
            return false;
        }
    } else {
        Serial.println("BME680: reading BSEC state failed!");
        return false;
    }
  }
  return false;
}


void setup() {
  bsec_version_t bsec_version;

  Serial.begin(115200);
  delay(500);
  Serial.printf("\nStarting Sketch %s v%s\n",SKETCH_NAME, SKETCH_VER);
  Serial.printf("Compiled on %s, %s\n", __DATE__, __TIME__);
  
  M5.begin(true, false, true, true, kMBusModeOutput);
  delay(500); // without sometimes ESP32 loops with exceptions on startup
  M5.Lcd.fillScreen(WHITE);
  M5.Lcd.pushImage(5, 70, logoWidth, logoHeight, logo);
  delay(3000);

  M5.Lcd.fillScreen(BLUE);
  M5.Lcd.setFreeFont(&FreeSans24pt7b);
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.setCursor(45, 80);
  M5.Lcd.print("Sensor Hub");
  M5.Lcd.setFreeFont(&FreeSans12pt7b);
  M5.Lcd.setCursor(12,125);
  M5.Lcd.print("MLX90614/SFA30/BME680");
  M5.Lcd.setFreeFont(&FreeSans8pt7b);
  M5.Lcd.setCursor(110,160);
  M5.Lcd.printf("Firmware %s", SKETCH_VER);

  if (!mlx.begin()) {
    M5.Lcd.fillRect(0, 205, 320, 40, RED);
    M5.Lcd.setFreeFont(&FreeSans12pt7b);
    M5.Lcd.setCursor(55, 230);
    M5.Lcd.print("MLX90614 failed!");
    Serial.println("ERROR: Failed to detect sensor MLX90614");
    delay(1000);
  } else {
    M5.Lcd.fillRect(0, 205, 320, 40, DARKGREEN);
    M5.Lcd.setFreeFont(&FreeSans12pt7b);
    M5.Lcd.setCursor(30, 230);
    M5.Lcd.print("Sensor MLX90614 ready");
    Serial.print("Sensor MLX90614 ready, emissivity ");
    Serial.println(mlx.readEmissivity());
    mlx90614_ready = true;
  }
  delay(2000);

  SFA30.begin(Wire);
  sfa30Error = SFA30.startContinuousMeasurement();
  if (sfa30Error) {
    M5.Lcd.fillRect(0, 205, 320, 40, RED);
    M5.Lcd.setFreeFont(&FreeSans12pt7b);
    M5.Lcd.setCursor(30, 230);
    M5.Lcd.print("SFA30 failed, error ");
    M5.Lcd.print(sfa30Error);
    errorToString(sfa30Error, sfa30ErrorMsg, 256);
    Serial.printf("ERROR: Failed to initialize sensor SFA30 (%s)\n", sfa30ErrorMsg);
    delay(1000);
  } else {
    M5.Lcd.fillRect(0, 205, 320, 40, DARKGREEN);
    M5.Lcd.setFreeFont(&FreeSans12pt7b);
    M5.Lcd.setCursor(50, 230);
    M5.Lcd.print("Sensor SFA30 ready");
    Serial.println("Sensor SFA30 ready, starting continuous measurement");
    sfa30_ready = true;
  }
  delay(2000);

  EEPROM.begin(BSEC_MAX_STATE_BLOB_SIZE + 1);
  bme680.begin(BME68X_I2C_ADDR_LOW, Wire);
  if (bme680_status()) {
    bme680.setConfig(bsec_config_iaq);
    if (!bme680_status()) {
      Serial.println("ERROR: Failed to set BME680 configuration");
    } else {
      bme680_loadState();
      bme680.updateSubscription(sensorList, 7, BSEC_SAMPLE_RATE_LP); // see bsec_config_iaq[]
      if (!bme680_status())
        Serial.println("ERROR: Failed to subscribe to BME680 sensors");
    }
  }
  if (!bme680_status()) {
    M5.Lcd.fillRect(0, 205, 320, 40, RED);
    M5.Lcd.setFreeFont(&FreeSans12pt7b);
    M5.Lcd.setCursor(20, 230);
    M5.Lcd.print("BME680 failed, error ");
    M5.Lcd.print(bme680.bme68xStatus);
    Serial.printf("ERROR: Failed to initialize sensor BME680");
    delay(1000);
  } else {
    M5.Lcd.fillRect(0, 205, 320, 40, DARKGREEN);
    M5.Lcd.setFreeFont(&FreeSans12pt7b);
    M5.Lcd.setCursor(40, 230);
    M5.Lcd.print("Sensor BME680 ready");
    bsec_get_version(&bsec_version);
    Serial.printf("Sensor BME680 ready, sample rate 3s, BSEC v%d.%d.%d.%d\n", bsec_version.major,
        bsec_version.minor, bsec_version.major_bugfix, bsec_version.minor_bugfix);
  }
  delay(2000);

  wifiConnect();
  timeClient.begin();
  if (WiFi.status() == WL_CONNECTED) {
    M5.Lcd.fillScreen(BLUE);
    M5.Lcd.setFreeFont(&FreeSans12pt7b);
    M5.Lcd.setCursor(20,40);
    M5.Lcd.print("Syncing time...");
    Serial.printf("Syncing time with NTP server %s...", NTP_ADDRESS);
    timeClient.forceUpdate();
    M5.Lcd.print("OK");
    Serial.println("OK");
    delay(1000);
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
  static double mlxLastObjectTemp = 0.0, mlxLastAmbientTemp = 0.0;
  double mlxObjectTemp = 0.0, mlxAmbientTemp = 0.0;
  uint16_t color, warncolor;
  int16_t sfa30HCHO = 0, sfa30LastHCHO = 0, sfa30Humdity = 0, sfa30Temp = 0;
  float bme680LastGasResistance = 0.0, bme680LastTemp = 0.0, bme680LastIAQ = 0.0;

  bme680.run();  // calculates readings every 3 seconds
  if (millis() - lastReading > (READING_INTERVAL_SEC * 1000)) {
    lastReading = millis();

    if (mlx90614_ready) {
        mlxObjectTemp = int(mlx.readObjectTempC() * 10) / 10.0;
        mlxAmbientTemp = int(mlx.readAmbientTempC() * 10) / 10.0;
    }
    if (sfa30_ready) {
        sfa30Error = SFA30.readMeasuredValues(sfa30HCHO, sfa30Humdity, sfa30Temp);
    }

    if ((mlxObjectTemp != NAN && abs(mlxObjectTemp - mlxLastObjectTemp) >= TEMP_PUBLISH_THRESHOLD) ||
          (mlxAmbientTemp != NAN && abs(mlxAmbientTemp - mlxLastAmbientTemp) >= TEMP_PUBLISH_THRESHOLD) ||
          (!sfa30Error && abs(sfa30HCHO - sfa30LastHCHO) >= HCHO_PUBLISH_TRESHOLD) ||
          (bme680_status() && (abs(bme680.temperature - bme680LastTemp) >= TEMP_PUBLISH_THRESHOLD ||
          abs(bme680.gasResistance - bme680LastGasResistance) >= GASRESISTANCE_PUBLISH_THRESHOLD)) ||
          (millis() - lastMqttPublish) > (MQTT_PUBLISH_INTERVAL_SECS * 1000)) {

      // TODO: change display color on thresholds for IAQ/HCHO
      if (mlxObjectTemp <= TEMP_THRESHOLD_GREEN)
        color = BLUE;
      else if (mlxObjectTemp >= TEMP_THRESHOLD_RED)
        color = RED;
      else if (mlxObjectTemp >= TEMP_THRESHOLD_ORANGE)
        color = ORANGE;
      else if (mlxObjectTemp >= TEMP_THRESHOLD_GREEN)
        color = DARKGREEN;
  
      M5.Lcd.fillScreen(color);
      M5.Lcd.setTextColor(WHITE);
      M5.Lcd.setFreeFont(&FreeSansBold36pt7b);
      M5.Lcd.setCursor(65,80);
      if (mlxObjectTemp == NAN) {
        M5.Lcd.print("--.-");
      } else {
        M5.Lcd.print(mlxObjectTemp, 1);
        mlxLastObjectTemp = mlxObjectTemp;
      }
      printDegree(color);
      M5.Lcd.print(" C");
  
      M5.Lcd.setFreeFont(&FreeSans12pt7b);
      M5.Lcd.setCursor(20, 120);
      M5.Lcd.print("Ambiant: ");
      if (!bme680_status()) { // prefer BME680 for ambient temperature reading
        if (mlxAmbientTemp == NAN) {
          M5.Lcd.print("--.-");
        } else {
          M5.Lcd.print(mlxAmbientTemp, 1);
          mlxLastAmbientTemp = mlxAmbientTemp;
        }
      } else {
        M5.Lcd.print(bme680.temperature, 1);
        mlxLastAmbientTemp = bme680.temperature;
      }

      Serial.print("MLX90614: objectTemperature(");
      Serial.print(mlxObjectTemp, 1);
      Serial.print(" C), ambientTemperature(");
      Serial.print(mlxAmbientTemp, 1);
      Serial.println(" C)");

      if (sfa30Error) {
        M5.Lcd.setCursor(170, 120);
        M5.Lcd.print("Humidity: ");
        if (bme680_status())
            M5.Lcd.print(bme680.humidity, 0);
        else
            M5.Lcd.print("--");
        M5.Lcd.setCursor(20, 150);
        M5.Lcd.print("HCHO: --.-");
      } else {
        Serial.print("SFA30: Formaldehyde(");
        Serial.print(sfa30HCHO / 5.0, 1);
        Serial.print(" ppb), Humidity(");
        Serial.print(sfa30Humdity / 100.0, 0);
        Serial.print(" %), Temperature(");
        Serial.print(sfa30Temp / 200.0, 1);
        Serial.println(" C)");
        M5.Lcd.setCursor(170, 120);
        M5.Lcd.print("Humidity: ");
        if (!bme680_status()) // prefer BME680 humidity reading
            M5.Lcd.print(sfa30Humdity / 100.0, 0);
        else
            M5.Lcd.print(bme680.humidity, 0);
        M5.Lcd.setCursor(20, 150);
        M5.Lcd.print("HCHO: ");
        M5.Lcd.print(sfa30HCHO / 5.0, 1);
        sfa30LastHCHO = sfa30HCHO;
      }

      if (bme680_status()) {
        Serial.print("BME680: ");
        if (bme680.runInStatus > 0) {
            M5.Lcd.print(" VOC: ");  // shown right after HCHO on display
            M5.Lcd.print(bme680.breathVocEquivalent, 1);
            M5.Lcd.print("/");
            M5.Lcd.print(bme680.breathVocAccuracy);
            M5.Lcd.setCursor(20, 180); // new line on display with IAQ/eCO2 readings
            M5.Lcd.print("IAQ: ");
            M5.Lcd.print(int(bme680.iaq));
            M5.Lcd.print("/");
            M5.Lcd.print(int(bme680.iaqAccuracy));
            M5.Lcd.print(" eCO2: ");
            M5.Lcd.print(int(bme680.co2Equivalent));
            M5.Lcd.print("/");
            M5.Lcd.print(int(bme680.co2Accuracy));

            Serial.printf("IAQ(%d, %s), eCO2(%d ppm), VOC(",
                int(bme680.iaq), iaq_accurracy_verbose[int(bme680.iaqAccuracy)].c_str(),
                int(bme680.co2Equivalent));
            Serial.print(bme680.breathVocEquivalent, 1);
            Serial.print(" ppm), ");
        } else {
            M5.Lcd.print(" VOC: warmup"); // placed on display after HCHO reading
            M5.Lcd.setCursor(20, 180);
            M5.Lcd.print("IAQ/eCO2: warmup");
            Serial.print("gas sensor warmup, ");
        }
        Serial.printf("Gas(%d kOhm), Humdity(%d %%), Temperature(",
            int(bme680.gasResistance/1000.0), int(bme680.humidity));
        Serial.print(bme680.temperature, 1);
        Serial.println(" C)");
        bme680LastIAQ = bme680.iaq;
        bme680LastTemp = bme680.temperature;
        bme680LastGasResistance = bme680.gasResistance;
        bme680_updateState();
      } else {
        M5.Lcd.print(" VOC: --.-"); // after HCHO
        M5.Lcd.setCursor(20, 180);
        M5.Lcd.print("IAQ: --/- eCO2: ---");
        Serial.printf("ERROR: Failed to read from sensor BME680 with error %d\n", bme680.bme68xStatus);
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
        if (!mqttSend(mlxObjectTemp, mlxAmbientTemp, sfa30HCHO, int(bme680.runInStatus),
            int(bme680.iaq), bme680.iaqAccuracy, int(bme680.co2Equivalent),
            bme680.breathVocEquivalent, int(bme680.gasResistance), int(bme680.humidity))) {
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
