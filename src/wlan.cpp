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
#include "wlan.h"
#include "utils.h"
#include "prefs.h"
#include "display.h"

static bool updateSettings = false;
static bool endButtonWaitLoop = false;
static char ssid[32], psk[32];
static bool portalTimedOut = false;

uint16_t wifiReconnectFail = 0, wifiReconnectSuccess = 0;
#ifdef MEMORY_DEBUG_INTERVAL_SECS
UBaseType_t stackWmWifiTask;
#endif


// show message on display on failed WiFi connection attempt
static void connectionFailed(const char* apname) {
    if (!strlen(apname) || portalTimedOut)
        return;
    M5.Lcd.clearDisplay(RED);
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.setCursor(30, 55);
    M5.Lcd.print("Failed to connect to SSID");
    M5.Lcd.setTextDatum(MC_DATUM);
    M5.Lcd.drawString(apname, 160, 120, 4);
    Serial.printf("WiFi: connection to SSID %s failed\n", apname);
}


// show message on display with details on established WiFi connection
static void connectionSuccess(bool success) {
    if (!success) {
        M5.Lcd.clearDisplay(BLUE);
        M5.Lcd.setTextColor(WHITE);
        M5.Lcd.setFreeFont(&FreeSans12pt7b);
        M5.Lcd.setCursor(20, 40);
        M5.Lcd.print("Connecting to WiFi...");
    } else {
        M5.Lcd.print("OK");
        vTaskDelay(500/portTICK_PERIOD_MS);
        M5.Lcd.setCursor(20, 100);
        M5.Lcd.print("WLAN: ");
        M5.Lcd.println(WiFi.SSID());
        M5.Lcd.setCursor(20, 130);
        M5.Lcd.print("RSSI: ");
        M5.Lcd.print(WiFi.RSSI());
        M5.Lcd.println(" dBm");
        M5.Lcd.setCursor(20, 160);
        M5.lcd.print("IP: ");
        M5.lcd.println(WiFi.localIP());
        Serial.print("WiFi: connected with IP ");
        Serial.print(WiFi.localIP());
        Serial.printf(" (RSSI %d dbm)\n", WiFi.RSSI());
    }
}


// create a random numeric password
// new password is generated after restart/powerup
static char* randomPassword() {
    static char pass[8] = { 0 };

    if (!strlen(pass))
        sprintf(pass, "%ld", random(10000000,99999999));
    return pass;
}


// callback function
// show message on display with details on how to connect to config portal
static void startConfigPortal(WiFiManager *wm) {
    if (!endButtonWaitLoop && wm->getWiFiSSID().length() > 0) {
        connectionFailed(wm->getWiFiSSID().c_str());
        delay(3000);
    }
    M5.Lcd.clearDisplay(BLUE);
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.setFreeFont(&FreeSans12pt7b);
    M5.Lcd.setCursor(20, 40);
    M5.Lcd.print("Start setup portal...");
    M5.Lcd.setCursor(20, 100);
    M5.Lcd.printf("WLAN: %s", wm->getConfigPortalSSID().c_str());
    M5.Lcd.setCursor(20, 130);
    M5.Lcd.printf("Password: %s", randomPassword());
    M5.Lcd.setCursor(20, 160);
    M5.Lcd.print("IP: 192.168.4.1");
    Serial.printf("WiFiManager: start setup portal on SSID %s...\n", wm->getConfigPortalSSID().c_str());
}


// callback function
// show message on display if config portal timeout was reached
static void portalTimeout() {
    M5.Lcd.clearDisplay(RED);
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.setFreeFont(&FreeSans12pt7b);
    M5.Lcd.setCursor(50, 75);
    M5.Lcd.print("Setup portal timeout");
    Serial.println("WiFiManager: setup portal timeout");
    portalTimedOut = true;
    startWatchdog();
    delay(3000);
}


// callback function
// triggers savePrefs() in wifi_manager()
static void saveSettings() {
    updateSettings = true;
    Serial.println("WiFiManager: save settings");
}


