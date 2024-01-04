#include "airvalent.h"

uint8_t AirvalentData::_lbits(uint8_t byte, uint8_t bits) {
    uint8_t mask = 0;
    for (uint8_t i=0;i<bits;i++) mask |= 0x80 >> i;
    return (byte & mask) >> (8 - bits);
}

uint8_t AirvalentData::_rbits(uint8_t byte, uint8_t bits) {
    uint8_t mask = 0;
    for (uint8_t i=0;i<bits;i++) mask |= 1 << i;
    return (byte & mask);
}

airv_err_t AirvalentData::parseFromGATT(uint8_t* data) {
    co2 = data[0] | _rbits(data[1], 7) << 8;
    temperature = (_lbits(data[1], 1) | data[2] << 1);
    if (_rbits(data[3], 1) > 0) temperature = -temperature;
    humidity = (_lbits(data[3], 7) | (_rbits(data[4], 2) << 7));
    pressure = (_lbits(data[4], 5) | (_rbits(data[5], 6) << 5));
    return AIRV_OK;
}


/// 

Airvalent::Airvalent(NimBLEClientCallbacks* callbacks) {
    pClient = NimBLEDevice::createClient();
    pClient->setClientCallbacks(callbacks, false);
}

Airvalent::~Airvalent() {
    disconnect();
    NimBLEDevice::deleteClient(pClient);
}

/**
 * @brief Initialize ESP32 bluetooth device and security profile
 * @param [in] cllbacks Pointer to AirvalentCallbacks class callback
 */
void Airvalent::init(uint16_t mtu) {
    // Set up bluetooth device and security
    NimBLEDevice::init("");
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);
    NimBLEDevice::setSecurityAuth(true, true, true);
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_KEYBOARD_ONLY);
    NimBLEDevice::setMTU(mtu);
}

/**
 * @brief Connect to Airvalent device
 * @param [in] adv Advertised bluetooth device
 * @param [in] secure Start in secure mode (bonded)
 * @return status code
 */
airv_err_t Airvalent::connect(NimBLEAdvertisedDevice* adv, bool secure) {
    if (pClient != nullptr && pClient->isConnected()) {
        Serial.println("WARNING: Previous connection was not closed and will be disconnected.");
        pClient->disconnect();
    }

    if(pClient->connect(adv)) {
        if (secure) return secureConnection();
        return AIRV_OK;
    } else {
        return AIRV_ERR_NOT_CONNECTED;
    }

    return AIRV_FAIL;
}

/**
 * @brief Connect to Airvalent device
 * @param [in] addr Address of bluetooth device
 * @param [in] secure Start in secure mode (bonded)
 * @return status code
 */
airv_err_t Airvalent::connect(NimBLEAddress addr, bool secure) {
    if (pClient != nullptr && pClient->isConnected()) {
        Serial.println("WARNING: Previous connection was not closed and will be disconnected.");
        pClient->disconnect();
    }

    if(pClient->connect(addr)) {
        if (secure) return secureConnection();
        return AIRV_OK;
    } else {
        return AIRV_ERR_NOT_CONNECTED;
    }

    return AIRV_FAIL;
}

/**
 * @brief Connect to Airvalent device
 * @param [in] addr Address of bluetooth device
 * @param [in] secure Start in secure mode (bonded)
 * @param [in] type Address type
 * @return status code
 */
airv_err_t Airvalent::connect(uint8_t* addr, bool secure, uint8_t type) {
    return connect(NimBLEAddress(addr, type), secure);
}

/**
 * @brief Connect to Airvalent device
 * @param [in] addr Address of bluetooth device
 * @param [in] secure Start in secure mode (bonded)
 * @param [in] type Address type
 * @return status code
 */
airv_err_t Airvalent::connect(String addr, bool secure, uint8_t type) {
    std::string addrstr(addr.c_str());
    return connect(NimBLEAddress(addrstr, type), secure);
}

/**
 * @brief Start secure mode / bond device
 * @return status code
 */
airv_err_t Airvalent::secureConnection() {
    if (pClient->secureConnection()) {
        return AIRV_OK;
    }
    return AIRV_FAIL;
}

/**
 * @brief Disconnects from bluetooth device
 */
void Airvalent::disconnect() {
    if (pClient != nullptr && pClient->isConnected()) {
      pClient->disconnect();
    }
}

/**
 * @brief Set the timeout to wait for connection attempt to complete
 */
void Airvalent::setConnectTimeout(uint8_t time) {
    if (pClient != nullptr) {
      pClient->setConnectTimeout(time);
    }
}

/**
 * @brief Are we connected to a server?
 */
bool Airvalent::isConnected() {
    return pClient != nullptr && pClient->isConnected();
}

/**
 * @brief Current readings from Airvalent
 */
