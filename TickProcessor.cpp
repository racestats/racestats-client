#include "TickProcessor.h"
#include "Config.h"

struct MetaData {
  unsigned char cls;
  unsigned char id;
  unsigned short protocol_id;  // backend should know how to parse data
  unsigned long iTOW;          // GPS time of week of the navigation epoch (ms)

  unsigned short year;         // Year (UTC)
  unsigned char month;         // Month, range 1..12 (UTC)
  unsigned char day;           // Day of month, range 1..31 (UTC)
  unsigned char hour;          // Hour of day, range 0..23 (UTC)
  unsigned char minute;        // Minute of hour, range 0..59 (UTC)
  unsigned char second;        // Seconds of minute, range 0..60 (UTC)
  char valid;                  // Validity Flags (see graphic below)
  unsigned long tAcc;          // Time accuracy estimate (UTC) (ns)
  long nano;                   // Fraction of second, range -1e9 .. 1e9 (UTC) (ns)
  unsigned char fixType;       // GNSSfix Type, range 0..5
  char flags;                  // Fix Status Flags
  unsigned char reserved1;     // reserved
  unsigned char numSV;         // Number of satellites used in Nav Solution

  long lon;                    // Longitude (deg)
  long lat;                    // Latitude (deg)
  long height;                 // Height above Ellipsoid (mm)
  long hMSL;                   // Height above mean sea level (mm)
  unsigned long hAcc;          // Horizontal Accuracy Estimate (mm)
  unsigned long vAcc;          // Vertical Accuracy Estimate (mm)

  long velN;                   // NED north velocity (mm/s)
  long velE;                   // NED east velocity (mm/s)
  long velD;                   // NED down velocity (mm/s)
  long gSpeed;                 // Ground Speed (2-D) (mm/s)
  long heading;                // Heading of motion 2-D (deg)
  unsigned long sAcc;          // Speed Accuracy Estimate
  unsigned long headingAcc;    // Heading Accuracy Estimate
  unsigned short pDOP;         // Position dilution of precision
  short reserved2;             // Reserved
  unsigned long reserved3;     // Reserved
};

const unsigned short PROTOCOL_ID = 86;

const int BACK_ACCEL_FINISHED_OFS = 10;
const int SAMPLE_OFS = 10;
const int ACCEL_SPEED_MIN = (int)(60 / 0.0036); // accelerate to at least 60 Kmh to save speedrun
const int BREAK_SPEED_MIN = (int)(60 / 0.0036);

const int SPEED_100 = (int)(100 / 0.0036);
const int SPEED_80 = (int)(80 / 0.0036);
const int SPEED_60 = (int)(60 / 0.0036);

bool have_zero_speed = false;
bool have_decel_speed = false;

//#define CHECK_FOR_DECEL

long calc_dt(CircBuf<TickData, 300>& ticks,  long t0, long speed_ofs, long speed_val)
{
    //tick with lower speed
    TickData *tick1 = ticks.back(speed_ofs + 1);
    //tick with higher speed
    TickData *tick2 = ticks.back(speed_ofs);

    long t2 = tick2->iTOW;
    long t1 = tick1->iTOW;
    long v2 = tick2->gSpeed;
    long v1 = tick1->gSpeed;

    long dt = (t2 - t1) * (speed_val - v1)/(v2 - v1) + t1 - t0;

    return dt;
}

