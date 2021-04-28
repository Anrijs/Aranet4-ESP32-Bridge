#ifndef __INFLUX_H
#define __INFLUX_H

#include "../main.h"
#include "../types.h"

// https://github.com/tobiasschuerg/InfluxDB-Client-for-Arduino
#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>
#include "Aranet4.h"

InfluxDBClient* influxCreateClient(NodeConfig *nodeCfg) {
  if (nodeCfg->dbver == 2) {
    InfluxDBClient* influxClient = new InfluxDBClient(nodeCfg->url, nodeCfg->organisation, nodeCfg->bucket, nodeCfg->token, InfluxDbCloud2CACert);
    influxClient->setWriteOptions(WriteOptions().bufferSize(WRITE_BUFFER_SIZE));
    return influxClient;
  }
  return new InfluxDBClient(nodeCfg->url, nodeCfg->bucket);
}

Point influxCreateStatusPoint(NodeConfig *nodeCfg) {
    Point point("device_status");
    point.addTag("device", nodeCfg->name);
    point.addField("rssi", WiFi.RSSI());
    point.addField("uptime", millis());
    point.addField("heap_free", ESP.getFreeHeap());
    point.addField("heap_used", ESP.getHeapSize());
    return point;
}

Point influxCreatePoint(NodeConfig *nodeCfg, AranetDevice* device, AranetData *data) {
    Point point("aranet4");
    point.addTag("device", nodeCfg->name);
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

void influxSendPoint(InfluxDBClient *influxClient, Point pt) {
    influxClient->writePoint(pt);
}

void influxFlushBuffer(InfluxDBClient *influxClient) {
    if (!influxClient->isBufferEmpty()) {
        influxClient->flushBuffer();
    }
}

#endif // __INFLUX_H