AirvalentData Airvalent::getCurrentReadings() {
    AirvalentData data;
    uint8_t raw[100];
    uint16_t len = 100;

    status = getValue(getAirvalentService(), UUID_Airvalent_CurrentReadings, raw, &len);
    if (status == AIRV_OK)
        status = data.parseFromGATT(raw);

    return data;
}

/**
 * @brief Airvalent measurement intervals
 */
uint16_t Airvalent::getInterval() {
    return getU16Value(getAirvalentService(), UUID_Airvalent_Interval);
}

/**
 * @brief Airvalent battery status
 */
uint16_t Airvalent::getBattery() {
    return getU16Value(getAirvalentService(), UUID_Airvalent_Battery);
}

/**
 * @brief Status code of last action
 */
airv_err_t Airvalent::getStatus() {
    return status;
}

/**
 * @brief Reads raw data from Airvalent
 * @param [in] serviceUuid GATT Service UUID to read
 * @param [in] charUuid GATT Char UUID to read
 * @param [out] data Pointer to where received data will be stored
 * @param [in|out] Size of data on input, received data size on output (truncated if larger than input)
 * @return Read status code (AIRV_READ_*)
 */
airv_err_t Airvalent::getValue(NimBLEUUID serviceUuid, NimBLEUUID charUuid, uint8_t* data, uint16_t* len) {
    if (pClient == nullptr) return AIRV_ERR_NO_CLIENT;
    if (!pClient->isConnected())  return AIRV_ERR_NOT_CONNECTED;
    return getValue(pClient->getService(serviceUuid), charUuid, data, len);
}

/**
 * @brief Reads raw data from Airvalent
 * @param [in] service GATT Service to read
 * @param [in] charUuid GATT Char UUID to read
 * @param [out] data Pointer to where received data will be stored
 * @param [in|out] Size of data on input, received data size on output (truncated if larger than input)
 * @return Read status code (AIRV_READ_*)
 */
airv_err_t Airvalent::getValue(NimBLERemoteService* service, NimBLEUUID charUuid, uint8_t* data, uint16_t* len) {
    if (pClient == nullptr) return AIRV_ERR_NO_CLIENT;
    if (!pClient->isConnected())  return AIRV_ERR_NOT_CONNECTED;
    if (service == nullptr) return AIRV_ERR_NO_GATT_SERVICE;

    NimBLERemoteCharacteristic* pRemoteCharacteristic = service->getCharacteristic(charUuid);
    if (pRemoteCharacteristic == nullptr) {
        return AIRV_ERR_NO_GATT_CHAR;
    }

    // Read the value of the characteristic.
    if(pRemoteCharacteristic->canRead()) {
        std::string str = pRemoteCharacteristic->readValue();
        if (str.length() < *len) *len = str.length();
        memcpy(data, str.c_str(), *len);
        return AIRV_OK;
    }

    return AIRV_FAIL;
}

/**
 * @brief Reads string value from Airvalent
 * @param [in] serviceUuid GATT Service UUID to read
 * @param [in] charUuid GATT Char UUID to read
 * @return String value
 */
String Airvalent::getStringValue(NimBLEUUID serviceUuid, NimBLEUUID charUuid) {
    return getStringValue(pClient->getService(serviceUuid), charUuid);
}

/**
 * @brief Reads string value from Airvalent
 * @param [in] service GATT Service to read
 * @param [in] charUuid GATT Char UUID to read
 * @return String value
 */
String Airvalent::getStringValue(NimBLERemoteService* service, NimBLEUUID charUuid) {
    uint8_t buf[33];
    uint16_t len = 32;
    status = getValue(service, charUuid, buf, &len);
    buf[len] = 0; // trerminate string
    return String((char *) buf);
}

/**
 * @brief Reads u16 value from Airvalent
 * @param [in] serviceUuid GATT Service UUID to read
 * @param [in] charUuid GATT Char UUID to read
 * @return u16 value
 */
uint16_t Airvalent::getU16Value(NimBLEUUID serviceUuid, NimBLEUUID charUuid) {
    return getU16Value(pClient->getService(serviceUuid), charUuid);
}

/**
 * @brief Reads u16 value from Airvalent
 * @param [in] service GATT Service to read
 * @param [in] charUuid GATT Char UUID to read
 * @return u16 value
 */
uint16_t Airvalent::getU16Value(NimBLERemoteService* service, NimBLEUUID charUuid) {
    uint16_t val = 0;
    uint16_t len = 2;
    status = getValue(service, charUuid, (uint8_t *) &val, &len);

    if (len == 2) {
        return val;
    }

    status = AIRV_FAIL;
    return 0;
}

NimBLERemoteService* Airvalent::getAirvalentService() {
    NimBLERemoteService* pRemoteService = pClient->getService(UUID_Airvalent);
    return pRemoteService;
}
