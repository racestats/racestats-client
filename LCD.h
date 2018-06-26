#ifndef LCD_h
#define LCD_h

#include <SSD1306.h>
#include <Wire.h>
#include "font.h"

struct Metering
{
  float accel60;
  float accel80;
  float accel100;
};

struct FullData
{
  unsigned int numSV;
  float hAcc;
  float sAcc;
  float accel;
  char gpsTime[9];
  char bufLatitude[10];
  char bufLongitude[10];
  unsigned int gSpeedKm = 0;
  int blobs_sent, blobs_ttl;
};


extern FullData fullData;

class LCD
{
  private:
    SSD1306 *display;
    char buf60[5], buf80[5], buf100[5];

  public:
    bool isset;
    LCD();
    void init();
    void drawText(const char *str);
    void gpsScreen();
    void updateScreen(FullData* fullData, Metering* metering);
};

#endif
