/*
 * Copyright (c) 2023, Texas Instruments Incorporated - http://www.ti.com
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *  ============ ti_msp_dl_config.h =============
 *  Configured MSPM0 DriverLib module declarations
 *
 *  DO NOT EDIT - This file is generated for the MSPM0G350X
 *  by the SysConfig tool.
 */
#ifndef ti_msp_dl_config_h
#define ti_msp_dl_config_h

#define CONFIG_MSPM0G350X
#define CONFIG_MSPM0G3507

#if defined(__ti_version__) || defined(__TI_COMPILER_VERSION__)
#define SYSCONFIG_WEAK __attribute__((weak))
#elif defined(__IAR_SYSTEMS_ICC__)
#define SYSCONFIG_WEAK __weak
#elif defined(__GNUC__)
#define SYSCONFIG_WEAK __attribute__((weak))
#endif

#include <ti/devices/msp/msp.h>
#include <ti/driverlib/driverlib.h>
#include <ti/driverlib/m0p/dl_core.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 *  ======== SYSCFG_DL_init ========
 *  Perform all required MSP DL initialization
 *
 *  This function should be called once at a point before any use of
 *  MSP DL.
 */


/* clang-format off */

#define POWER_STARTUP_DELAY                                                (16)


#define GPIO_HFXT_PORT                                                     GPIOA
#define GPIO_HFXIN_PIN                                             DL_GPIO_PIN_5
#define GPIO_HFXIN_IOMUX                                         (IOMUX_PINCM10)
#define GPIO_HFXOUT_PIN                                            DL_GPIO_PIN_6
#define GPIO_HFXOUT_IOMUX                                        (IOMUX_PINCM11)
#define CPUCLK_FREQ                                                     80000000
/* Defines for SYSPLL_ERR_01 Workaround */
/* Represent 1.000 as 1000 */
#define FLOAT_TO_INT_SCALE                                               (1000U)
#define FCC_EXPECTED_RATIO                                                  2000
#define FCC_UPPER_BOUND                       (FCC_EXPECTED_RATIO * (1 + 0.003))
#define FCC_LOWER_BOUND                       (FCC_EXPECTED_RATIO * (1 - 0.003))

bool SYSCFG_DL_SYSCTL_SYSPLL_init(void);


/* Defines for PWM_0 */
#define PWM_0_INST                                                         TIMA1
#define PWM_0_INST_IRQHandler                                   TIMA1_IRQHandler
#define PWM_0_INST_INT_IRQN                                     (TIMA1_INT_IRQn)
#define PWM_0_INST_CLK_FREQ                                             80000000
/* GPIO defines for channel 0 */
#define GPIO_PWM_0_C0_PORT                                                 GPIOB
#define GPIO_PWM_0_C0_PIN                                          DL_GPIO_PIN_2
#define GPIO_PWM_0_C0_IOMUX                                      (IOMUX_PINCM15)
#define GPIO_PWM_0_C0_IOMUX_FUNC                     IOMUX_PINCM15_PF_TIMA1_CCP0
#define GPIO_PWM_0_C0_IDX                                    DL_TIMER_CC_0_INDEX
/* GPIO defines for channel 1 */
#define GPIO_PWM_0_C1_PORT                                                 GPIOB
#define GPIO_PWM_0_C1_PIN                                          DL_GPIO_PIN_3
#define GPIO_PWM_0_C1_IOMUX                                      (IOMUX_PINCM16)
#define GPIO_PWM_0_C1_IOMUX_FUNC                     IOMUX_PINCM16_PF_TIMA1_CCP1
#define GPIO_PWM_0_C1_IDX                                    DL_TIMER_CC_1_INDEX



/* Defines for TIMER_0 */
#define TIMER_0_INST                                                     (TIMG0)
#define TIMER_0_INST_IRQHandler                                 TIMG0_IRQHandler
#define TIMER_0_INST_INT_IRQN                                   (TIMG0_INT_IRQn)
#define TIMER_0_INST_LOAD_VALUE                                          (7999U)
/* Defines for TIMER_1 */
#define TIMER_1_INST                                                     (TIMA0)
#define TIMER_1_INST_IRQHandler                                 TIMA0_IRQHandler
#define TIMER_1_INST_INT_IRQN                                   (TIMA0_INT_IRQn)
#define TIMER_1_INST_LOAD_VALUE                                         (49999U)
/* Defines for STEP_TIM */
#define STEP_TIM_INST                                                    (TIMG6)
#define STEP_TIM_INST_IRQHandler                                TIMG6_IRQHandler
#define STEP_TIM_INST_INT_IRQN                                  (TIMG6_INT_IRQn)
#define STEP_TIM_INST_LOAD_VALUE                                          (999U)




