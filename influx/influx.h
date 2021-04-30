#ifndef __INFLUX_H
#define __INFLUX_H

#include "../main.h"
#include "../types.h"

// https://github.com/tobiasschuerg/InfluxDB-Client-for-Arduino
#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>
#include "Aranet4.h"

InfluxDBClient* influxCreateClient(Preferences *prefs) {
  InfluxDBClient* influxClient = nullptr;

  if (prefs->getString("url").length() < 1 || prefs->getString("bucket").length() < 1) {
      return influxClient;
  }

  if (prefs->getUChar("dbver") == 2) {
    influxClient = new InfluxDBClient(
        prefs->getString("url").c_str(),
        prefs->getString("organisation").c_str(),
        prefs->getString("bucket").c_str(),
        prefs->getString("token").c_str(),
        InfluxDbCloud2CACert
    );
  } else {
    influxClient = new InfluxDBClient(prefs->getString("url").c_str(), prefs->getString("bucket").c_str());
  }

  influxClient->setWriteOptions(WriteOptions().writePrecision(WRITE_PRECISION).bufferSize(WRITE_BUFFER_SIZE));
  return influxClient;
}

Point influxCreateStatusPoint(Preferences *prefs) {
    Point point("device_status");
    point.addTag("device", prefs->getString("name"));
    point.addField("rssi", WiFi.RSSI());
    point.addField("uptime", millis());
    point.addField("heap_free", ESP.getFreeHeap());
    point.addField("heap_used", ESP.getHeapSize());
    return point;
}

Point influxCreatePoint(Preferences *prefs, AranetDevice* device, AranetData *data) {
    Point point("aranet4");
    point.addTag("device", prefs->getString("name"));
    point.addTag("name", device->name);
    point.addField("co2", data->co2);
    point.addField("temperature", data->temperature / 20.0);
    point.addField("pressure", data->pressure / 10.0);
    point.addField("humidity", data->humidity);
    point.addField("battery", data->battery);
    point.addField("interval", data->interval);
    point.addField("ago", data->ago);
    return point;
}

Point influxCreatePointWithTimestamp(Preferences *prefs, AranetDevice* device, AranetData *data, long timestamp) {
    Point point = influxCreatePoint(prefs, device, data);
    point.setTime(WRITE_PRECISION);
    point.setTime(timestamp);
    return point;
}

void influxSendPoint(InfluxDBClient *influxClient, Point pt) {
    if (influxClient != nullptr) {
        influxClient->writePoint(pt);
    }
}

void influxFlushBuffer(InfluxDBClient *influxClient) {
    if (influxClient != nullptr && !influxClient->isBufferEmpty()) {
        influxClient->flushBuffer();
    }
}

#endif // __INFLUX_H