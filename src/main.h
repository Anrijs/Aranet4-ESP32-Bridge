#include <vector>
#ifndef __AR4BR_MAIN_H
#define __AR4BR_MAIN_H

#include <EEPROM.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h>
#include "SPIFFS.h"
#include <rom/rtc.h>
#include <vector>
#include <ArduinoJson.h>
#include <WireGuard-ESP32.h>
//#include "esp_task_wdt.h"

#include "vector"
#include "config.h"
#include "utils.h"
#include "types.h"
#include "bt.h"
#include "html.h"
#include "Aranet4.h"
#include "include/airvalent.h"

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

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

MyAranet4Callbacks ar4callbacks;
std::vector<AranetDevice*> ar4devices;
std::vector<AranetDevice*> newDevices;

Aranet4 ar4(&ar4callbacks);
Airvalent airv(&ar4callbacks);
InfluxDBClient* influxClient = nullptr;

WiFiClient espClient;
MqttClient mqttClient(espClient);

AranetDataCompact logs[CFG_HISTORY_CHUNK_SIZE];

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
long ntpSyncTime = 0;
uint8_t ntpSyncFails = 0;
bool spiffsOk = false;

static WireGuard wg;

// ---------------------------------------------------
//                 Function declarations
// ---------------------------------------------------
int configLoad();
void devicesLoad();
void devicesSave();

int createInfluxClient();
bool getBootWiFiMode();
bool startWebserver();
bool webAuthenticate(AsyncWebServerRequest *request);
bool isManualIp();
void task_sleep(uint32_t ms);
void startNtpSyncTask();
void log(String msg, ILog level);
bool ntpSync();

void setupWireguard();

void setupWatchdog();
void startWatchdog(uint32_t period);
bool cancelWatchdog();

void WiFiTaskCode(void* pvParameters);
void NtpSyncTaskCode(void* pvParameters);

AranetDevice* findScannedDevice(NimBLEAddress macaddr);
AranetDevice* findSavedDevice(NimBLEAddress macaddr);
AranetDevice* findSavedDevice(NimBLEAdvertisedDevice adv);

// ---------------------------------------------------
//                 Function definitions
// ---------------------------------------------------

bool isManualIp() {
    return (prefs.getBool(PREF_K_WIFI_IP_STATIC));
}

void wipeStoredDevices() {
    for (AranetDevice* d : ar4devices) {
        delete d;
    }
    ar4devices.clear();
    SPIFFS.remove("/devices.json");
    devicesSave();
}

void devicesLoad() {
    for (AranetDevice* d : ar4devices) {
        delete d;
    }
    ar4devices.clear();
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
                uint8_t devid = 0;
                for (JsonObject dev : devices) {
                    devid++;
                    if (dev.isNull()) continue;

                    Serial.printf("Load device %u\n", devid);
                    JsonObject settings = dev["settings"];

                    AranetDevice* d = new AranetDevice();

                    d->enabled = settings["enabled"];
                    d->state = settings["paired"] ? STATE_PAIRED : STATE_NOT_PAIRED;
                    d->gatt = settings["gatt"];
                    d->history = settings["history"];

                    const char* name = dev["name"];
                    const char* mac = dev["mac"];

                    d->addr = NimBLEAddress(mac, BLE_ADDR_RANDOM);
                    strcpy(d->name, name);

                    ar4devices.push_back(d);
                }
            }
            file.close();
        }
    } else {
        Serial.println("config file not exist!");
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

    // clear devices array
    doc.remove("devices");

    for (AranetDevice* d : ar4devices) {
        d->saveConfig(doc);
    }

    if (serializeJsonPretty(doc, cfg) == 0) {
        log_e("web: failed to write config file");
    }

    // Close the file
    cfg.close();
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
    char buf[100];
    String page = String("");

    long tnow = millis();

    int index = 0;
    for (AranetDevice* d : ar4devices) {
        page += String(index++) + ";";
        page += d->enabled ? "e;" : ";";

        page += String(d->name);
        page += ";" + String(d->addr.toString().c_str()) + ";";

        sprintf(buf, "%u;%i;%.1f;%.1f;%.1f;%lu;%llu;%llu;%i;%i;%i;%i\n",
                d->data.type,
                d->data.getCO2(),
                d->data.getTemperature(),
                d->data.getPressure(),
                d->data.getHumidity(),
                d->data.getRadiationRate(),
                d->data.getRadiationTotal(),
                d->data.getRadiationDuration(),
                d->data.battery,
                d->data.interval,
                d->data.ago,
                (tnow - d->updated) / 1000
        );
        page += String(buf);
    }
    return page;
}

