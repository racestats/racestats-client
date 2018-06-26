/*
  Connections:
  GPS TX -> D7
  GPS RX -> D8
  GPS INT -> D6, not used for now
  Screen SDA -> Wemos SDA (D2)
  Screen SCK -> Wemos SCL (D1)
  MPU SDA -> Wemos SDA (D2)
  MPU SCL -> Wemos SCL (D1)
*/

#include "Config.h"

#include <Arduino.h>
#include "LCD.h"

#include "UbloxGPS.h"
#include "WebClient.h"
#include "TickProcessor.h"

// https://github.com/jrowberg/i2cdevlib/zipball/master
#include "I2Cdev.h"


#ifdef ACCEL_SAMPLING_ENABLE
#include "MPU6050.h"
#endif

LCD lcd;
WebClient web;

Metering metering;

FullData fullData;
TickProcessor processor(web, lcd);

#ifdef ACCEL_SAMPLING_ENABLE
MPU6050 mpu(0x68);
#endif

int16_t AcX, AcY, AcZ, Tmp, GyX, GyY, GyZ;

volatile byte interruptCounter = 0;
volatile unsigned long interruptTime = 0;
int numberOfInterrupts = 0;

void handleInterrupt() {
  interruptCounter++;
  interruptTime = micros();
}


#include <ESP8266WiFi.h>

bool check_backend_done = false;

void check_backend()
{
  if (check_backend_done) return;
  
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  check_backend_done = true;

  lcd.drawText("Send test msg");
  // trySendData could overwrite this data anyway
  processor.data.resize_back(0);
  int x = 0;
  web.trySendData(0,0,0,0,0,0, &x, sizeof(x), 0, 0, 0, 0, false, processor.data.temp_buf(), processor.data.temp_size());
}

#include "FS.h"

void setup() {
  Serial.begin(500000);
  serial.begin(115200);
  delay(500);

  pinMode(D6, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(D6), handleInterrupt, RISING);

  lcd.init();
  web.init();
  metering = {0.0, 0.0, 0.0};
  gpsSetup();

#ifdef ACCEL_SAMPLING_ENABLE
  mpu.initialize();

  mpu.setXGyroOffset(15);
  mpu.setYGyroOffset(-53);
  mpu.setZGyroOffset(10);
  mpu.setXAccelOffset(-5976);
  mpu.setYAccelOffset(624);
  mpu.setZAccelOffset(1589);
  //mpu.setDLPFMode(MPU6050_DLPF_BW_98); // 94, 3.0
  //mpu.setDLPFMode(MPU6050_DLPF_BW_42); // 44, 4.9
  mpu.setDLPFMode(MPU6050_DLPF_BW_20); // 21, 8.5
  //mpu.setDLPFMode(MPU6050_DLPF_BW_10); // 10, 13.8
  //mpu.setDLPFMode(MPU6050_DLPF_BW_5); // 5, 19

  Serial.println(mpu.testConnection() ? "MPU6050 connection successful" : "MPU6050 connection failed");
#endif

  Serial.println(SPIFFS.begin() ? "FS Ok" : "FS Failed!");

  FSInfo fs_info;
  SPIFFS.info(fs_info);
    
  char sinfo[33];
  sprintf(sinfo, "fs %d %d %d %d %d %d", fs_info.totalBytes, fs_info.usedBytes,fs_info.blockSize,fs_info.pageSize,fs_info.maxOpenFiles,fs_info.maxPathLength);
  Serial.println(sinfo);
      
  Serial.println("init end");
}

unsigned long lastScreenUpdate = 0;
unsigned long lastDataUpdate = 0;

unsigned long dt0 = 0, dt1 = 0, dt2 = 0, dt3 = 0, t3prev = 0, dt4 = 0;

unsigned long iTOWprev = 0;
// 60, 120, 180, 240, 300, 300+
int histo[6];

const int MS_STEP = 100;

unsigned long tprev = 0;

//#define LOG_PERF

struct AccVec {
  float x, y, z;
};

struct AccSampler {
    AccSampler() {
      n = 0;
      ax = ay = az = 0;
    }

    void Tick(int16_t x, int16_t y, int16_t z)
    {
      ax += x; ay += y; az += z;
      n++;
    }

