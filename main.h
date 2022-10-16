#include <vector>
#ifndef __AR4BR_MAIN_H
#define __AR4BR_MAIN_H

#include <EEPROM.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include "SPIFFS.h"
#include <rom/rtc.h>
#include <vector>
#include <ArduinoJson.h>
//#include "esp_task_wdt.h"

#include "vector"
#include "config.h"
#include "utils.h"
#include "types.h"
#include "bt.h"
#include "html.h"
#include "Aranet4.h"

#include "influx/influx.h"
#include "mqtt/mqtt.h"

#define MODE_PIN 13
#define LED_PIN  2

const char compile_date[] = __DATE__ " " __TIME__;

char* www_username;
char* www_password;

// ---------------------------------------------------
//                 Global variables
// ---------------------------------------------------
Preferences prefs;

WebServer server(80);

MyAranet4Callbacks ar4callbacks;
AranetDeviceConfig ar4devices;
AranetDeviceStatus ar4status[CFG_MAX_DEVICES];

Aranet4 ar4(&ar4callbacks);
InfluxDBClient* influxClient = nullptr;

WiFiClient espClient;
MqttClient mqttClient(espClient);

AranetDataCompact logs[CFG_HISTORY_CHUNK_SIZE];

std::vector<ScanCache*> scanCache;

// RTOS
TaskHandle_t BtScanTask;
TaskHandle_t WiFiTask;
TaskHandle_t NtpSyncTask;
TimerHandle_t WatchdogTimer;

bool wifiTaskRunning = false;

// Blue
NimBLEScan *pScan = NimBLEDevice::getScan();

bool isAp = true;
long wifiConnectedAt = 0;
long scanBlockTimeout = 0;
long ntpSyncTime = 0;
uint8_t ntpSyncFails = 0;
bool spiffsOk = false;

// ---------------------------------------------------
//                 Function declarations
// ---------------------------------------------------
int configLoad();
void devicesLoad();
void devicesSave();

int createInfluxClient();
bool getBootWiFiMode();
bool startWebserver();
bool webAuthenticate();
bool isManualIp();
void task_sleep(uint32_t ms);
void runBtScan();
void startNtpSyncTask();
void log(String msg, ILog level);
bool ntpSync();

void setupWatchdog();
void startWatchdog(uint32_t period);
bool cancelWatchdog();

void BtScanTaskCode(void* pvParameters);
void WiFiTaskCode(void* pvParameters);
void NtpSyncTaskCode(void* pvParameters);

AranetDeviceStatus* findDeviceStatus(uint8_t* macaddr);
AranetDeviceStatus* findDeviceStatus(NimBLEAdvertisedDevice adv);

// ---------------------------------------------------
//                 Function definitions
// ---------------------------------------------------

bool isManualIp() {
  return (prefs.getBool(PREF_K_WIFI_IP_STATIC));
}

bool isScanOpen() {
  return (scanBlockTimeout > millis());
}

void wipeStoredDevices() {
  ar4devices.size = 0;
  SPIFFS.remove("/devices.json");
  devicesSave();
}

void devicesLoad() {
  ar4devices.size = 0;
  Serial.println("Loading devices...");
  if (SPIFFS.exists("/devices.json")) {
    File file = SPIFFS.open("/devices.json");

    if (file) {
        DynamicJsonDocument doc(4096); // 4k
        DeserializationError error = deserializeJson(doc, file);

        if (error) {
            log_e("config: failed to read config file");\
            file.close();
            return;
        }

        if (doc.containsKey("devices")) {
            JsonArray devices = doc["devices"];
            int index = 0;
            for (JsonObject dev : devices) {
              if (dev.isNull()) continue;
              Serial.printf("Load device %u\n", index);
              JsonObject settings = dev["settings"];
              ar4devices.devices[index].enabled = settings["enabled"];
              ar4devices.devices[index].paired = settings["paired"];
              ar4devices.devices[index].gatt = settings["gatt"];
              ar4devices.devices[index].history = settings["history"];

              const char* name = dev["name"];
              const char* mac = dev["mac"];

              NimBLEAddress addr(mac);
              memcpy(ar4devices.devices[index].addr, addr.getNative(), 6);
              strcpy(ar4devices.devices[index].name, name);

              index++;
            }
            ar4devices.size = index;
        }
        file.close();
    }
  } else {
    Serial.println("config file not exist!");
  }

  // link devices
  for (int i = 0; i < ar4devices.size; i++) {
    ar4status[i].device = &ar4devices.devices[i];
  }
}