// Deprecated!
String printNewDevices() {
    String page = "#new";
    int index = 0;

    for (AranetDevice* d : newDevices) {
        page += "\n";
        page += String(index++) + ";";
        page += d->csv();
    }

    return page;
}

String printDevices() {
    String page = "#saved";
    int index = 0;

    for (AranetDevice* d : ar4devices) {
        page += "\n";
        page += String(index++) + ";";
        page += d->csv();
    }

    page += "\n#new";
    index = 0;

    for (AranetDevice* d : newDevices) {
        page += "\n";
        page += String(index++) + ";";
        page += d->csv();
    }

    return page;
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
        10000,          /* Stack size of task */
        NULL,           /* parameter of the task */
        1,              /* priority of the task */
        &WiFiTask,      /* Task handle to keep track of created task */
        0);             /* pin task to core 0 */
}


// JS: var ws = new WebSocket("ws://" + location.host + "/ws");
void onWsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len) {
    if(type == WS_EVT_CONNECT){
        Serial.printf("ws[%s][%u] connect\n", server->url(), client->id());
        client->printf("Hello Client %u :)", client->id());
        client->ping();
    } else if(type == WS_EVT_DISCONNECT){
        Serial.printf("ws[%s][%u] disconnect\n", server->url(), client->id());
    } else if(type == WS_EVT_ERROR){
        Serial.printf("ws[%s][%u] error(%u): %s\n", server->url(), client->id(), *((uint16_t*)arg), (char*)data);
    } else if(type == WS_EVT_PONG){
        Serial.printf("ws[%s][%u] pong[%u]: %s\n", server->url(), client->id(), len, (len)?(char*)data:"");
    } else if(type == WS_EVT_DATA){
        AwsFrameInfo * info = (AwsFrameInfo*)arg;
        bool cmdread = true;
        String cmd = "";
        String msg = "";
        if(info->final && info->index == 0 && info->len == len) {
            //the whole message is in a single frame and we got all of it's data
            Serial.printf("ws[%s][%u] %s-message[%llu]: ", server->url(), client->id(), (info->opcode == WS_TEXT)?"text":"binary", info->len);

            if(info->opcode == WS_TEXT){
                for(size_t i=0; i < info->len; i++) {
                    char c = data[i];
                    if (cmdread && c == ':') {
                        cmdread = false;
                    } else {
                        if (cmdread) cmd += c;
                        else msg += c;
                    }
                }

                Serial.print("command: [");
                Serial.print(cmd);
                Serial.print("], message: [");
                Serial.print(msg);
                Serial.println("]");

                if (cmd == "PAIR_BEGIN" || cmd == "PAIR_BEGIN_FORCE") {
                    bool forcePair = cmd == "PAIR_BEGIN_FORCE";
                    // lookup device by mac
                    NimBLEAddress addr(msg.c_str());
                    AranetDevice* d = findSavedDevice(addr);
                    if (!d) {
                        uint8_t rev[6];
                        rev[0] = addr.getNative()[5];
                        rev[1] = addr.getNative()[4];
                        rev[2] = addr.getNative()[3];
                        rev[3] = addr.getNative()[2];
                        rev[4] = addr.getNative()[1];
                        rev[5] = addr.getNative()[0];
                        NimBLEAddress addr2(rev, addr.getType());
                        d = findSavedDevice(addr);
                    }
                    if (d && (forcePair || d->state == STATE_NOT_PAIRED)) {
                        Serial.print("Pair device: ");
                        Serial.println(d->name);

                        ar4callbacks.providePin(-1); // reset pin
                        d->state = STATE_BEGIN_PAIR;
                    } else {
                        client->text("ERROR:Invalid device");
                    }
                } else if (cmd == "PAIR_PIN") {
                    uint32_t pin = msg.toInt();
                    if (pin == 0) {
                        client->text("ERROR:Bad PIN");
                        ar4callbacks.providePin(0);
                    } else {
                        ar4callbacks.providePin(pin);
                    }
                } else if (cmd == "UNPAIR") {
                    NimBLEAddress addr(msg.c_str());
                    AranetDevice* d = findSavedDevice(addr);

                    if (d) {
                        Serial.println("[UNPAIR] Do unpair");
                        NimBLEDevice::deleteBond(d->addr);
                        Serial.println("[UNPAIR] Done unpair");
                        d->state = STATE_NOT_PAIRED;
                        // save state
                        devicesSave();
                        client->text("ERROR:Unpaired.");
                    } else {
                        client->text("ERROR:Device not found.");
                    }
                } else if (cmd == "RENAME") {
                    int split = -1;
                    for(size_t i=0; i < msg.length(); i++) {
                        if (msg.charAt(i) == ';') {
                            split = i;
                            break;
                        }
                    }

                    String addrstr = msg.substring(0, split);
                    String newname = msg.substring(split + 1, split + 24);

                    NimBLEAddress addr(addrstr.c_str());
                    AranetDevice* d = findSavedDevice(addr);

                    if (d) {
                        strcpy(d->name, newname.c_str());
                        // save state
                        devicesSave();
                        client->text("RENAME:OK");
                    } else {
                        client->text("ERROR:Device not found.");
                    }
                }
            }
        } else {
            // no multipart support
        }
    }
}


