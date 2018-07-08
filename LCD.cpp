#include <ESP8266WiFi.h>

#include "Config.h"
#include "LCD.h"
#include "WebClient.h"


//https://github.com/esp8266/Arduino/blob/master/variants/d1_mini/pins_arduino.h

LCD lcd;

LCD::LCD() : SSD1306(0x3c, 4, 5)
{
  Wire.begin(4, 5);
  Wire.beginTransmission(0x3c);
  uint8_t error = Wire.endTransmission();
  if (error == 0) {
    isset = true;
  } else {
    isset = false;
  }
}

void LCD::drawSingleString(int16_t x, int16_t y, const char* ascii)
{
  uint16_t length = strlen(ascii);
  drawStringInternal(x, y, (char*)ascii, length, getStringWidth(ascii, length));  
}

void LCD::init()
{
  if (!isset) {
    return;
  }
  SSD1306::init();
  flipScreenVertically();
  setTextAlignment(TEXT_ALIGN_LEFT);
  setFont(ArialMT_Plain_10);

  drawSingleString(51, 0, "Racestats");
  display();
}

void LCD::gpsScreen(bool rx, bool tx)
{
  clear();
  setTextAlignment(TEXT_ALIGN_CENTER);
  setFont(ArialMT_Plain_16);
  drawSingleString(64, 16, "GPS");
  drawSingleString(64, 34, "Bridge Mode");

  setFont(ArialMT_Plain_16);
  setTextAlignment(TEXT_ALIGN_LEFT);
  if (rx) drawSingleString(0, 0, "rx");
  if (tx) drawSingleString(0, 16, "tx");

  if (web.enableAP) {
    drawSingleString(32, 0, "srv");
  }

  char buf[16];
  sprintf(buf, "%d", cfg.serialRate);  
  drawSingleString(64, 0, buf); 

  display();
}

void LCD::drawText(const char *str)
{
  if (!isset) {
    return;
  }

  clear();
  setTextAlignment(TEXT_ALIGN_LEFT);
  setFont(ArialMT_Plain_10);
  drawSingleString(11, 0, str);
  display();
}

extern bool standing_still;
extern bool mpu_inited;
extern float diffAccLen;
//0-15 - yellow
void LCD::updateScreen(FullData* fullData, Metering* metering)
{
  char buf[16];
  clear();

  drawVerticalLine(7, 2, 8);
  drawVerticalLine(6, 2, 8);
  drawVerticalLine(4, 5, 5);
  drawVerticalLine(3, 5, 5);
  drawVerticalLine(1, 8, 2);
  drawVerticalLine(0, 8, 2);

  //drawHorizontalLine(0, 0, 128); 
  drawHorizontalLine(0, 15, 128); 

  setTextAlignment(TEXT_ALIGN_LEFT);
  setFont(ArialMT_Plain_10);

  sprintf(buf, "%d", fullData->numSV);
  drawSingleString(11, 0, buf);

  if (WiFi.status() == WL_CONNECTED || web.enableAP) {
    drawSingleString(0, 13, web.enableAP ? "srv" : "wifi");
  }

  if (!mpu_inited) {
    drawSingleString(0, 24, "!G");
  } else if (cfg.ax_offset == 0 && cfg.ay_offset == 0 && cfg.az_offset == 0) {
    // not calibrated
    drawSingleString(0, 24, "!CG");
  } else if (standing_still) {
    drawSingleString(0, 24, "zG");   
  }

  drawSingleString(25, 0, fullData->hAcc); 
  drawSingleString(56, 0, fullData->sAcc);

  setTextAlignment(TEXT_ALIGN_RIGHT);
  drawSingleString(128, 0, fullData->gpsTime);

  setTextAlignment(TEXT_ALIGN_RIGHT);
  dtostrf(metering->accel60, 3, 1, buf60);
  drawSingleString(126, 13, buf60);

  setTextAlignment(TEXT_ALIGN_RIGHT);
  dtostrf(metering->accel80, 3, 1, buf80);
  drawSingleString(126, 25, buf80);

  setTextAlignment(TEXT_ALIGN_RIGHT);
  dtostrf(metering->accel100, 3, 1, buf100);
  drawSingleString(126, 37, buf100);

  
  sprintf(buf, "s %d/%d %d", fullData->blobs_sent, fullData->blobs_ttl, web.last_post_code);
  setTextAlignment(TEXT_ALIGN_LEFT);
  drawSingleString(16, 40, buf);

  setTextAlignment(TEXT_ALIGN_LEFT);
  dtostrf(fullData->accel, 4, 2, buf);
  drawSingleString(60, 16, buf);

  // show diff in G from average for last 2 seconds
  dtostrf(diffAccLen, 4, 2, buf);
  drawSingleString(85, 16, buf);

  sprintf(buf, "%03d", (int)fullData->gSpeedKm);
  setTextAlignment(TEXT_ALIGN_LEFT);
  setFont(ArialMT_Plain_16);
  
  drawSingleString(16, 16, buf);

  display();
}