// background task to periodically reconnect to WiFi if disconnected
static void wifiConnectionTask(void* parameter) {
    static char statusMsg[32];
    uint8_t wifiTimeout = 0, loopStatusMsg = 0;
    time_t wifiReconnect = 0;
#ifdef MEMORY_DEBUG_INTERVAL_SECS
    uint8_t loopCounter = 0;
#endif

    while (true) {
        if ((strlen(ssid) > 0) && (millis() > wifiReconnect) && !WiFi.isConnected()) {
            wifiReconnect = millis() + (WIFI_RETRY_SECS * 1000);
            Serial.printf("WiFi: trying to connect to SSID %s...\n", ssid);
            queueStatusMsg("WiFi connect", 85, true);

            if (WiFi.getMode() != WIFI_MODE_NULL) {
                WiFi.disconnect();
                vTaskDelay(1000/portTICK_PERIOD_MS);
            }
            WiFi.mode(WIFI_STA);
            WiFi.begin(ssid, psk);
            wifiTimeout = 0;
            while (!WiFi.isConnected() && wifiTimeout++ < (WIFI_CONNECT_TIMEOUT_SECS * 2)) {
                vTaskDelay(500/portTICK_PERIOD_MS);
                esp_task_wdt_reset();
            }
            if (!WiFi.isConnected()) {
                Serial.printf("WiFi: connection to SSID %s failed, next attempt in %d seconds\n",
                    ssid, WIFI_RETRY_SECS);
                queueStatusMsg("WiFi failed", 100, true);
                WiFi.setAutoReconnect(false);
                wifiReconnectFail++;
            } else {
                Serial.printf("WiFi: connected to SSID %s with IP %s (RSSI %d dbm)\n",
                    ssid, WiFi.localIP().toString().c_str(), WiFi.RSSI());
                snprintf(statusMsg, sizeof(statusMsg), "WiFi ready (%d dbm)", WiFi.RSSI());
                queueStatusMsg(statusMsg, 45, false);
                wifiReconnectSuccess++;
            }
            loopStatusMsg = 0; // avoid overlapping status messages
        }

        if (loopStatusMsg++ >= 10) {
            loopStatusMsg = 0;
            if (!WiFi.isConnected()) {
                Serial.printf("WiFi: not connected, %d connection attempts (%d successful, %d failed)\n",
                    (wifiReconnectSuccess + wifiReconnectFail), wifiReconnectSuccess, wifiReconnectFail);
                queueStatusMsg("WiFi unavailable", 65, true);
            }
        }

 #ifdef MEMORY_DEBUG_INTERVAL_SECS
        if (loopCounter++ >= MEMORY_DEBUG_INTERVAL_SECS) {
            stackWmWifiTask = printFreeStackWatermark("wifiTask");
            loopCounter = 0;
        }
#endif
        vTaskDelay(1000/portTICK_PERIOD_MS);
    }
}


