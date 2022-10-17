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
    uint8_t addr[6]; // TODO: replace with NimBLEAddress
    char name[24];
    bool paired;
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

        char buf[24];
        mac2str(addr, buf, true);

        JsonObject device = devices.createNestedObject();
        device["mac"] = buf;
        device["name"] = name;

        JsonObject settings = device.createNestedObject("settings");
        settings["paired"] = paired;
        settings["enabled"] = enabled;
        settings["gatt"] = gatt;
        settings["history"] = history;
    }

    bool equals(NimBLEAddress address) {
      return memcmp(address.getNative(), addr, 6) == 0;
    }
};

#endif
