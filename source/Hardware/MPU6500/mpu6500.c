#include <math.h>
#include <string.h>

#include "ti_msp_dl_config.h"

#include "Hardware/MPU6500/mpu6500.h"

#define MPU6500_ADDRESS_AD0_LOW              (0x68U)
#define MPU6500_ADDRESS_AD0_HIGH             (0x69U)
#define MPU6050_WHO_AM_I_VALUE               (0x68U)
#define MPU6500_WHO_AM_I_VALUE               (0x70U)

#define MPU6500_REG_SMPLRT_DIV               (0x19U)
#define MPU6500_REG_CONFIG                   (0x1AU)
#define MPU6500_REG_GYRO_CONFIG              (0x1BU)
#define MPU6500_REG_ACCEL_CONFIG             (0x1CU)
#define MPU6500_REG_ACCEL_CONFIG_2           (0x1DU)
#define MPU6500_REG_ACCEL_XOUT_H             (0x3BU)
#define MPU6500_REG_USER_CTRL                (0x6AU)
#define MPU6500_REG_PWR_MGMT_1               (0x6BU)
#define MPU6500_REG_PWR_MGMT_2               (0x6CU)
#define MPU6500_REG_WHO_AM_I                 (0x75U)

#define MPU6500_DEVICE_RESET                 (0x80U)
#define MPU6500_CLOCK_PLL_XGYRO              (0x01U)
#define MPU6500_DLPF_41HZ                    (0x03U)
#define MPU6500_GYRO_RANGE_2000DPS           (0x18U)
#define MPU6500_ACCEL_RANGE_16G              (0x18U)
#define MPU6500_SAMPLE_RATE_DIV              (9U)

#define MPU6500_BURST_DATA_LENGTH            (14U)
#define MPU6500_I2C_TIMEOUT_LOOPS            (CPUCLK_FREQ / 100U)
#define MPU6500_DEFAULT_DELTA_TIME_S          (0.01f)
#define MPU6500_MAX_DELTA_TIME_S              (0.5f)
#define MPU6500_COMPLEMENTARY_ALPHA           (0.98f)
#define MPU6500_RAD_TO_DEG                    (57.2957795f)
#define MPU6500_ACCEL_SCALE_LSB_PER_G         (2048.0f)
#define MPU6500_GYRO_SCALE_LSB_PER_DPS        (16.4f)

volatile MPU6500_Data_t MPU6500_Data;
volatile uint8_t MPU6500_UpdateFlag = 0U;
volatile uint8_t MPU6500_DeviceAddress = 0U;
volatile uint8_t MPU6500_WhoAmI = 0U;
volatile MPU6500_Status_t MPU6500_LastStatus = MPU6500_STATUS_NOT_INITIALIZED;
volatile MPU6XXX_DeviceType_t MPU6500_DeviceType = MPU6XXX_DEVICE_UNKNOWN;

static bool g_mpu6500_initialized = false;
static bool g_mpu6500_attitude_initialized = false;
static float g_mpu6500_gyro_bias_x = 0.0f;
static float g_mpu6500_gyro_bias_y = 0.0f;
static float g_mpu6500_gyro_bias_z = 0.0f;

static void MPU6500_DelayMs(uint32_t milliseconds)
{
    while (milliseconds > 0U)
    {
        delay_cycles(CPUCLK_FREQ / 1000U);
        milliseconds--;
    }
}

static void MPU6500_ResetControllerTransfer(void)
{
    DL_I2C_resetControllerTransfer(I2C_0_INST);
    DL_I2C_flushControllerTXFIFO(I2C_0_INST);
    DL_I2C_flushControllerRXFIFO(I2C_0_INST);
}

static MPU6500_Status_t MPU6500_WaitControllerIdle(void)
{
    uint32_t timeout = MPU6500_I2C_TIMEOUT_LOOPS;

    while (timeout > 0U)
    {
        uint32_t controller_status = DL_I2C_getControllerStatus(I2C_0_INST);

        if ((controller_status & DL_I2C_CONTROLLER_STATUS_IDLE) != 0U)
        {
            return MPU6500_STATUS_OK;
        }
        timeout--;
    }

    MPU6500_ResetControllerTransfer();
    return MPU6500_STATUS_TIMEOUT;
}

