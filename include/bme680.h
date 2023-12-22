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

#ifndef _BME680_H
#define _BME680_H

#include <Arduino.h>
#include <M5Tough.h>
#include <bsec.h>
#include "sensors.h"

#define BME680_STATE_SAVE_PERIOD  UINT32_C(120 * 60 * 1000)  // every 2 hours

class BME680 : public Sensors {
    public:
        BME680();
        bool setup();
        bool read();
        uint8_t status();
        bool changed();
        void display();
        void console();
    private:
        Bsec bsec;
        bool ready;
        bool error;
        static const char* accuracy(uint8_t data);
        static void dialogResetBSEC();
        static bool updateState(Bsec bsec);
        static void loadState(Bsec bsec);
        static uint8_t status(Bsec bsec);
        static void eventResetBSEC(Event& e);
};

extern BME680 bme680;
#endif