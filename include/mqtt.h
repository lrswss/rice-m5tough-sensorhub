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

#ifndef _MQTT_H
#define _MQTT_H

#include <Arduino.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include "bme680.h"
#include "sfa30.h"
#include "mlx90614.h"
#include "config.h"

#define MQTT_RETRY_SECS 10
#ifdef MEMORY_DEBUG_INTERVAL_SECS
extern UBaseType_t stackMqttPublishTask;
#endif

class MQTT {
    public:
        MQTT();
        void begin();
        bool queue(sensorReadings_t data);
        bool schedule();
        ~MQTT();
    private:
        bool connect(bool startup);
        bool publish(sensorReadings_t data);
        void publishTask();
        static void publishTaskWrapper(void* parameter);
        time_t lastPublished;
        PubSubClient mqtt;
        WiFiClient espClient;
        QueueHandle_t msgQueue;
        TaskHandle_t publishTaskHandle;
};

extern MQTT Publisher;
#endif