// starts config portal if no WiFi credentials are available
// if 'forcePortal' is true start config portal anyway
static void wifi_manager(bool forcePortal) {
    WiFiManager wm;
    const char* menu[] = { "wifi", "param", "sep", "update", "restart" };
    String apname = "SensorHub-" + getSystemID();
    char mqttPortStr[8], sensorIntervalStr[4], mqttIntervalStr[4], lorawanIntervalStr[4];

    memset(ssid, 0, sizeof(ssid));
    wm.setDebugOutput(false, "WiFi: ");
    wm.setMinimumSignalQuality(WIFI_MIN_RSSI);
    wm.setScanDispPerc(true);
    wm.setConnectTimeout(WIFI_CONNECT_TIMEOUT_SECS);
    wm.setConfigPortalTimeout(WIFI_SETUP_TIMEOUT_SECS);
    wm.setAPCallback(startConfigPortal);
    wm.setSaveParamsCallback(saveSettings);
    wm.setConfigPortalTimeoutCallback(portalTimeout);
    wm.setWebServerCallback(stopWatchdog);
    wm.setSaveConfigCallback(startWatchdog);
    wm.setTitle("SensorHub");
    wm.setMenu(menu, 5);

    sprintf(sensorIntervalStr, "%d", prefs.readingsIntervalSecs);
    sprintf(mqttIntervalStr, "%d", prefs.mqttIntervalSecs);
    sprintf(lorawanIntervalStr, "%d", prefs.lorawanIntervalSecs);

    WiFiManagerParameter sensor_interval("sensor_interval", "Sensor reading interval (3-60 secs)", sensorIntervalStr, 2);
    WiFiManagerParameter mqtt_interval("mqtt_interval", "MQTT Publish Interval (10-120 secs)", mqttIntervalStr, 3);
    WiFiManagerParameter mqtt_broker("broker", "MQTT Broker", prefs.mqttBroker, PARAMETER_SIZE);
    sprintf(mqttPortStr, "%d", prefs.mqttBrokerPort);
    WiFiManagerParameter mqtt_port("port", "MQTT Broker Port", mqttPortStr, 5);
    WiFiManagerParameter mqtt_topic("topic", "MQTT Base Topic", prefs.mqttTopic, PARAMETER_SIZE);
    WiFiManagerParameter mqtt_user("user", "MQTT Username", prefs.mqttUsername, PARAMETER_SIZE);
    WiFiManagerParameter mqtt_pass("pass", "MQTT Password", prefs.mqttPassword, PARAMETER_SIZE);
    WiFiManagerParameter mqtt_auth("auth", "MQTT Authentication", "1", 1, prefs.mqttEnableAuth ? "type=\"checkbox\" checked" : "type=\"checkbox\"", WFM_LABEL_AFTER);
    WiFiManagerParameter ntp_server("ntp", "NTP Server", prefs.ntpServer, PARAMETER_SIZE);
    WiFiManagerParameter ble_server("ble", "Enable BLE Server", "1", 1, prefs.bleServer ? "type=\"checkbox\" checked" : "type=\"checkbox\"", WFM_LABEL_AFTER);
    WiFiManagerParameter lorawan_node("lorawan", "Enable LoRaWAN", "1", 1, prefs.lorawanEnable ? "type=\"checkbox\" checked" : "type=\"checkbox\"", WFM_LABEL_AFTER);
    WiFiManagerParameter lorawan_interval("lorawan_interval", "LoRaWAN Transmit Interval (30-300 secs)", lorawanIntervalStr, 3);
    WiFiManagerParameter lorawan_confirm("lorawan_confirm", "Confirm Transmit", "1", 1, prefs.lorawanConfirm ? "type=\"checkbox\" checked" : "type=\"checkbox\"", WFM_LABEL_AFTER);
    WiFiManagerParameter lorawan_appeui("lorawan_appeui", "LoRaWAN AppEUI (16 hex chars)", prefs.lorawanAppEUI, 16);
    WiFiManagerParameter lorawan_appkey("lorawan_appkey", "LoRaWAN AppKey (32 hex chars)", prefs.lorawanAppKey, 32);
    WiFiManagerParameter html_br("<br>");

    wm.addParameter(&sensor_interval);
    wm.addParameter(&mqtt_interval);
    wm.addParameter(&mqtt_broker);
    wm.addParameter(&mqtt_port);
    wm.addParameter(&mqtt_topic);
    wm.addParameter(&mqtt_user);
    wm.addParameter(&mqtt_pass);
    wm.addParameter(&mqtt_auth);
    wm.addParameter(&html_br);
    wm.addParameter(&html_br);
    wm.addParameter(&ntp_server);
    wm.addParameter(&html_br);
    wm.addParameter(&ble_server);
    wm.addParameter(&html_br);
    wm.addParameter(&lorawan_node);
    wm.addParameter(&html_br);
    wm.addParameter(&lorawan_interval);
    wm.addParameter(&lorawan_confirm);
    wm.addParameter(&html_br);
    wm.addParameter(&html_br);
    wm.addParameter(&lorawan_appeui);
    wm.addParameter(&lorawan_appkey);

    if (!forcePortal && wm.getWiFiSSID().length()) {
        Serial.printf("WiFiManager: autoconnect to SSID %s\n", wm.getWiFiSSID().c_str());
        strlcpy(ssid, wm.getWiFiSSID().c_str(), 32); // used by wifiReconnectTask()
        strlcpy(psk, wm.getWiFiPass().c_str(), 32);
    }

    connectionSuccess(false);
    // autoconnect to preconfigure WiFi network
    // if unavailable or WiFi credentials are invalid start configuration portal
    if (!wm.autoConnect(apname.c_str(), randomPassword())) {
        connectionFailed(wm.getWiFiSSID().c_str());
        delay(3000);
    }

    if (forcePortal) {
        wm.setConfigPortalTimeout(0);
        wm.setConfigPortalBlocking(false);
        wm.startConfigPortal(apname.c_str(), randomPassword());
        while (!updateSettings) { // wait until (new) settings are saved
            wm.process();
            esp_task_wdt_reset();
        }
        wm.stopConfigPortal();
    }

    if (WiFi.isConnected()) {
        connectionSuccess(true);
        delay(1500);
    }

    if (updateSettings) {
        prefs.readingsIntervalSecs = strtoumax(sensor_interval.getValue(), NULL, 10);
        prefs.mqttIntervalSecs = strtoumax(mqtt_interval.getValue(), NULL, 10);
        strlcpy(prefs.mqttBroker, mqtt_broker.getValue(), PARAMETER_SIZE);
        prefs.mqttBrokerPort = strtoumax(mqtt_port.getValue(), NULL, 10);
        strlcpy(prefs.mqttTopic, mqtt_topic.getValue(), PARAMETER_SIZE);
        prefs.mqttEnableAuth = *mqtt_auth.getValue();
        strlcpy(prefs.mqttUsername, mqtt_user.getValue(), PARAMETER_SIZE);
        strlcpy(prefs.mqttPassword, mqtt_pass.getValue(), PARAMETER_SIZE);
        strlcpy(prefs.ntpServer, ntp_server.getValue(), PARAMETER_SIZE);
        prefs.bleServer = *ble_server.getValue();
        prefs.lorawanEnable = *lorawan_node.getValue();
        prefs.lorawanIntervalSecs = strtoumax(lorawan_interval.getValue(), NULL, 10);
        prefs.lorawanConfirm = *lorawan_confirm.getValue();
        strlcpy(prefs.lorawanAppEUI, lorawan_appeui.getValue(), 16);
        strlcpy(prefs.lorawanAppKey, lorawan_appkey.getValue(), 32);

        savePrefs(false);
    }

    // background task to check/reastablish WiFi uplink
    xTaskCreatePinnedToCore(wifiConnectionTask, "wifiTask", 2560, NULL, 3, NULL, 1);
}