static MPU6500_Status_t MPU6500_WaitTransferComplete(void)
{
    uint32_t timeout = MPU6500_I2C_TIMEOUT_LOOPS;

    while (timeout > 0U)
    {
        uint32_t controller_status = DL_I2C_getControllerStatus(I2C_0_INST);

        if ((controller_status & (DL_I2C_CONTROLLER_STATUS_ERROR |
                                  DL_I2C_CONTROLLER_STATUS_ARBITRATION_LOST)) != 0U)
        {
            MPU6500_ResetControllerTransfer();
            return MPU6500_STATUS_I2C_ERROR;
        }
        if ((controller_status & DL_I2C_CONTROLLER_STATUS_BUSY) == 0U)
        {
            return MPU6500_STATUS_OK;
        }
        timeout--;
    }

    MPU6500_ResetControllerTransfer();
    return MPU6500_STATUS_TIMEOUT;
}

static MPU6500_Status_t MPU6500_WaitBusReleased(void)
{
    uint32_t timeout = MPU6500_I2C_TIMEOUT_LOOPS;

    while (timeout > 0U)
    {
        uint32_t controller_status = DL_I2C_getControllerStatus(I2C_0_INST);

        if ((controller_status & DL_I2C_CONTROLLER_STATUS_BUSY_BUS) == 0U)
        {
            return MPU6500_STATUS_OK;
        }
        timeout--;
    }

    MPU6500_ResetControllerTransfer();
    return MPU6500_STATUS_TIMEOUT;
}

static MPU6500_Status_t MPU6500_WriteRegister(
    uint8_t device_address, uint8_t register_address, uint8_t value)
{
    uint8_t tx_data[2] = {register_address, value};
    MPU6500_Status_t result;

    result = MPU6500_WaitControllerIdle();
    if (result != MPU6500_STATUS_OK)
    {
        return result;
    }

    DL_I2C_flushControllerTXFIFO(I2C_0_INST);
    if (DL_I2C_fillControllerTXFIFO(I2C_0_INST, tx_data, sizeof(tx_data)) != sizeof(tx_data))
    {
        MPU6500_ResetControllerTransfer();
        return MPU6500_STATUS_I2C_ERROR;
    }

    DL_I2C_startControllerTransferAdvanced(I2C_0_INST, device_address,
        DL_I2C_CONTROLLER_DIRECTION_TX, sizeof(tx_data),
        DL_I2C_CONTROLLER_START_ENABLE, DL_I2C_CONTROLLER_STOP_ENABLE,
        DL_I2C_CONTROLLER_ACK_DISABLE);

    result = MPU6500_WaitTransferComplete();
    if (result != MPU6500_STATUS_OK)
    {
        return result;
    }
    return MPU6500_WaitBusReleased();
}

static MPU6500_Status_t MPU6500_ReadRegisters(uint8_t device_address,
    uint8_t register_address, uint8_t *data, uint16_t length)
{
    uint16_t received = 0U;
    uint32_t timeout;
    MPU6500_Status_t result;

    if ((data == NULL) || (length == 0U))
    {
        return MPU6500_STATUS_INVALID_ARGUMENT;
    }

    result = MPU6500_WaitControllerIdle();
    if (result != MPU6500_STATUS_OK)
    {
        return result;
    }

    DL_I2C_flushControllerTXFIFO(I2C_0_INST);
    DL_I2C_flushControllerRXFIFO(I2C_0_INST);
    if (DL_I2C_fillControllerTXFIFO(I2C_0_INST, &register_address, 1U) != 1U)
    {
        MPU6500_ResetControllerTransfer();
        return MPU6500_STATUS_I2C_ERROR;
    }

    /* 先发送寄存器地址但不产生 STOP，随后通过重复 START 进入读取。 */
    DL_I2C_startControllerTransferAdvanced(I2C_0_INST, device_address,
        DL_I2C_CONTROLLER_DIRECTION_TX, 1U,
        DL_I2C_CONTROLLER_START_ENABLE, DL_I2C_CONTROLLER_STOP_DISABLE,
        DL_I2C_CONTROLLER_ACK_DISABLE);

    result = MPU6500_WaitTransferComplete();
    if (result != MPU6500_STATUS_OK)
    {
        return result;
    }

    DL_I2C_startControllerTransferAdvanced(I2C_0_INST, device_address,
        DL_I2C_CONTROLLER_DIRECTION_RX, length,
        DL_I2C_CONTROLLER_START_ENABLE, DL_I2C_CONTROLLER_STOP_ENABLE,
        DL_I2C_CONTROLLER_ACK_DISABLE);

    timeout = MPU6500_I2C_TIMEOUT_LOOPS;
    while ((received < length) && (timeout > 0U))
    {
        uint32_t controller_status = DL_I2C_getControllerStatus(I2C_0_INST);

        if ((controller_status & (DL_I2C_CONTROLLER_STATUS_ERROR |
                                  DL_I2C_CONTROLLER_STATUS_ARBITRATION_LOST)) != 0U)
        {
            MPU6500_ResetControllerTransfer();
            return MPU6500_STATUS_I2C_ERROR;
        }

        while ((received < length) &&
               !DL_I2C_isControllerRXFIFOEmpty(I2C_0_INST))
        {
            data[received] = DL_I2C_receiveControllerData(I2C_0_INST);
            received++;
            timeout = MPU6500_I2C_TIMEOUT_LOOPS;
        }
        timeout--;
    }

    if (received != length)
    {
        MPU6500_ResetControllerTransfer();
        return MPU6500_STATUS_TIMEOUT;
    }

    result = MPU6500_WaitTransferComplete();
    if (result != MPU6500_STATUS_OK)
    {
        return result;
    }
    return MPU6500_WaitBusReleased();
}

