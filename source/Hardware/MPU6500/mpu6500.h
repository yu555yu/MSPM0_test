#ifndef HARDWARE_MPU6500_MPU6500_H_
#define HARDWARE_MPU6500_MPU6500_H_

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    MPU6500_STATUS_OK = 0,
    MPU6500_STATUS_NOT_INITIALIZED,
    MPU6500_STATUS_TIMEOUT,
    MPU6500_STATUS_I2C_ERROR,
    MPU6500_STATUS_DEVICE_NOT_FOUND,
    MPU6500_STATUS_WHO_AM_I_ERROR,
    MPU6500_STATUS_INVALID_ARGUMENT
} MPU6500_Status_t;

typedef enum
{
    MPU6XXX_DEVICE_UNKNOWN = 0,
    MPU6XXX_DEVICE_MPU6050,
    MPU6XXX_DEVICE_MPU6500
} MPU6XXX_DeviceType_t;

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
} MPU6500_Data_t;

extern volatile MPU6500_Data_t MPU6500_Data;
extern volatile uint8_t MPU6500_UpdateFlag;
extern volatile uint8_t MPU6500_DeviceAddress;
extern volatile uint8_t MPU6500_WhoAmI;
extern volatile MPU6500_Status_t MPU6500_LastStatus;
extern volatile MPU6XXX_DeviceType_t MPU6500_DeviceType;

/**
 * @brief 探测并初始化 MPU6500，也兼容基础六轴功能相同的 MPU6050。
 *
 * 会依次探测 7 位地址 0x68 和 0x69。调用前必须已经执行
 * SYSCFG_DL_init()，且 I2C0 已由 SysConfig 配置完成。
 */
MPU6500_Status_t MPU6500_Init(void);

/**
 * @brief 读取一次传感器并更新 MPU6500_Data。
 *
 * @param delta_time_s 距离上次成功更新的秒数，建议传入 0.01f。
 *                     参数异常时会自动按 0.01s 处理。
 */
MPU6500_Status_t MPU6500_Update(float delta_time_s);

/**
 * @brief 静止状态下采样并计算陀螺仪零偏。
 *
 * @param sample_count 建议 200，必须大于 0。
 */
MPU6500_Status_t MPU6500_CalibrateGyro(uint16_t sample_count);

bool MPU6500_IsReady(void);

#endif
