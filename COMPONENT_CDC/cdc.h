/***************************************************************************//**
* \copyright
* (c) (2026), Cypress Semiconductor Corporation (an Infineon company) or
* an affiliate of Cypress Semiconductor Corporation.
*
* SPDX-License-Identifier: Apache-2.0
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*******************************************************************************/

#ifndef INCLUDED_CDC_H
#define INCLUDED_CDC_H

#include "cy_pdl.h"
#include "cy_device.h"
#include "cy_usb_common.h"
#include "cy_usbhs_cal_drv.h"
#include "cy_hbdma.h"
#include "cy_hbdma_mgr.h"
#include "cy_usb_usbd.h"
#include "cy_usb_app.h"
#include "cy_debug.h"
#include "cy_usbhs_dw_wrapper.h"

/*****************************************************************************
*                            Macros                                          *
******************************************************************************/
#define CDC_APP_INFO(str, ...)                                  (Cy_Debug_AddToLog(3, "CDC:APP:"str, ##__VA_ARGS__))
#define CDC_SET_LINE_CODING                                     (0x20)
#define CDC_GET_LINE_CODING                                     (0x21)
#define CDC_SET_CONTROL_LINE_STATE                              (0x22)
#define CY_USB_VCOM_DEVICE_MSG_DIAG_DUMP                        (0x26)

#define CDC_VCOM_DEVICE_MSG_QUEUE_SIZE                          (96)
#define CDC_VCOM_MAX_QUEUE_SIZE                                 (2)
#define CDC_VCOM_DEVICE_MSG_SIZE                                (sizeof (cy_stc_usbd_app_msg_t))
#define CDC_VCOM_DIAG_DUMP_PERIOD_MS                            (15000u)
#define CDC_VCOM_RESPONSE_TIME                                  (1000)  /**< UART RX buffer polling duration in milliseconds. */
#define CDC_VCOM_UART_MAX_RD_SIZE                               (64u)   /**< Maximum size of data read from UART RX FIFO in one burst. */

#define CDC_UART_WRITE                                          (1)
#define CDC_UART_READ                                           (2)
#define CDC_UART_SCB1_WRITE_DMA_CHANNEL                         (18)
#define CDC_UART_SCB1_READ_DMA_CHANNEL                          (19)
#define CDC_UART_SCB0_WRITE_DMA_CHANNEL                         (16)
#define CDC_UART_SCB0_READ_DMA_CHANNEL                          (17)

#define SCB4_DMAC                                               (4)
#define SCB4_TX_DMAC_CHANNEL                                    (2)
#define SCB4_RX_DMAC_CHANNEL                                    (3)

#define SCB6_DMAC                                               (6)
#define SCB6_TX_DMAC_CHANNEL                                    (4)
#define SCB6_RX_DMAC_CHANNEL                                    (5)

#define CHECK_CDC_FN_CONFIG(cfgTablePtr,function)               (((cy_stc_cdc_fn_config_t*)cfgTablePtr->fnConfig[function]->fnConfig))
#define CDC_DATA_SOURCE_SINK(cfgTablePtr,function)              (((cy_stc_cdc_fn_config_t*)cfgTablePtr->fnConfig[function]->fnConfig)->cdcDataSourceSink)
#define CDC_DMA_BUFFER_SIZE(cfgTablePtr,function)               (((cy_stc_cdc_fn_config_t*)cfgTablePtr->fnConfig[function]->fnConfig)->dmaBufferSize)
#define CDC_DMA_BUFFER_COUNT(cfgTablePtr,function)              (((cy_stc_cdc_fn_config_t*)cfgTablePtr->fnConfig[function]->fnConfig)->dmaBufferCount)
#define CDC_DATA_RECEIVE_ENABLED(cfgTablePtr,function)          (((cy_stc_cdc_fn_config_t*)cfgTablePtr->fnConfig[function]->fnConfig)->dataReceiveEnabled)

/*****************************************************************************
*                               Variables                                    *
******************************************************************************/
extern uint8_t glCdcFnScbMap[MAX_CDC_FUNCTIONS];
extern uint8_t glCdcUartRdChNum[MAX_CDC_FUNCTIONS];
extern uint8_t glCdcUartWrChNum[MAX_CDC_FUNCTIONS];
extern  cy_israddress glCdcInIsrList[MAX_CDC_FUNCTIONS];
extern  cy_israddress glCdcOutIsrList[MAX_CDC_FUNCTIONS];
extern CySCB_Type* scbMapping[8];

/*****************************************************************************
*                            Functions                                       *
******************************************************************************/