void devicesSave() {
  File cfg = SPIFFS.open("/devices.json");
  if (!cfg) return;

  DynamicJsonDocument doc(4096); // 4k
  DeserializationError error = deserializeJson(doc, cfg);
  if (error) {
    log_e("web: failed to read file, using default configuration");
  }
  cfg.close();

  SPIFFS.remove("/devices.json");
  cfg = SPIFFS.open("/devices.json", FILE_WRITE);

  for (int i = 0; i < ar4devices.size; i++) {
    AranetDevice devc = ar4devices.devices[i];
    devc.saveConfig(doc);
  }

  if (serializeJsonPretty(doc, cfg) == 0) {
    log_e("web: failed to write config file");
  }

  // Close the file
  cfg.close();
}

void saveDevice(uint8_t* addr, String name, bool paired) {
  memcpy(&ar4devices.devices[ar4devices.size].addr, addr, 6);
  memcpy(&ar4devices.devices[ar4devices.size].name, name.c_str(), 24);

  ar4devices.devices[ar4devices.size].paired = paired;

  ar4status[ar4devices.size].device = &ar4devices.devices[ar4devices.size];
  ar4devices.size += 1;

  devicesSave();
}

/*
   Loads configuration
   @return 1 on success or 0 on failure
*/
int configLoad() {
  if (!prefs.begin("nodeCfg")) {
    Serial.println("failed to read node config");
    return 0;
  }

  if (!prefs.getBool(PREF_K_CFG_INIT, false)) {
    wipeStoredDevices();
    prefs.putBool(PREF_K_CFG_INIT, true);
  }

  return 1;
}

/*
   Create influxdb client
*/
int createInfluxClient() {
  if (influxClient != nullptr) {
    delete influxClient;
  }
  influxClient = influxCreateClient(&prefs);
  return 1;
}

