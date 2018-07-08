#include <FS.h>

#include "Config.h"

const int CONFIG_VERSION = 1;

Config cfg;

void Config::Reset()
{
  memset(this, sizeof(*this), 0);
  
  version = CONFIG_VERSION;
  strcpy(host, "54.202.243.139");
  port = 5055;
  strcpy(fingerprint, "07 e3 3c 13 c7 df e8 d2 e9 ab e1 ab 57 46 8a 50 88 b6 d6 14");

  serialRate = 115200;
}

void Config::DumpInfo(bool success)
{
  Serial.print(F("Settings from flash - ")); Serial.printf("%s\n", success ? "OK" : "Failed");
  Serial.print(F("ssid "));Serial.println(ssid);
  Serial.print(F("pass "));Serial.println(pass);
  Serial.print(F("token "));Serial.println(token);
  Serial.print(F("host:port "));Serial.printf("'%s:%d'\n", host, port);
  Serial.print(F("fingerprint "));Serial.println(fingerprint);
  Serial.print(F("mpu "));Serial.printf("%d %d %d\n", ax_offset, ay_offset, az_offset);
  Serial.print(F("gps serial rate "));Serial.printf("%d\n", serialRate);
}

bool Config::Read()
{
  bool success = false;
  File f = SPIFFS.open("settings.txt", "r");
  if (f) {
    if (f.size() <= sizeof(*this)) {
      Reset();
      
      size_t rsz = f.readBytes((char*)this, f.size());

      if (rsz == f.size()){
        if (version == 1 && rsz == sizeof(ConfigV1))
        {
          // V1, ok
          success = true;
        }
        if (version == CONFIG_VERSION && rsz == sizeof(Config))
        {
          // current, ok
          success = true;
        }
      }
    }
    
    f.close();   
  }

  if (!success) Reset();
  
  return success;
}

bool Config::Write()
{
  File f = SPIFFS.open("settings.txt", "w");
  if (f) {
    Serial.println(F("Write Settings"));
    
    f.write((const uint8_t*)this, sizeof(*this));
    f.close();

    return true;
  } else {
    Serial.println(F("Failed to write Settings"));
    return false;
  }
}

