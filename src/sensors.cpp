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

#include "config.h"
#include "bme680.h"
#include "sfa30.h"
#include "mlx90614.h"
#include "utils.h"
#include "prefs.h"
#include "display.h"
#include "sensors.h"

sensorReadings_t readings;

void Sensors::init() {
    mlx90614.setup();
    sfa30.setup();
    bme680.setup();
}