    void Avg()
    {
      if (n > 0)
      {
        avg.x = (ax / n) / 16384.0;
        avg.y = (ay / n) / 16384.0;
        avg.z = (az / n) / 16384.0;
      }
      else{
        avg.x = avg.y = avg.z = 0;
      }
      n = 0;
      ax = ay = az = 0;
    }

    AccVec avg;

  private:
    int32_t ax, ay, az;
    int32_t n;
};



AccVec avg_cur, avg_prev, stillAcc;
float stillAccLen;
unsigned long last_gps_move;
bool standing_still = false;

//#define DEBUG_ACCEL
//#define DEBUG_ACCEL_NO_GPS

//#define LOG_ALL

void loop() {
  check_backend();
  
  unsigned long t0 = t3prev;
  unsigned long t1 = micros();
  int msgType = processGPS();
  unsigned long t2 = micros();

  if (interruptCounter > 0) {
    interruptCounter--;
    numberOfInterrupts++;
#ifdef LOG_ALL
    Serial.printf("INT,%d,%d,%d\n", micros(), interruptTime, numberOfInterrupts);
#endif
  }

  static unsigned long prev_acc_sample;

  static AccSampler avgGps;

#ifdef ACCEL_SAMPLING_ENABLE
  if (t2 - prev_acc_sample >= 10000) {
    // 100Hz sampling and low pass 21Hz, 8.5ms delay
    prev_acc_sample = t2;

    mpu.getAcceleration(&AcX, &AcY, &AcZ);
    unsigned long t4 = micros();
#ifdef LOG_ALL
    Serial.printf("ACC,%d,%d,%d,%d\n", t4, AcX, AcY, AcZ);
#endif
    // 100Hz avg
    static AccSampler avg;
    avg.Tick(AcX, AcY, AcZ);


#ifdef DEBUG_ACCEL
#ifdef DEBUG_ACCEL_NO_GPS
    if (last_gps_move == 0) last_gps_move = t2;
#endif

    float x = AcX / 16384.0 - stillAcc.x;
    float y = AcY / 16384.0 - stillAcc.y;
    float z = AcZ / 16384.0 - stillAcc.z;

    float acc_value = 0;
    if (stillAccLen > 0.5) {
      // accel in G
      acc_value = sqrt(x * x + y * y + z * z) / stillAccLen * 1000;
      Serial.println(acc_value);
    }
#endif

    static unsigned long prev_chk = 0;
    if (t4 - prev_chk >= 2000000) {
      prev_chk = t4;
      avg.Avg();

#ifdef LOG_ALL
      Serial.printf("ACC_AVG,%d,%d,%d,%d\n", t4, int(avg.avg.x * 16384), int(avg.avg.y * 16384), int(avg.avg.z * 16384));
#endif

      if (t4 - last_gps_move >= 4000000) {
        // no gps move for 4 sec, lets use [-4s;-2s) avg as stillAcc (acceleration when vhc not moving)
        stillAcc = avg_prev;
        stillAccLen = sqrt(stillAcc.x * stillAcc.x + stillAcc.y * stillAcc.y + stillAcc.z * stillAcc.z);
#ifdef LOG_ALL
        Serial.printf("ACC_STILL,%d,%d\n", t4, int(stillAccLen * 16384));
#endif

        standing_still = true;
      }

      if (t4 - last_gps_move >= 2000000) {
#ifdef LOG_ALL
        Serial.printf("ACC_PREV,%d\n", t4);
#endif
        // save cur avg as prev avg and last cur as prev
        avg_prev = avg_cur;
        avg_cur = avg.avg;
      }
    }

    avgGps.Tick(AcX, AcY, AcZ);
  }
#endif  

  if ( msgType == MT_NAV_PVT ) {
    // avg samples between gps solutions
    avgGps.Avg();

    //accel - accel when standing still
    float x = avgGps.avg.x - stillAcc.x;
    float y = avgGps.avg.y - stillAcc.y;
    float z = avgGps.avg.z - stillAcc.z;

    float acc_value = 0;
    if (stillAccLen > 0.5) {
      // accel in G
      acc_value = sqrt(x * x + y * y + z * z) / stillAccLen;

      /*
        static unsigned long prev_log = 0;
        if (t4 - prev_log > 200) {
        prev_log = t4;

        char sbuf[16];
        dtostrf(acc_value, 7, 3, sbuf);
        Serial.println(sbuf);
        }
      */
    }

#ifdef LOG_PERF
    if (iTOWprev != 0) {
      int dt = ubxMessage.navPvt.iTOW - iTOWprev;
      switch (dt) {
        case MS_STEP*1: histo[0]++; break;
        case MS_STEP*2: histo[1]++; break;
        case MS_STEP*3: histo[2]++; break;
        case MS_STEP*4: histo[3]++; break;
        case MS_STEP*5: histo[4]++; break;
        default:
          histo[5]++; break;
      }

      unsigned long tt1 = micros();

      if (tt1 - tprev >= 1000000) {
        Serial.printf("dtow %d %d %d %d %d %d\n", histo[0], histo[1], histo[2], histo[3], histo[4], histo[5]);
        tprev = tt1;
      }
    }
    iTOWprev = ubxMessage.navPvt.iTOW;
#endif

    TickData tick;
    tick.iTOW = ubxMessage.navPvt.iTOW;
    tick.lat = ubxMessage.navPvt.lat;
    tick.lon = ubxMessage.navPvt.lon;
    tick.gSpeed = ubxMessage.navPvt.gSpeed;
    tick.hAcc = ubxMessage.navPvt.hAcc;
    tick.sAcc = ubxMessage.navPvt.sAcc;
    tick.accel = long(acc_value * 16384);
    tick.hMSL = ubxMessage.navPvt.hMSL;

#ifdef LOG_ALL
    Serial.printf("GPS_NAV,%d,%d,%d,%d,%d,%d\n", t2, tick.iTOW, tick.gSpeed, tick.accel,tick.hAcc,tick.sAcc);
#endif


    // need speed acc at least 2km/h to do accel calibration
    if (tick.gSpeed > tick.sAcc || tick.sAcc >= (int)(2 / 0.0036)) {
      standing_still = false;
#ifndef DEBUG_ACCEL_NO_GPS
      last_gps_move = t2;
#ifdef LOG_ALL
      Serial.printf("GPS_MOVE,%d\n", t2);
#endif
#endif
    }

    if (ubxMessage.navPvt.numSV > 0) {
      processor.Process(tick, &ubxMessage.navPvt, sizeof(ubxMessage.navPvt), &metering);
    }

    if (t1 - lastDataUpdate >= 300000) {
      lastDataUpdate = t1;
      fullData.accel = acc_value;
      fullData.numSV = ubxMessage.navPvt.numSV;
      fullData.gSpeedKm = ubxMessage.navPvt.gSpeed * 0.0036;
      sprintf(fullData.gpsTime, "%02d:%02d:%02d", ubxMessage.navPvt.hour, ubxMessage.navPvt.minute, ubxMessage.navPvt.second);
      fullData.hAcc = ubxMessage.navPvt.hAcc / 1000.0f;
      fullData.sAcc = ubxMessage.navPvt.sAcc / 1000.0f;

      if (fullData.hAcc > 99) fullData.hAcc = 99.0f;
      if (fullData.sAcc > 99) fullData.sAcc = 99.0f;
    }
  }

  unsigned long now = micros();
  if (now - lastScreenUpdate >= 300000) {
    lcd.updateScreen(&fullData, &metering);
    lastScreenUpdate = now;
  }


#ifdef LOG_PERF
  // log perf

  if (t0 && t1 > t0 && t1 - t0 > dt0) {
    dt0 = t1 - t0;
    Serial.print("dt0 ");
    Serial.println(dt0);
  }
  if (t2 > t1 && t2 - t1 > dt1) {
    dt1 = t2 - t1;
    Serial.print("dt1 ");
    Serial.println(dt1);
  }
  if (now > t2 && now - t2 > dt2) {
    dt2 = now - t2;
    Serial.print("dt2 ");
    Serial.println(dt2);
  }
  if (t3 > now && t3 - now > dt3) {
    dt3 = t3 - now;
    Serial.print("dt3 ");
    Serial.println(dt3);
  }
  if (t4 > t3 && t4 - t3 > dt4) {
    dt4 = t4 - t3;
    Serial.print("dt4 ");
    Serial.println(dt4);
  }
  t3prev = micros();
#endif
}

