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

MyAranet4Callbacks ar4callbacks;
NodeConfig nodeCfg;
AranetDeviceConfig ar4devices;
AranetDeviceStatus ar4status[CFG_MAX_DEVICES];

Aranet4 ar4(&ar4callbacks);

// RTOS
TaskHandle_t BtScanTask;
TaskHandle_t WiFiTask;
TaskHandle_t NtpSyncTask;

bool wifiTaskRunning = false;

// Blue
NimBLEScan *pScan = NimBLEDevice::getScan();

bool isAp = true;
long wifiConnectedAt = 0;
long scanBlockTimeout = 0;

// ---------------------------------------------------
//                 Function declarations
// ---------------------------------------------------
int configLoad();
void configSave(bool fix);
bool configFix();
bool getBootWiFiMode();
bool startWebserver();
bool isManualIp();
void task_sleep(uint32_t ms);
void runBtScan();
void ntpSync();

void BtScanTaskCode(void* pvParameters);
void WiFiTaskCode(void* pvParameters);
void NtpSyncTaskCode(void* pvParameters);

// ---------------------------------------------------
//                 Function definitions
// ---------------------------------------------------

bool isManualIp() {
    return (nodeCfg.extras & CFG_EXTRA_BIT_STATIC_IP) != 0;
}