void TickProcessor::Process(const TickData& tick, void* m, int meta_size, Metering *metering)
{
  if (data.full()) data.pop();
  data.push(tick);

  MetaData *meta = (MetaData*)m;  
    
  const long ZERO_SPEED = meta->sAcc;
    
  if (!have_zero_speed && tick.gSpeed < ZERO_SPEED)
    have_zero_speed = true;

#ifdef CHECK_FOR_DECEL    
  if (!have_decel_speed && tick.gSpeed > BREAK_SPEED_MIN)
    have_decel_speed = true;
#endif
    
  // check accel
  const int used = data.used();

  if (have_zero_speed && tick.gSpeed > ZERO_SPEED && used > BACK_ACCEL_FINISHED_OFS) {
    TickData *prev = data.back(BACK_ACCEL_FINISHED_OFS);
    // speed decreased, but was greater than 60 Kmh, save measurement
    if (prev->gSpeed > tick.gSpeed && prev->gSpeed > ACCEL_SPEED_MIN) {
      have_zero_speed = false;
      have_decel_speed = false;
        
      TickData *t = nullptr;

      int ofs = BACK_ACCEL_FINISHED_OFS + 1;
        
      for (; ofs < used; ofs ++) {
        t = data.back(ofs);
          
        if (t->gSpeed < ZERO_SPEED) {
          // check if prev speed is lower, then do one more step
          if (ofs + 1 < used) {
            if (data.back(ofs+1)->gSpeed < t->gSpeed) {
              continue;
            }
          }
          break; // zero speed detected
        }
      }       

      if (t && t->gSpeed < ZERO_SPEED) {
        unsigned long a60 = 0, a80 = 0, a100 = 0;
  
        long v0 = t->gSpeed;
        long t0 = t->iTOW;
  
        int dump_count = ofs + 1;
  
        for (; ofs >= 0; ofs --) {
          TickData *curt = data.back(ofs);
          long curspeed = curt->gSpeed - v0;
      
          if (a100 == 0 && curspeed >= SPEED_100) a100 = ofs;
          if (a80 == 0 && curspeed >= SPEED_80) a80 = ofs;
          if (a60 == 0 && curspeed >= SPEED_60) a60 = ofs;
        }
  
        metering->accel60 = a60 ? calc_dt(data, t0, a60, SPEED_60) * 0.001 : 0;
        metering->accel80 = a80 ? calc_dt(data, t0, a80, SPEED_80) * 0.001 : 0;
        metering->accel100 = a100 ? calc_dt(data, t0, a100, SPEED_100) * 0.001 : 0;

       // add 20 (about 2 seconds) points to check start dt for accuracy
       for (int i = 0; i < 20; i++) {
          if (dump_count < used) dump_count++;
       }
       
        // resize to only needed part
        data.resize_back(dump_count);
        lcd.drawText("Send acc req ..");

        TickData *d1, *d2;
        int s1, s2;
        data.read(d1, s1, d2, s2);
        // got accel, send to server
        meta->protocol_id = PROTOCOL_ID;
        fullData.blobs_ttl++;

        data.resize_back(0);
        
        if (web.trySendData(meta->year, meta->month, meta->day, meta->hour, meta->minute, meta->second, m, meta_size, d1, s1 * sizeof(TickData), d2, s2 * sizeof(TickData), true, data.temp_buf(), data.temp_size())) {
          fullData.blobs_sent++;
        }
        
        return;
      }
    }
  }

  // check break
  if (have_decel_speed && tick.gSpeed < ZERO_SPEED && used > 1) {
    TickData *prev = data.back(1);
    
    if (prev->gSpeed < tick.gSpeed) {
      //lowest zero speed at prev tick
      
      unsigned long a60 = 0, a80 = 0, a100 = 0;
      
      long v0 = prev->gSpeed;
      long t0 = prev->iTOW;
      
      int ofs = 2;
      
      for (; ofs < used; ofs ++) {
        TickData *curt = data.back(ofs);
        long curspeed = curt->gSpeed - v0;
      
        if (a100 == 0 && curspeed >= SPEED_100) {
          a100 = ofs;
          break;
        }
        if (a80 == 0 && curspeed >= SPEED_80) a80 = ofs;
        if (a60 == 0 && curspeed >= SPEED_60) a60 = ofs;
      }       
      
      metering->accel60 = a60 ? calc_dt(data, t0, a60, SPEED_60) * 0.001 : 0;
      metering->accel80 = a80 ? calc_dt(data, t0, a80, SPEED_80) * 0.001 : 0;
      metering->accel100 = a100 ? calc_dt(data, t0, a100, SPEED_100) * 0.001 : 0;
      
      // resize to only needed part
      data.resize_back(ofs);
      lcd.drawText("Send brk req ..");
      
      TickData *d1, *d2;
      int s1, s2;
      data.read(d1, s1, d2, s2);
      // got decel, send to server
      meta->protocol_id = PROTOCOL_ID;
      fullData.blobs_ttl++;

      data.resize_back(0);
        
      if (web.trySendData(meta->year, meta->month, meta->day, meta->hour, meta->minute, meta->second, m, meta_size, d1, s1 * sizeof(TickData), d2, s2 * sizeof(TickData), true, data.temp_buf(), data.temp_size())) {
        fullData.blobs_sent++;
      }
      
      have_decel_speed = false;
      have_zero_speed = false;
    }
  }
}


