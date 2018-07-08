#ifndef MPU_H
#define MPU_H

#include "MPU6050.h"
extern bool mpu_inited;
extern MPU6050 mpu;

void mpu_calibrate() ICACHE_FLASH_ATTR;

#endif
