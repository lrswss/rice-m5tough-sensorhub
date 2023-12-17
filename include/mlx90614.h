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

#ifndef _MLX90614_H
#define _MLX90614_H

#include <Arduino.h>
#include <M5Tough.h>
#include <Adafruit_MLX90614.h>

bool mlx90614_init();
bool mlx90614_read();
bool mlx90614_status();
bool mlx90614_changed();
void mlx90614_display();
void mlx90614_console();

#endif