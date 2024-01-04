/*
 *  Name:       airvalent.h
 *  Created:    2023-12-26
 *  Author:     Anrijs Jargans <anrijs@anrijs.lv>
 *  Url:        https://github.com/Anrijs/Airvalent-ESP32
 */

#ifndef __AIRVALENT_H
#define __AIRVALENT_H

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <NimBLEDevice.h>

// services
static NimBLEUUID UUID_Airvalent("b81c94a4-6b2b-4d41-9357-0c8229ea02df");

// chars
static NimBLEUUID UUID_Airvalent_CurrentReadings("e8694d30-a155-4fb4-befa-548e64c88965");
static NimBLEUUID UUID_Airvalent_Battery        ("e8694d30-a155-4fb4-befa-548e64c88965");
static NimBLEUUID UUID_Airvalent_Interval       ("b1c48eea-4f5c-44f7-9797-73e0ce294881");

// b'\x02\x15\xe2\xc5m\xb5\xdf\xfbH\xd2\xb0`\xd0\xf5\xa7\x10\x96\xe0\x88\xab\x04\x01\xdf'
// b'\x02\x15\xe2\xc5m\xb5\xdf\xfbH\xd2\xb0`\xd0\xf5\xa7\x10\x96\xe0\x88\xab\x04\x01\xdf'
// b'\x02\x15\xe2\xc5m\xb5\xdf\xfbH\xd2\xb0`\xd0\xf5\xa7\x10\x96\xe0\x88\xab\x04\x01\xdf'

typedef uint16_t airv_err_t;

// airv_err_t Status/Error codes
#define AIRV_OK           0 // airv_err_t value indicating success (no error)
#define AIRV_FAIL        -1 // Generic airv_err_t code indicating failure

#define AIRV_ERR_NO_GATT_SERVICE    0x01
#define AIRV_ERR_NO_GATT_CHAR       0x02
#define AIRV_ERR_NO_CLIENT          0x03
#define AIRV_ERR_NOT_CONNECTED      0x04

class AirvalentData {
public:
    // 8 bytes total ?
    uint16_t co2;
    int16_t temperature;
    uint16_t humidity;
    uint16_t pressure;

    uint8_t  battery;
    uint16_t  interval;

    airv_err_t parseFromGATT(uint8_t* data);

private:
    // datatype processing
    uint8_t _lbits(uint8_t byte, uint8_t bits);
    uint8_t _rbits(uint8_t byte, uint8_t bits);
};

class Airvalent {
public:
    Airvalent(NimBLEClientCallbacks* callbacks);
    ~Airvalent();
    static void init(uint16_t mtu = 247);
    airv_err_t connect(NimBLEAdvertisedDevice* adv, bool secure = true);
    airv_err_t connect(NimBLEAddress addr, bool secure = true);
    airv_err_t connect(uint8_t* addr, bool secure = true, uint8_t type = BLE_ADDR_RANDOM);
    airv_err_t connect(String addr, bool secure = true, uint8_t type = BLE_ADDR_RANDOM);
    airv_err_t secureConnection();
    void      disconnect();
    void      setConnectTimeout(uint8_t time);
    bool      isConnected();

    AirvalentData getCurrentReadings();
    //uint16_t      getSecondsSinceUpdate();
    //uint16_t      getTotalReadings();
    uint16_t      getInterval();
    //String        getName();
    //String        getSwVersion();
    //String        getFwVersion();
    //String        getHwVersion();

    //ar4_err_t   writeCmd(uint8_t* data, uint16_t len);

    //int         getHistoryCO2(int start, uint16_t count, uint16_t* data);
    //int         getHistoryTemperature(int start, uint16_t count, uint16_t* data);
    //int         getHistoryPressure(int start, uint16_t count, uint16_t* data);
    //int         getHistoryHumidity(int start, uint16_t count, uint16_t* data);
    //int         getHistoryHumidity2(int start, uint16_t count, uint16_t* data);
    //int         getHistory(int start, uint16_t count, AranetDataCompact* data, uint8_t params = AR4_PARAM_FLAGS);
    airv_err_t   getStatus();
private:
    NimBLEClient* pClient = nullptr;
    airv_err_t status = AIRV_OK;

    NimBLERemoteService* getAirvalentService();

    airv_err_t getValue(NimBLEUUID serviceUuid, NimBLEUUID charUuid, uint8_t* data, uint16_t* len);;
    airv_err_t getValue(NimBLERemoteService* service, NimBLEUUID charUuid, uint8_t* data, uint16_t* len);;
    String    getStringValue(NimBLEUUID serviceUuid, NimBLEUUID charUuid);
    String    getStringValue(NimBLERemoteService* service, NimBLEUUID charUuid);
    uint16_t  getU16Value(NimBLEUUID serviceUuid, NimBLEUUID charUuid);
    uint16_t  getU16Value(NimBLERemoteService* service, NimBLEUUID charUuid);

    // History stuff
    //int       getHistoryByParam(int start, uint16_t count, uint16_t* data, uint8_t param);
    //airv_err_t subscribeHistory(uint8_t* cmd);

    //static QueueHandle_t historyQueue;
    //static void historyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify);
};
#endif