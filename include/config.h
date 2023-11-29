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

#ifndef _CONFIG_H
#define _CONFIG_H

#define WIFI_SSID "XXXXXXXXX"
#define WIFI_PASS "XXXXXXXXX"

#define MQTT_PUBLISH_INTERVAL_SECS 15
#define MQTT_BROKER_HOST "192.168.10.1"
#define MQTT_BROKER_PORT 1883
#define MQTT_TOPIC  "m5tough/state"
//#define MQTT_USER "username"
//#define MQTT_PASS "password"

#define READING_INTERVAL_SEC 5
#define TEMP_THRESHOLD_RED 30.0
#define TEMP_THRESHOLD_ORANGE 25.0
#define TEMP_THRESHOLD_GREEN 18.0
#define TEMP_PUBLISH_THRESHOLD 0.2
#define HCHO_PUBLISH_TRESHOLD 1.0
#define GASRESISTANCE_PUBLISH_THRESHOLD 5

#define SKETCH_NAME "TEP-RICE-M5Tough-SensorHub"
#define SKETCH_VER  "1.1b2"

// clear NVS if new firmware is detected
#define CLEAR_NVS_ON_UPDATE

#endif