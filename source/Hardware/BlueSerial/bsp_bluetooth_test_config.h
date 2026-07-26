// #ifndef __BSP_BLUETOOTH_TEST_CONFIG_H__
// #define __BSP_BLUETOOTH_TEST_CONFIG_H__

// /*
//  * 裸板蓝牙测试目标选择：一次只允许一个模块占用 UART0。
//  * 这只是测试驱动选择，不改变 HC-05 固定为 MASTER、HC-04 固定为 SLAVE 的身份。
//  */
// #define BLUETOOTH_TEST_HC05_MASTER  1U
// #define BLUETOOTH_TEST_HC04_SLAVE   2U

// /* 当前安全测试：MCU UART0 <-> HC-04 SLAVE，HC-05 + CH340 接电脑。 */
// #define BLUETOOTH_TEST_TARGET       BLUETOOTH_TEST_HC04_SLAVE

// #if ((BLUETOOTH_TEST_TARGET != BLUETOOTH_TEST_HC05_MASTER) && \
//      (BLUETOOTH_TEST_TARGET != BLUETOOTH_TEST_HC04_SLAVE))
// #error "Invalid BLUETOOTH_TEST_TARGET"
// #endif

// #endif
