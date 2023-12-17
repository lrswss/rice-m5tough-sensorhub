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

#ifndef _SFA30_H
#define _SFA30_H

#include <Arduino.h>
#include <M5Tough.h>
#include <SensirionI2CSfa3x.h>

bool sfa30_init();
bool sfa30_read();
bool sfa30_status();
bool sfa30_changed();
void sfa30_display();
void sfa30_console();

#endif