/* Defines for I2C_0 */
#define I2C_0_INST                                                          I2C0
#define I2C_0_INST_IRQHandler                                    I2C0_IRQHandler
#define I2C_0_INST_INT_IRQN                                        I2C0_INT_IRQn
#define I2C_0_BUS_SPEED_HZ                                                100000
#define GPIO_I2C_0_SDA_PORT                                                GPIOA
#define GPIO_I2C_0_SDA_PIN                                        DL_GPIO_PIN_28
#define GPIO_I2C_0_IOMUX_SDA                                      (IOMUX_PINCM3)
#define GPIO_I2C_0_IOMUX_SDA_FUNC                       IOMUX_PINCM3_PF_I2C0_SDA
#define GPIO_I2C_0_SCL_PORT                                                GPIOA
#define GPIO_I2C_0_SCL_PIN                                        DL_GPIO_PIN_31
#define GPIO_I2C_0_IOMUX_SCL                                      (IOMUX_PINCM6)
#define GPIO_I2C_0_IOMUX_SCL_FUNC                       IOMUX_PINCM6_PF_I2C0_SCL


/* Defines for UART_0 */
#define UART_0_INST                                                        UART0
#define UART_0_INST_FREQUENCY                                           10000000
#define UART_0_INST_IRQHandler                                  UART0_IRQHandler
#define UART_0_INST_INT_IRQN                                      UART0_INT_IRQn
#define GPIO_UART_0_RX_PORT                                                GPIOB
#define GPIO_UART_0_TX_PORT                                                GPIOB
#define GPIO_UART_0_RX_PIN                                         DL_GPIO_PIN_1
#define GPIO_UART_0_TX_PIN                                         DL_GPIO_PIN_0
#define GPIO_UART_0_IOMUX_RX                                     (IOMUX_PINCM13)
#define GPIO_UART_0_IOMUX_TX                                     (IOMUX_PINCM12)
#define GPIO_UART_0_IOMUX_RX_FUNC                      IOMUX_PINCM13_PF_UART0_RX
#define GPIO_UART_0_IOMUX_TX_FUNC                      IOMUX_PINCM12_PF_UART0_TX
#define UART_0_BAUD_RATE                                                  (9600)
#define UART_0_IBRD_10_MHZ_9600_BAUD                                        (65)
#define UART_0_FBRD_10_MHZ_9600_BAUD                                         (7)
/* Defines for UART_1 */
#define UART_1_INST                                                        UART1
#define UART_1_INST_FREQUENCY                                           40000000
#define UART_1_INST_IRQHandler                                  UART1_IRQHandler
#define UART_1_INST_INT_IRQN                                      UART1_INT_IRQn
#define GPIO_UART_1_RX_PORT                                                GPIOB
#define GPIO_UART_1_TX_PORT                                                GPIOB
#define GPIO_UART_1_RX_PIN                                         DL_GPIO_PIN_5
#define GPIO_UART_1_TX_PIN                                         DL_GPIO_PIN_4
#define GPIO_UART_1_IOMUX_RX                                     (IOMUX_PINCM18)
#define GPIO_UART_1_IOMUX_TX                                     (IOMUX_PINCM17)
#define GPIO_UART_1_IOMUX_RX_FUNC                      IOMUX_PINCM18_PF_UART1_RX
#define GPIO_UART_1_IOMUX_TX_FUNC                      IOMUX_PINCM17_PF_UART1_TX
#define UART_1_BAUD_RATE                                                (115200)
#define UART_1_IBRD_40_MHZ_115200_BAUD                                      (21)
#define UART_1_FBRD_40_MHZ_115200_BAUD                                      (45)
/* Defines for UART_2 */
#define UART_2_INST                                                        UART2
#define UART_2_INST_FREQUENCY                                           40000000
#define UART_2_INST_IRQHandler                                  UART2_IRQHandler
#define UART_2_INST_INT_IRQN                                      UART2_INT_IRQn
#define GPIO_UART_2_RX_PORT                                                GPIOB
#define GPIO_UART_2_TX_PORT                                                GPIOB
#define GPIO_UART_2_RX_PIN                                        DL_GPIO_PIN_16
#define GPIO_UART_2_TX_PIN                                        DL_GPIO_PIN_15
#define GPIO_UART_2_IOMUX_RX                                     (IOMUX_PINCM33)
#define GPIO_UART_2_IOMUX_TX                                     (IOMUX_PINCM32)
#define GPIO_UART_2_IOMUX_RX_FUNC                      IOMUX_PINCM33_PF_UART2_RX
#define GPIO_UART_2_IOMUX_TX_FUNC                      IOMUX_PINCM32_PF_UART2_TX
#define UART_2_BAUD_RATE                                                (115200)
#define UART_2_IBRD_40_MHZ_115200_BAUD                                      (21)
#define UART_2_FBRD_40_MHZ_115200_BAUD                                      (45)
/* Defines for UART_3 */
#define UART_3_INST                                                        UART3
#define UART_3_INST_FREQUENCY                                           80000000
#define UART_3_INST_IRQHandler                                  UART3_IRQHandler
#define UART_3_INST_INT_IRQN                                      UART3_INT_IRQn
#define GPIO_UART_3_RX_PORT                                                GPIOA
#define GPIO_UART_3_TX_PORT                                                GPIOA
#define GPIO_UART_3_RX_PIN                                        DL_GPIO_PIN_25
#define GPIO_UART_3_TX_PIN                                        DL_GPIO_PIN_26
#define GPIO_UART_3_IOMUX_RX                                     (IOMUX_PINCM55)
#define GPIO_UART_3_IOMUX_TX                                     (IOMUX_PINCM59)
#define GPIO_UART_3_IOMUX_RX_FUNC                      IOMUX_PINCM55_PF_UART3_RX
#define GPIO_UART_3_IOMUX_TX_FUNC                      IOMUX_PINCM59_PF_UART3_TX
#define UART_3_BAUD_RATE                                                (115200)
#define UART_3_IBRD_80_MHZ_115200_BAUD                                      (43)
#define UART_3_FBRD_80_MHZ_115200_BAUD                                      (26)




