# Aranet4-ESP32 Bridge
Connect your Aranet4 to ESP32 and view measurements in browser or send them to InfluxDB and MQTT server.

## Installation
1. Set up [Arduino](https://www.arduino.cc/) with [ESP32 board library](https://github.com/espressif/arduino-esp32) (tested on v1.0.6)
2. Install Arduino libraries:
	* [Aranet4-ESP32](https://github.com/Anrijs/Aranet4-ESP32)
	* [InfluxDB-Client-for-Arduino](https://github.com/tobiasschuerg/InfluxDB-Client-for-Arduino)
	* [Arduino Mqtt Client](https://github.com/arduino-libraries/ArduinoMqttClient/)
3. Change Tools->Partition scheme to`No OTA (2MB APP/2MB SPIFFS)`
4. Compile and flash ESP32
5. For image resources SPIFFS also needs to be flashed. Install[arduino-esp32fs-plugin](https://github.com/me-no-dev/arduino-esp32fs-plugin) and after restarting Arduino IDE use Tools->ESP32 Sketch Data Upload

## Usage
1. On first boot, Wi-Fi access point will be created. SSID by default is `Aranet4-ESP32 Bridge` and password: `Ar@net4Br1dge`. You can change these in `config.h` file.

2. When connected to AP go to http://192.168.4.1 and make sure page opens. Username and password will be required here. By default, username is `admin` and password is same as AP Wi-Fi password (`Ar@net4Br1dge`). You can also change username in  `config.h`  file.

3. When connected to AP, go to settings: http://192.168.4.1/settings. Enter your Wi-Fi name and password here. After restart, ESP32 will connect to this network.

4. ow you can pair ESP32 with Aranet4. In home screen click `Add new device` or go to http://\<ip\>/scan. Wait for scan to end (5 seconds) and click `Pair device` to start pairing process.

5. PIN code dialog should now pop up. Enter PIN code shown on Aranet4 screen and click OK. Your Aranet4 now should be paired with ESP32.

Now in home screen You should see measurements from all paired Aranet4 devices.

## InfluxDB
If InfluxDB is set up, all measurements will be sent to database right after new measaurement has been made.

If Some measurements have been skipped, since last successful measurement, program will atempt to read and send missed measurements, with adjusted timestamp.

## MQTT
If MQTT client is set up, it will send measurements to server right after new measaurement has been made. Data is sent to following topics:

`aranet4bridge/<deviceid>/co2`

`aranet4bridge/<deviceid>/temperature`

`aranet4bridge/<deviceid>/pressure`

`aranet4bridge/<deviceid>/humidity`

`aranet4bridge/<deviceid>/battery`


Aranet4 name is used for `<deivceid>`. If name is `Aranet4 000ABC`, `<deviceid>` will be `aranet4-000abc`