/**
 * \name Cy_CDC_TaskCreate
 * \brief Function to create CDC Task
 * \param pAppCtxt User Context
 * \retval None
 */
void Cy_CDC_TaskCreate(cy_stc_usb_app_ctxt_t *pAppCtxt);

/**
 * \name Cy_CDC_DeviceTaskHandler
 * \brief CDC Task Handler function
 * \param pTaskParam Task parameter
 * \retval None
 */
void Cy_CDC_DeviceTaskHandler (void *pTaskParam);

/**
 * \name Cy_CDC_Fn0TimerCallback
 * \brief Timer callback for CDC function 0
 * \param xTimer Timer Handle
 * \retval None
 */
void Cy_CDC_Fn0TimerCallback (TimerHandle_t xTimer);

/**
 * \name Cy_CDC_Fn1TimerCallback
 * \brief Timer callback for CDC function 1
 * \param xTimer Timer Handle
 * \retval None
 */
void Cy_CDC_Fn1TimerCallback (TimerHandle_t xTimer);

/**
 * \name Cy_CDC_Fn2TimerCallback
 * \brief Timer callback for CDC function 2
 * \param xTimer Timer Handle
 * \retval None
 */
void Cy_CDC_Fn2TimerCallback (TimerHandle_t xTimer);

/**
 * \name Cy_CDC_Fn3TimerCallback
 * \brief Timer callback for CDC function 3
 * \param xTimer Timer Handle
 * \retval None
 */
void Cy_CDC_Fn3TimerCallback (TimerHandle_t xTimer);

/**
 * \name Cy_CDC_UartInitDmaIntr
 * \brief Function to register an ISR for the DMA channel associated with a UART Read/Write
 * \param uart_read_write UART READ/WRITE
 * \param scbIndex UART SCB index
 * \retval None
 */
void Cy_CDC_UartInitDmaIntr(uint8_t uart_read_write, uint8_t scbIndex);

/**
 * \name Cy_UART_SCB0Read_ISR
 * \brief UART SCB 0 read ISR
 * \retval None
 */
void Cy_UART_SCB0Read_ISR(void);

/**
 * \name Cy_UART_SCB1Read_ISR
 * \brief UART SCB 1 read ISR
 * \retval None
 */
void Cy_UART_SCB1Read_ISR(void);

/**
 * \name Cy_UART_SCB4Read_ISR
 * \brief UART SCB 4 read ISR
 * \retval None
 */
void Cy_UART_SCB4Read_ISR(void);

/**
 * \name Cy_UART_SCB6Read_ISR
 * \brief UART SCB 6 read ISR
 * \retval None
 */
void Cy_UART_SCB6Read_ISR(void);

/**
 * \name Cy_UART_SCB0Write_ISR
 * \brief UART SCB#0 write ISR
 * \retval None
 */
void Cy_UART_SCB0Write_ISR(void);

/**
 * \name Cy_UART_SCB1Write_ISR
 * \brief UART SCB#1 write ISR
 * \retval None
 */
void Cy_UART_SCB1Write_ISR(void);

/**
 * \name Cy_UART_SCB4Write_ISR
 * \brief UART SCB 4 write ISR
 * \retval None
 */
void Cy_UART_SCB4Write_ISR(void);

/**
 * \name Cy_UART_SCB6Write_ISR
 * \brief UART SCB 6 write ISR
 * \retval None
 */
void Cy_UART_SCB6Write_ISR(void);

#if !CY_CPU_CORTEX_M4
/**
 * \name Cy_CDC_UartDmaWrite_Combined_ISR
 * \brief Combined ISR for UART DMA write channels.
 * \details Routes to the appropriate UART write ISR based on which channel triggered the interrupt.
 * \retval None
 */
void Cy_CDC_UartDmaWrite_Combined_ISR(void);

/**
 * \name Cy_CDC_UartDmaRead_Combined_ISR
 * \brief Combined ISR for UART DMA read channels.
 * \details Routes to the appropriate UART read ISR based on which channel triggered the interrupt.
 * \retval None
 */
void Cy_CDC_UartDmaRead_Combined_ISR(void);
#endif

/**
 * \name Cy_CDC_Ch0OutEpDma_ISR
 * \brief CDC Channel#0 EP Out ISR
 * \retval None
 */
void Cy_CDC_Ch0OutEpDma_ISR (void);

/**
 * \name Cy_CDC_Ch1OutEpDma_ISR
 * \brief CDC Channel#1 EP OUT ISR
 * \retval None
 */
void Cy_CDC_Ch1OutEpDma_ISR (void);

/**
 * \name Cy_CDC_Ch2OutEpDma_ISR
 * \brief CDC Channel#2 EP OUT ISR
 * \retval None
 */