/* Defines for SPI_LCD */
#define SPI_LCD_INST                                                       SPI1
#define SPI_LCD_INST_IRQHandler                                 SPI1_IRQHandler
#define SPI_LCD_INST_INT_IRQN                                     SPI1_INT_IRQn
#define GPIO_SPI_LCD_PICO_PORT                                            GPIOB
#define GPIO_SPI_LCD_PICO_PIN                                     DL_GPIO_PIN_8
#define GPIO_SPI_LCD_IOMUX_PICO                                 (IOMUX_PINCM25)
#define GPIO_SPI_LCD_IOMUX_PICO_FUNC                 IOMUX_PINCM25_PF_SPI1_PICO
#define GPIO_SPI_LCD_POCI_PORT                                            GPIOB
#define GPIO_SPI_LCD_POCI_PIN                                     DL_GPIO_PIN_7
#define GPIO_SPI_LCD_IOMUX_POCI                                 (IOMUX_PINCM24)
#define GPIO_SPI_LCD_IOMUX_POCI_FUNC                 IOMUX_PINCM24_PF_SPI1_POCI
/* GPIO configuration for SPI_LCD */
#define GPIO_SPI_LCD_SCLK_PORT                                            GPIOB
#define GPIO_SPI_LCD_SCLK_PIN                                     DL_GPIO_PIN_9
#define GPIO_SPI_LCD_IOMUX_SCLK                                 (IOMUX_PINCM26)
#define GPIO_SPI_LCD_IOMUX_SCLK_FUNC                 IOMUX_PINCM26_PF_SPI1_SCLK



/* Port definition for Pin Group DEBUG_LED */
#define DEBUG_LED_PORT                                                   (GPIOB)

/* Defines for PIN_22: GPIOB.22 with pinCMx 50 on package pin 21 */
#define DEBUG_LED_PIN_22_PIN                                    (DL_GPIO_PIN_22)
#define DEBUG_LED_PIN_22_IOMUX                                   (IOMUX_PINCM50)
/* Port definition for Pin Group Light */
#define Light_PORT                                                       (GPIOA)

/* Defines for PIN_29: GPIOA.29 with pinCMx 4 on package pin 36 */
#define Light_PIN_29_PIN                                        (DL_GPIO_PIN_29)
#define Light_PIN_29_IOMUX                                        (IOMUX_PINCM4)
/* Port definition for Pin Group Buzzer */
#define Buzzer_PORT                                                      (GPIOA)

/* Defines for PIN_11: GPIOA.11 with pinCMx 22 on package pin 57 */
#define Buzzer_PIN_11_PIN                                       (DL_GPIO_PIN_11)
#define Buzzer_PIN_11_IOMUX                                      (IOMUX_PINCM22)
/* Port definition for Pin Group ElectroMagnet */
#define ElectroMagnet_PORT                                               (GPIOA)

