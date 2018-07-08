#ifndef CONFIG_H
#define CONFIG_H

#define UBLOX_TOKEN ""

struct ConfigV1 {
  int version;
  // wifi name, password
  char ssid[32];
  char pass[64];

  // backend host, port, auth token, fingerprint
  char host[64];
  int  port;
  char token[256];
  char fingerprint[60];

  // mpu settings
  int ax_offset,ay_offset,az_offset,gx_offset,gy_offset,gz_offset;
};

// for ICACHE_FLASH_ATTR
#include <c_types.h>

struct Config : ConfigV1 {
  int bridgeMode;
  int serialRate;

  void DumpInfo(bool success) ICACHE_FLASH_ATTR;
  bool Read() ICACHE_FLASH_ATTR;
  bool Write() ICACHE_FLASH_ATTR;
  void Reset() ICACHE_FLASH_ATTR;
};

extern Config cfg;

#endif