// button event handler
static void eventStartPortal(Event& e) {
    endButtonWaitLoop = true;
    if (!strcmp(e.objName(), "Yes"))
        wifi_manager(true);
}


// renders yes/no dialog on display to start configuration portal
// if dialog times out WifiManager tries to connect to WiFi
void wifi_init() {
    uint16_t timeout = 0;
    wifi_config_t conf;

    // check for saved WiFi credentials
    // if missing, proceed to setup portal
    WiFi.mode(WIFI_AP_STA);
    esp_wifi_get_config(WIFI_IF_STA, &conf);
    if (!strlen(reinterpret_cast<const char*>(conf.sta.ssid))) {
        wifi_manager(false);
        return;
    }

    ButtonColors onColor = {RED, WHITE, WHITE};
    ButtonColors offColor = {DARKGREEN, WHITE, WHITE};
    Button bYes(35, 130, 120, 60, false, "Yes", offColor, onColor, MC_DATUM);
    Button bNo(165, 130, 120, 60, false, "No", offColor, onColor, MC_DATUM);

    M5.Lcd.fillScreen(WHITE);
    M5.Lcd.setTextColor(BLACK);
    M5.Lcd.setFreeFont(&FreeSans12pt7b);
    M5.Lcd.setCursor(60, 70);
    M5.Lcd.print("Start WiFi portal to");
    M5.Lcd.setCursor(50, 100);
    M5.Lcd.print("(re)configure device?");

    M5.Buttons.draw();
    bYes.addHandler(eventStartPortal, E_RELEASE);
    bNo.addHandler(eventStartPortal, E_RELEASE);
    while (timeout++ < (DISPLAY_DIALOG_TIMEOUT_SECS * 1000) && !endButtonWaitLoop) {
        M5.update();
        delay(1);
    }
    if (!endButtonWaitLoop)
        wifi_manager(false);
}