bool setupWifiAndWebserver(bool restart = false) {
    if (restart) {
        WiFi.disconnect();
        server.end();
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

bool webAuthenticate(AsyncWebServerRequest *request) {
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

    return request->authenticate(www_username, www_password);
}

bool startWebserver() {
    // setup webserver handles
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!webAuthenticate(request)) return request->requestAuthentication();
        request->send(200, "text/html", printHtmlIndex(ar4devices));
    });

    server.on("/settings", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!webAuthenticate(request)) return request->requestAuthentication();
        request->send(200, "text/html", printHtmlConfig(&prefs));
    });

    server.on("/settings", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (!webAuthenticate(request)) return request->requestAuthentication();

        // process data...

        // Generic
        if (request->hasArg(PREF_K_SYS_NAME))     {
            prefs.putString(PREF_K_SYS_NAME, request->arg(PREF_K_SYS_NAME));
        }
        if (request->hasArg(PREF_K_SCAN_REBOOT))     {
            prefs.putUShort(PREF_K_SCAN_REBOOT, request->arg(PREF_K_SCAN_REBOOT).toInt());
        }
        if (request->hasArg(PREF_K_NTP_URL))      {
            prefs.putString(PREF_K_NTP_URL, request->arg(PREF_K_NTP_URL));
        }

        // Wireless
        if (request->hasArg(PREF_K_WIFI_SSID))     {
            prefs.putString(PREF_K_WIFI_SSID, request->arg(PREF_K_WIFI_SSID));
        }
        if (request->hasArg(PREF_K_WIFI_PASSWORD)) {
            prefs.putString(PREF_K_WIFI_PASSWORD, request->arg(PREF_K_WIFI_PASSWORD));
        }

        // IP
        prefs.putBool(PREF_K_WIFI_IP_STATIC, request->hasArg(PREF_K_WIFI_IP_STATIC));
        if (request->hasArg(PREF_K_WIFI_IP_ADDR)) {
            prefs.putUInt(PREF_K_WIFI_IP_ADDR, str2ip(request->arg(PREF_K_WIFI_IP_ADDR)));
        }
        if (request->hasArg(PREF_K_WIFI_IP_MASK)) {
            prefs.putUInt(PREF_K_WIFI_IP_MASK, str2ip(request->arg(PREF_K_WIFI_IP_MASK)));
        }
        if (request->hasArg(PREF_K_WIFI_IP_GW)) {
            prefs.putUInt(PREF_K_WIFI_IP_GW, str2ip(request->arg(PREF_K_WIFI_IP_GW)));
        }
        if (request->hasArg(PREF_K_WIFI_IP_DNS)) {
            prefs.putUInt(PREF_K_WIFI_IP_DNS, str2ip(request->arg(PREF_K_WIFI_IP_DNS)));
        }

        // Wireguard
        prefs.putBool(PREF_K_WG_ENABLED, request->hasArg(PREF_K_WG_ENABLED));
        if (request->hasArg(PREF_K_WG_ENDPOINT))     {
            prefs.putString(PREF_K_WG_ENDPOINT, request->arg(PREF_K_WG_ENDPOINT));
        }
        if (request->hasArg(PREF_K_WG_PORT))     {
            prefs.putUShort(PREF_K_WG_PORT, request->arg(PREF_K_WG_PORT).toInt());
        }
        if (request->hasArg(PREF_K_WG_PUB_KEY))     {
            prefs.putString(PREF_K_WG_PUB_KEY, request->arg(PREF_K_WG_PUB_KEY));
        }
        if (request->hasArg(PREF_K_WG_PRIVATE_KEY))     {
            prefs.putString(PREF_K_WG_PRIVATE_KEY, request->arg(PREF_K_WG_PRIVATE_KEY));
        }
        if (request->hasArg(PREF_K_WG_LOCAL_IP)) {
            prefs.putUInt(PREF_K_WG_LOCAL_IP, str2ip(request->arg(PREF_K_WG_LOCAL_IP)));
        }

        // Influx
        if (request->hasArg(PREF_K_INFLUX_URL)) {
            prefs.putString(PREF_K_INFLUX_URL, request->arg(PREF_K_INFLUX_URL));
        }
        if (request->hasArg(PREF_K_INFLUX_ORG)) {
            prefs.putString(PREF_K_INFLUX_ORG, request->arg(PREF_K_INFLUX_ORG));
        }
        if (request->hasArg(PREF_K_INFLUX_TOKEN)) {
            prefs.putString(PREF_K_INFLUX_TOKEN, request->arg(PREF_K_INFLUX_TOKEN));
        }
        if (request->hasArg(PREF_K_INFLUX_BUCKET)) {
            prefs.putString(PREF_K_INFLUX_BUCKET, request->arg(PREF_K_INFLUX_BUCKET));
        }
        if (request->hasArg(PREF_K_INFLUX_LOG))     {
            prefs.putUShort(PREF_K_INFLUX_LOG, request->arg(PREF_K_INFLUX_LOG).toInt());
        }
        prefs.putUChar(PREF_K_INFLUX_DBVER, request->hasArg(PREF_K_INFLUX_DBVER) ? 2 : 1);

        // MQTT
        if (request->hasArg(PREF_K_MQTT_SERVER)) {
            prefs.putUInt(PREF_K_MQTT_SERVER, str2ip(request->arg(PREF_K_MQTT_SERVER)));
        }
        if (request->hasArg(PREF_K_MQTT_PORT)) {
            prefs.putUShort(PREF_K_MQTT_PORT, request->arg(PREF_K_MQTT_PORT).toInt());
        }
        if (request->hasArg(PREF_K_MQTT_USER)) {
            prefs.putString(PREF_K_MQTT_USER, request->arg(PREF_K_MQTT_USER));
        }
        if (request->hasArg(PREF_K_MQTT_PASSWORD)) {
            prefs.putString(PREF_K_MQTT_PASSWORD, request->arg(PREF_K_MQTT_PASSWORD));
        }

        createInfluxClient();
        ntpSyncTime = 0; // sync now
        ntpSyncFails = 0;

        request->send(200, "text/html", printHtmlConfig(&prefs, true));
    });

    server.on("/data", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!webAuthenticate(request)) return request->requestAuthentication();

        request->send(200, "text/plain", printData());
    });

    server.on("/devices", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!webAuthenticate(request)) return request->requestAuthentication();

        request->send(200, "text/html", printDevicesPage());
    });

    server.on("/devices_list", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!webAuthenticate(request)) return request->requestAuthentication();

        request->send(200, "text/plain", printDevices());
    });

    server.on("/devices_add", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (!webAuthenticate(request)) return request->requestAuthentication();

        if (!request->hasArg("devicemac")) {
            request->send(200, "text/html", "no mac address");
            return;
        }

        String name = "";
        if (request->hasArg("name")) {
            name = request->arg("name");
        }

        String devicemac = request->arg("devicemac");
        NimBLEAddress addr(devicemac.c_str());
        AranetDevice* d = findScannedDevice(addr);
        strcpy(d->name, name.c_str());

        if (!d) {
            request->send(200, "text/html", "unknown mac");
            return;
        }

        // move to saved devices
        ar4devices.push_back(d);
        newDevices.erase(
            std::remove(newDevices.begin(), newDevices.end(), d),
            newDevices.end()
        );

        devicesSave();
        request->send(200, "text/html", "OK");
    });

    server.on("/devices_set", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (!webAuthenticate(request)) return request->requestAuthentication();

        if (!request->hasArg("devicemac")) {
            request->send(200, "text/html", "no mac address");
            return;
        }

        String devicemac = request->arg("devicemac");
        NimBLEAddress addr(devicemac.c_str());

        AranetDevice* d = findSavedDevice(addr);

        if (!d) {
            request->send(200, "text/html", "unknown mac");
            return;
        }

        int changed = 0;

        if (request->hasArg("enabled")) {
            bool en = request->arg("enabled").toInt();
            if (d->enabled != en) {
                ++changed;
                d->enabled = en;
            }
        }

        if (request->hasArg("history")) {
            bool en = request->arg("history").toInt();
            if (d->history != en) {
                ++changed;
                d->history = en;
            }
        }

        if (request->hasArg("gatt")) {
            bool en = request->arg("gatt").toInt();
            if (d->gatt != en) {
                ++changed;
                d->gatt = en;
            }
        }

        if (changed) {
            devicesSave();
        }
        request->send(200, "text/html", "OK");
    });

    server.on("/clrbnd", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!webAuthenticate(request)) return request->requestAuthentication();

        ble_store_clear();
        wipeStoredDevices();
        request->send(200, "text/html", "removed paired devices");
    });

    server.on("/restart", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!webAuthenticate(request)) return request->requestAuthentication();

        request->send(200, "text/html", "restarting...");
        delay(1000);
        ESP.restart();
    });

    server.on("/force", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!webAuthenticate(request)) return request->requestAuthentication();

        String devicemac = request->arg("devicemac");
        NimBLEAddress addr(devicemac.c_str());
        AranetDevice* d = findSavedDevice(addr);

        if (!d) {
            request->send(200, "text/html", "invalid mac");
            return;
        }

        uint16_t count = 2048; // max
        if (request->hasArg("count")) {
            count = request->arg("count").toInt();
        }

        char buf[60];
        sprintf(buf, "Will download %u records...", count);

        if (request->hasArg("hrs")) {
            int hours = request->arg("hrs").toInt();
            int seconds = hours * 3600;
            int interval = d->data.interval;
            if (interval < 1) {
                request->send(200, "text/html", "Unknown interval. Can't download.");
                return;
            } else {
                count = seconds / interval;
                sprintf(buf, "Will download %u records (%u hours)...", count, hours);
            }
        }

        request->send(200, "text/html", buf);

        d->pending = count;
    });

    server.on("/devices_template", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!webAuthenticate(request)) return request->requestAuthentication();
        request->send(200, "text/html", deviceCardHtml);
    });

    server.on("/devices_template2", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!webAuthenticate(request)) return request->requestAuthentication();
        request->send(200, "text/html", deviceCard2Html);
    });

    server.on("/devices_template3", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!webAuthenticate(request)) return request->requestAuthentication();
        request->send(200, "text/html", deviceCard3Html);
    });

    // Image resources
    // server static content from images folder with 10 minute cache
    if (spiffsOk) {
        server.serveStatic("/img/", SPIFFS, "/img/", "max-age=86400"); // 1 day cache
        server.serveStatic("/js/", SPIFFS, "/js/", "max-age=86400"); // 1 day cache
        server.serveStatic("/devices.json", SPIFFS, "/devices.json", "max-age=1"); // no cache
    }

    server.onNotFound([](AsyncWebServerRequest *request) {
      request->send(404, "text/html", "404");
    });

    ws.onEvent(onWsEvent);
    server.addHandler(&ws);
    server.begin();

    return true;
}