/* Defines for PINA_13: GPIOA.13 with pinCMx 35 on package pin 6 */
#define ElectroMagnet_PINA_13_PIN                               (DL_GPIO_PIN_13)
#define ElectroMagnet_PINA_13_IOMUX                              (IOMUX_PINCM35)
/* Defines for KEY_RIGHT: GPIOB.19 with pinCMx 45 on package pin 16 */
#define KEY_KEY_RIGHT_PORT                                               (GPIOB)
#define KEY_KEY_RIGHT_PIN                                       (DL_GPIO_PIN_19)
#define KEY_KEY_RIGHT_IOMUX                                      (IOMUX_PINCM45)
/* Defines for KEY_MID: GPIOB.17 with pinCMx 43 on package pin 14 */
#define KEY_KEY_MID_PORT                                                 (GPIOB)
#define KEY_KEY_MID_PIN                                         (DL_GPIO_PIN_17)
#define KEY_KEY_MID_IOMUX                                        (IOMUX_PINCM43)
/* Defines for KEY_UP: GPIOB.20 with pinCMx 48 on package pin 19 */
#define KEY_KEY_UP_PORT                                                  (GPIOB)
#define KEY_KEY_UP_PIN                                          (DL_GPIO_PIN_20)
#define KEY_KEY_UP_IOMUX                                         (IOMUX_PINCM48)
/* Defines for KEY_LEFT: GPIOA.14 with pinCMx 36 on package pin 7 */
#define KEY_KEY_LEFT_PORT                                                (GPIOA)
#define KEY_KEY_LEFT_PIN                                        (DL_GPIO_PIN_14)
#define KEY_KEY_LEFT_IOMUX                                       (IOMUX_PINCM36)
/* Defines for AIN1: GPIOB.6 with pinCMx 23 on package pin 58 */
#define TB6612_AIN1_PORT                                                 (GPIOB)
#define TB6612_AIN1_PIN                                          (DL_GPIO_PIN_6)
#define TB6612_AIN1_IOMUX                                        (IOMUX_PINCM23)
/* Defines for AIN2: GPIOA.12 with pinCMx 34 on package pin 5 */
#define TB6612_AIN2_PORT                                                 (GPIOA)
#define TB6612_AIN2_PIN                                         (DL_GPIO_PIN_12)
#define TB6612_AIN2_IOMUX                                        (IOMUX_PINCM34)
/* Defines for BIN1: GPIOB.23 with pinCMx 51 on package pin 22 */
#define TB6612_BIN1_PORT                                                 (GPIOB)
#define TB6612_BIN1_PIN                                         (DL_GPIO_PIN_23)
#define TB6612_BIN1_IOMUX                                        (IOMUX_PINCM51)
/* Defines for BIN2: GPIOB.27 with pinCMx 58 on package pin 29 */
#define TB6612_BIN2_PORT                                                 (GPIOB)
#define TB6612_BIN2_PIN                                         (DL_GPIO_PIN_27)
#define TB6612_BIN2_IOMUX                                        (IOMUX_PINCM58)
/* Port definition for Pin Group ENCODER */
#define ENCODER_PORT                                                     (GPIOA)

/* Defines for E1A: GPIOA.1 with pinCMx 2 on package pin 34 */
// pins affected by this interrupt request:["E1A","E1B","E2A","E2B"]
#define ENCODER_INT_IRQN                                        (GPIOA_INT_IRQn)
#define ENCODER_INT_IIDX                        (DL_INTERRUPT_GROUP1_IIDX_GPIOA)
#define ENCODER_E1A_IIDX                                     (DL_GPIO_IIDX_DIO1)
#define ENCODER_E1A_PIN                                          (DL_GPIO_PIN_1)
#define ENCODER_E1A_IOMUX                                         (IOMUX_PINCM2)
/* Defines for E1B: GPIOA.0 with pinCMx 1 on package pin 33 */
#define ENCODER_E1B_IIDX                                     (DL_GPIO_IIDX_DIO0)
#define ENCODER_E1B_PIN                                          (DL_GPIO_PIN_0)
#define ENCODER_E1B_IOMUX                                         (IOMUX_PINCM1)
/* Defines for E2A: GPIOA.8 with pinCMx 19 on package pin 54 */
#define ENCODER_E2A_IIDX                                     (DL_GPIO_IIDX_DIO8)
#define ENCODER_E2A_PIN                                          (DL_GPIO_PIN_8)
#define ENCODER_E2A_IOMUX                                        (IOMUX_PINCM19)
/* Defines for E2B: GPIOA.9 with pinCMx 20 on package pin 55 */
#define ENCODER_E2B_IIDX                                     (DL_GPIO_IIDX_DIO9)
#define ENCODER_E2B_PIN                                          (DL_GPIO_PIN_9)
#define ENCODER_E2B_IOMUX                                        (IOMUX_PINCM20)
/* Port definition for Pin Group LCD */
#define LCD_PORT                                                         (GPIOB)