void Cy_CDC_Ch2OutEpDma_ISR (void);

/**
 * \name Cy_CDC_Ch3OutEpDma_ISR
 * \brief CDC Channel#3 EP OUT ISR
 * \retval None
 */
void Cy_CDC_Ch3OutEpDma_ISR (void);

/**
 * \name Cy_CDC_Ch0InEpDma_ISR
 * \brief CDC Channel#0 EP IN ISR
 * \retval None
 */
void Cy_CDC_Ch0InEpDma_ISR (void);

/**
 * \name Cy_CDC_Ch1InEpDma_ISR
 * \brief CDC Channel#1 EP IN ISR
 * \retval None
 */
void Cy_CDC_Ch1InEpDma_ISR (void);

/**
 * \name Cy_CDC_Ch2InEpDma_ISR
 * \brief CDC Channel#2 EP IN ISR
 * \retval None
 */
void Cy_CDC_Ch2InEpDma_ISR (void);

/**
 * \name Cy_CDC_Ch3InEpDma_ISR
 * \brief CDC Channel#3 EP IN ISR
 * \retval None
 */
void Cy_CDC_Ch3InEpDma_ISR (void);

/**
 * \name Cy_CDC_UartDmaWriteCompletion
 * \brief Handler for DMA transfer completion on UART TX_FIFO
 * \param pApp User context
 * \param scbIndex SCB index
 * \retval None
 */
void Cy_CDC_UartDmaWriteCompletion(void *pApp, uint8_t scbIndex);

/**
 * \name Cy_CDC_UartDmaReadCompletion
 * \brief Handler for DMA transfer completion on UART RX_FIFO
 * \param pApp User context
 * \param scbIndex SCB index
 * \retval None
 */
void Cy_CDC_UartDmaReadCompletion(void *pApp,uint8_t scbIndex);

/**
 * \name Cy_CDC_USBDmaReadCompletion
 * \brief Handler for DMA transfer completion on OUT endpoint
 * \param pApp User context
 * \param cdcIndex CDC index
 * \param data
 * \retval None
 */
void Cy_CDC_USBDmaReadCompletion(void *pApp, uint8_t cdcIndex, uint16_t dataLength);

/**
 * \name Cy_CDC_USBDmaWriteCompletion
 * \brief Handler for DMA transfer completion on IN endpoint
 * \param pApp User context
 * \param cdcIndex CDC index
 * \retval None
 */
void Cy_CDC_USBDmaWriteCompletion(void *pApp, uint8_t cdcIndex);

/**
 * \name Cy_CDC_InitializeTimer
 * \brief Function to initialize VCOM Timers for UART RX
 * \param pAppCtxt User Context
 * \param pAppCtxt CDC index
 * \retval None
 */
void Cy_CDC_InitializeTimer(cy_stc_usb_app_ctxt_t *pAppCtxt, uint8_t cdcIndex);

/**
 * \name Cy_CDC_QueueUartRead
 * \details Function which identifies the DMA buffer address where
 * the next data from UART RX_FIFO should be read and queues DataWire
 * transfer to read a specific amount of data into it.
 * \param pAppCtxt User context
 * \param dataLength Size of data to read
 * \param scbIndex UART SCB index
 * \retval None
 */
void Cy_CDC_QueueUartRead (cy_stc_usb_app_ctxt_t *pAppCtxt,
                                     uint32_t dataLength, uint8_t scbIndex, uint32_t pY_index, bool isY_indexStuck);

/**
 * \name Cy_CDC_UartDwWrite
 * \brief Function to write operation on an UART TX_FIFO using DataWire.
 * \param pBuffer Data buffer pointer
 * \param dataSize Size of data to write
 * \param scbIndex UART SCB index
 * \retval None
 */
void Cy_CDC_UartDwWrite (uint8_t *pBuffer, uint16_t dataSize, uint8_t scbIndex);

/**
 * \name Cy_CDC_UartDmacWrite
 * \brief Function to write operation on an UART TX_FIFO using DMAC.
 * \param pBuffer Data buffer pointer
 * \param dataSize Size of data to write
 * \param scbIndex UART SCB index
 * \retval None
 */
void Cy_CDC_UartDmacWrite (uint8_t *pBuffer, uint16_t dataSize, uint8_t scbIndex);

/**
 * \name Cy_CDC_SendUartDataToUsb
 * \brief Function to send the data received from UART to the USB endpoint using High BandWidth DMA channel.
 * \param pAppCtxt User context
 * \param dataBuf_p Buffer pointer
 * \param dataLength Size of data to send
 * \param cdcIndex CDC index
 * \retval None
 */
