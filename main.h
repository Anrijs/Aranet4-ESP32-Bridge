#ifndef __AR4BR_MAIN_H
#define __AR4BR_MAIN_H

#include <EEPROM.h>
#include <WiFi.h>
#include <WebServer.h>
#include "SPIFFS.h"
//#include "esp_task_wdt.h"

#define CFG_EXTRA_BIT_STATIC_IP   0x02

#include "vector"
#include "config.h"
#include "utils.h"
#include "types.h"
#include "bt.h"
#include "html.h"
#include "Aranet4.h"
#include "influx/influx.h"

#define MODE_PIN 13
#define LED_PIN  2

const char compile_date[] = __DATE__ " " __TIME__;

// ---------------------------------------------------
//                 Global variables
// ---------------------------------------------------
WebServer server(80);

MyAranet4Callbacks* ar4callbacks = new MyAranet4Callbacks();
NodeConfig nodeCfg;
AranetDeviceConfig ar4devices;
AranetDeviceStatus ar4status[CFG_MAX_DEVICES];

// RTOS
TaskHandle_t BtScanTask;
TaskHandle_t WiFiTask;

// Blue
BtScanAdvertisedDeviceCallbacks* scanCallbacks = new BtScanAdvertisedDeviceCallbacks();

// ---------------------------------------------------
//                 Function declarations
// ---------------------------------------------------
int configLoad();
void configSave();
void configFix();
bool getBootWiFiMode();
bool startWebserver(bool ap);
bool isManualIp();
void task_sleep(uint32_t ms);
void runBtScan();

void BtScanTaskCode(void* pvParameters);
void WiFiTaskCode(void* pvParameters);

// ---------------------------------------------------
//                 Function definitions
// ---------------------------------------------------

bool isManualIp() {
    return (nodeCfg.extras & CFG_EXTRA_BIT_STATIC_IP) != 0;
}

void wipe(uint32_t start = 0) {
    if (!EEPROM.begin(CFG_DEVICE_OFFSET + sizeof(AranetDeviceConfig))) {
        Serial.println("failed to initialize ar4 EEPROM");
        return;
    }

    for (uint16_t j = start; j < CFG_DEVICE_OFFSET + sizeof(AranetDeviceConfig); j++) {
        EEPROM.write(j, 0);
    }
    EEPROM.commit();
}

void devicesLoad() {
    if (!EEPROM.begin(CFG_DEVICE_OFFSET + sizeof(AranetDeviceConfig))) {
        Serial.println("failed to initialize ar4 EEPROM");
        return;
    }

    for (int i = 0; i < sizeof(AranetDeviceConfig); i++) {
        char b = EEPROM.read(CFG_DEVICE_OFFSET + i);
        ((char *) &ar4devices)[i] = b;
    }

    Serial.printf("SIZE OF DEVICES: %i\n", sizeof(AranetDeviceConfig)); 

    // link devices
    for (int i = 0; i < ar4devices.size; i++) {
        ar4status[i].device = &ar4devices.devices[i];
    }
}

void devicesSave() {
    for (uint16_t j = 0; j < sizeof(AranetDeviceConfig); j++) {
        char b = ((char *) &ar4devices)[j];
        EEPROM.write(CFG_DEVICE_OFFSET + j, b);
      }
    EEPROM.commit();
}

void saveDevice(uint8_t* addr, String name) {
    memcpy(&ar4devices.devices[ar4devices.size].addr, addr, 6);
    memcpy(&ar4devices.devices[ar4devices.size].name, name.c_str(), 24);

    ar4status[ar4devices.size].device = &ar4devices.devices[ar4devices.size];
    ar4devices.size += 1;

    devicesSave();
}
/*
   @brief Default configuration + version migration
   @return 1 on success or 0 on failure
*/
void configFix() {
    // Load defaults
    if (nodeCfg.name[0] == 0) strcpy(nodeCfg.name, CFG_DEF_NAME);
    if (nodeCfg.url[0] == 0) strcpy(nodeCfg.url, CFG_DEF_URL);

    // version migration
    if (nodeCfg.version == 0) {
        // set IPs
        nodeCfg.ip_addr = 0;
        nodeCfg.netmask = 0;
        nodeCfg.gateway = 0;
        nodeCfg.dns = 0;
    }

    // set version
    nodeCfg.version = CFG_VER;
}

