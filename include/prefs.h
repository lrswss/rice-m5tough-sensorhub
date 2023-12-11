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

#ifndef _PREFS_H
#define _PREFS_H

#include <Arduino.h>
#include <Preferences.h>
#include "bsec.h"

#define PARAMETER_SIZE 32

extern Preferences nvs;

typedef struct {
    uint8_t bsecState[BSEC_MAX_STATE_BLOB_SIZE+1];
    uint16_t readingsIntervalSecs;
    char mqttBroker[PARAMETER_SIZE+1];
    uint16_t mqttBrokerPort;
    char mqttTopic[PARAMETER_SIZE+1];
    uint16_t mqttIntervalSecs;
    bool mqttEnableAuth;
    char mqttUsername[PARAMETER_SIZE+1];
    char mqttPassword[PARAMETER_SIZE+1];
    char ntpServer[PARAMETER_SIZE+1];
    bool bleServer;
    bool lorawanEnable;
    char lorawanAppEUI[17];
    char lorawanAppKey[33];
    uint16_t lorawanIntervalSecs;
    bool lorawanConfirm;
    bool clearNVSUpdate;
    uint8_t sha256[32];
} appPrefs_t;

extern Preferences nvs;
extern appPrefs_t prefs;

void startPrefs();
void savePrefs(bool restart);

#endif