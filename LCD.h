#ifndef LCD_h
#define LCD_h

#include <SSD1306.h>
#include <Wire.h>

struct Metering
{
  float accel60;
  float accel80;
  float accel100;
};

struct FullData
{
  unsigned int numSV;
  char hAcc[6];
  char sAcc[6];
  float accel;
  char gpsTime[9];
  char bufLatitude[10];
  char bufLongitude[10];
  float gSpeedKm;
  int blobs_sent, blobs_ttl;
};


extern FullData fullData;

class LCD : public SSD1306
{
  private:
    char buf60[5], buf80[5], buf100[5];
    void drawSingleString(int16_t x, int16_t y, const char* ascii);
  public:
    bool isset;
    LCD();
    void init();
    void drawText(const char *str);
    void gpsScreen(bool rx, bool tx);
    void updateScreen(FullData* fullData, Metering* metering);
};

extern LCD lcd;
#endif
