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

#define BME680_STATE_SAVE_PERIOD  UINT32_C(120 * 60 * 1000)  // every 2 hours

#include <Arduino.h>
#include <M5Tough.h>
#include <Adafruit_MLX90614.h>
#include <SensirionI2CSfa3x.h>
#include <bsec.h>

typedef struct {
    float mlxObjectTemp;
    float mlxAmbientTemp;
    float sfa30HCHO;
    float sfa30Temp;
    uint8_t sfa30Hum;
    float bme680Temp;
    uint8_t bme680Hum;
    uint16_t bme680Iaq;
    uint8_t bme680IaqAccuracy;
    uint16_t bme680GasResistance;
    uint16_t bme680eCO2;
    float bme680VOC;
} sensorReadings_t;

extern sensorReadings_t sensors;

bool mlx90614_init();
bool mlx90614_read();
bool mlx90614_status();
bool mlx90614_changed();
void mlx90614_display();

bool sfa30_init();
bool sfa30_read();
bool sfa30_status();
bool sfa30_changed();
void sfa30_display();

bool bme680_init();
bool bme680_read();
uint8_t bme680_status();
const char* bme680_accuracy();
bool bme680_changed();
void bme680_display();

#endif