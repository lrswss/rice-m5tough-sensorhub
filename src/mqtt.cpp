/***************************************************************************
  Copyright (c) 2023 Lars Wessels

  This file a part of the "RICE-M5Tough-SensorHub" source code.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

  http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

***************************************************************************/

#include "mqtt.h"
#include "wlan.h"
#include "rtc.h"
#include "sensors.h"
#include "utils.h"
#include "config.h"

// setup wifi and mqtt client
WiFiClient espClient;
PubSubClient mqtt(espClient);

// connect to mqtt broker with random client id
bool mqtt_connect(bool startup) {
  static char clientid[16];
  
  if (!mqtt.connected()) {
    // generate pseudo random client id
    snprintf(clientid, sizeof(clientid), "M5Tough_%lx", random(0xffff));
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


void mqtt_init() {
    mqtt.setServer(MQTT_BROKER, 1883);
    if (WiFi.status() == WL_CONNECTED) {
        M5.Lcd.fillScreen(BLUE);
        M5.Lcd.setFreeFont(&FreeSans12pt7b);
        M5.Lcd.setCursor(20,40);
        M5.Lcd.print("Connecting to MQTT...");
        if (mqtt_connect(true)) {
            M5.Lcd.print("OK");
            delay(1000);
        }
    }
}


int mqtt_state() {
    return mqtt.state();
}


// try to publish sensor reedings
bool mqtt_publish() {
    StaticJsonDocument<256> JSON;
    static char topic[64], buf[208];

    JSON.clear();
    JSON["systemId"] = getSystemID();
    if (mlx90614_status()) {
        JSON["objectTemp"] = int(sensors.mlxObjectTemp*10)/10.0;
        JSON["ambientTemp"] = int(sensors.mlxAmbientTemp*10)/10.0;
    }
    if (sfa30_status())
        JSON["hcho"] = int(sensors.sfa30HCHO*10)/10.0;
    if (bme680_status()) {
        JSON["humidity"] = sensors.bme680Hum; // 0-100%
        JSON["gasResistance"] = sensors.bme680GasResistance; // kOhms
        JSON["iaqAccuracy"] = sensors.bme680IaqAccuracy; // 0-3
        if (sensors.bme680IaqAccuracy >= 1) {
            JSON["iaq"] = sensors.bme680Iaq; // 0-500
            JSON["VOC"] = int(sensors.bme680VOC*10)/10.0; // ppm
            JSON["eCO2"] = sensors.bme680eCO2; // ppm
        }
    }
    JSON["rssi"] = WiFi.RSSI();
    JSON["runtime"] = getRuntimeMinutes();
    JSON["version"] = SKETCH_VER;

    memset(buf, 0, sizeof(buf));
    size_t s = serializeJson(JSON, buf);
    if (mqtt_connect(false)) {
        snprintf(topic, sizeof(topic)-1, "%s", MQTT_TOPIC);
        if (mqtt.publish(topic, buf, s)) {
            Serial.printf("MQTT: published %d bytes to %s on %s\n", s, MQTT_TOPIC, MQTT_BROKER);
            return true;
        }
    }
    Serial.printf("MQTT: failed to publish to %s on %s (error %d)\n", MQTT_TOPIC, MQTT_BROKER, mqtt.state());
    return false;
}