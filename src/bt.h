#ifndef __AR4BR_BT_H
#define __AR4BR_BT_H

#include "Aranet4.h"
#include "utils.h"

#define DEVICE_NAME_MAX_LEN  18

class MyAranet4Callbacks: public Aranet4Callbacks {
    uint32_t pin = -1;

    uint32_t onPinRequested() {
        Serial.println("PIN Requested. Waiting for web or timeout");
        long timeout = millis() + 30000;
        while (pin == -1 && timeout > millis())
            vTaskDelay(500 / portTICK_PERIOD_MS);

        uint32_t ret = pin;
        pin = -1;

        return  ret;
    }
public:
    bool providePin(uint32_t newpin) {
        if (pin == -1) {
            pin = newpin;
            return true;
        }

        return false;
    }
};

#endif