void task_sleep(uint32_t ms) {
    vTaskDelay(ms / portTICK_PERIOD_MS);
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
        0                 /* pin task to core 0 */
    );
}

bool ntpSync() {
    Serial.println("NTP: sync time");
    configTime(0, 0, prefs.getString(PREF_K_NTP_URL).c_str());
    struct tm timeinfo;

    long timeout = millis() + 10000;
    while(time(nullptr) <= 100000 && millis() < timeout) {
        delay(100);
    }

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
        task_sleep(1);
    }
}

void restart(TimerHandle_t xTimer) {
    log("watchdog: timed out. Restarting ESP32...", ILog::ERROR);
    ESP.restart();
}

// watchdog stuff
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

AranetDevice* findScannedDevice(NimBLEAddress macaddr) {
    AranetDevice* d = nullptr;

    for (AranetDevice* d : newDevices) {
        if (d->addr.equals(macaddr)) return d;
    }

    return nullptr;
}

AranetDevice* findSavedDevice(NimBLEAddress macaddr) {
    AranetDevice* d = nullptr;

    for (AranetDevice* d : ar4devices) {
        if (d->addr.equals(macaddr)) return d;
    }

    return nullptr;
}

AranetDevice* findSavedDevice(NimBLEAdvertisedDevice* adv) {
    // find matching aranet device
    return findSavedDevice(adv->getAddress());
}

