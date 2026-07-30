#ifndef __JY901S_H
#define __JY901S_H

#include <stdint.h>
#include "ti_msp_dl_config.h"

typedef struct
{
    float angle_x;
    float angle_y;
    float angle_z;

    float gyro_x;
    float gyro_y;
    float gyro_z;

    float accel_x;
    float accel_y;
    float accel_z;

    uint8_t status;
} Gyro_Data_t;

extern volatile Gyro_Data_t Gyro_Data;
extern volatile uint8_t Gyro_UpdateFlag;

void Gyro_Init(void);
void Gyro_ProcessByte(uint8_t byte);
void Gyro_ParseData(uint8_t *data, uint16_t length);

#endif
