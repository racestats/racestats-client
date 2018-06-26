#include "LCD.h"
#include <ESP8266WiFi.h>

LCD::LCD()
{
  Wire.begin(4, 5);
  Wire.beginTransmission(0x3c);
  uint8_t error = Wire.endTransmission();
  if (error == 0) {
    isset = true;
    display = new SSD1306(0x3c, 4, 5); // https://github.com/esp8266/Arduino/blob/master/variants/d1_mini/pins_arduino.h
  } else {
    isset = false;
  }
}

void LCD::init()
{
  if (!isset) {
    return;
  }
  display->init();
  display->flipScreenVertically();
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->setFont(ArialMT_Plain_10);
}

void LCD::gpsScreen()
{
  display->clear();
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(Orbitron_Light_30);
  display->drawString(64, 6, "GPS");
  display->drawString(64, 34, "search");
  display->display();
}

void LCD::drawText(const char *str)
{
  if (!isset) {
    return;
  }

  display->clear();
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->setFont(ArialMT_Plain_10);
  display->drawString(11, 0, str);
  display->display();
}

extern bool standing_still;
extern int last_web_post_code;

//0-15 - yellow
void LCD::updateScreen(FullData* fullData, Metering* metering)
{
  display->clear();

  display->drawVerticalLine(7, 2, 8);
  display->drawVerticalLine(6, 2, 8);
  display->drawVerticalLine(4, 5, 5);
  display->drawVerticalLine(3, 5, 5);
  display->drawVerticalLine(1, 8, 2);
  display->drawVerticalLine(0, 8, 2);

  display->drawHorizontalLine(0, 0, 128); 
  display->drawHorizontalLine(0, 15, 128); 

  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->setFont(ArialMT_Plain_10);
  display->drawString(11, 0, (String)fullData->numSV);

  if (WiFi.status() == WL_CONNECTED) {
    display->setTextAlignment(TEXT_ALIGN_LEFT);
    display->setFont(ArialMT_Plain_10);
    display->drawString(0, 13, "wifi");
  }

  if (standing_still) {
    display->setTextAlignment(TEXT_ALIGN_LEFT);
    display->setFont(ArialMT_Plain_10);
    display->drawString(0, 24, "zG");   
  }

  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->setFont(ArialMT_Plain_10);
  display->drawString(25, 0, (String)fullData->hAcc);

  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->setFont(ArialMT_Plain_10);
  display->drawString(56, 0, (String)fullData->sAcc);

  display->setTextAlignment(TEXT_ALIGN_RIGHT);
  display->setFont(ArialMT_Plain_10);
  display->drawString(128, 0, fullData->gpsTime);

  display->setTextAlignment(TEXT_ALIGN_RIGHT);
  display->setFont(ArialMT_Plain_10);
  dtostrf(metering->accel60, 3, 1, buf60);
  display->drawString(126, 13, (String)buf60);

  display->setTextAlignment(TEXT_ALIGN_RIGHT);
  display->setFont(ArialMT_Plain_10);
  dtostrf(metering->accel80, 3, 1, buf80);
  display->drawString(126, 25, (String)buf80);

  display->setTextAlignment(TEXT_ALIGN_RIGHT);
  display->setFont(ArialMT_Plain_10);
  dtostrf(metering->accel100, 3, 1, buf100);
  display->drawString(126, 37, (String)buf100);

  char sent[16];
  sprintf(sent, "s %d/%d %d", fullData->blobs_sent, fullData->blobs_ttl, last_web_post_code);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->setFont(ArialMT_Plain_10);
  display->drawString(16, 40, sent);

  char gpsSpeed[4];
  sprintf(gpsSpeed, "%03d", fullData->gSpeedKm);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->setFont(ArialMT_Plain_16);
  display->drawString(16, 16, gpsSpeed);

  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->setFont(ArialMT_Plain_10);
  display->drawString(50, 16, (String)fullData->accel);


  display->display();
}