static int16_t MPU6500_MakeInt16(uint8_t high_byte, uint8_t low_byte)
{
    return (int16_t)(((uint16_t)high_byte << 8U) | (uint16_t)low_byte);
}

static MPU6500_Status_t MPU6500_ReadConvertedData(float *accel_x,
    float *accel_y, float *accel_z, float *gyro_x, float *gyro_y, float *gyro_z)
{
    uint8_t raw_data[MPU6500_BURST_DATA_LENGTH];
    MPU6500_Status_t result;

    result = MPU6500_ReadRegisters(MPU6500_DeviceAddress,
        MPU6500_REG_ACCEL_XOUT_H, raw_data, sizeof(raw_data));
    if (result != MPU6500_STATUS_OK)
    {
        return result;
    }

    *accel_x = (float)MPU6500_MakeInt16(raw_data[0], raw_data[1]) /
        MPU6500_ACCEL_SCALE_LSB_PER_G;
    *accel_y = (float)MPU6500_MakeInt16(raw_data[2], raw_data[3]) /
        MPU6500_ACCEL_SCALE_LSB_PER_G;
    *accel_z = (float)MPU6500_MakeInt16(raw_data[4], raw_data[5]) /
        MPU6500_ACCEL_SCALE_LSB_PER_G;

    /* raw_data[6:7] 是温度，本驱动按需求不换算、不输出。 */
    *gyro_x = (float)MPU6500_MakeInt16(raw_data[8], raw_data[9]) /
        MPU6500_GYRO_SCALE_LSB_PER_DPS;
    *gyro_y = (float)MPU6500_MakeInt16(raw_data[10], raw_data[11]) /
        MPU6500_GYRO_SCALE_LSB_PER_DPS;
    *gyro_z = (float)MPU6500_MakeInt16(raw_data[12], raw_data[13]) /
        MPU6500_GYRO_SCALE_LSB_PER_DPS;

    return MPU6500_STATUS_OK;
}

static MPU6500_Status_t MPU6500_SetStatus(MPU6500_Status_t status)
{
    MPU6500_LastStatus = status;
    MPU6500_Data.status = (uint8_t)status;
    if (status != MPU6500_STATUS_OK)
    {
        MPU6500_UpdateFlag = 0U;
    }
    return status;
}

