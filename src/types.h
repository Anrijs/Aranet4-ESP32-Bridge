#ifndef __AR4BR_TYPES_H
#define __AR4BR_TYPES_H

#include "config.h"
#include "Aranet4.h"
#include "utils.h"

enum DeviceType {
    UNKNOWN = 0,
    ARANET4 = 1
};

enum PairState {
    STATE_NOT_PAIRED,
    STATE_BEGIN_PAIR,
    STATE_PAIRING,
    STATE_PAIRED
};

typedef struct AranetDevice { 
    NimBLEAddress addr;
    char name[24];

    // pair status
    PairState state = STATE_NOT_PAIRED;

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
        device["mac"] = String(addr.toString().c_str()); // make string because, otherrwise it will use same mac for all devices. 
        device["name"] = name;

        JsonObject settings = device.createNestedObject("settings");
        settings["paired"] = state == STATE_PAIRED;
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
        if (state == STATE_PAIRED) page += 'p';
        if (state == STATE_BEGIN_PAIR || state == STATE_PAIRING) page += 'P';
        if (history) page += 'h';
        if (gatt)    page += 'g';

        page += ";";

        return page;
    }
};

#endif
