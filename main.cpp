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

//#define WAIT_ON_START
//#define LOG_PERF
//#define DEBUG_ACCEL
//#define LOG_ALL
//#define ENABLE_TIM_TP

#include <Arduino.h>

// https://github.com/jrowberg/i2cdevlib/zipball/master
#include <I2Cdev.h>

#include "Config.h"
#include "LCD.h"
#include "UbloxGPS.h"
#include "WebClient.h"
#include "TickProcessor.h"
#include "MPU.h"

Metering metering;

FullData fullData;
TickProcessor processor;

#ifdef ENABLE_TIM_TP
volatile byte interruptCounter = 0;
volatile unsigned long interruptTime = 0;
int numberOfInterrupts = 0;

void handleInterrupt() {
  interruptCounter++;
  interruptTime = micros();
}
#endif


#include <ESP8266WiFi.h>

bool check_backend_done = false;

void check_backend()
{
  web.update();
  
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
  // FS init
  bool fsOk = SPIFFS.begin();
  bool cfgOk = cfg.Read();

  if (cfg.bridgeMode)
  {
    Serial.begin(cfg.serialRate);
    serial.begin(cfg.serialRate);       
  }
  else
  {
    Serial.begin(500000);
    serial.begin(cfg.serialRate);   
  }
  
  delay(500);

  lcd.init();


  if (cfg.bridgeMode)
  {
    // don't need mpu and etc for gps bridge mode
    lcd.gpsScreen(true, true);
    return;
  }

  gpsSetup();

#ifdef WAIT_ON_START
  // to debug start-up
  while (Serial.available() && Serial.read()); // empty buffer
  while (!Serial.available()){
    Serial.println(F("Send any character to start.\n"));
    delay(1000);
  }
  while (Serial.available() && Serial.read()); // empty buffer again
#endif  

#ifdef ENABLE_TIM_TP
  pinMode(D6, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(D6), handleInterrupt, RISING);
#endif

  Serial.println(fsOk ? F("FS Ok") : F("FS Failed!"));

  FSInfo fs_info;
  SPIFFS.info(fs_info);
    
  Serial.print(F("FS totalBytes "));
  Serial.print(fs_info.totalBytes);
  Serial.print(F(" usedBytes "));
  Serial.print(fs_info.usedBytes);
  Serial.print(F(" blockSize "));
  Serial.print(fs_info.blockSize);
  Serial.print(F(" pageSize "));
  Serial.print(fs_info.pageSize);
  Serial.print(F(" maxOpenFiles "));
  Serial.print(fs_info.maxOpenFiles);
  Serial.print(F(" maxPathLength "));
  Serial.print(fs_info.maxPathLength);  
  Serial.print(F("\n"));

  cfg.DumpInfo(cfgOk);
  
  // WiFi
  web.init();
  metering = {0.0, 0.0, 0.0};

  // accelerometer
  mpu.initialize();
  mpu_inited = mpu.testConnection();
  Serial.println(mpu_inited ? F("MPU6050 connection successful") : F("MPU6050 connection failed"));

  if (mpu_inited)
  {
    mpu.setXGyroOffset(cfg.ax_offset);
    mpu.setYGyroOffset(cfg.ay_offset);
    mpu.setZGyroOffset(cfg.az_offset);
    mpu.setXAccelOffset(cfg.gx_offset);
    mpu.setYAccelOffset(cfg.gy_offset);
    mpu.setZAccelOffset(cfg.gz_offset);
    //mpu.setDLPFMode(MPU6050_DLPF_BW_98); // 94, 3.0
    //mpu.setDLPFMode(MPU6050_DLPF_BW_42); // 44, 4.9
    mpu.setDLPFMode(MPU6050_DLPF_BW_20); // 21, 8.5
    //mpu.setDLPFMode(MPU6050_DLPF_BW_10); // 10, 13.8
    //mpu.setDLPFMode(MPU6050_DLPF_BW_5); // 5, 19
  }
  
  Serial.println(F("init end"));
}

unsigned long lastScreenUpdate = 0;
unsigned long lastDataUpdate = 0;

unsigned long dt0 = 0, dt1 = 0, dt2 = 0, dt3 = 0, t3prev = 0, dt4 = 0;

unsigned long iTOWprev = 0;

#ifdef LOG_PERF
// 60, 120, 180, 240, 300, 300+
int histo[6];
#endif

const int MS_STEP = 100;