/* Defines for RES: GPIOB.10 with pinCMx 27 on package pin 62 */
#define LCD_RES_PIN                                             (DL_GPIO_PIN_10)
#define LCD_RES_IOMUX                                            (IOMUX_PINCM27)
/* Defines for DC: GPIOB.11 with pinCMx 28 on package pin 63 */
#define LCD_DC_PIN                                              (DL_GPIO_PIN_11)
#define LCD_DC_IOMUX                                             (IOMUX_PINCM28)
/* Defines for CS: GPIOB.14 with pinCMx 31 on package pin 2 */
#define LCD_CS_PIN                                              (DL_GPIO_PIN_14)
#define LCD_CS_IOMUX                                             (IOMUX_PINCM31)
/* Defines for BLK: GPIOB.26 with pinCMx 57 on package pin 28 */
#define LCD_BLK_PIN                                             (DL_GPIO_PIN_26)
#define LCD_BLK_IOMUX                                            (IOMUX_PINCM57)
/* Defines for T1: GPIOA.24 with pinCMx 54 on package pin 25 */
#define Track_T1_PORT                                                    (GPIOA)
#define Track_T1_PIN                                            (DL_GPIO_PIN_24)
#define Track_T1_IOMUX                                           (IOMUX_PINCM54)
/* Defines for T2: GPIOB.24 with pinCMx 52 on package pin 23 */
#define Track_T2_PORT                                                    (GPIOB)
#define Track_T2_PIN                                            (DL_GPIO_PIN_24)
#define Track_T2_IOMUX                                           (IOMUX_PINCM52)
/* Defines for T3: GPIOA.22 with pinCMx 47 on package pin 18 */
#define Track_T3_PORT                                                    (GPIOA)
#define Track_T3_PIN                                            (DL_GPIO_PIN_22)
#define Track_T3_IOMUX                                           (IOMUX_PINCM47)
/* Defines for T4: GPIOA.15 with pinCMx 37 on package pin 8 */
#define Track_T4_PORT                                                    (GPIOA)
#define Track_T4_PIN                                            (DL_GPIO_PIN_15)
#define Track_T4_IOMUX                                           (IOMUX_PINCM37)
/* Defines for T5: GPIOB.12 with pinCMx 29 on package pin 64 */
#define Track_T5_PORT                                                    (GPIOB)
#define Track_T5_PIN                                            (DL_GPIO_PIN_12)
#define Track_T5_IOMUX                                           (IOMUX_PINCM29)
/* Defines for T6: GPIOB.13 with pinCMx 30 on package pin 1 */
#define Track_T6_PORT                                                    (GPIOB)
#define Track_T6_PIN                                            (DL_GPIO_PIN_13)
#define Track_T6_IOMUX                                           (IOMUX_PINCM30)
/* Defines for T7: GPIOA.17 with pinCMx 39 on package pin 10 */
#define Track_T7_PORT                                                    (GPIOA)
#define Track_T7_PIN                                            (DL_GPIO_PIN_17)
#define Track_T7_IOMUX                                           (IOMUX_PINCM39)
/* Defines for T8: GPIOA.16 with pinCMx 38 on package pin 9 */
#define Track_T8_PORT                                                    (GPIOA)
#define Track_T8_PIN                                            (DL_GPIO_PIN_16)
#define Track_T8_IOMUX                                           (IOMUX_PINCM38)


/* clang-format on */

void SYSCFG_DL_init(void);
void SYSCFG_DL_initPower(void);
void SYSCFG_DL_GPIO_init(void);
void SYSCFG_DL_SYSCTL_init(void);
void SYSCFG_DL_SYSCTL_CLK_init(void);

bool SYSCFG_DL_SYSCTL_SYSPLL_init(void);
void SYSCFG_DL_PWM_0_init(void);
void SYSCFG_DL_TIMER_0_init(void);
void SYSCFG_DL_TIMER_1_init(void);
void SYSCFG_DL_STEP_TIM_init(void);
void SYSCFG_DL_I2C_0_init(void);
void SYSCFG_DL_UART_0_init(void);
void SYSCFG_DL_UART_1_init(void);
void SYSCFG_DL_UART_2_init(void);
void SYSCFG_DL_UART_3_init(void);
void SYSCFG_DL_SPI_LCD_init(void);


bool SYSCFG_DL_saveConfiguration(void);
bool SYSCFG_DL_restoreConfiguration(void);

#ifdef __cplusplus
}
#endif

#endif /* ti_msp_dl_config_h */
