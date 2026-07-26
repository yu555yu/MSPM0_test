// #ifndef __DL_30_H__
// #define __DL_30_H__

// #include <stdint.h>

// // DL-30 备用方案：1=启用，0=禁用（当前使用 HC-05）
// #define DL30_DRIVER_ENABLE  0

// #if DL30_DRIVER_ENABLE
// extern volatile uint32_t g_dl30_rx_bytes;
// extern volatile uint32_t g_dl30_tx_bytes;
// extern volatile uint32_t g_dl30_rx_overflow;
// extern volatile uint32_t g_dl30_tx_overflow;
// extern volatile uint8_t g_dl30_last_rx_byte;

// void DL30_Init(void);
// void DL30_Task(void);
// uint16_t DL30_Read(uint8_t *data, uint16_t length);
// uint16_t DL30_Write(const uint8_t *data, uint16_t length);
// #endif

// #endif