void Cy_CDC_SendUartDataToUsb (cy_stc_usb_app_ctxt_t *pAppCtxt,
                           uint8_t *dataBuf_p, uint32_t dataLength, uint8_t cdcIndex);

/**
 * \name Cy_CDC_ClearUartDmaInterrupt
 * \brief Function to clear the pending DMA interrupt associated with an UART.
 * \param pAppCtxt User context
 * \param uart_read_write UART Read/Write Channel
 * \param scbIndex SCB index
 * \retval None
 */
void Cy_CDC_ClearUartDmaInterrupt(cy_stc_usb_app_ctxt_t *pAppCtxt,uint8_t uart_read_write, uint8_t scbIndex);

/**
 * \name Cy_CDC_UartDwRead
 * \brief This function activate DataWire to read from UART RX_FIFO and put dataSize length of data into SRAM buffer.
 * \note    Refer to \ref Cy_CDC_UartDmacRead "Cy_CDC_UartDmacRead" for
 *          explanation of the read queuing conditions used
 * \param pAppCtxt Application context pointer
 * \param pHbSramBuffer Buffer pointer
 * \param dataSize Data size to read
 * \param scbIndex SCB index
 * \retval None
 */
void Cy_CDC_UartDwRead (cy_stc_usb_app_ctxt_t *pAppCtxt, uint8_t *pHbSramBuffer,
        uint16_t dataSize, uint8_t scbIndex, uint32_t pY_index, bool isY_indexStuck);

/**
 * \name Cy_CDC_UartDmacRead
 * \brief This function activate DMAC to read from UART RX_FIFO and put dataSize length of data into SRAM buffer.
 * \param pAppCtxt Application context pointer
 * \param pHbSramBuffer Buffer pointer
 * \param dataSize Data size to read
 * \param scbIndex SCB index
 * \retval None
 */
void Cy_CDC_UartDmacRead (cy_stc_usb_app_ctxt_t *pAppCtxt, uint8_t *pHbSramBuffer,
        uint16_t dataSize, uint8_t scbIndex, uint32_t pY_index, bool isY_indexStuck);

/**
 * \name Cy_USB_AppQueueRead
 * \brief Function to queue read operation on an OUT endpoint.
 * \param pAppCtxt User context
 * \param endpNumber Endpoint number
 * \param pBuffer Buffer pointer
 * \param dataSize Data size to read
 * \retval None
 */
void
Cy_USB_AppQueueRead (cy_stc_usb_app_ctxt_t *pAppCtxt, uint8_t endpNumber,
                    uint8_t *pBuffer, uint16_t dataSize);

/**
 * \name Cy_CDC_HbwDmaCallback
 * \brief HBW DMA callback whenever HBW-DMA transfer completes.
 * \param pHandle Channel handle
 * \param type Channel type
 * \param pbufStat Buffer status
 * \param userCtx User context
 * \retval None
 */
void
Cy_CDC_HbwDmaCallback (struct cy_stc_hbdma_channel *pHandle,
                         cy_en_hbdma_cb_type_t type,
                         cy_stc_hbdma_buff_status_t* pbufStat, void *userCtx);

/**
 * \name Cy_CDC_SetLineCodingHandler
 * \brief CDC Set Line coding request handler
 * \param pAppCtxt User context
 * \param wLength wLength field of the control request
 * \param function function Index
 * \retval None
 */
void Cy_CDC_SetLineCodingHandler(cy_stc_usb_app_ctxt_t *pAppCtxt,uint16_t wLength, uint8_t function);

/**
 * \name Cy_USB_HandleCDCRequests
 * \brief CDC Requests Handler
 * \param pAppCtxt User context
 * \param bRequest bRequest field of the control request
 * \param wLength wLength field of the control request
 * \param wValue wValue field of the control request
 * \retval True if request is handled, else false.
 */

bool Cy_USB_HandleCDCRequests(cy_stc_usb_app_ctxt_t *pAppCtxt,uint8_t bRequest,
                              uint16_t wLength, uint16_t wValue);

/**
 * \name Cy_CDC_ConfigureUartSCBClock
 * \brief Configure the clock applied to SCB.
 * \param scbIndex SCB Index
 * \param divider Clock divider
 * \retval None
 */
void Cy_CDC_ConfigureUartSCBClock(uint8_t scbIndex, uint16_t divider);

/**
 * \name Cy_CDC_UartSCBInit
 * \brief Initialize SCB1 as UART.
 * \param scbIndex SCB Index
 * \retval None
 */
void Cy_CDC_UartSCBInit(uint8_t scbIndex);

#endif /* INCLUDED_CDC_H */
