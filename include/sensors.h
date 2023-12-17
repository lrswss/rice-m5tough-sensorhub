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

#ifndef _SENSORS_H
#define _SENSORS_H

#include <Arduino.h>
#include "bme680.h"
#include "sfa30.h"
#include "mlx90614.h"

#define BME680_STATE_SAVE_PERIOD  UINT32_C(120 * 60 * 1000)  // every 2 hours

typedef struct {
    float mlxObjectTemp;
    float mlxAmbientTemp;
    float sfa30HCHO; // 0-1000 ppb
    float sfa30Temp;
    uint8_t sfa30Hum;
    float bme680Temp;
    uint8_t bme680Hum;
    uint16_t bme680Iaq; // 0-500
    uint8_t bme680IaqAccuracy; // 0-3
    uint16_t bme680GasResistance; // kOhm
    uint16_t bme680eCO2; // 400–2000 ppm
    float bme680VOC; // 0.13–2.5 ppm
} sensorReadings_t;

extern sensorReadings_t sensors;

void sensors_init();

#endif