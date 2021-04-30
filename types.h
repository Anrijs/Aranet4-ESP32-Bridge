#ifndef __AR4BR_TYPES_H
#define __AR4BR_TYPES_H

#include "config.h"
#include "Aranet4.h"

typedef struct AranetDevice {
    uint8_t addr[6];
    char name[24];
};

// Saved device config
typedef struct AranetDeviceConfig {
    uint16_t version = CFG_DEV_VER;
    uint16_t size = 0;
    uint8_t _reserved[32]; 
    AranetDevice devices[CFG_MAX_DEVICES];
};

typedef struct AranetDeviceStatus {
    AranetDevice* device;
    AranetData data;
    long updated = 0;
};

#endif