unsigned long tprev = 0;


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
      else
      {
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


struct AccSamplerMinMax {
    AccSamplerMinMax() 
    {
      Reset();
    }

    void Reset()
    {
      n = 0;
      ax = ay = az = 0;
      minax = minay = minaz = 0;
      maxax = maxay = maxaz = 0;
      diffLen = avgLen = 0;
    }

    void Tick(int16_t x, int16_t y, int16_t z)
    {
      if (n == 0)
      {
        minax = maxax = x;
        minay = maxay = y;
        minaz = maxaz = z;
      }
      if (minax > x) minax = x;
      if (maxax < x) maxax = x;
      if (minay > y) minay = y;
      if (maxay < y) maxay = y;
      if (minaz > z) minaz = z;
      if (maxaz < z) maxaz = z;
      
      ax += x; ay += y; az += z;
      n++;
    }

    int32_t Max(int32_t a, int32_t b)
    {
      return a > b ? a : b;
    }
    
    void Avg()
    {
      if (n > 0)
      {
        avg.x = (ax / n) / 16384.0;
        avg.y = (ay / n) / 16384.0;
        avg.z = (az / n) / 16384.0;

        float dx = Max(maxax - ax/n, ax/n - minax) / 16384.0;
        float dy = Max(maxay - ay/n, ay/n - minay) / 16384.0;
        float dz = Max(maxaz - az/n, az/n - minaz) / 16384.0;

        diffLen = sqrt(dx*dx+dy*dy+dz*dz);
        avgLen = sqrt(avg.x*avg.x+avg.y*avg.y+avg.z*avg.z);
        n = 0;
        ax = ay = az = 0;
      }
      else
      {
        Reset();
      }
    }

    AccVec avg;
    float diffLen;
    float avgLen;

  private:
    int32_t ax, ay, az;    
    int32_t n;
    int32_t minax, minay, minaz;
    int32_t maxax, maxay, maxaz;
};

AccVec gValue;
float gLen;
float diffAccLen;
bool standing_still = false;

static long frames = 0;


void loop() {
  if (cfg.bridgeMode)
  {
    static unsigned long lastGpsScreen = 0;
    static bool rx = false;
    static bool tx = false;

    while (serial.available() > 0) {
      Serial.write(serial.read());
      rx = true;
    }
    while (Serial.available() > 0) {
      serial.write(Serial.read());
      tx = true;
    }

    unsigned long now = micros();
    if (now - lastGpsScreen >= 500000) {
      lcd.gpsScreen(rx, tx);
      lastGpsScreen = now;
      tx = rx = false;
    }

    web.update();
    return;
  }
  
  check_backend();
  frames++;
  
  unsigned long t0 = t3prev;
  unsigned long t1 = micros();
  int msgType = processGPS();
  unsigned long t2 = micros();

#ifdef ENABLE_TIM_TP
  if (interruptCounter > 0) {
    interruptCounter--;
    numberOfInterrupts++;
#ifdef LOG_ALL
    Serial.printf("INT,%d,%d,%d\n", micros(), interruptTime, numberOfInterrupts);
#endif
  }
#endif  

  static unsigned long prev_acc_sample;

  static AccSampler avgGps;

  int16_t AcX, AcY, AcZ, Tmp, GyX, GyY, GyZ;

  if (mpu_inited && t2 - prev_acc_sample >= 10000) {
    // 100Hz sampling and low pass 21Hz, 8.5ms delay
    prev_acc_sample = t2;

    mpu.getAcceleration(&AcX, &AcY, &AcZ);
    unsigned long t4 = micros();
#ifdef LOG_ALL
    Serial.printf("ACC,%d,%d,%d,%d\n", t4, AcX, AcY, AcZ);
#endif
    // 100Hz avg
    static AccSamplerMinMax avg;
    avg.Tick(AcX, AcY, AcZ);
    diffAccLen = avg.diffLen;

#ifdef DEBUG_ACCEL
    float x = AcX / 16384.0 - gValue.x;
    float y = AcY / 16384.0 - gValue.y;
    float z = AcZ / 16384.0 - gValue.z;

    float acc_value = 0;
    if (gLen > 0.5) {
      // accel in G
      acc_value = sqrt(x * x + y * y + z * z) / gLen * 1000;
      Serial.println(acc_value);
    }
#endif

    static unsigned long prev_chk = 0;
    if (t4 - prev_chk >= 2000000) {
      prev_chk = t4;
      avg.Avg();

#ifdef LOG_ALL
      Serial.printf("ACC_AVG,%d,%d,%d,%d,%d,%d\n", t4, int(avg.avg.x * 16384), int(avg.avg.y * 16384), int(avg.avg.z * 16384), int(avg.avgLen * 1000), int(avg.diffLen * 1000));
#endif

      if (avg.diffLen <= 0.04) {
        gValue = avg.avg;
        gLen = sqrt(gValue.x * gValue.x + gValue.y * gValue.y + gValue.z * gValue.z);
#ifdef LOG_ALL
        Serial.printf("ACC_STILL,%d,%d\n", t4, int(gLen * 16384));
#endif
        standing_still = true;
      } else {
        // moving with const speed (impossible in real life) or standing
        standing_still = false;
      }
    }

    avgGps.Tick(AcX, AcY, AcZ);
  }

  unsigned long t3 = micros();
  
  if ( msgType == MT_NAV_PVT ) {
    // avg samples between gps solutions
    avgGps.Avg();

    //accel - g
    float x = avgGps.avg.x - gValue.x;
    float y = avgGps.avg.y - gValue.y;
    float z = avgGps.avg.z - gValue.z;

    float acc_value = 0;
    if (gLen > 0.5) {
      // accel in G
      acc_value = sqrt(x * x + y * y + z * z) / gLen;
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

    if (ubxMessage.navPvt.numSV > 0) {
      TickData tick;
      tick.Pack(ubxMessage.navPvt, acc_value);

/*
      static long itowprev = 0;

      if (tick.iTOW - itowprev > 100) {
        Serial.println("*");
        Serial.printf("GPS_NAV,%d,%d,%d,%d,%d,%d,%d,%d\n", t2, tick.iTOW, tick.gSpeed, tick.accel,tick.hAcc,tick.sAcc, tick.iTOW - itowprev, frames);
      }

      itowprev = tick.iTOW;
*/
    
#ifdef LOG_ALL
      Serial.printf("GPS_NAV,%d,%d,%d,%d,%d,%d\n", t2, tick.iTOW, tick.gSpeed, tick.accel,tick.hAcc,tick.sAcc);
#endif
      processor.Process(tick, &ubxMessage.navPvt, sizeof(ubxMessage.navPvt), &metering);
    }

    frames = 0;

    if (t1 - lastDataUpdate >= 300000) {
      lastDataUpdate = t1;
      fullData.accel = acc_value;
      fullData.numSV = ubxMessage.navPvt.numSV;
      fullData.gSpeedKm = ubxMessage.navPvt.gSpeed * 0.0036;
      sprintf(fullData.gpsTime, "%02d:%02d:%02d", ubxMessage.navPvt.hour, ubxMessage.navPvt.minute, ubxMessage.navPvt.second);

      // horizontal acc
      if (ubxMessage.navPvt.hAcc > 9990) {
        strcpy(fullData.hAcc, "p?");
      } else {
        fullData.hAcc[0] = 'p';
        dtostrf(ubxMessage.navPvt.hAcc * 0.001f, 4, 2, fullData.hAcc + 1);
      }

      // speed accuracy
      // 9990 / 3.6   1m/s = 3.6km/h
      if (ubxMessage.navPvt.sAcc > 2775) {
        strcpy(fullData.sAcc, "s?");
      } else {
        fullData.sAcc[0] = 's';
        dtostrf(ubxMessage.navPvt.sAcc * 0.0036f, 4, 2, fullData.sAcc + 1);
      }
    }
  }
  

  unsigned long now = micros();
  if (now - lastScreenUpdate >= 300000) {
    lcd.updateScreen(&fullData, &metering);
    lastScreenUpdate = now;
  }

#ifdef LOG_PERF
  // log perf

  unsigned long t4 = micros();
  
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
  if (t3 > t2 && t3 - t2 > dt2) {
    dt2 = t3 - t2;
    Serial.print("dt2 ");
    Serial.println(dt2);
  }
  if (now > t3 && now - t3 > dt3) {
    dt3 = now - t3;
    Serial.print("dt3 ");
    Serial.println(dt3);
  }
  if (t4 > now && t4 - now > dt4) {
    dt4 = t4 - now;
    Serial.print("dt4 ");
    Serial.println(dt4);
  }
  t3prev = micros();
#endif
}

void TickData::Pack(const NAV_PVT& p, float a)
{
  // convert m/s -> km/h
  int sAccKmH = (int)(float(p.sAcc) * 3.6f);
  
  iTOW = p.iTOW;
  lat = p.lat;
  lon = p.lon;
  gSpeed = p.gSpeed > 65535 ? 65535 : p.gSpeed;
  hAcc = p.hAcc > 10000 ? 200 : p.hAcc / 50;
  sAcc = sAccKmH > 10000 ? 200 : sAccKmH / 50;
  accel = uint16_t(a * 16384.f);
  hMSL = p.hMSL / 100;
}