MPU6500_Status_t MPU6500_Init(void)
{
    static const uint8_t candidate_addresses[] = {
        MPU6500_ADDRESS_AD0_LOW,
        MPU6500_ADDRESS_AD0_HIGH
    };
    uint8_t index;
    uint8_t who_am_i = 0U;
    MPU6500_Status_t result = MPU6500_STATUS_DEVICE_NOT_FOUND;

    memset((void *)&MPU6500_Data, 0, sizeof(MPU6500_Data));
    MPU6500_UpdateFlag = 0U;
    MPU6500_DeviceAddress = 0U;
    MPU6500_WhoAmI = 0U;
    MPU6500_DeviceType = MPU6XXX_DEVICE_UNKNOWN;
    g_mpu6500_initialized = false;
    g_mpu6500_attitude_initialized = false;
    g_mpu6500_gyro_bias_x = 0.0f;
    g_mpu6500_gyro_bias_y = 0.0f;
    g_mpu6500_gyro_bias_z = 0.0f;
    MPU6500_ResetControllerTransfer();

    for (index = 0U; index < sizeof(candidate_addresses); index++)
    {
        result = MPU6500_ReadRegisters(candidate_addresses[index],
            MPU6500_REG_WHO_AM_I, &who_am_i, 1U);
        if (result == MPU6500_STATUS_OK)
        {
            MPU6500_DeviceAddress = candidate_addresses[index];
            MPU6500_WhoAmI = who_am_i;
            break;
        }
    }

    if (MPU6500_DeviceAddress == 0U)
    {
        return MPU6500_SetStatus(MPU6500_STATUS_DEVICE_NOT_FOUND);
    }
    if (MPU6500_WhoAmI == MPU6500_WHO_AM_I_VALUE)
    {
        MPU6500_DeviceType = MPU6XXX_DEVICE_MPU6500;
    }
    else if (MPU6500_WhoAmI == MPU6050_WHO_AM_I_VALUE)
    {
        MPU6500_DeviceType = MPU6XXX_DEVICE_MPU6050;
    }
    else
    {
        return MPU6500_SetStatus(MPU6500_STATUS_WHO_AM_I_ERROR);
    }

    result = MPU6500_WriteRegister(MPU6500_DeviceAddress,
        MPU6500_REG_PWR_MGMT_1, MPU6500_DEVICE_RESET);
    if (result != MPU6500_STATUS_OK)
    {
        return MPU6500_SetStatus(result);
    }
    MPU6500_DelayMs(100U);

    result = MPU6500_WriteRegister(MPU6500_DeviceAddress,
        MPU6500_REG_PWR_MGMT_1, MPU6500_CLOCK_PLL_XGYRO);
    if (result == MPU6500_STATUS_OK)
    {
        result = MPU6500_WriteRegister(MPU6500_DeviceAddress,
            MPU6500_REG_PWR_MGMT_2, 0x00U);
    }
    if (result == MPU6500_STATUS_OK)
    {
        result = MPU6500_WriteRegister(MPU6500_DeviceAddress,
            MPU6500_REG_USER_CTRL, 0x00U);
    }
    if (result == MPU6500_STATUS_OK)
    {
        result = MPU6500_WriteRegister(MPU6500_DeviceAddress,
            MPU6500_REG_CONFIG, MPU6500_DLPF_41HZ);
    }
    if (result == MPU6500_STATUS_OK)
    {
        result = MPU6500_WriteRegister(MPU6500_DeviceAddress,
            MPU6500_REG_SMPLRT_DIV, MPU6500_SAMPLE_RATE_DIV);
    }
    if (result == MPU6500_STATUS_OK)
    {
        result = MPU6500_WriteRegister(MPU6500_DeviceAddress,
            MPU6500_REG_GYRO_CONFIG, MPU6500_GYRO_RANGE_2000DPS);
    }
    if (result == MPU6500_STATUS_OK)
    {
        result = MPU6500_WriteRegister(MPU6500_DeviceAddress,
            MPU6500_REG_ACCEL_CONFIG, MPU6500_ACCEL_RANGE_16G);
    }
    if ((result == MPU6500_STATUS_OK) &&
        (MPU6500_DeviceType == MPU6XXX_DEVICE_MPU6500))
    {
        result = MPU6500_WriteRegister(MPU6500_DeviceAddress,
            MPU6500_REG_ACCEL_CONFIG_2, MPU6500_DLPF_41HZ);
    }
    if (result != MPU6500_STATUS_OK)
    {
        return MPU6500_SetStatus(result);
    }

    MPU6500_DelayMs(10U);
    g_mpu6500_initialized = true;
    return MPU6500_SetStatus(MPU6500_STATUS_OK);
}

