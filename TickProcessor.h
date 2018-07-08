#ifndef TICK_PROCESSOR_H
#define TICK_PROCESSOR_H

#include "CircBuf.h"
#include "WebClient.h"
#include "LCD.h"

struct NAV_PVT;

// 20 bytes
struct TickData {
  // GPS time of week of the navigation epoch (ms)
  uint32_t iTOW;
  // Longitude (deg)
  int32_t lon;
  // Latitude (deg)
  int32_t lat;
  // Ground Speed (2-D) (mm/s), 235.926Km/h max for 65535
  uint16_t gSpeed;
  // 200 <-> 10m, 0.05m step, min 0m, max 10m
  uint8_t hAcc;
  // 200 <-> 10m, 0.05m step, min 0m, max 10m
  uint8_t sAcc;
  // 16384 per G, 4G max
  uint16_t accel;
  // Height above mean sea level, 10 <-> 1m, 0.1m step, min -3.2km, max +3.2km
  int16_t hMSL;

  void Pack(const NAV_PVT& p, float accel);
};

struct TickProcessor {
  CircBuf<TickData, 300> data;
  
  void Process(const TickData& tick, void* meta, int meta_size, Metering *metering);
};
#endif