bool getBootWiFiMode() {
  bool isAp = false;
  if (prefs.getString(PREF_K_WIFI_SSID).length() == 0) return true;

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

  for (uint8_t i = 0; i < ar4devices.size; i++) {
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
  String page = String("stopped");

  int index = 0;
  long ms = millis();
  for (ScanCache* c : scanCache) {
    NimBLEAddress addr(c->umac);

    uint8_t macaddr[6];
    macaddr[0] = addr.getNative()[5];
    macaddr[1] = addr.getNative()[4];
    macaddr[2] = addr.getNative()[3];
    macaddr[3] = addr.getNative()[2];
    macaddr[4] = addr.getNative()[1];
    macaddr[5] = addr.getNative()[0];

    AranetDeviceStatus* s = findDeviceStatus(macaddr);

    page += "\n";
    page += String(c->umac);
    page += ";";
    page += String(addr.toString().c_str());
    page += ";";
    page += String(c->rssi);
    page += ";";
    page += String(c->name);
    page += ";";
    page += String(ms - c->lastSeen);
    page += ";";
    if (c->connectable) page += 'C';
    if (c->beacon) page += 'B';
    if (s && s->device->paired) page += 'P';

    page += ";";
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
    Serial.printf("Starting STATION: %s\n", prefs.getString(PREF_K_WIFI_SSID).c_str());
    WiFi.mode(WIFI_STA);
    WiFi.setHostname(prefs.getString(PREF_K_SYS_NAME).c_str());

    // manual ip
    if (isManualIp()) {
      if (!WiFi.config(prefs.getUInt(PREF_K_WIFI_IP_ADDR), prefs.getUInt(PREF_K_WIFI_IP_GW), prefs.getUInt(PREF_K_WIFI_IP_MASK), prefs.getUInt(PREF_K_WIFI_IP_DNS))) {
        Serial.println("STA Failed to configure");
      }
    }

    WiFi.begin(prefs.getString(PREF_K_WIFI_SSID).c_str(), prefs.getString(PREF_K_WIFI_PASSWORD).c_str(), 0, NULL);

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
    server.close();
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

  if (!restart) {
    startWebserverTask();
  }

  return true;
}

bool webAuthenticate() {
    if (!www_username) {
        String str = prefs.getString(PREF_K_LOGIN_USER, CFG_DEF_LOGIN_USER);
        int len = str.length() + 1;
        if (www_username) {
            free(www_username);
        }
        www_username = (char*) malloc(len);
        memcpy(www_username, str.c_str(), len);
    }

    if (!www_password) {
        String str = prefs.getString(PREF_K_LOGIN_PASSWORD, CFG_DEF_LOGIN_PASSWORD);
        int len = str.length() + 1;
        if (www_password) {
            free(www_password);
        }
        www_password = (char*) malloc(len);
        memcpy(www_password, str.c_str(), len);
    }

    Serial.printf("u%s:p:%s\n", www_username, www_password);

    return server.authenticate(www_username, www_password);
}

bool startWebserver() {
  // setup webserver handles
  server.on("/", HTTP_GET, []() {
    if (!webAuthenticate()) {
      return server.requestAuthentication();
    }
    server.send(200, "text/html", printHtmlIndex(ar4status, ar4devices.size));
  });

  server.on("/settings", HTTP_GET, []() {
    if (!webAuthenticate()) {
      return server.requestAuthentication();
    }
    server.send(200, "text/html", printHtmlConfig(&prefs));
  });

  server.on("/settings", HTTP_POST, []() {
    if (!webAuthenticate()) {
      return server.requestAuthentication();
    }

    // process data...

    // Generic
    if (server.hasArg(PREF_K_SYS_NAME))     {
      prefs.putString(PREF_K_SYS_NAME, server.arg(PREF_K_SYS_NAME));
    }
    if (server.hasArg(PREF_K_NTP_URL))      {
      prefs.putString(PREF_K_NTP_URL, server.arg(PREF_K_NTP_URL));
    }

    // Network
    if (server.hasArg(PREF_K_WIFI_SSID))     {
      prefs.putString(PREF_K_WIFI_SSID, server.arg(PREF_K_WIFI_SSID));
    }

    if (server.hasArg(PREF_K_WIFI_PASSWORD)) {
      prefs.putString(PREF_K_WIFI_PASSWORD, server.arg(PREF_K_WIFI_PASSWORD));
    }

    // IP
    prefs.putBool(PREF_K_WIFI_IP_STATIC, server.hasArg(PREF_K_WIFI_IP_STATIC));

    if (server.hasArg(PREF_K_WIFI_IP_ADDR)) {
      prefs.putUInt(PREF_K_WIFI_IP_ADDR, str2ip(server.arg(PREF_K_WIFI_IP_ADDR)));
    }

    if (server.hasArg(PREF_K_WIFI_IP_MASK)) {
      prefs.putUInt(PREF_K_WIFI_IP_MASK, str2ip(server.arg(PREF_K_WIFI_IP_MASK)));
    }

    if (server.hasArg(PREF_K_WIFI_IP_GW)) {
      prefs.putUInt(PREF_K_WIFI_IP_GW, str2ip(server.arg(PREF_K_WIFI_IP_GW)));
    }

    if (server.hasArg(PREF_K_WIFI_IP_DNS)) {
      prefs.putUInt(PREF_K_WIFI_IP_DNS, str2ip(server.arg(PREF_K_WIFI_IP_DNS)));
    }

    // Influx
    if (server.hasArg(PREF_K_INFLUX_URL)) {
      prefs.putString(PREF_K_INFLUX_URL, server.arg(PREF_K_INFLUX_URL));
    }
    if (server.hasArg(PREF_K_INFLUX_ORG)) {
      prefs.putString(PREF_K_INFLUX_ORG, server.arg(PREF_K_INFLUX_ORG));
    }
    if (server.hasArg(PREF_K_INFLUX_TOKEN)) {
      prefs.putString(PREF_K_INFLUX_TOKEN, server.arg(PREF_K_INFLUX_TOKEN));
    }
    if (server.hasArg(PREF_K_INFLUX_BUCKET)) {
      prefs.putString(PREF_K_INFLUX_BUCKET, server.arg(PREF_K_INFLUX_BUCKET));
    }
    if (server.hasArg(PREF_K_INFLUX_LOG))     {
      prefs.putUShort(PREF_K_INFLUX_LOG, server.arg(PREF_K_INFLUX_LOG).toInt());
    }
    prefs.putUChar(PREF_K_INFLUX_DBVER, server.hasArg(PREF_K_INFLUX_DBVER) ? 2 : 1);

    // MQTT
    if (server.hasArg(PREF_K_MQTT_SERVER)) {
      prefs.putUInt(PREF_K_MQTT_SERVER, str2ip(server.arg(PREF_K_MQTT_SERVER)));
    }
    if (server.hasArg(PREF_K_MQTT_PORT)) {
      prefs.putUShort(PREF_K_MQTT_PORT, server.arg(PREF_K_MQTT_PORT).toInt());
    }
    if (server.hasArg(PREF_K_MQTT_USER)) {
      prefs.putString(PREF_K_MQTT_USER, server.arg(PREF_K_MQTT_USER));
    }
    if (server.hasArg(PREF_K_MQTT_PASSWORD)) {
      prefs.putString(PREF_K_MQTT_PASSWORD, server.arg(PREF_K_MQTT_PASSWORD));
    }

    createInfluxClient();
    ntpSyncTime = 0; // sync now
    ntpSyncFails = 0;

    server.send(200, "text/html", printHtmlConfig(&prefs, true));
  });

  server.on("/data", HTTP_GET, []() {
    if (!webAuthenticate()) {
      return server.requestAuthentication();
    }
    server.send(200, "text/plain", printData());
  });
  server.on("/scan", HTTP_GET, []() {
    if (!webAuthenticate()) {
      return server.requestAuthentication();
    }

    //runBtScan();

    server.send(200, "text/html", printScanPage());
  });

  server.on("/scanresults", HTTP_GET, []() {
    if (!webAuthenticate()) {
      return server.requestAuthentication();
    }
    scanBlockTimeout = millis() + 5000;
    server.send(200, "text/plain", printScannedDevices());
  });

  server.on("/pair", HTTP_POST, []() {
    if (!webAuthenticate()) {
      return server.requestAuthentication();
    }

    // Abort task
    if (pScan->isScanning()) {
       pScan->stop();
    }

    uint32_t pin = -1;
    bool hasMac = false;
    String devicemac;

    if (server.hasArg("pin")) {
      pin = server.arg("pin").toInt();
    }

    // Generic
    if (server.hasArg("devicemac")) {
      hasMac = true;
      devicemac = server.arg("devicemac");
    }

    if (hasMac) {
      Serial.println("Begin pair");
      NimBLEAddress addr(devicemac.c_str());

      uint8_t maddr[6];
      memcpy(maddr, addr.getNative(), 6);

      if (false /* FIXME Aranet4::isPaired(addr) */) {
        //Serial.printf("Already paired... %02X:%02X:%02X...\n", addr[0], addr[1], addr[2]);
        //String ar4name = ar4.getName();
        //String result = "Connected to " + ar4name;
        //Serial.println(result);
        //saveDevice(addr, ar4name, true);
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
            saveDevice(maddr, ar4name, true);
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
          Serial.printf("Connecting to device: %s\n", addr.toString().c_str());
          ar4callbacks.providePin(-1); // reset pin

          NimBLEAdvertisedDevice* adv = nullptr;
          NimBLEScanResults results = pScan->getResults();
          for (uint32_t i = 0; i < results.getCount(); i++) {
              NimBLEAdvertisedDevice ax = results.getDevice(i);
              if ((uint64_t) ax.getAddress() == (uint64_t) addr) {
                adv = &ax;
                break;
              }
          }

          if (adv && ar4.connect(adv, false) == AR4_OK) {
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
    if (!webAuthenticate()) {
      return server.requestAuthentication();
    }
    ble_store_clear();
    wipeStoredDevices();
    server.send(200, "text/html", "removed paired devices");
  });

  server.on("/restart", HTTP_GET, []() {
    if (!webAuthenticate()) {
      return server.requestAuthentication();
    }
    server.send(200, "text/html", "restarting...");
    delay(1000);
    ESP.restart();
  });

  server.on("/force", HTTP_GET, []() {
    if (!webAuthenticate()) {
      return server.requestAuthentication();
    }

    uint8_t id = 0xFF;
    if (server.hasArg("id")) {
      id = server.arg("id").toInt();
    }

    uint16_t count = 2048; // max
    if (server.hasArg("count")) {
      count = server.arg("count").toInt();
    }

    char buf[40];
    sprintf(buf, "Will download %u records...", count);

    server.send(200, "text/html", buf);

    for (int j=0; j<ar4devices.size; j++) {
      if (id == 0xFF || id == j) {
        AranetDeviceStatus* s = s = &ar4status[j];
        s->pending = count;
      }
    }
  });

  // Image resources
  // server static content from images folder with 10 minute cache
  if (spiffsOk) {
    server.serveStatic("/img/", SPIFFS, "/img/", "max-age=86400"); // 1 day cache
    server.serveStatic("/js/", SPIFFS, "/js/", "max-age=86400"); // 1 day cache
    server.serveStatic("/devices.json", SPIFFS, "/devices.json", "max-age=1"); // no cache
  }

  server.onNotFound(handleNotFound);
  server.begin();

  return true;
}

void task_sleep(uint32_t ms) {
  vTaskDelay(ms / portTICK_PERIOD_MS);
}

void runBtScan() {
  if (!pScan->isScanning()) {
    xTaskCreatePinnedToCore(
      BtScanTaskCode,  /* Task function. */
      "BtScanTask",    /* name of task. */
      10000,           /* Stack size of task */
      NULL,            /* parameter of the task */
      3,               /* priority of the task */
      &BtScanTask,     /* Task handle to keep track of created task */
      0);              /* pin task to core 0 */
  }
}

void BtScanTaskCode(void* pvParameters) {
  Serial.println("Scanning BT devices");

  pScan->start(CFG_BT_SCAN_DURATION);
  vTaskDelete(BtScanTask);

  // This should't be reached...
  for (;;) {
    Serial.println("waiting for death...");
    task_sleep(100);
  }
}

void startNtpSyncTask() {
  // Start NTP sync task
  xTaskCreatePinnedToCore(
    NtpSyncTaskCode,  /* Task function. */
    "NtpSyncTask",    /* name of task. */
    4096,             /* Stack size of task */
    NULL,             /* parameter of the task */
    3,                /* priority of the task */
    &NtpSyncTask,     /* Task handle to keep track of created task */
    0);               /* pin task to core 0 */
}

bool ntpSync() {
  Serial.println("NTP: sync time");
  configTime(0, 0, prefs.getString(PREF_K_NTP_URL).c_str());
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.printf("NTP: Failed to obtain time (%d)\n",ntpSyncFails);
    return false;
  }
  Serial.println(&timeinfo, "NTP: %A, %B %d %Y %H:%M:%S");
  return true;
}

void NtpSyncTaskCode(void * pvParameters) {
  for (;;) {
    if (millis() > ntpSyncTime) {
      if (ntpSync()) {
        ntpSyncTime = millis() + (CFG_NTP_SYNC_INTERVAL * 60000);
        ntpSyncFails = 0;
      } else {
        if (ntpSyncFails < 2) task_sleep(10000); // 10s
        else if (ntpSyncFails < 3) task_sleep(30000); //60s
        else if (ntpSyncFails < 4) task_sleep(60000); //60s
        else task_sleep(300000); //5m
        ntpSyncFails++;
         // sleep 10 seconds
        continue;
      }
    }
    task_sleep(60000); // sleep 1 minute
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

void restart(TimerHandle_t xTimer) {
  log("watchdog: timed out. Restarting ESP32...", ILog::ERROR);
  ESP.restart();
}

int wdId = 1;
uint32_t period = 30;

void setupWatchdog() {
  if (!WatchdogTimer) {
    WatchdogTimer = xTimerCreate("WatchdogTimer", pdMS_TO_TICKS(period * 1000), pdTRUE, ( void * )wdId, &restart);
  }
}

void startWatchdog(uint32_t p) {
  if (p != period) {
    period = p;
    xTimerChangePeriod(WatchdogTimer, pdMS_TO_TICKS(period * 1000), pdMS_TO_TICKS(1000));
  }
  if (xTimerIsTimerActive(WatchdogTimer)) {
    xTimerReset(WatchdogTimer, pdMS_TO_TICKS(1000));
  } else {
    xTimerStart(WatchdogTimer, pdMS_TO_TICKS(1000));
  }
}

bool cancelWatchdog() {
  if (xTimerStop(WatchdogTimer, pdMS_TO_TICKS(1000)) == pdPASS) {
    return true;
  }
  return false;
}

void log(String msg, ILog level) {
  influxSendLog(influxClient, &prefs, msg, level);
}

char* rst_reasons[] = {
    "POWERON_RESET",          /**<1, Vbat power on reset*/
    "SW_RESET",               /**<3, Software reset digital core*/
    "OWDT_RESET",             /**<4, Legacy watch dog reset digital core*/
    "DEEPSLEEP_RESET",        /**<5, Deep Sleep reset digital core*/
    "SDIO_RESET",             /**<6, Reset by SLC module, reset digital core*/
    "TG0WDT_SYS_RESET",       /**<7, Timer Group0 Watch dog reset digital core*/
    "TG1WDT_SYS_RESET",       /**<8, Timer Group1 Watch dog reset digital core*/
    "RTCWDT_SYS_RESET",       /**<9, RTC Watch dog Reset digital core*/
    "INTRUSION_RESET",       /**<10, Instrusion tested to reset CPU*/
    "TGWDT_CPU_RESET",       /**<11, Time Group reset CPU*/
    "SW_CPU_RESET",          /**<12, Software reset CPU*/
    "RTCWDT_CPU_RESET",      /**<13, RTC Watch dog Reset CPU*/
    "EXT_CPU_RESET",         /**<14, for APP CPU, reseted by PRO CPU*/
    "RTCWDT_BROWN_OUT_RESET",/**<15, Reset when the vdd voltage is not stable*/
    "RTCWDT_RTC_RESET",      /**<16, RTC Watch dog reset digital core and rtc module*/
    "NO_MEAN"
};

char* getResetReason(RESET_REASON reason) {
  switch (reason) {
    case 1 : return rst_reasons[0];   /**<1, Vbat power on reset*/
    case 3 : return rst_reasons[1];   /**<3, Software reset digital core*/
    case 4 : return rst_reasons[2];   /**<4, Legacy watch dog reset digital core*/
    case 5 : return rst_reasons[3];   /**<5, Deep Sleep reset digital core*/
    case 6 : return rst_reasons[4];   /**<6, Reset by SLC module, reset digital core*/
    case 7 : return rst_reasons[5];   /**<7, Timer Group0 Watch dog reset digital core*/
    case 8 : return rst_reasons[6];   /**<8, Timer Group1 Watch dog reset digital core*/
    case 9 : return rst_reasons[7];   /**<9, RTC Watch dog Reset digital core*/
    case 10 : return rst_reasons[8];  /**<10, Instrusion tested to reset CPU*/
    case 11 : return rst_reasons[9];  /**<11, Time Group reset CPU*/
    case 12 : return rst_reasons[10]; /**<12, Software reset CPU*/
    case 13 : return rst_reasons[11]; /**<13, RTC Watch dog Reset CPU*/
    case 14 : return rst_reasons[12]; /**<14, for APP CPU, reseted by PRO CPU*/
    case 15 : return rst_reasons[13]; /**<15, Reset when the vdd voltage is not stable*/
    case 16 : return rst_reasons[14]; /**<16, RTC Watch dog reset digital core and rtc module*/
    default : return rst_reasons[15];
  }
}

AranetDeviceStatus* findDeviceStatus(uint8_t* macaddr) {
    AranetDeviceStatus* s = nullptr;
    AranetDevice* d = nullptr;

    for (int j=0; j<ar4devices.size; j++) {
        s = &ar4status[j];
        d = s->device;
        if (memcmp(macaddr, d->addr, 6) == 0) {
            return s;
        }
    }

    return nullptr;
}

AranetDeviceStatus* findDeviceStatus(NimBLEAdvertisedDevice* adv) {
    // find matching aranet device
    uint8_t macaddr[6];
    memcpy(macaddr, adv->getAddress().getNative(), sizeof(macaddr));

    return findDeviceStatus(macaddr);
}

void registerScannedDevice(NimBLEAdvertisedDevice* adv, DeviceType type) {
    // find existing
    uint64_t umac = (uint64_t) adv->getAddress();
    int rssi = adv->getRSSI();
    bool exists = false;
    for (ScanCache* c : scanCache) {
      if (c->umac == umac) {
        c->lastSeen = millis();
        c->type = type;
        c->rssi = rssi;
        strcpy(c->name, adv->getName().c_str());
        exists = true;
      }
    }

    if (!exists) {
        ScanCache* c = new ScanCache();
        c->umac = umac;
        c->type = type;
        c->lastSeen = millis();
        c->rssi = rssi;
        strcpy(c->name, adv->getName().c_str());
        scanCache.push_back(c);
    }
}

void cleanupScannedDevices() {
    scanCache.erase(
        std::remove_if(
            scanCache.begin(), scanCache.end(),
    [](const ScanCache* c) {
        return (millis() - c->lastSeen) > 30000; // remove 30s and older
    }), scanCache.end());
}

void printScannecDevices() {
  Serial.println("----------------------");
  for (ScanCache* c : scanCache) {
      NimBLEAddress adr(c->umac);
      long ago = millis() - c->lastSeen;
      Serial.printf("[%s],  %lu ms\n", adr.toString().c_str(), ago);
  }
}

#endif
