// #include "DL_30.h"

// #if DL30_DRIVER_ENABLE

// #include "ti_msp_dl_config.h"

// #define DL30_ECHO_TEST_ENABLE  1
// #define DL30_RX_BUFFER_SIZE    256U
// #define DL30_TX_BUFFER_SIZE    256U
// #define DL30_TASK_BYTE_BUDGET  32U

// #define DL30_RX_BUFFER_MASK    (DL30_RX_BUFFER_SIZE - 1U)
// #define DL30_TX_BUFFER_MASK    (DL30_TX_BUFFER_SIZE - 1U)

// static volatile uint8_t dl30_rx_buffer[DL30_RX_BUFFER_SIZE];
// static volatile uint16_t dl30_rx_head;
// static volatile uint16_t dl30_rx_tail;

// static volatile uint8_t dl30_tx_buffer[DL30_TX_BUFFER_SIZE];
// static volatile uint16_t dl30_tx_head;
// static volatile uint16_t dl30_tx_tail;

// volatile uint32_t g_dl30_rx_bytes;
// volatile uint32_t g_dl30_tx_bytes;
// volatile uint32_t g_dl30_rx_overflow;
// volatile uint32_t g_dl30_tx_overflow;
// volatile uint8_t g_dl30_last_rx_byte;

// static uint16_t DL30_NextIndex(uint16_t index, uint16_t mask)
// {
//     return (uint16_t)((index + 1U) & mask);
// }

// static bool DL30_TxQueueByte(uint8_t data)
// {
//     uint16_t next = DL30_NextIndex(dl30_tx_head, DL30_TX_BUFFER_MASK);

//     if (next == dl30_tx_tail)
//     {
//         g_dl30_tx_overflow++;
//         return false;
//     }

//     dl30_tx_buffer[dl30_tx_head] = data;
//     dl30_tx_head = next;
//     return true;
// }

// static void DL30_TxIrqService(void)
// {
//     while (dl30_tx_tail != dl30_tx_head)
//     {
//         uint8_t data = dl30_tx_buffer[dl30_tx_tail];

//         if (!DL_UART_Main_transmitDataCheck(UART_0_INST, data))
//         {
//             break;
//         }

//         dl30_tx_tail = DL30_NextIndex(dl30_tx_tail, DL30_TX_BUFFER_MASK);
//         g_dl30_tx_bytes++;
//     }

//     if (dl30_tx_tail == dl30_tx_head)
//     {
//         DL_UART_Main_disableInterrupt(UART_0_INST, DL_UART_MAIN_INTERRUPT_TX);
//     }
// }

// void DL30_Init(void)
// {
//     dl30_rx_head = 0U;
//     dl30_rx_tail = 0U;
//     dl30_tx_head = 0U;
//     dl30_tx_tail = 0U;

//     g_dl30_rx_bytes = 0U;
//     g_dl30_tx_bytes = 0U;
//     g_dl30_rx_overflow = 0U;
//     g_dl30_tx_overflow = 0U;
//     g_dl30_last_rx_byte = 0U;

//     DL_UART_Main_disableInterrupt(UART_0_INST, DL_UART_MAIN_INTERRUPT_TX);
//     DL_UART_Main_enableInterrupt(UART_0_INST, DL_UART_MAIN_INTERRUPT_RX);
//     NVIC_ClearPendingIRQ(UART_0_INST_INT_IRQN);
//     NVIC_EnableIRQ(UART_0_INST_INT_IRQN);
// }

// uint16_t DL30_Read(uint8_t *data, uint16_t length)
// {
//     uint16_t count = 0U;

//     if (data == 0)
//     {
//         return 0U;
//     }

//     while ((count < length) && (dl30_rx_tail != dl30_rx_head))
//     {
//         data[count] = dl30_rx_buffer[dl30_rx_tail];
//         dl30_rx_tail = DL30_NextIndex(dl30_rx_tail, DL30_RX_BUFFER_MASK);
//         count++;
//     }

//     return count;
// }

// uint16_t DL30_Write(const uint8_t *data, uint16_t length)
// {
//     uint16_t count = 0U;

//     if (data == 0)
//     {
//         return 0U;
//     }

//     while ((count < length) && DL30_TxQueueByte(data[count]))
//     {
//         count++;
//     }

//     if (count > 0U)
//     {
//         DL_UART_Main_enableInterrupt(UART_0_INST, DL_UART_MAIN_INTERRUPT_TX);
//     }

//     return count;
// }

// void DL30_Task(void)
// {
// #if DL30_ECHO_TEST_ENABLE
//     uint16_t count = 0U;

//     while ((count < DL30_TASK_BYTE_BUDGET) && (dl30_rx_tail != dl30_rx_head))
//     {
//         uint8_t data = dl30_rx_buffer[dl30_rx_tail];

//         if (!DL30_TxQueueByte(data))
//         {
//             break;
//         }

//         dl30_rx_tail = DL30_NextIndex(dl30_rx_tail, DL30_RX_BUFFER_MASK);
//         count++;
//     }

//     if (count > 0U)
//     {
//         DL_UART_Main_enableInterrupt(UART_0_INST, DL_UART_MAIN_INTERRUPT_TX);
//     }
// #endif
// }

// void UART_0_INST_IRQHandler(void)
// {
//     switch (DL_UART_Main_getPendingInterrupt(UART_0_INST))
//     {
//     case DL_UART_MAIN_IIDX_RX:
//         while (!DL_UART_Main_isRXFIFOEmpty(UART_0_INST))
//         {
//             uint8_t data = DL_UART_Main_receiveData(UART_0_INST);
//             uint16_t next = DL30_NextIndex(dl30_rx_head, DL30_RX_BUFFER_MASK);

//             g_dl30_last_rx_byte = data;
//             g_dl30_rx_bytes++;

//             if (next == dl30_rx_tail)
//             {
//                 g_dl30_rx_overflow++;
//             }
//             else
//             {
//                 dl30_rx_buffer[dl30_rx_head] = data;
//                 dl30_rx_head = next;
//             }
//         }
//         break;

//     case DL_UART_MAIN_IIDX_TX:
//         DL30_TxIrqService();
//         break;

//     default:
//         break;
//     }
// }

// #endif