/*
   Loads configuration from EEPROM
   @return 1 on success or 0 on failure
*/
int configLoad() {
    if (!EEPROM.begin(CFG_DEVICE_OFFSET + sizeof(AranetDeviceConfig))) {
        Serial.println("failed to initialize EEPROM");
        return 0;
    }

    for (int i = 0; i < sizeof(NodeConfig); i++) {
        char b = EEPROM.read(i);
        ((char *) &nodeCfg)[i] = b;
    }

    Serial.printf("Loaded config v%u\n", nodeCfg.version);

    // Wipe memory
    if (nodeCfg.version > CFG_VER) {
        for (uint16_t j = 0; j < sizeof(NodeConfig); j++) {
            EEPROM.write(j, 0);
        }
        EEPROM.commit();
        return configLoad();
    }

    configFix();
    return 1;
}

/*
   Saves configuration to EEPROM
*/
void configSave() {
    configFix();
    for (uint16_t j = 0; j < sizeof(NodeConfig); j++) {
        char b = ((char *) &nodeCfg)[j];
        EEPROM.write(j, b);
    }
    EEPROM.commit();
}

bool getBootWiFiMode() {
    bool isAp = false;

    if (nodeCfg.ssid[0] == 0) return true;

    long timeout = millis() + 3000;

    if (!digitalRead(MODE_PIN) && !isAp) {
        digitalWrite(LED_PIN, HIGH);

        while (!digitalRead(MODE_PIN) && !isAp) {
            delay(50);
            if (timeout < millis()) {
                isAp = true;
            }
        }
    }

    if (isAp) {
        Serial.println("Release button in next 3 seconds to start in AP mode");
    }

    timeout = millis() + 3000;

    while (!digitalRead(MODE_PIN) && isAp) {
        if (timeout < millis()) {
            isAp = false;
            break;
        }
        delay(150);
        digitalWrite(LED_PIN, LOW);
        delay(150);
        digitalWrite(LED_PIN, HIGH);
    }
    digitalWrite(LED_PIN, LOW);

    return isAp;
}

String printData() {
    char buf[48];
    String page = String("");

    long tnow = millis();
  
    for (uint8_t i=0; i < ar4devices.size; i++) {
        page += String(i) + ";";

        mac2str(ar4status[i].device->addr, buf, false);
        page += String(ar4status[i].device->name);
        page += ";" + String(buf) + ";";

        sprintf(buf, "%i;%.1f;%.1f;%i;%i;%i;%i;%i\n",
            ar4status[i].data.co2,
            ar4status[i].data.temperature / 20.0,
            ar4status[i].data.pressure / 10.0,
            ar4status[i].data.humidity,
            ar4status[i].data.battery,
            ar4status[i].data.interval,
            ar4status[i].data.ago,
            (tnow - ar4status[i].updated) / 1000
        );
        page += String(buf);
    }
    return page;
}

void handleNotFound() {
    server.send(404, "text/html", "404");
}

