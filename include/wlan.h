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

#ifndef _WLAN_H
#define _WLAN_H

#include <Arduino.h>
#include <M5Tough.h>
#include <WiFi.h>

#define WIFI_WAIT_SECS  10
#define WIFI_RETRY_SECS 30

void wifi_connect();
void wifi_reconnect();

#endif