bool isScanOpen() {
    return (scanBlockTimeout > millis());
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
bool configFix() {
  bool updatecfg = nodeCfg.version != CFG_VER;

    // Load defaults
    if (nodeCfg.name[0] == 0) { strcpy(nodeCfg.name, CFG_DEF_NAME); updatecfg = true; }
    if (nodeCfg.url[0] == 0) { strcpy(nodeCfg.url, CFG_DEF_URL); updatecfg = true; }
    if (nodeCfg.ntpserver[0] == 0) { strcpy(nodeCfg.ntpserver, CFG_DEF_NTP); updatecfg = true; }

    // version migration
    if (nodeCfg.version == 0) {
        // set IPs
        nodeCfg.ip_addr = 0;
        nodeCfg.netmask = 0;
        nodeCfg.gateway = 0;
        nodeCfg.dns = 0;
    }

    if (nodeCfg.version < 2) {
        strcpy(nodeCfg.ntpserver, CFG_DEF_NTP);
    }

    // set version
    nodeCfg.version = CFG_VER;

    return updatecfg;
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

    if (configFix()) configSave(false);

    return 1;
}

/*
   Saves configuration to EEPROM
*/
void configSave(bool fix = true) {
    if (fix) configFix();
    for (uint16_t j = 0; j < sizeof(NodeConfig); j++) {
        char b = ((char *) &nodeCfg)[j];
        EEPROM.write(j, b);
    }
    EEPROM.commit();
    ntpSync();
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

String printScannedDevices() {
    NimBLEScanResults results = pScan->getResults();
    String page = pScan->isScanning() ? String("running") : String("stopped");    
    
    for (int i = 0; i < results.getCount(); i++) {
        NimBLEAdvertisedDevice adv = results.getDevice(i);

        if (adv.getName().find("Aranet") == std::string::npos) {
            continue;
        }
        
        page += "\n";
        page += String(i);
        page += ";";
        page += String(adv.getAddress().toString().c_str());
        page += ";";
        page += String(adv.getRSSI());
        page += ";";
        page += String(adv.getName().c_str());
        page += ";";

        int count = ar4devices.size;
        bool paired = false;
        for (int j=0; j<count; j++) {
            AranetDeviceStatus* s = s = &ar4status[j];
            AranetDevice* d = d = s->device;

            uint8_t rev[6];
            rev[0] = adv.getAddress().getNative()[5];
            rev[1] = adv.getAddress().getNative()[4];
            rev[2] = adv.getAddress().getNative()[3];
            rev[3] = adv.getAddress().getNative()[2];
            rev[4] = adv.getAddress().getNative()[1];
            rev[5] = adv.getAddress().getNative()[0];

            if (memcmp(rev, d->addr, 6) == 0) {
                paired = true;
                break;
            }
        }
        page += paired ? "1" : "0";
    }
  return page;
}

void handleNotFound() {
    server.send(404, "text/html", "404");
}

bool setupWiFi() {
    if (WiFi.status() == WL_CONNECTED) {
        return true;
    }

    if (isAp) {
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

    wifiConnectedAt = millis();
    return true;
}

void startWebserverTask() {
    if (wifiTaskRunning) {
        vTaskDelete(WiFiTask);
        wifiTaskRunning = false;
    }
    xTaskCreatePinnedToCore(
        WiFiTaskCode,   /* Task function. */
        "WiFiTask",     /* name of task. */
        10000,            /* Stack size of task */
        NULL,             /* parameter of the task */
        1,                /* priority of the task */
        &WiFiTask,      /* Task handle to keep track of created task */
        0);          /* pin task to core 0 */
}

bool setupWifiAndWebserver(bool restart = false) {
      if (restart) {
          WiFi.disconnect();
          server.stop();
      }

      if (setupWiFi()) {
          Serial.println("WiFi started");
      } else {
          Serial.println("Failed to start WiFi");
          return false;
      }
      
      if (startWebserver()) {
          Serial.println("Web server started");
      } else {
          Serial.println("Failed to start WiFi");
          return false;
      }

      return true;      
}

bool startWebserver() {
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
        if (server.hasArg("ntpserver"))      {
            server.arg("ntpserver").toCharArray(nodeCfg.ntpserver, sizeof(nodeCfg.ntpserver) - 1);
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

        server.send(200, "text/html", printScanPage());
    });

    server.on("/scanresults", HTTP_GET, []() {
        if (!server.authenticate(www_username, password)) {
            return server.requestAuthentication();
        }
        scanBlockTimeout = millis() + 5000;
        server.send(200, "text/plain", printScannedDevices());
    });

    server.on("/pair", HTTP_POST, []() {
        if (!server.authenticate(www_username, password)) {
            return server.requestAuthentication();
        }

        // Abort task
        if (pScan->isScanning()) {
            pScan->stop();
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

        if (deviceid != -1 && deviceid < pScan->getResults().getCount()) {
            Serial.println("Begin pair");
            NimBLEAdvertisedDevice adv = pScan->getResults().getDevice(deviceid);

            uint8_t addr[6];
            addr[0] = adv.getAddress().getNative()[5];
            addr[1] = adv.getAddress().getNative()[4];
            addr[2] = adv.getAddress().getNative()[3];
            addr[3] = adv.getAddress().getNative()[2];
            addr[4] = adv.getAddress().getNative()[1];
            addr[5] = adv.getAddress().getNative()[0];

            if (false /* FIXME Aranet4::isPaired(addr) */) {
                //Serial.printf("Already paired... %02X:%02X:%02X...\n", addr[0], addr[1], addr[2]);
                //String ar4name = ar4.getName();
                //String result = "Connected to " + ar4name;
                //Serial.println(result);
                //saveDevice(addr, ar4name);
                //ar4.disconnect();
                //server.send(200, "text/html", result);
                server.send(200, "text/html", "Already paired.");
            } else {
                if (pin != -1) {
                    Serial.println("recvd PIN");
                    String result = "Failed";
                    ar4callbacks.providePin(pin);
                    
                    String ar4name = ar4.getName();
                    if (ar4name.length() > 1) {
                        saveDevice(addr, ar4name);
                        result = "Connected to " + ar4name;
                    } else {
                        result = "Failed to get name";
                        ar4name = ar4.getName();
                        Serial.println(ar4name);
                        ar4name = ar4.getSwVersion();
                        Serial.println(ar4name);
                        ar4name = ar4.getFwVersion();
                        Serial.println(ar4name);
                        ar4name = ar4.getHwVersion();
                        Serial.println(ar4name);
                        AranetData meas = ar4.getCurrentReadings();
                        Serial.printf("MEAS:\n  %i ppm\n", meas.co2);
                    }
                    
                    ar4.disconnect();
                    Serial.println(result);
                    server.send(200, "text/html", result);
                 } else {
                    Serial.printf("Connecting to device: %02X:%02X:%02X...\n", addr[0], addr[1], addr[2]);
                    ar4callbacks.providePin(-1); // reset pin

                    if (ar4.connect(&adv, false) == AR4_OK) {
                        server.send(200, "text/html", "OK");
                        
                        // PIN prompt might lock web request, so we give extra 30s for pairing process
                        // It will be teset to +5 sec once prompt is closed and web requests starts again
                        scanBlockTimeout = millis() + 30000;

                        ar4.secureConnection();
                    } else {
                        server.send(200, "text/html", "Connection failed");
                    }
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
        ble_store_clear();
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

    startWebserverTask();

    return true;
}

void task_sleep(uint32_t ms) {
    vTaskDelay(ms / portTICK_PERIOD_MS);
}

void runBtScan() {
    if (!pScan->isScanning()) {
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
    Serial.println("Pairing BT device");
    
    pScan->start(CFG_BT_SCAN_DURATION);
    vTaskDelete(BtScanTask);

    // This should't be reached...
    for (;;) {
        Serial.println("waiting for death...");
        task_sleep(100);
    }
}

void ntpSync() {
    configTime(0, 0, nodeCfg.ntpserver);
    struct tm timeinfo;
    if(!getLocalTime(&timeinfo)){
        Serial.println("Failed to obtain time");
        return;
    }
    Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
}

void NtpSyncTaskCode(void * pvParameters) {
    for (;;) {
        ntpSync();
        task_sleep(3600000); // hourly time sync
    }
}

void WiFiTaskCode(void * pvParameters) {
    wifiTaskRunning = true;
    Serial.print("Task1 running on core ");
    Serial.println(xPortGetCoreID());

    task_sleep(500);

    Serial.print("Waiting for clients....");
    for (;;) {
       if (!isAp && WiFi.status() != WL_CONNECTED) {
            setupWifiAndWebserver(true);
        }
        server.handleClient();
        task_sleep(1);
    }
}

#endif