MPU6500_Status_t MPU6500_Update(float delta_time_s)
{
    float accel_x;
    float accel_y;
    float accel_z;
    float gyro_x;
    float gyro_y;
    float gyro_z;
    float accel_angle_x;
    float accel_angle_y;
    MPU6500_Status_t result;

    if (!g_mpu6500_initialized)
    {
        return MPU6500_SetStatus(MPU6500_STATUS_NOT_INITIALIZED);
    }
    if ((delta_time_s <= 0.0f) || (delta_time_s > MPU6500_MAX_DELTA_TIME_S))
    {
        delta_time_s = MPU6500_DEFAULT_DELTA_TIME_S;
    }

    result = MPU6500_ReadConvertedData(&accel_x, &accel_y, &accel_z,
        &gyro_x, &gyro_y, &gyro_z);
    if (result != MPU6500_STATUS_OK)
    {
        return MPU6500_SetStatus(result);
    }

    gyro_x -= g_mpu6500_gyro_bias_x;
    gyro_y -= g_mpu6500_gyro_bias_y;
    gyro_z -= g_mpu6500_gyro_bias_z;

    accel_angle_x = atan2f(accel_y,
        sqrtf((accel_x * accel_x) + (accel_z * accel_z))) * MPU6500_RAD_TO_DEG;
    accel_angle_y = atan2f(-accel_x,
        sqrtf((accel_y * accel_y) + (accel_z * accel_z))) * MPU6500_RAD_TO_DEG;

    if (!g_mpu6500_attitude_initialized)
    {
        MPU6500_Data.angle_x = accel_angle_x;
        MPU6500_Data.angle_y = accel_angle_y;
        MPU6500_Data.angle_z = 0.0f;
        g_mpu6500_attitude_initialized = true;
    }
    else
    {
        MPU6500_Data.angle_x = MPU6500_COMPLEMENTARY_ALPHA *
            (MPU6500_Data.angle_x + (gyro_x * delta_time_s)) +
            ((1.0f - MPU6500_COMPLEMENTARY_ALPHA) * accel_angle_x);
        MPU6500_Data.angle_y = MPU6500_COMPLEMENTARY_ALPHA *
            (MPU6500_Data.angle_y + (gyro_y * delta_time_s)) +
            ((1.0f - MPU6500_COMPLEMENTARY_ALPHA) * accel_angle_y);
        MPU6500_Data.angle_z += gyro_z * delta_time_s;

        if (MPU6500_Data.angle_z > 180.0f)
        {
            MPU6500_Data.angle_z -= 360.0f;
        }
        else if (MPU6500_Data.angle_z < -180.0f)
        {
            MPU6500_Data.angle_z += 360.0f;
        }
    }

    MPU6500_Data.accel_x = accel_x;
    MPU6500_Data.accel_y = accel_y;
    MPU6500_Data.accel_z = accel_z;
    MPU6500_Data.gyro_x = gyro_x;
    MPU6500_Data.gyro_y = gyro_y;
    MPU6500_Data.gyro_z = gyro_z;
    MPU6500_UpdateFlag = 1U;

    return MPU6500_SetStatus(MPU6500_STATUS_OK);
}

MPU6500_Status_t MPU6500_CalibrateGyro(uint16_t sample_count)
{
    uint16_t sample_index;
    float accel_x;
    float accel_y;
    float accel_z;
    float gyro_x;
    float gyro_y;
    float gyro_z;
    float gyro_sum_x = 0.0f;
    float gyro_sum_y = 0.0f;
    float gyro_sum_z = 0.0f;
    MPU6500_Status_t result;

    if (!g_mpu6500_initialized)
    {
        return MPU6500_SetStatus(MPU6500_STATUS_NOT_INITIALIZED);
    }
    if (sample_count == 0U)
    {
        return MPU6500_SetStatus(MPU6500_STATUS_INVALID_ARGUMENT);
    }

    for (sample_index = 0U; sample_index < sample_count; sample_index++)
    {
        result = MPU6500_ReadConvertedData(&accel_x, &accel_y, &accel_z,
            &gyro_x, &gyro_y, &gyro_z);
        if (result != MPU6500_STATUS_OK)
        {
            return MPU6500_SetStatus(result);
        }

        gyro_sum_x += gyro_x;
        gyro_sum_y += gyro_y;
        gyro_sum_z += gyro_z;
        MPU6500_DelayMs(2U);
    }

    g_mpu6500_gyro_bias_x = gyro_sum_x / (float)sample_count;
    g_mpu6500_gyro_bias_y = gyro_sum_y / (float)sample_count;
    g_mpu6500_gyro_bias_z = gyro_sum_z / (float)sample_count;
    g_mpu6500_attitude_initialized = false;

    return MPU6500_SetStatus(MPU6500_STATUS_OK);
}

bool MPU6500_IsReady(void)
{
    return g_mpu6500_initialized;
}
