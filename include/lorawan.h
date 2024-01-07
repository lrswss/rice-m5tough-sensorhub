/***************************************************************************
  Copyright (c) 2023 Lars Wessels
  
  This file a part of the "RICE-M5Tough-SensorHub" source code.
  https://github.com/lrswss/rice-m5tough-sensorhub

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

#ifndef _LORAWAN_H
#define _LORAWAN_H

#include <Arduino.h>
#include <CayenneLPP.h>
#include "freertos/queue.h"
#include "bme680.h"
#include "sfa30.h"
#include "mlx90614.h"
#include "utils.h"

#define LORAWAN_MODULE_TIMEOUT_SECS 5
#define LORAWAN_COMMAND_TIMEOUT_MS 1000
#define LORAWAN_JOIN_TIMEOUT_SECS 20
#define LORAWAN_JOIN_RETRY_SECS 180
#define LORAWAN_LPP_SIZE 64
//#define LORAWAN_DEBUG_SERIAL_CMDS

// ensure exclusive access to serial port
#define SEMAPHORE_BLOCKTIME_MS 2000/portTICK_PERIOD_MS

enum lorawanState {
    NONE = 0,
    IDLE,
    COMMAND,
    JOINING,
    JOINFAIL,
    JOINED,
    SENDING,
    ERROR
};

class ASR6501 {
    public:
        ASR6501();
        ASR6501(HardwareSerial* serialPort, uint8_t rxPin, uint8_t txPin);
        bool begin(HardwareSerial* serialPort, uint8_t rxPin, uint8_t txPin);
        bool queue(sensorReadings_t data);
        lorawanState status();
        ~ASR6501();
    private:
        bool connected();
        void join();
        bool joined();
        const char* getDevEUI();
        bool setOTAA(const char* appEUI, const char* appKey);
        bool setupFailed(const char* msg);
        void joinTask();
        static void joinTaskWrapper(void* parameter);
        void queueTask();
        static void queueTaskWrapper(void* parameter);
        const char* sendCmd(const char* cmd);
        const char* sendCmd(const char* cmd, uint16_t timeout);
        const char* encodeLPP(sensorReadings_t sensors);
        HardwareSerial *serial;
        lorawanState deviceState;
        SemaphoreHandle_t SerialLock;
        QueueHandle_t msgQueue;
        TaskHandle_t joinTaskHandle, queueTaskHandle;
};

extern ASR6501 LoRaWAN;
#ifdef MEMORY_DEBUG_INTERVAL_SECS
extern UBaseType_t stackWmJoinTask, stackWmQueueTask;
#endif

#endif