bool startWebserver(bool ap) {
    if (WiFi.status() == WL_CONNECTED) {
        return true;
    }

    if (ap) {
        Serial.printf("Starting AP: %s : %s\n", ssid, password);
        WiFi.mode(WIFI_AP_STA);
        WiFi.softAP(ssid, password);
    } else {
        Serial.printf("Starting STATION: %s\n", nodeCfg.ssid);
        WiFi.mode(WIFI_STA);
        WiFi.setHostname(nodeCfg.name);

        // manual ip
        if (isManualIp()) {
            if (!WiFi.config(nodeCfg.ip_addr, nodeCfg.gateway, nodeCfg.netmask, nodeCfg.dns)) {
                Serial.println("STA Failed to configure");
            }
         }

        WiFi.begin(nodeCfg.ssid, nodeCfg.password, 0, NULL);

        long timeout = millis() + 15000;
        while (WiFi.status() != WL_CONNECTED) {
            task_sleep(500);
            Serial.print(".");
            if (timeout < millis()) {
                Serial.print("\nFailed to connect.\n");
                return false;
            }
        }
        Serial.println();
    }

    // setup webserver handles
    server.on("/", HTTP_GET, []() {
        if (!server.authenticate(www_username, password)) {
            return server.requestAuthentication();
        }
        server.send(200, "text/html", printHtmlIndex(ar4status, ar4devices.size));
    });

    server.on("/settings", HTTP_GET, []() {
        if (!server.authenticate(www_username, password)) {
            return server.requestAuthentication();
        }
        server.send(200, "text/html", printHtmlConfig(&nodeCfg));
    });

    server.on("/settings", HTTP_POST, []() {
        if (!server.authenticate(www_username, password)) {
            return server.requestAuthentication();
        }

        // process data...

        // Generic
        if (server.hasArg("name"))     {
            server.arg("name").toCharArray(nodeCfg.name, sizeof(nodeCfg.name) - 1);
        }

        // Network
        if (server.hasArg("ssid"))     {
            server.arg("ssid").toCharArray(nodeCfg.ssid, sizeof(nodeCfg.ssid) - 1);
        }

        if (server.hasArg("password")) {
            server.arg("password").toCharArray(nodeCfg.password, sizeof(nodeCfg.password) - 1);
        }

        // IP
        if (server.hasArg("ip_addr")) {
            nodeCfg.ip_addr = str2ip(server.arg("ip_addr"));
        }

        if (server.hasArg("netmask")) {
            nodeCfg.netmask = str2ip(server.arg("netmask"));
        }

        if (server.hasArg("gateway")) {
            nodeCfg.gateway = str2ip(server.arg("gateway"));
        }

        if (server.hasArg("dns")) {
            nodeCfg.dns = str2ip(server.arg("dns"));
        }

        // Influx
        if (server.hasArg("url"))     {
            server.arg("url").toCharArray(nodeCfg.url, sizeof(nodeCfg.url) - 1);
        }
        if (server.hasArg("org"))      {
            server.arg("org").toCharArray(nodeCfg.organisation, sizeof(nodeCfg.organisation) - 1);
        }
        if (server.hasArg("token"))      {
            server.arg("token").toCharArray(nodeCfg.token, sizeof(nodeCfg.token) - 1);
        }
        if (server.hasArg("bucket"))       {
            server.arg("bucket").toCharArray(nodeCfg.bucket, sizeof(nodeCfg.bucket) - 1);
        }

        // Extras
        uint32_t extras = 0;
        if (server.hasArg("static_ip")) {
            extras |= CFG_EXTRA_BIT_STATIC_IP;
        }

        nodeCfg.dbver = (server.hasArg("dbver")) ? 2 : 1;

        nodeCfg.extras = extras;

        configSave();

        server.send(200, "text/html", printHtmlConfig(&nodeCfg, true));
    });

    server.on("/data", HTTP_GET, []() {
      if (!server.authenticate(www_username, password)) {
        return server.requestAuthentication();
      }
      server.send(200, "text/plain", printData());
    });
    server.on("/scan", HTTP_GET, []() {
        if (!server.authenticate(www_username, password)) {
            return server.requestAuthentication();
        }

        runBtScan();

        long timeout = millis() + 1000;
        while (!scanCallbacks->isRunning() && timeout < millis()) {
            task_sleep(5);
        }

        server.send(200, "text/html", printScanPage());
    });

    server.on("/scanresults", HTTP_GET, []() {
        if (!server.authenticate(www_username, password)) {
            return server.requestAuthentication();
        }
        server.send(200, "text/plain", scanCallbacks->printDevices());
    });

    server.on("/pair", HTTP_POST, []() {
        if (!server.authenticate(www_username, password)) {
            return server.requestAuthentication();
        }

        // Abort task
        if (scanCallbacks->isRunning()) {
            scanCallbacks->abort();
        }

        uint32_t pin = -1;
        int deviceid = -1;

        if (server.hasArg("pin")) {
            pin = server.arg("pin").toInt();
        }

        // Generic
        if (server.hasArg("deviceid")) {
            deviceid = server.arg("deviceid").toInt();
        }

        if (deviceid != -1 && deviceid < scanCallbacks->getDeviceCount()) {
            Serial.println("Begin pair");
            uint8_t addr[ESP_BD_ADDR_LEN];
            scanCallbacks->getAddress(deviceid, addr);

            if (Aranet4::isPaired(addr)) {
                Serial.printf("Already paired... %02X:%02X:%02X...\n", addr[0], addr[1], addr[2]);
                String ar4name = "FIXME1"; //ar4.getName();
                String result = "Connected to " + ar4name;
                Serial.println(result);
                saveDevice(addr, ar4name);
                //ar4.disconnect();
                server.send(200, "text/html", result);
            } else {
                if (pin != -1) {
                    Serial.println("recvd PIN");
                    ar4callbacks->providePin(pin);
                    String ar4name = "FIXME2";//ar4.getName();
                    String result = "Connected to " + ar4name;
                    Serial.println(result);

                    saveDevice(addr, ar4name);

                    //ar4.disconnect();
                    server.send(200, "text/html", result);
                 } else {
                    Serial.printf("Connecting to device: %02X:%02X:%02X...\n", addr[0], addr[1], addr[2]);
                    ar4callbacks->providePin(-1); // reset pin
                    /*
                    bool status = ar4.connect(addr);
                    if (status == AR4_OK) {
                      server.send(200, "text/html", "OK");
                    } else {
                      server.send(200, "text/html", "Connection failed");
                    }
                    */
                    server.send(200, "text/html", "FIXME3");
                }
            }
        } else {
            server.send(200, "text/html", "unknown error");
        }
    });

    server.on("/clrbnd", HTTP_GET, []() {
        if (!server.authenticate(www_username, password)) {
            return server.requestAuthentication();
        }
        remove_all_bonded_devices();
        wipe(CFG_DEVICE_OFFSET);
        server.send(200, "text/html", "removed paired devices");
    });
  
    server.on("/restart", HTTP_GET, []() {
        if (!server.authenticate(www_username, password)) {
            return server.requestAuthentication();
        }
        server.send(200, "text/html", "restarting...");
        delay(1000);
        ESP.restart();  
    });

    // Image resources
    // server static content from images folder with 10 minute cache
    server.serveStatic("/img/", SPIFFS, "/img/", "max-age=86400"); // 1 day cache

    server.onNotFound(handleNotFound);
    server.begin();

    xTaskCreate(
        WiFiTaskCode,   /* Task function. */
        "WiFiTask",     /* name of task. */
        10000,            /* Stack size of task */
        NULL,             /* parameter of the task */
        1,                /* priority of the task */
        &WiFiTask);     /* Task handle to keep track of created task */

    return true;
}

