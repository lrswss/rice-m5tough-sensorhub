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
#include "utils.h"
#include "prefs.h"
#include "config.h"
#include "lorawan.h"
#include "display.h"

MQTT Publisher;
#ifdef MEMORY_DEBUG_INTERVAL_SECS
UBaseType_t stackMqttPublishTask;
#endif


MQTT::MQTT() {
    this->msgQueue = xQueueCreate(1, sizeof(sensorReadings_t));
    this->mqtt.setClient(this->espClient);
    this->lastPublished = 0;
}


MQTT::~MQTT() {
    vQueueDelete(this->msgQueue);
    vTaskDelete(this->publishTaskHandle);
    this->mqtt.~PubSubClient();
}


// connect to MQTT broker with random client id
bool MQTT::connect(bool startup) {
  static char clientid[16];
  
  if (!mqtt.connected()) {
    // generate pseudo random client id
    snprintf(clientid, sizeof(clientid), "M5Tough_%lx", random(0xffff));
    Serial.printf("MQTT: connecting to MQTT Broker %s as %s...", prefs.mqttBroker, clientid);

#ifdef MQTT_USER
    if (mqtt.connect(clientid, prefs.mqttUsername, prefs.mqttPassword)) {
#else
    if (mqtt.connect(clientid)) {
#endif
      Serial.println("OK");
      return true;

    } else {
      Serial.printf("failed (error %d)\n", mqtt.state());
      if (startup) {
        M5.Lcd.clearDisplay(RED);
        M5.Lcd.setTextColor(WHITE);
        M5.Lcd.setCursor(20,70);
        M5.Lcd.print("Failed to connect to MQTT");
        M5.Lcd.setTextDatum(MC_DATUM);
        M5.Lcd.drawString(prefs.mqttBroker, 160, 120, 4);
        delay(5000);
      }
      return false;
    }
  }

  return true;
}


// setup MQTT client and try to connect to MQTT broker
void MQTT::begin() {
    mqtt.setServer(prefs.mqttBroker, prefs.mqttBrokerPort);
    mqtt.setBufferSize(384);
    if (WiFi.isConnected()) {
        M5.Lcd.clearDisplay(BLUE);
        M5.Lcd.setTextColor(WHITE);
        M5.Lcd.setFreeFont(&FreeSans12pt7b);
        M5.Lcd.setCursor(20,40);
        M5.Lcd.print("Connecting to MQTT...");
        if (this->connect(true)) {
            M5.Lcd.print("OK");
            delay(1500);
        }

        // start checking the MQTT message queue to publish sensor readings
        xTaskCreatePinnedToCore(this->publishTaskWrapper,
            "mqttTask", 3072, this, 10, &this->publishTaskHandle, 0);
    }
}


// returns true if sensor readings have been published
bool MQTT::publish(sensorReadings_t data) {
    StaticJsonDocument<384> JSON;
    static char topic[64], buf[336], statusMsg[32];

    if (!WiFi.isConnected()) {
        Serial.println("MQTT: publish skipped, no WiFi uplink");
        return false;
    }

    JSON.clear();
    JSON["systemId"] = getSystemID();
    if (mlx90614.status()) {
        JSON["objectTemp"] = int(data.mlxObjectTemp*10)/10.0;
        JSON["ambientTemp"] = int(data.mlxAmbientTemp*10)/10.0;
    }
    if (sfa30.status())
        JSON["hcho"] = int(data.sfa30HCHO*10)/10.0;
    if (bme680.status()) {
        JSON["humidity"] = data.bme680Hum; // 0-100%
        JSON["gasResistance"] = data.bme680GasResistance; // kOhms
        JSON["iaqAccuracy"] = data.bme680IaqAccuracy; // 0-3
        if (data.bme680IaqAccuracy >= 1) {
            JSON["iaq"] = data.bme680Iaq; // 0-500
            JSON["VOC"] = int(data.bme680VOC*10)/10.0; // ppm
            JSON["eCO2"] = data.bme680eCO2; // ppm
        }
    }
    JSON["rssi"] = WiFi.RSSI();
    JSON["wifiCons"] = wifiReconnectSuccess + wifiReconnectFail;
    if (M5.Axp.GetBatVoltage() >= 1.0) {
        JSON["batLevel"] = int(M5.Axp.GetBatteryLevel());
        JSON["usbPower"] = usbPowered() ? 1 : 0;
    }
    JSON["runtime"] = getRuntimeMinutes();

#ifdef MEMORY_DEBUG_INTERVAL_SECS
    JSON["heap"] = ESP.getFreeHeap();
    JSON["joinTask"] = stackWmJoinTask;
    JSON["queueTask"] = stackWmQueueTask;
    JSON["wifiTask"] = stackWmWifiTask;
    JSON["ntpTask"] = stackWmNtpTask;
    JSON["mqttTask"] = stackMqttPublishTask;
#endif
    JSON["version"] = FIRMWARE_VERSION;

    memset(buf, 0, sizeof(buf));
    size_t s = serializeJson(JSON, buf);
    if (this->connect(false)) {
        snprintf(topic, sizeof(topic)-1, "%s", MQTT_TOPIC);
        if (mqtt.publish(topic, buf, s)) {
            Serial.printf("MQTT: published %d bytes to %s on %s\n", s,
                prefs.mqttTopic, prefs.mqttBroker);
            queueStatusMsg("MQTT publish", 80, false);
            return true;
        }
    }

    return false;
}


// returns true if waiting time to queue a new message has been reached
bool MQTT::schedule() {
    return (tsDiff(lastPublished) > (prefs.mqttIntervalSecs * 1000));
}


// background task to publish queued messages
void MQTT::publishTask() {
    time_t mqttRetryTime = 0;
    static char statusMsg[32];
#ifdef MEMORY_DEBUG_INTERVAL_SECS
    uint16_t loopCounter = 0;
#endif

    while (true) {
        if ((mqttRetryTime <= millis()) && (xQueueReceive(this->msgQueue, &readings, 0) == pdTRUE)) {
            if (!this->publish(readings)) {
                Serial.printf("MQTT: failed to publish to %s on %s (error %d), retry in %d secs\n",
                    prefs.mqttTopic, prefs.mqttBroker, mqtt.state(), MQTT_RETRY_SECS);
                snprintf(statusMsg, sizeof(statusMsg), "MQTT failed (error %d)", mqtt.state());
                queueStatusMsg(statusMsg, 40, true);
                mqttRetryTime = millis() + (MQTT_RETRY_SECS * 1000);
            } else {
                this->lastPublished = millis();
                mqttRetryTime = 0;
            }
        }
#ifdef MEMORY_DEBUG_INTERVAL_SECS
        if (loopCounter++ >= MEMORY_DEBUG_INTERVAL_SECS) {
            stackMqttPublishTask = printFreeStackWatermark("mqttTask");
            loopCounter = 0;
        }
#endif
        if (lowBattery)
            vTaskDelete(NULL);
        vTaskDelay(1000/portTICK_PERIOD_MS);
    }
}


void MQTT::publishTaskWrapper(void* _this) {
    static_cast<MQTT*>(_this)->publishTask();
}


// place current sensor reading in MQTT publish queue
bool MQTT::queue(sensorReadings_t data) {
    Serial.println("MQTT: queuing sensor data");
    return (xQueueOverwrite(this->msgQueue, (void*)&data) == pdTRUE);
}