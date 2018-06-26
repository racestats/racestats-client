#ifndef TICK_PROCESSOR_H
#define TICK_PROCESSOR_H

#include "CircBuf.h"
#include "WebClient.h"
#include "LCD.h"

struct TickData {
  unsigned long iTOW;          // GPS time of week of the navigation epoch (ms)
  long lon;                    // Longitude (deg)
  long lat;                    // Latitude (deg)
  long gSpeed;                 // Ground Speed (2-D) (mm/s)
  unsigned long hAcc;
  unsigned long sAcc;
  long accel; // 16384 per G
  long hMSL;                   // Height above mean sea level (mm)
};


struct TickProcessor {
  CircBuf<TickData, 300> data;
  WebClient& web;
  LCD& lcd;

  TickProcessor(WebClient& w, LCD& l) : web(w), lcd(l) {}
  
  void Process(const TickData& tick, void* meta, int meta_size, Metering *metering);
};
#endif