void task_sleep(uint32_t ms) {
    vTaskDelay(ms / portTICK_PERIOD_MS);
}

void runBtScan() {
    if (!scanCallbacks->isRunning()) {
        scanCallbacks->setRunning(true);
        xTaskCreate(
            BtScanTaskCode,   /* Task function. */
            "BtScanTask",     /* name of task. */
            10000,            /* Stack size of task */
            NULL,             /* parameter of the task */
            1,                /* priority of the task */
            &BtScanTask);     /* Task handle to keep track of created task */
    }
}

void BtScanTaskCode(void* pvParameters) {
    BLEDevice::init("");
    
    scanCallbacks->clearResults();

    Serial.println("Scanning BT devices");

    BLEScan* pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(scanCallbacks);
    pBLEScan->setInterval(1349);
    pBLEScan->setWindow(449);
    pBLEScan->setActiveScan(false);
    pBLEScan->start(CFG_BT_SCAN_DURATION);

    Serial.println("BT Scan complete");
    scanCallbacks->setRunning(false);
    vTaskDelete(BtScanTask);

    // This should't be reached...
    for (;;) {
        Serial.println("waiting for death...");
        task_sleep(100);
    }
}

void WiFiTaskCode(void * pvParameters) {
    Serial.print("Task1 running on core ");
    Serial.println(xPortGetCoreID());

    task_sleep(500);

    Serial.print("Waiting for clients....");
    for (;;) {
        server.handleClient();
        task_sleep(1);
    }
}

#endif
