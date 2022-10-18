#ifndef __AR4BR_TYPES_H
#define __AR4BR_TYPES_H

#include "config.h"
#include "Aranet4.h"
#include "utils.h"

enum DeviceType {
    UNKNOWN = 0,
    ARANET4 = 1
};

typedef struct AranetDevice { 
    NimBLEAddress addr;
    char name[24];

    // pair status
    bool paired;

    // toggles
    bool enabled;
    bool gatt;
    bool history;

    // moved from status struct
    AranetData data;
    long updated = 0;
    uint16_t pending = 0;
    bool mqttReported = false;

    // extra data
    int rssi;
    long lastSeen;

    void saveConfig(DynamicJsonDocument &doc) {
        JsonArray devices;

        if (doc.containsKey("devices")) {
            devices = doc["devices"];
        } else {
            devices = doc.createNestedArray("devices");
        }

        JsonObject device = devices.createNestedObject();
        device["mac"] = addr.toString().c_str();
        device["name"] = name;

        JsonObject settings = device.createNestedObject("settings");
        settings["paired"] = paired;
        settings["enabled"] = enabled;
        settings["gatt"] = gatt;
        settings["history"] = history;
    }

    bool equals(NimBLEAddress address) {
        return addr.equals(address);
    }

    String csv() {
        String page = String(addr.toString().c_str());
        page += ";";
        page += String(name);
        page += ";";
        page += String(rssi);
        page +=";";
        page += String(millis() - lastSeen);
        page += ";";

        // flags
        if (enabled) page += 'e';
        if (paired)  page += 'p';
        if (history) page += 'h';
        if (gatt)    page += 'g';

        page += ";";

        return page;
    }
};

#endif