void registerScannedDevice(NimBLEAdvertisedDevice* adv, char* name) {
    // find existing
    NimBLEAddress umac = adv->getAddress();
    int rssi = adv->getRSSI();

    AranetDevice* dev = nullptr;

    for (AranetDevice* d : ar4devices) {
        if (d->equals(umac)) {
            dev = d;
            break;
        }
    }

    if (!dev) {
        // try scanned
        for (AranetDevice* d : newDevices) {
            NimBLEAddress dmac = NimBLEAddress(d->addr);
            if (d->equals(umac)) {
                dev = d;
                if (name == nullptr) strcpy(dev->name, adv->getName().c_str());
                break;
            }
        }

        // make new
        if (!dev) {
            dev = new AranetDevice();
            if (name != nullptr) {
                strcpy(dev->name, name);
            } else {
                strcpy(dev->name, adv->getName().c_str());
            }
            dev->addr = umac;
            newDevices.push_back(dev);
        }
    }

    if (dev) {
        dev->lastSeen = millis();
        dev->rssi = rssi;
    }
}

void cleanupScannedDevices() {
    for (std::vector<AranetDevice*>::iterator it=newDevices.begin(); it!=newDevices.end(); ) {
        AranetDevice* d = *it;
        if ((millis() - d->lastSeen) > (5 * 60 * 1000)) {
            it = newDevices.erase(it);
            delete d;
        } else {
            ++it;
        }
    }
}

void printScannecDevices() {
    Serial.println("----------------------");
    for (AranetDevice* d : newDevices) {
        NimBLEAddress adr(d->addr);
        long ago = millis() - d->lastSeen;
        Serial.printf("[%s],  %lu ms\n", adr.toString().c_str(), ago);
    }
}

void setupWireguard() {
    if (!isAp && prefs.getBool(PREF_K_WG_ENABLED)) {
        Serial.println("Initializing WireGuard...");
        IPAddress local_ip(htobe32(prefs.getUInt(PREF_K_WG_LOCAL_IP)));
        wg.begin(
            local_ip,
            prefs.getString(PREF_K_WG_PRIVATE_KEY).c_str(),
            prefs.getString(PREF_K_WG_ENDPOINT).c_str(),
            prefs.getString(PREF_K_WG_PUB_KEY).c_str(),
            prefs.getUShort(PREF_K_WG_PORT, 13231));
    }
}

#endif
