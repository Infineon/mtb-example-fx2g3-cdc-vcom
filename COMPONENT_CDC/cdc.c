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

#include "cy_dscr.h"
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
#include "vcom_config.h"
#include "cdc.h"

/* CDC function information */
cy_stc_scb_uart_context_t glCdcUartCtxt[MAX_CDC_FUNCTIONS];             /* SCB-UART context structure */
cy_stc_scb_uart_config_t glCdcUartConfig[MAX_CDC_FUNCTIONS];            /* SCB-UART config structure */
cy_stc_cdc_info_t glCDCInfo[MAX_CDC_FUNCTIONS];                         /* CDC function info structure */

/* Data transfer progress trackers */
volatile uint32_t Y_index[4] = {0,0,0,0};   /* Global tracker for Y-index in 2D DMA transfers */
uint32_t glCdcTimerLoop[MAX_CDC_FUNCTIONS]; /* Flag to indicate that an active DMA transfer triggered by timer expiry is in progress */

/** UART Rx FIFO data lengths assist with timer-based UART read of stale data. See \ref Cy_CDC_Fn_TimerCallback */
static uint32_t glCdcUartRxFifoDataLenCur[MAX_CDC_FUNCTIONS] = {0x00,0x00,0x00,0x00};
static uint32_t glCdcUartRxFifoDataLenPrev[MAX_CDC_FUNCTIONS] = {0x00,0x00,0x00,0x00};

/*
 * Configuration data for the CDC Virtual COM Port is stored here.
 * This data is updated from the host through the CDC line-coding requests and returned on
 * CDC_GET_LINE_CODING. This application uses it for baud-rate configuration, while
 * UART framing remains fixed at 8N1.
 */
uint8_t USB3_DESC_ATTRIBUTES glCdcVcomCfgData[7U] = {0x80,0x25,0x00,0x00,0x02,0x00,0x08};

/* DMA read and write descriptors for DataWire-based CDC functions */
static cy_stc_dma_descriptor_t  glCdcUartWrDmaDesc1[MAX_CDC_FUNCTIONS],
                                glCdcUartWrDmaDesc2[MAX_CDC_FUNCTIONS]; /* DMA write descriptors for DataWire based UART writes (desc chain) */
static cy_stc_dma_descriptor_t  glCdcUartRdDmaDesc[MAX_CDC_FUNCTIONS]; /* DMA read descriptor for DataWire based UART reads */

/* DMA read and write descriptors for DMAC-based CDC functions */
static cy_stc_dmac_descriptor_t glCdcUartRdDmacDesc1, glCdcUartRdDmacDesc2; /* DMAC read descriptors, for the 2 DMAC-based cdc functions */
static cy_stc_dmac_descriptor_t glCdcUartWrDmacDesc1, glCdcUartWrDmacDesc2; /* DMAC write descriptors for 1 of 2 DMAC-based cdc functions */
static cy_stc_dmac_descriptor_t glCdcUartWrDmacDesc3, glCdcUartWrDmacDesc4; /* DMAC write descriptors for 2 of 2 DMAC-based cdc functions */

/* Mapping SCBs to indices */
CySCB_Type* scbMapping[8] =
{
    NULL,   /** SCB unmapped at index 0, to use CPU as source/sink instead of an SCB */
    SCB0,
    SCB1,
    NULL,   /* SCB2 is unavailable on device */
    NULL,   /* SCB3 is unavailable on device */
    SCB4,
    SCB5,
    SCB6
};

/* UART DataWire read channel */
uint8_t glCdcUartRdChNum[MAX_CDC_FUNCTIONS] =
{
    CDC_UART_SCB0_READ_DMA_CHANNEL,
    CDC_UART_SCB1_READ_DMA_CHANNEL
};

/* UART DataWire write channels */
uint8_t glCdcUartWrChNum[MAX_CDC_FUNCTIONS] =
{
    CDC_UART_SCB0_WRITE_DMA_CHANNEL,
    CDC_UART_SCB1_WRITE_DMA_CHANNEL
};

/* Uart Read Channel ISR List */
cy_israddress uartRdChannelIsr[] = 
{
    Cy_UART_SCB0Read_ISR,
    Cy_UART_SCB1Read_ISR,
    NULL,
    NULL,
    Cy_UART_SCB4Read_ISR,
    NULL,
    Cy_UART_SCB6Read_ISR
};

/* Uart Write Channel ISR List */
cy_israddress uartWrChannelIsr[] = 
{
    Cy_UART_SCB0Write_ISR,
    Cy_UART_SCB1Write_ISR,
    NULL,
    NULL,
    Cy_UART_SCB4Write_ISR,
    NULL,
    Cy_UART_SCB6Write_ISR
};

/* CDC function to SCB map */
uint8_t glCdcFnScbMap[MAX_CDC_FUNCTIONS] = 
{
    CY_APP_ERROR_CODE,
    CY_APP_ERROR_CODE,
    CY_APP_ERROR_CODE,
    CY_APP_ERROR_CODE
};

/* CDC IN ISR List */
cy_israddress glCdcInIsrList[MAX_CDC_FUNCTIONS] =
{
    Cy_CDC_Ch0InEpDma_ISR,                          
    Cy_CDC_Ch1InEpDma_ISR,
    Cy_CDC_Ch2InEpDma_ISR,
    Cy_CDC_Ch3InEpDma_ISR
};

/* CDC OUT ISR List */
cy_israddress glCdcOutIsrList[MAX_CDC_FUNCTIONS] =
{
    Cy_CDC_Ch0OutEpDma_ISR,                          
    Cy_CDC_Ch1OutEpDma_ISR,
    Cy_CDC_Ch2OutEpDma_ISR,
    Cy_CDC_Ch3OutEpDma_ISR
};

/* SCB IRQ Map */
uint8_t glCdcUartScbIrq[] =
{
    scb_0_interrupt_IRQn,
    scb_1_interrupt_IRQn,
    CY_APP_ERROR_CODE,
    CY_APP_ERROR_CODE,
    scb_4_interrupt_IRQn,
    CY_APP_ERROR_CODE,
    scb_6_interrupt_IRQn
};

/**
 * \name Cy_CDC_GetOverSampleFromBaudRate
 * \brief Get oversample from selected Baud rate
 * \param baudRate The baud rate
 * \retval uint32_t The oversample value
 */
uint32_t Cy_CDC_GetOverSampleFromBaudRate(uint32_t baudRate)
{
    uint32_t sample = 0;
    switch(baudRate)
    {
        case 9600:
            sample = 15;    /*150000/9600*/
            break;
        case 14400:
            sample = 10;    /*150000/14400*/
            break;
        case 19200:     
            sample = 16;   /*312500/19200*/
            break;
        case 38400:
            sample = 12;   /*468750/38400*/
            break;
        case 57600:     
            sample = 8;    /*468750/57600*/
            break;
        case 115200:
            sample = 16;   /*1875000/115200*/
            break;
        case 230400:
            sample = 8;    /*1875000/230400*/
            break;
        case 460800:
            sample = 16;   /* 7500000/460800*/
            break;
        case 921600:
            sample = 8;    /* 7500000/921600*/
            break;
        case 3686400:
            sample = 9;
            break;
        default:
            sample = 8;
            break;
    }
    return sample;
}

/**
 * \name Cy_CDC_GetDividerFromBaudRate
 * \brief Gets the divider value for the selected baud rate
 * \param baudRate The desired baud rate
 * \retval The divider value corresponding to the baud rate
 */
uint16_t Cy_CDC_GetDividerFromBaudRate(uint32_t baudRate)
{
    uint16_t divider = 0;
    switch(baudRate)
    {
        case 9600:
        case 14400:
            divider = 500;
            break;
        case 19200:
            divider = 240;
            break;
        case 38400: 
        case 57600:
            divider = 160;
            break;
        case 115200:
        case 230400:
            divider = 40;
            break;
        case 460800:
        case 921600:
            divider = 9;
            break;
        case 3686400:
            divider = 2;
            break;
        default:
            divider = 9;
            break;
    }
    return divider;
}

/**
 * \name Cy_CDC_GetIndexFromFunctionIndex
 * \brief Function to get CDC Index from Function Number
 * \param pAppCtxt User Context
 * \param function Function number
 * \retval CDC Index Number.
 */
uint8_t Cy_CDC_GetIndexFromFunctionIndex(cy_stc_usb_app_ctxt_t *pAppCtxt,uint8_t function)
{
    uint8_t i = CY_APP_ERROR_CODE;
    for(i = 0; i < pAppCtxt->numCDCFunctions; i++ )
    {
        if(function == pAppCtxt->cdcInfo[i]->cdcFuncIndex)
        {
            DBG_APP_TRACE("Cy_CDC_GetIndexFromFunctionIndex %d %x \r\n",i,pAppCtxt->cdcInfo[i]->cdcFuncIndex);
            return i;
        }
    }

    return CY_APP_ERROR_CODE;
}

/**
 * \name Cy_CDC_TaskCreate
 * \brief Function to create CDC Task
 * \param pAppCtxt User Context
 * \retval None
 */
void Cy_CDC_TaskCreate(cy_stc_usb_app_ctxt_t *pAppCtxt)
{
    BaseType_t status;

    /* create queue and register it to kernel. */
    pAppCtxt->vcomDeviceQueue = xQueueCreate(CDC_VCOM_DEVICE_MSG_QUEUE_SIZE,
                                             CDC_VCOM_DEVICE_MSG_SIZE);

    if(NULL == pAppCtxt->vcomDeviceQueue)
    {
        DBG_APP_ERR("CDC Queue failed\r\n");
        return;
    }

    vQueueAddToRegistry(pAppCtxt->vcomDeviceQueue, "VCOMDeviceMsgQueue");

    /* Create task and check status to confirm task created properly. */
    status = xTaskCreate(Cy_CDC_DeviceTaskHandler, "VCOMDeviceTask", 1024,
                         (void *)pAppCtxt, 6, &(pAppCtxt->vcomDevicetaskHandle));

    ASSERT_NON_BLOCK(pdPASS == status, status);
                                        
}

/**
 * \name Cy_CDC_InitializeTimer
 * \brief Function to initialize VCOM Timers for UART RX
 * \param pAppCtxt User Context
 * \param pAppCtxt CDC index
 * \retval None
 */
void Cy_CDC_InitializeTimer(cy_stc_usb_app_ctxt_t *pAppCtxt, uint8_t cdcIndex)
{
    /* Default SCB-UART configuration. */
    cy_stc_scb_uart_config_t uartCfg = {
        .uartMode                   = CY_SCB_UART_STANDARD,
        .oversample                 = 8,
        .dataWidth                  = 8,
        .enableMsbFirst             = false,
        .stopBits                   = CY_SCB_UART_STOP_BITS_1,
        .parity                     = CY_SCB_UART_PARITY_NONE,
        .enableInputFilter          = false,
        .dropOnParityError          = false,
        .dropOnFrameError           = false,
        .enableMutliProcessorMode   = false,
        .receiverAddress            = 0,
        .receiverAddressMask        = 0,
        .acceptAddrInFifo           = false,
        .irdaInvertRx               = false,
        .irdaEnableLowPowerReceiver = false,
        .smartCardRetryOnNack       = false,
        .enableCts                  = false,
        .ctsPolarity                = CY_SCB_UART_ACTIVE_LOW,
        .rtsRxFifoLevel             = 0,
        .rtsPolarity                = CY_SCB_UART_ACTIVE_LOW,
        .breakWidth                 = 8,
        .rxFifoTriggerLevel         = CDC_VCOM_UART_MAX_RD_SIZE - 1,
        .rxFifoIntEnableMask        = 0,
        .txFifoTriggerLevel         = CDC_VCOM_UART_MAX_RD_SIZE,
        .txFifoIntEnableMask        = 0
    };


    if(cdcIndex != CY_APP_ERROR_CODE) /* Create a timer for a CDC function with a valid given index */
    {
        pAppCtxt->pUartCtxt[cdcIndex] = &glCdcUartCtxt[cdcIndex];
        memcpy((uint8_t *)&glCdcUartConfig[cdcIndex], (uint8_t *)&uartCfg, sizeof(uartCfg));
        if(cdcIndex<4) pAppCtxt->vcomtimerExpiry[cdcIndex] = CDC_VCOM_RESPONSE_TIME;
        switch(cdcIndex){
            case 0:
                pAppCtxt->vcomTimerHandle[cdcIndex] = xTimerCreate("VcomTimerFn0", pAppCtxt->vcomtimerExpiry[0],
                                            pdFALSE, (void *)pAppCtxt, Cy_CDC_Fn0TimerCallback);
                break;
            case 1:
                pAppCtxt->vcomTimerHandle[cdcIndex] = xTimerCreate("VcomTimerFn1", pAppCtxt->vcomtimerExpiry[1],
                                            pdFALSE, (void *)pAppCtxt, Cy_CDC_Fn1TimerCallback);
                break;
            case 2:
                pAppCtxt->vcomTimerHandle[cdcIndex] = xTimerCreate("VcomTimerFn2", pAppCtxt->vcomtimerExpiry[2],
                                            pdFALSE, (void *)pAppCtxt, Cy_CDC_Fn2TimerCallback);
                break;
            case 3:
                pAppCtxt->vcomTimerHandle[cdcIndex] = xTimerCreate("VcomTimerFn3", pAppCtxt->vcomtimerExpiry[3],
                                            pdFALSE, (void *)pAppCtxt, Cy_CDC_Fn3TimerCallback);
                break;
            default:
                DBG_APP_ERR("Cy_CDC_InitializeTimer: Invalid cdcIndex: %d", cdcIndex);
                break;
        }
    }
}

static volatile uint32_t cdcRxPktCount[4] = {0};
static volatile uint32_t cdcTxPktCount[4] = {0};
static volatile uint32_t cdcIsrQueueSendFailCount[4] = {0};

static void Cy_CDC_PostVcomMsgFromISR(cy_stc_usb_app_ctxt_t *pAppCtxt,
                                      cy_stc_usbd_app_msg_t *pMsg,
                                      uint8_t cdcIndex,
                                      const char *context)
{
    BaseType_t status;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    status = xQueueSendFromISR(pAppCtxt->vcomDeviceQueue, pMsg,
                               &xHigherPriorityTaskWoken);
    if (status != pdPASS) {
        if (cdcIndex < MAX_CDC_FUNCTIONS) {
            cdcIsrQueueSendFailCount[cdcIndex]++;
            if ((cdcIsrQueueSendFailCount[cdcIndex] & 0x3FU) == 1U) {
                DBG_APP_ERR("%s: vcom queue full cdc=%d fail=%lu\r\n",
                        context,
                        cdcIndex,
                        (unsigned long)cdcIsrQueueSendFailCount[cdcIndex]);
            }
        } else {
            DBG_APP_ERR("%s: vcom queue full invalid cdc=%d\r\n",
                    context,
                    cdcIndex);
        }
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/**
 * \name Cy_CDC_DeviceTaskHandler
 * \brief CDC Task Handler function
 * \param pTaskParam Task parameter
 * \retval None
 */
void Cy_CDC_DeviceTaskHandler (void *pTaskParam)
{
    cy_stc_usbd_app_msg_t queueMsg;
    BaseType_t xStatus;
    uint8_t cdcIndex = 0;
    cy_stc_usb_app_ctxt_t *pAppCtxt = (cy_stc_usb_app_ctxt_t *)pTaskParam;
    uint32_t intMask;

    DBG_APP_INFO("CDC Device Thread Created\r\n");

    do {
        /*
         * Wait until some data is received from the queue.
         * Timeout after 1000 ms.
         */
        xStatus = xQueueReceive(pAppCtxt->vcomDeviceQueue, &queueMsg, 1000);
        if (xStatus != pdPASS) {
            continue;
        }

        cdcIndex = (uint8_t)queueMsg.data[0];
        if(cdcIndex == CY_APP_ERROR_CODE)
        {
            DBG_APP_ERR("Cy_CDC_DeviceTaskHandler: Invalid CDC Configuration. Check the DMA channel setup for CDC\r\n");
            continue;
        }

        switch (queueMsg.type) {
        case CY_USB_VCOM_DEVICE_MSG_START_DATA_XFER:
            DBG_APP_TRACE("CY_USB_VCOM_DEVICE_MSG_START_DATA_XFER: cdc index %d \r\n",cdcIndex);
            
            /* Queue 1 bulk packet size worth of data tp be read from UART FIFO */
            if(CDC_DATA_SOURCE_SINK(pAppCtxt->cfgTablePtr,pAppCtxt->cdcInfo[cdcIndex]->cdcFuncIndex) != CY_CDC_CPU)
            {
                /* Enable DataWire channel to read CDC_VCOM_UART_MAX_RD_SIZE bytes from the UART RX_FIFO. */
                DBG_APP_TRACE("\r\n[Q] %s: len %d SCB %d\r\n", "START_DATA_XFER", USB_HS_BULK_XFER_SIZE, glCdcFnScbMap[cdcIndex]);
                Cy_CDC_QueueUartRead(pAppCtxt, USB_HS_BULK_XFER_SIZE, glCdcFnScbMap[cdcIndex], 0, false);
            }
            break;

        case CY_USB_VCOM_DEVICE_MSG_READ_COMPLETE:
            DBG_APP_TRACE("CY_USB_VCOM_DEVICE_MSG_READ_COMPLETE: cdc index: %d \r\n",cdcIndex);

            if(CDC_DATA_SOURCE_SINK(pAppCtxt->cfgTablePtr,pAppCtxt->cdcInfo[cdcIndex]->cdcFuncIndex) != CY_CDC_CPU)
            {
                intMask = Cy_SysLib_EnterCriticalSection();

                cdcTxPktCount[cdcIndex]++;
                /* Increment the number of occupied data buffers and store the size of the data received. */
                pAppCtxt->cdcInfo[cdcIndex]->usbRxDataLen[pAppCtxt->cdcInfo[cdcIndex]->nxtUsbRxBufIdx] = queueMsg.data[1]; //pAppCtxt->cdcInfo[cdcIndex]->usbOutPktLen;
                pAppCtxt->cdcInfo[cdcIndex]->nxtUsbRxBufIdx++;
                if (pAppCtxt->cdcInfo[cdcIndex]->nxtUsbRxBufIdx >= pAppCtxt->cdcInfo[cdcIndex]->dmaTxBufferCount) {
                    pAppCtxt->cdcInfo[cdcIndex]->nxtUsbRxBufIdx = 0;
                }
                pAppCtxt->cdcInfo[cdcIndex]->usbRxBusyBufCount++;

                /* If this is the first occupied buffer, we can queue UART write. */
                if (pAppCtxt->cdcInfo[cdcIndex]->usbRxBusyBufCount == 1) {
                    Cy_SysLib_ExitCriticalSection(intMask);

                        if(glCdcFnScbMap[cdcIndex] != 4 && glCdcFnScbMap[cdcIndex] != 6)
                        {
                            /* Configure the DMA channel to write the received data to the SCB transmit FIFO. */
                            Cy_CDC_UartDwWrite(pAppCtxt->cdcInfo[cdcIndex]->pUsbRxBuffer[pAppCtxt->cdcInfo[cdcIndex]->uartTxBufIndex],
                            pAppCtxt->cdcInfo[cdcIndex]->usbRxDataLen[pAppCtxt->cdcInfo[cdcIndex]->uartTxBufIndex],glCdcFnScbMap[cdcIndex]);
                        }
                        else
                        {
                            /* Configure the DMA channel to write the received data to the SCB transmit FIFO. */
                            Cy_CDC_UartDmacWrite(pAppCtxt->cdcInfo[cdcIndex]->pUsbRxBuffer[pAppCtxt->cdcInfo[cdcIndex]->uartTxBufIndex],
                            pAppCtxt->cdcInfo[cdcIndex]->usbRxDataLen[pAppCtxt->cdcInfo[cdcIndex]->uartTxBufIndex],glCdcFnScbMap[cdcIndex]);
                        }
                } else {
                    Cy_SysLib_ExitCriticalSection(intMask);
                }
            }
            break;

        case CY_USB_VCOM_DEVICE_MSG_SLP_OUT:
            DBG_APP_TRACE(" CY_USB_VCOM_DEVICE_MSG_SLP_OUT cdcIndex %d (handled by HBDMA)\n\r",cdcIndex);
            break;

        case CY_USB_VCOM_DEVICE_MSG_WRITE_COMPLETE:
            DBG_APP_TRACE("CY_USB_VCOM_DEVICE_MSG_WRITE_COMPLETE index:%d \r\n",cdcIndex);

            if (pAppCtxt->cdcInfo[cdcIndex]->uartRxFreeBufCount == 0) {
                Cy_CDC_QueueUartRead(pAppCtxt, 512, glCdcFnScbMap[cdcIndex], 0, false);
            }
            pAppCtxt->cdcInfo[cdcIndex]->uartRxFreeBufCount++;

            /* If we have at least one occupied buffer, queue the next write operation. */
            if (pAppCtxt->cdcInfo[cdcIndex]->uartRxFreeBufCount < pAppCtxt->cdcInfo[cdcIndex]->dmaRxBufferCount) {
                Cy_CDC_SendUartDataToUsb(pAppCtxt,
                                        pAppCtxt->cdcInfo[cdcIndex]->pUartRxBuffer[pAppCtxt->cdcInfo[cdcIndex]->nxtUsbTxBufIdx],
                                        pAppCtxt->cdcInfo[cdcIndex]->uartRxDataLen[pAppCtxt->cdcInfo[cdcIndex]->nxtUsbTxBufIdx], cdcIndex);
            } else {
                if (pAppCtxt->cdcInfo[cdcIndex]->usbZlpReqd) {
                    Cy_USBD_SendEgressZLP(pAppCtxt->pUsbdCtxt, pAppCtxt->cdcInfo[cdcIndex]->vcomInEpNum);
                }
            }
            pAppCtxt->cdcInfo[cdcIndex]->usbZlpReqd = false;
            break;

        case CY_USB_UART_MSG_WRITE_COMPLETE:
            DBG_APP_TRACE("MEM_TO_CDC_UART_WRITE_COMPLETE index: %d\r\n",cdcIndex);
            if(CDC_DATA_SOURCE_SINK(pAppCtxt->cfgTablePtr,pAppCtxt->cdcInfo[cdcIndex]->cdcFuncIndex) != CY_CDC_CPU)
            {
                intMask = Cy_SysLib_EnterCriticalSection();

                /* Move to the next TX buffer for the next transfer */
                pAppCtxt->cdcInfo[cdcIndex]->uartTxBufIndex++;
                if (pAppCtxt->cdcInfo[cdcIndex]->uartTxBufIndex >= pAppCtxt->cdcInfo[cdcIndex]->dmaTxBufferCount) {
                    pAppCtxt->cdcInfo[cdcIndex]->uartTxBufIndex = 0;
                }

                /* Decrement the number of used DMA buffers on the USB RX side */
                pAppCtxt->cdcInfo[cdcIndex]->usbRxBusyBufCount--;

                /* If we still have a used buffer, send it out via UART */
                if (pAppCtxt->cdcInfo[cdcIndex]->usbRxBusyBufCount > 0) {
                    Cy_SysLib_ExitCriticalSection(intMask);

                    if(glCdcFnScbMap[cdcIndex] != 4 && glCdcFnScbMap[cdcIndex] != 6)
                    {
                        Cy_CDC_UartDwWrite(pAppCtxt->cdcInfo[cdcIndex]->pUsbRxBuffer[pAppCtxt->cdcInfo[cdcIndex]->uartTxBufIndex],
                                            pAppCtxt->cdcInfo[cdcIndex]->usbRxDataLen[pAppCtxt->cdcInfo[cdcIndex]->uartTxBufIndex],
                                            glCdcFnScbMap[cdcIndex]);
                    }
                    else
                    {
                        Cy_CDC_UartDmacWrite(pAppCtxt->cdcInfo[cdcIndex]->pUsbRxBuffer[pAppCtxt->cdcInfo[cdcIndex]->uartTxBufIndex],
                                                pAppCtxt->cdcInfo[cdcIndex]->usbRxDataLen[pAppCtxt->cdcInfo[cdcIndex]->uartTxBufIndex],
                                                glCdcFnScbMap[cdcIndex]);
                    }
                } else {
                    Cy_SysLib_ExitCriticalSection(intMask);
                }

                {
                    cy_stc_hbdma_buff_status_t bufStat = {0};

                    /* Discard the consumed buffer on the USB OUT channel so HBDMA can reuse it. */
                    Cy_HBDma_Channel_DiscardBuffer(pAppCtxt->pUsbToUartChn[cdcIndex], &bufStat);
                }
            }
            break;

        case CY_USB_UART_MSG_READ_COMPLETE:
            DBG_APP_TRACE("UART_MSG_READ_COMPLETE\r\n");
            if (pAppCtxt->cdcInfo[cdcIndex]->blockUartReads) {
                DBG_APP_INFO("Block transfer on UART read complete\r\n");
                break;
            }

            xTimerReset(pAppCtxt->vcomTimerHandle[cdcIndex], 0);

            /* Send the data that has been received from UART, to the USB IN endpoint */
            glCdcTimerLoop[cdcIndex] = 0x00;
            Cy_CDC_SendUartDataToUsb(pAppCtxt,
                    pAppCtxt->cdcInfo[cdcIndex]->pUartRxBuffer[pAppCtxt->cdcInfo[cdcIndex]->nxtUsbTxBufIdx],
                    pAppCtxt->cdcInfo[cdcIndex]->uartRxDataLen[pAppCtxt->cdcInfo[cdcIndex]->nxtUsbTxBufIdx],
                    cdcIndex);

            break;

        case CY_USB_VCOM_SETLINECODING_CMD:
            Cy_CDC_SetLineCodingHandler(pAppCtxt, queueMsg.data[0],(uint8_t) queueMsg.data[1]);
            break;

        default:
            DBG_APP_TRACE("VcomMsgDefault\r\n");
            break;
        }/* end of switch() */
    } while (1);
}

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
                              uint16_t wLength, uint16_t wValue)
{
    bool isReqHandled = true;
    cy_en_usbd_ret_code_t retStatus;
    BaseType_t xHigherPriorityTaskWoken;
    cy_stc_usbd_app_msg_t xMsg;

    /* get_line_coding */
    if (bRequest == CDC_GET_LINE_CODING)
    {
        DBG_APP_TRACE("CDC_GET_LINE_CODING %d\r\n");

        retStatus = Cy_USB_USBD_SendEndp0Data(pAppCtxt->pUsbdCtxt,(uint8_t *)glCdcVcomCfgData,
                                              CY_USB_MIN(wLength,0x07));
        if (retStatus != CY_USBD_STATUS_SUCCESS)
        {
            isReqHandled = false;
        }
    }
    /* CDC_SET_CONTROL_LINE_STATE */
    else if (bRequest == CDC_SET_CONTROL_LINE_STATE)
    {
        DBG_APP_INFO("CDC_SET_CONTROL_LINE_STATE\r\n");
        if (pAppCtxt->devState == CY_USB_DEVICE_STATE_CONFIGURED)
        {  
            Cy_USBD_SendACkSetupDataStatusStage(pAppCtxt->pUsbdCtxt);
            if((wValue != 3) && (pAppCtxt->cdcDebugEnabled == 1u))
            {
                Cy_Debug_CdcPrintNow(false);
            }
            else if((wValue == 3) && (pAppCtxt->cdcDebugEnabled == 1u))
            {
                Cy_Debug_CdcPrintNow(true);
                Cy_SysLib_Delay(500);
                xMsg.type = CY_USB_PRINT_LOGS;
                xQueueSendFromISR(pAppCtxt->appMsgQueue, &(xMsg), &(xHigherPriorityTaskWoken));
            }
        }
        else
        {
            Cy_USB_USBD_EndpSetClearStall(pAppCtxt->pUsbdCtxt, 0x00,CY_USB_ENDP_DIR_IN, TRUE);
        }
    }
    else
    {
        DBG_APP_ERR("Error SetupCallback\r\n");
        isReqHandled = false;
    }
    return isReqHandled;
}

/**
 * \name Cy_CDC_SetLineCodingHandler
 * \brief CDC Set Line coding request handler
 * \param pAppCtxt User context
 * \param wLength wLength field of the control request
 * \param function function Index
 * \retval None
 */
void
Cy_CDC_SetLineCodingHandler (cy_stc_usb_app_ctxt_t *pAppCtxt, uint16_t wLength, uint8_t function)
{
    cy_stc_scb_uart_config_t uartCfg;
    cy_en_usbd_ret_code_t    retStatus = CY_USBD_STATUS_SUCCESS;
    uint32_t baudRate = 0;
    uint32_t sample = 0;
    uint16_t divider = 0;
    uint8_t cdcIndex = CY_APP_ERROR_CODE;
    uint8_t srcIndex = CY_APP_ERROR_CODE;
    cy_en_scb_uart_status_t status = CY_SCB_UART_SUCCESS;

    retStatus = Cy_USB_RecvEndp0Data(pAppCtxt->pUsbdCtxt, ((uint8_t *)glCdcVcomCfgData), wLength);
    if (retStatus != CY_USBD_STATUS_SUCCESS)
    {
        Cy_USB_USBD_EndpSetClearStall(pAppCtxt->pUsbdCtxt, 0x00, CY_USB_ENDP_DIR_IN, TRUE);
    }
    else
    {
        cdcIndex = Cy_CDC_GetIndexFromFunctionIndex(pAppCtxt,function);
        if(cdcIndex == CY_APP_ERROR_CODE)
        {
            DBG_APP_ERR("Cy_CDC_SetLineCodingHandler: Invalid CDC Configuration. Check the DMA channel setup for CDC\r\n");
            return;
        }

        if(CDC_DATA_SOURCE_SINK(pAppCtxt->cfgTablePtr,pAppCtxt->cdcInfo[cdcIndex]->cdcFuncIndex) != CY_CDC_CPU)
        {
            baudRate = (uint32_t)((glCdcVcomCfgData[3]<<24)|(glCdcVcomCfgData[2]<<16)|(glCdcVcomCfgData[1]<<8 )| glCdcVcomCfgData[0]);
            sample = Cy_CDC_GetOverSampleFromBaudRate(baudRate);
            divider = Cy_CDC_GetDividerFromBaudRate(baudRate);
            srcIndex = glCdcFnScbMap[cdcIndex];
            if(srcIndex == CY_APP_ERROR_CODE)
            {
                DBG_APP_ERR("Invalid CDC Source Index. Check CDC configuration\r\n");
                return;
            }; 

            DBG_APP_INFO("SET_LINE_CODING: BAUDRATE SET: baudRate: %d over sample: %d dividerNum:%d srcIndex :%d\r\n",baudRate, sample,divider,srcIndex);

            memset((uint8_t *)&uartCfg, 0, sizeof (cy_stc_scb_uart_config_t));
            uartCfg.uartMode            = CY_SCB_UART_STANDARD;
            uartCfg.oversample          = sample;
            uartCfg.stopBits            = CY_SCB_UART_STOP_BITS_1;
            uartCfg.parity              = CY_SCB_UART_PARITY_NONE;
            uartCfg.dataWidth           = 8;
            uartCfg.enableMsbFirst      = false;
            uartCfg.breakWidth          = 12;
            uartCfg.rxFifoTriggerLevel  = CDC_VCOM_UART_MAX_RD_SIZE - 1;
            uartCfg.txFifoTriggerLevel  = CDC_VCOM_UART_MAX_RD_SIZE;
            uartCfg.rxFifoIntEnableMask = 0;

            Cy_SCB_UART_DeInit(scbMapping[srcIndex  + 1]);

            Cy_CDC_ConfigureUartSCBClock(srcIndex, divider);

            status = Cy_SCB_UART_Init(scbMapping[srcIndex + 1], &uartCfg, pAppCtxt->pUartCtxt[cdcIndex]);
            ASSERT_NON_BLOCK(CY_SCB_UART_SUCCESS == status, status);

            Cy_SCB_UART_Enable(scbMapping[srcIndex + 1]);
            Cy_SCB_ClearTxFifo(scbMapping[srcIndex + 1]);
            Cy_SCB_ClearRxFifo(scbMapping[srcIndex + 1]);
            pAppCtxt->cdcInfo[cdcIndex]->blockUartReads = false;

            /* Set the uart configuration */
            memcpy((uint8_t *)&glCdcUartConfig[cdcIndex],(uint8_t *)&uartCfg,sizeof (uartCfg));
        }
    }
}

/**
 * \name Cy_CDC_Fn_TimerCallback
 * \brief Common handler routine for all CDC timers
 * \param xTimer Timer context
 * \param cdcIndex CDC function index
 * \retval None
 */
static void Cy_CDC_Fn_TimerCallback(TimerHandle_t xTimer, uint8_t cdcIndex){
    cy_stc_usb_app_ctxt_t *pAppCtxt = (cy_stc_usb_app_ctxt_t *)pvTimerGetTimerID(xTimer);
    uint8_t scbIndex = glCdcFnScbMap[cdcIndex]; /* Identify the SCB index used with the current cdcIndex */

    if(scbIndex == CY_APP_ERROR_CODE)           /* Ensure valid scbIndex */
    {
        return;
    }

    if (pAppCtxt->cdcInfo[cdcIndex]->blockUartReads) {
        /* If the flag to block UART reads is set, do nothing. */
        xTimerReset(xTimer, 0); /* Reset the timer to start the next timer expiry interval */
        return;
    }
    if (pAppCtxt->devSpeed >= CY_USBD_USB_DEV_FS) { /* Ensure device is running at least at full-speed */

        glCdcUartRxFifoDataLenPrev[cdcIndex] = glCdcUartRxFifoDataLenCur[cdcIndex];             /* Note the length of previous data present in the UART Rx FIFO */
        glCdcUartRxFifoDataLenCur[cdcIndex] = Cy_SCB_GetNumInRxFifo(scbMapping[scbIndex + 1]);  /* Get the length of data currently present in the UART Rx FIFO */
        uint32_t channel =  scbIndex==0 ? CDC_UART_SCB0_READ_DMA_CHANNEL :                      /* Identify the DMA channel used for the associated SCB */
                            scbIndex==1 ? CDC_UART_SCB1_READ_DMA_CHANNEL :
                            scbIndex==4 ? SCB4_RX_DMAC_CHANNEL :
                            scbIndex==6 ? SCB6_RX_DMAC_CHANNEL :
                                          0xFF;
        uint32_t Y_index_prev = Y_index[cdcIndex];                                              /* Note the previous Y-index value of the DMA channel's 2D transfer */
        Y_index[cdcIndex] = channel>16  ? Cy_DMA_Channel_GetCurrentYIndex(DW0, channel)         /* Get the current Y-index value of the DMA channel's 2D transfer*/
                                        : Cy_DMAC_Channel_GetCurrentYloopIndex(DMAC, channel);

        if (((Y_index[cdcIndex] != 0) || (glCdcUartRxFifoDataLenCur[cdcIndex] != 0)) && (glCdcTimerLoop[cdcIndex] == 0)) {
            Cy_Debug_AddToLog(3, "[EXPIRY:%x] Y_index[%d]: %d, pred: %d FIFO cnt: %d, prev: %d\r\n",   /* Log current status of the transfer and the data in FIFO */
                    Cy_USBD_GetTimerTick(),
                    cdcIndex, Y_index[cdcIndex], Y_index_prev, glCdcUartRxFifoDataLenCur[cdcIndex], glCdcUartRxFifoDataLenPrev[cdcIndex]);
        }

        if (    (glCdcUartRxFifoDataLenCur[cdcIndex] > 0x00)                                    /* Check if data is available on the FIFO */
            &&  (glCdcUartRxFifoDataLenCur[cdcIndex] == glCdcUartRxFifoDataLenPrev[cdcIndex])   /* Check if the data in the FIFO is stagnant */
            &&  (glCdcTimerLoop[cdcIndex] == 0x00)                                              /* Ensure that timer expiry based operation is not already active */
        ) {
            DBG_APP_TRACE("\r\n[Q] %s: len %d TE-len %d SCB %d\r\n",
                            __func__,
                            glCdcUartRxFifoDataLenCur[cdcIndex]+(Y_index[cdcIndex]*CDC_VCOM_UART_MAX_RD_SIZE),
                            glCdcUartRxFifoDataLenCur[cdcIndex], scbIndex);
            /* Prepare the DMA channel to read the available data from the FIFO */
            Cy_CDC_QueueUartRead(pAppCtxt, glCdcUartRxFifoDataLenCur[cdcIndex], scbIndex, Y_index[cdcIndex], false);

            if (glCdcUartRxFifoDataLenCur[cdcIndex] != 0) {
                /* SCB.RX_TRIGGER needs to be asserted high manually to initiate the DMA transfer of the stagnant data in FIFO */
                switch(scbIndex)
                {
                    case 0:
                        Cy_TrigMux_SwTrigger(TRIG_OUT_1TO1_0_SCB0_RX_TO_PDMA0_TR_IN17, CY_TRIGGER_TWO_CYCLES);
                        break;
                    case 1:
                        Cy_TrigMux_SwTrigger(TRIG_OUT_1TO1_0_SCB1_RX_TO_PDMA0_TR_IN19, CY_TRIGGER_TWO_CYCLES);
                        break;
                    case 4:
                        Cy_TrigMux_SwTrigger(TRIG_IN_MUX_5_SCB4_RX_REQ, CY_TRIGGER_TWO_CYCLES);
                        break;
                    case 6:
                        Cy_TrigMux_SwTrigger(TRIG_IN_MUX_5_SCB6_RX_REQ, CY_TRIGGER_TWO_CYCLES);
                        break;
                }
            }

            /* Set flag to indicate timer based transfer has been initiated */
            glCdcTimerLoop[cdcIndex] = 0x01;
        } else if (
            (Y_index[cdcIndex] > 0)                                                             /* Check if any data has already been transferred via 2D DMA transfer */
            &&  (Y_index[cdcIndex] == Y_index_prev)                                             /* Check if the DMA Y-index is stagnant */
            &&  (glCdcUartRxFifoDataLenCur[cdcIndex] == 0x00)                                   /* Check if the FIFO is empty */
            &&  (glCdcUartRxFifoDataLenCur[cdcIndex] == glCdcUartRxFifoDataLenPrev[cdcIndex])   /* Check if the FIFO data (no-data) is stagnant */
            &&  (glCdcTimerLoop[cdcIndex] == 0x00)                                              /* Ensure that a timer expiry execution is not already active */
        ) {
            DBG_APP_TRACE("\r\n[QY] %s: len %d TE-len %d SCB %d\r\n",
                            __func__,
                            glCdcUartRxFifoDataLenCur[cdcIndex]+(Y_index[cdcIndex]*CDC_VCOM_UART_MAX_RD_SIZE),
                            glCdcUartRxFifoDataLenCur[cdcIndex], scbIndex);

            /* Prepare the DMA channel to read the available amount of data */
            Cy_CDC_QueueUartRead(pAppCtxt, glCdcUartRxFifoDataLenCur[cdcIndex], scbIndex, Y_index[cdcIndex], true);

            /* SCB.RX_TRIGGER needs to be asserted high manually to initiate the DMA transfer of the stagnant data in FIFO */
            switch(scbIndex)
            {
                case 0:
                    Cy_TrigMux_SwTrigger(TRIG_OUT_1TO1_0_SCB0_RX_TO_PDMA0_TR_IN17, CY_TRIGGER_TWO_CYCLES);
                break;
                case 1:
                    Cy_TrigMux_SwTrigger(TRIG_OUT_1TO1_0_SCB1_RX_TO_PDMA0_TR_IN19, CY_TRIGGER_TWO_CYCLES);
                break;
                case 4:
                    Cy_TrigMux_SwTrigger(TRIG_IN_MUX_5_SCB4_RX_REQ, CY_TRIGGER_TWO_CYCLES);
                break;
                case 6:
                    Cy_TrigMux_SwTrigger(TRIG_IN_MUX_5_SCB6_RX_REQ, CY_TRIGGER_TWO_CYCLES);
                break;
            }

            /* Set flag to indicate timer based transfer has been initiated */
            glCdcTimerLoop[cdcIndex] = 0x01;
        }
    }

    xTimerReset(xTimer, 0); /* Reset the timer to start the next timer expiry interval */
}   /* end of function  */

/**
 * \name Cy_CDC_Fn0TimerCallback
 * \brief Timer callback for CDC function 0
 * \details DMA transfer from the UART RX FIFO will only happen when
 * the amount of data received crosses the threshold. To handle the cases
 * where lesser amount of data is received, a timer is being run. If the
 * RX FIFO has valid data which is below the threshold level, the UART DMA
 * will be re-configured to read it out
 * \param xTimer Timer Handle
 * \retval None
 */
void
Cy_CDC_Fn0TimerCallback (TimerHandle_t xTimer)
{
    Cy_CDC_Fn_TimerCallback(xTimer, 0);
}   /* end of function  */

/**
 * \name Cy_CDC_Fn1TimerCallback
 * \brief Timer callback for CDC function 1
 * \details DMA transfer from the UART RX FIFO will only happen when
 * the amount of data received crosses the threshold. To handle the cases
 * where lesser amount of data is received, a timer is being run. If the
 * RX FIFO has valid data which is below the threshold level, the UART DMA
 * will be re-configured to read it out
 * \param xTimer Timer Handle
 * \retval None
 */
void
Cy_CDC_Fn1TimerCallback (TimerHandle_t xTimer)
{
    Cy_CDC_Fn_TimerCallback(xTimer, 1);
}   /* end of function  */

/**
 * \name Cy_CDC_Fn2TimerCallback
 * \brief Timer callback for CDC function 2
 * \details DMA transfer from the UART RX FIFO will only happen when
 * the amount of data received crosses the threshold. To handle the cases
 * where lesser amount of data is received, a timer is being run. If the
 * RX FIFO has valid data which is below the threshold level, the UART DMA
 * will be re-configured to read it out
 * \param xTimer Timer Handle
 * \retval None
 */
void
Cy_CDC_Fn2TimerCallback (TimerHandle_t xTimer)
{
    Cy_CDC_Fn_TimerCallback(xTimer, 2);
}   /* end of function  */

/**
 * \name Cy_CDC_Fn3TimerCallback
 * \brief Timer callback for CDC function 3
 * \details DMA transfer from the UART RX FIFO will only happen when
 * the amount of data received crosses the threshold. To handle the cases
 * where lesser amount of data is received, a timer is being run. If the
 * RX FIFO has valid data which is below the threshold level, the UART DMA
 * will be re-configured to read it out
 * \param xTimer Timer Handle
 * \retval None
 */
void
Cy_CDC_Fn3TimerCallback (TimerHandle_t xTimer)
{
    Cy_CDC_Fn_TimerCallback(xTimer, 3);
}   /* end of function  */

/**
 * \name Cy_CDC_UartInitDmaIntr
 * \brief Function to register an ISR for the DMA channel associated with a UART Read/Write
 * \param uart_read_write UART READ/WRITE
 * \param scbIndex UART SCB index
 * \retval None
 */
void
Cy_CDC_UartInitDmaIntr(uint8_t uart_read_write,uint8_t scbIndex)
{
    cy_stc_sysint_t intrCfg;

    if (uart_read_write == CDC_UART_WRITE) {
#if CY_CPU_CORTEX_M4
        intrCfg.intrPriority = 5;
        if(scbIndex != SCB4_DMAC && scbIndex != SCB6_DMAC)
        {
            /* UART Write Interrupt Source */
            intrCfg.intrSrc = (IRQn_Type)(cpuss_interrupts_dw0_0_IRQn + glCdcUartWrChNum[scbIndex]);
        }
        else if(scbIndex == SCB6_DMAC)
        {
            /* UART Write Interrupt Source */
            intrCfg.intrSrc = (IRQn_Type)(cpuss_interrupts_dmac_4_IRQn);
        }
        else
        {
            /* UART Write Interrupt Source */
            intrCfg.intrSrc = (IRQn_Type)(cpuss_interrupts_dmac_2_IRQn);
        }
#else
        intrCfg.intrPriority = 3;
        intrCfg.intrSrc = NvicMux6_IRQn;
        if(scbIndex != SCB4_DMAC && scbIndex != SCB6_DMAC)
        {
            /* UART Write Interrupt Source */
            intrCfg.cm0pSrc = (cy_en_intr_t)(cpuss_interrupts_dw0_0_IRQn + glCdcUartWrChNum[scbIndex]);
        }
        else if(scbIndex == SCB6_DMAC)
        {
            /* UART Write Interrupt Source */
            intrCfg.cm0pSrc = (cy_en_intr_t)(cpuss_interrupts_dmac_4_IRQn);
        }
        else
        {
            /* UART Write Interrupt Source */
            intrCfg.cm0pSrc = (cy_en_intr_t)(cpuss_interrupts_dmac_2_IRQn);
        }
#endif

#if CY_CPU_CORTEX_M4
        Cy_SysInt_Init(&intrCfg, (cy_israddress)uartWrChannelIsr[scbIndex]);
#else
        Cy_SysInt_Init(&intrCfg, (cy_israddress)Cy_CDC_UartDmaWrite_Combined_ISR);
#endif
        NVIC_EnableIRQ(intrCfg.intrSrc);

    } else {
#if CY_CPU_CORTEX_M4
        intrCfg.intrPriority = 5;
        if(scbIndex != SCB4_DMAC && scbIndex != SCB6_DMAC)
        {
            /* UART Read Interrupt Source */
            intrCfg.intrSrc = (IRQn_Type)(cpuss_interrupts_dw0_0_IRQn + glCdcUartRdChNum[scbIndex]);
        }
        else if(scbIndex == SCB6_DMAC)
        {
            /* UART Read Interrupt Source */
            intrCfg.intrSrc = (IRQn_Type)(cpuss_interrupts_dmac_5_IRQn);
        }
        else
        {
            /* UART Read Interrupt Source */
            intrCfg.intrSrc = (IRQn_Type)(cpuss_interrupts_dmac_3_IRQn);
        }
#else
        intrCfg.intrPriority = 3;
        intrCfg.intrSrc = NvicMux3_IRQn;
        if(scbIndex != SCB4_DMAC && scbIndex != SCB6_DMAC)
        {
            /* UART Read Interrupt Source */
            intrCfg.cm0pSrc = (cy_en_intr_t)(cpuss_interrupts_dw0_0_IRQn + glCdcUartRdChNum[scbIndex]);
        }
        else if(scbIndex == SCB6_DMAC)
        {
            /* UART Read Interrupt Source */
            intrCfg.cm0pSrc = (cy_en_intr_t)(cpuss_interrupts_dmac_5_IRQn);
        }
        else
        {
            /* UART Read Interrupt Source */
            intrCfg.cm0pSrc = (cy_en_intr_t)(cpuss_interrupts_dmac_3_IRQn);
        }
#endif

#if CY_CPU_CORTEX_M4
        Cy_SysInt_Init(&intrCfg,(cy_israddress) uartRdChannelIsr[scbIndex]);
#else
        Cy_SysInt_Init(&intrCfg, (cy_israddress)Cy_CDC_UartDmaRead_Combined_ISR);
#endif
        NVIC_EnableIRQ(intrCfg.intrSrc);
    }
    return;
} /* end of function. */

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
                                     uint32_t dataLength, uint8_t scbIndex, uint32_t pY_index, bool isY_indexStuck)
{

    /* Identify the cdcIndex associated with the SCB to read from */
    uint8_t cdcIndex =  (((scbIndex == glCdcFnScbMap[0]) && (scbIndex != CY_APP_ERROR_CODE))? 0 :
                        (((scbIndex == glCdcFnScbMap[1]) && (scbIndex != CY_APP_ERROR_CODE))? 1 : 
                        (((scbIndex == glCdcFnScbMap[2]) && (scbIndex != CY_APP_ERROR_CODE))? 2 : 
                        (((scbIndex == glCdcFnScbMap[3]) && (scbIndex != CY_APP_ERROR_CODE))? 3 : 
                        CY_APP_ERROR_CODE))));
    if(cdcIndex == CY_APP_ERROR_CODE)
    {
        DBG_APP_ERR("Cy_CDC_QueueUartRead: Invalid CDC Configuration. Check the DMA channel setup for CDC\r\n");
        return;
    }

    /* vTaskDelay(1); */

    /* The buffer address to be used for the UART read operation */
    uint8_t *rxBuf = pAppCtxt->cdcInfo[cdcIndex]->pUartRxBuffer[pAppCtxt->cdcInfo[cdcIndex]->uartRxBufIndex];

    /* Save the current total size of data that is eventually to be sent over USB */
    pAppCtxt->cdcInfo[cdcIndex]->uartRxDataLen[pAppCtxt->cdcInfo[cdcIndex]->uartRxBufIndex]
                = dataLength+(pY_index * CDC_VCOM_UART_MAX_RD_SIZE);

    /* Configure the DMA channel to read specified amount of data
     * into the next Rx buffer.
     */
    if(scbIndex != SCB4_DMAC && scbIndex != SCB6_DMAC)
    {
        Cy_CDC_UartDwRead(pAppCtxt, rxBuf, dataLength, scbIndex, pY_index, isY_indexStuck); 
    }
    else
    {
        Cy_CDC_UartDmacRead(pAppCtxt, rxBuf, dataLength, scbIndex, pY_index, isY_indexStuck);
    }
}

/**
 * \name Cy_CDC_UartDmacWrite
 * \brief Function to write operation on an UART TX_FIFO using DMAC.
 * \param pBuffer Data buffer pointer
 * \param dataSize Size of data to write
 * \param scbIndex UART SCB index
 * \retval None
 */
void
Cy_CDC_UartDmacWrite (uint8_t *pBuffer, uint16_t dataSize, uint8_t scbIndex)
{
    cy_stc_dmac_descriptor_config_t desc_cfg;
    cy_stc_dmac_channel_config_t    chan_cfg;
    cy_en_dmac_status_t stat;

    DBG_APP_TRACE("Cy_CDC_UartDmacWrite: scbIndex=%x dataSize %d \r\n", scbIndex,dataSize);
    
    if (scbIndex==SCB4_DMAC){    
        /* If the DMA channel is already enabled, disable it. */
        Cy_DMAC_Channel_Disable(DMAC, SCB4_TX_DMAC_CHANNEL);

        if (dataSize > CDC_VCOM_UART_MAX_RD_SIZE) {
            /* Data may not fit in the SCB TX FIFO. Use 2-D DMA
            * with wait for TRIGGER from SCB after each 64 bytes are
            * transferred. A second descriptor is used to send any
            * residual portion of data at the end.
            */
            if ((dataSize % CDC_VCOM_UART_MAX_RD_SIZE) != 0) {
                desc_cfg.retrigger          = CY_DMAC_RETRIG_16CYC;
                desc_cfg.interruptType      = CY_DMAC_DESCR_CHAIN;
                desc_cfg.triggerOutType     = CY_DMAC_DESCR_CHAIN;
                desc_cfg.triggerInType      = CY_DMAC_DESCR;
                desc_cfg.dataPrefetch       = false;
                desc_cfg.dataSize           = CY_DMAC_BYTE;
                desc_cfg.srcTransferSize    = CY_DMAC_TRANSFER_SIZE_DATA;
                desc_cfg.dstTransferSize    = CY_DMAC_TRANSFER_SIZE_WORD;
                desc_cfg.descriptorType     = CY_DMAC_1D_TRANSFER;
                desc_cfg.srcAddress         = (void *)(pBuffer + (dataSize & (~(CDC_VCOM_UART_MAX_RD_SIZE - 1))));
                desc_cfg.dstAddress         = (void *)(&(SCB4->TX_FIFO_WR));
                desc_cfg.srcXincrement      = 1;
                desc_cfg.dstXincrement      = 0;
                desc_cfg.xCount             = dataSize % CDC_VCOM_UART_MAX_RD_SIZE;
                desc_cfg.srcYincrement      = 0;
                desc_cfg.dstYincrement      = 0;
                desc_cfg.yCount             = 0;
                desc_cfg.channelState       = CY_DMAC_CHANNEL_DISABLED;
                desc_cfg.nextDescriptor     = NULL;
                stat = Cy_DMAC_Descriptor_Init (&glCdcUartWrDmacDesc2, &desc_cfg);
                ASSERT_NON_BLOCK(stat == CY_DMAC_SUCCESS, stat);

                /* Descriptor 1 needs to be linked to descriptor 2. */
                desc_cfg.channelState       = CY_DMAC_CHANNEL_ENABLED;
                desc_cfg.nextDescriptor     = &glCdcUartWrDmacDesc2;
            } else {
                desc_cfg.channelState       = CY_DMAC_CHANNEL_DISABLED;
                desc_cfg.nextDescriptor     = NULL;
            }

            desc_cfg.retrigger          = CY_DMAC_RETRIG_16CYC;
            desc_cfg.interruptType      = CY_DMAC_DESCR_CHAIN;
            desc_cfg.triggerOutType     = CY_DMAC_DESCR_CHAIN;
            desc_cfg.triggerInType      = CY_DMAC_X_LOOP;
            desc_cfg.dataPrefetch       = false;
            desc_cfg.dataSize           = CY_DMAC_BYTE;
            desc_cfg.srcTransferSize    = CY_DMAC_TRANSFER_SIZE_DATA;
            desc_cfg.dstTransferSize    = CY_DMAC_TRANSFER_SIZE_WORD;
            desc_cfg.descriptorType     = CY_DMAC_2D_TRANSFER;
            desc_cfg.srcAddress         = (void *)pBuffer;
            desc_cfg.dstAddress         = (void *)(&(SCB4->TX_FIFO_WR));
            desc_cfg.srcXincrement      = 1;
            desc_cfg.dstXincrement      = 0;
            desc_cfg.xCount             = CDC_VCOM_UART_MAX_RD_SIZE;
            desc_cfg.srcYincrement      = CDC_VCOM_UART_MAX_RD_SIZE;
            desc_cfg.dstYincrement      = 0;
            desc_cfg.yCount             = (dataSize / CDC_VCOM_UART_MAX_RD_SIZE);

        } else {
            /* All the available data will fill in the SCB TX FIFO.
            * Use 1-D DMA to transfer the entire data in one burst.
            */
            desc_cfg.retrigger          = CY_DMAC_RETRIG_4CYC;
            desc_cfg.interruptType      = CY_DMAC_DESCR;
            desc_cfg.triggerOutType     = CY_DMAC_DESCR;
            desc_cfg.channelState       = CY_DMAC_CHANNEL_DISABLED;
            desc_cfg.triggerInType      = CY_DMAC_DESCR;
            desc_cfg.dataPrefetch       = false;
            desc_cfg.dataSize           = CY_DMAC_BYTE;
            desc_cfg.srcTransferSize    = CY_DMAC_TRANSFER_SIZE_DATA;
            desc_cfg.dstTransferSize    = CY_DMAC_TRANSFER_SIZE_WORD;
            desc_cfg.descriptorType     = CY_DMAC_1D_TRANSFER;
            desc_cfg.srcAddress         = (void *)pBuffer;
            desc_cfg.dstAddress         = (void *)(&(SCB4->TX_FIFO_WR));
            desc_cfg.srcXincrement      = 1;
            desc_cfg.dstXincrement      = 0;
            desc_cfg.xCount             = dataSize;
            desc_cfg.srcYincrement      = 0;
            desc_cfg.dstYincrement      = 0;
            desc_cfg.yCount             = 1;
            desc_cfg.nextDescriptor     = NULL;
        }

        stat = Cy_DMAC_Descriptor_Init (&glCdcUartWrDmacDesc1, &desc_cfg);
        ASSERT_NON_BLOCK(stat == CY_DMAC_SUCCESS,stat);

        chan_cfg.descriptor  = &glCdcUartWrDmacDesc1;
        chan_cfg.priority    = 0;
        chan_cfg.enable      = false;
        chan_cfg.bufferable  = false;

        stat = Cy_DMAC_Channel_Init (DMAC, SCB4_TX_DMAC_CHANNEL, &chan_cfg);;
        ASSERT_NON_BLOCK(stat == CY_DMAC_SUCCESS,stat);

        /* Make sure SLOW AHB read cache is evicted before new transfer is started. */
        MAIN_REG->CTRL |= MAIN_REG_CTRL_EVICT_SLOW_AHB_RD_CACHE_Msk;
        while ((MAIN_REG->CTRL & MAIN_REG_CTRL_EVICT_SLOW_AHB_RD_CACHE_Msk) != 0);

        Cy_DMAC_Channel_SetInterruptMask(DMAC,SCB4_TX_DMAC_CHANNEL,CY_DMAC_INTR_MASK);
        Cy_DMAC_Channel_Enable(DMAC, SCB4_TX_DMAC_CHANNEL);

    } else if (scbIndex == SCB6_DMAC) {
        /* If the DMA channel is already enabled, disable it. */
        Cy_DMAC_Channel_Disable(DMAC, SCB6_TX_DMAC_CHANNEL);

        if (dataSize > CDC_VCOM_UART_MAX_RD_SIZE) {
            /* Data may not fit in the SCB TX FIFO. Use 2-D DMA
            * with wait for TRIGGER from SCB after each 64 bytes are
            * transferred. A second descriptor is used to send any
            * residual portion of data at the end.
            */
            if ((dataSize % CDC_VCOM_UART_MAX_RD_SIZE) != 0) {
                desc_cfg.retrigger          = CY_DMAC_RETRIG_16CYC;
                desc_cfg.interruptType      = CY_DMAC_DESCR_CHAIN;
                desc_cfg.triggerOutType     = CY_DMAC_DESCR_CHAIN;
                desc_cfg.triggerInType      = CY_DMAC_DESCR;
                desc_cfg.dataPrefetch       = false;
                desc_cfg.dataSize           = CY_DMAC_BYTE;
                desc_cfg.srcTransferSize    = CY_DMAC_TRANSFER_SIZE_DATA;
                desc_cfg.dstTransferSize    = CY_DMAC_TRANSFER_SIZE_WORD;
                desc_cfg.descriptorType     = CY_DMAC_1D_TRANSFER;
                desc_cfg.srcAddress         = (void *)(pBuffer + (dataSize & (~(CDC_VCOM_UART_MAX_RD_SIZE - 1))));
                desc_cfg.dstAddress         = (void *)(&(SCB6->TX_FIFO_WR));
                desc_cfg.srcXincrement      = 1;
                desc_cfg.dstXincrement      = 0;
                desc_cfg.xCount             = dataSize % CDC_VCOM_UART_MAX_RD_SIZE;
                desc_cfg.srcYincrement      = 0;
                desc_cfg.dstYincrement      = 0;
                desc_cfg.yCount             = 0;
                desc_cfg.channelState       = CY_DMAC_CHANNEL_DISABLED;
                desc_cfg.nextDescriptor     = NULL;
                stat = Cy_DMAC_Descriptor_Init (&glCdcUartWrDmacDesc4, &desc_cfg);
                ASSERT_NON_BLOCK(stat == CY_DMAC_SUCCESS,stat);

                /* Descriptor 3 needs to be linked to descriptor 4. */
                desc_cfg.channelState       = CY_DMAC_CHANNEL_ENABLED;
                desc_cfg.nextDescriptor     = &glCdcUartWrDmacDesc4;
            } else {
                desc_cfg.channelState       = CY_DMAC_CHANNEL_DISABLED;
                desc_cfg.nextDescriptor     = NULL;
            }

            desc_cfg.retrigger          = CY_DMAC_RETRIG_16CYC;
            desc_cfg.interruptType      = CY_DMAC_DESCR_CHAIN;
            desc_cfg.triggerOutType     = CY_DMAC_DESCR_CHAIN;
            desc_cfg.triggerInType      = CY_DMAC_X_LOOP;
            desc_cfg.dataPrefetch       = false;
            desc_cfg.dataSize           = CY_DMAC_BYTE;
            desc_cfg.srcTransferSize    = CY_DMAC_TRANSFER_SIZE_DATA;
            desc_cfg.dstTransferSize    = CY_DMAC_TRANSFER_SIZE_WORD;
            desc_cfg.descriptorType     = CY_DMAC_2D_TRANSFER;
            desc_cfg.srcAddress         = (void *)pBuffer;
            desc_cfg.dstAddress         = (void *)(&(SCB6->TX_FIFO_WR));
            desc_cfg.srcXincrement      = 1;
            desc_cfg.dstXincrement      = 0;
            desc_cfg.xCount             = CDC_VCOM_UART_MAX_RD_SIZE;
            desc_cfg.srcYincrement      = CDC_VCOM_UART_MAX_RD_SIZE;
            desc_cfg.dstYincrement      = 0;
            desc_cfg.yCount             = (dataSize / CDC_VCOM_UART_MAX_RD_SIZE);

        } else {
            /* All the available data will fill in the SCB TX FIFO.
            * Use 1-D DMA to transfer the entire data in one burst.
            */
            desc_cfg.retrigger          = CY_DMAC_RETRIG_4CYC;
            desc_cfg.interruptType      = CY_DMAC_DESCR;
            desc_cfg.triggerOutType     = CY_DMAC_DESCR;
            desc_cfg.channelState       = CY_DMAC_CHANNEL_DISABLED;
            desc_cfg.triggerInType      = CY_DMAC_DESCR;
            desc_cfg.dataPrefetch       = false;
            desc_cfg.dataSize           = CY_DMAC_BYTE;
            desc_cfg.srcTransferSize    = CY_DMAC_TRANSFER_SIZE_DATA;
            desc_cfg.dstTransferSize    = CY_DMAC_TRANSFER_SIZE_WORD;
            desc_cfg.descriptorType     = CY_DMAC_1D_TRANSFER;
            desc_cfg.srcAddress         = (void *)pBuffer;
            desc_cfg.dstAddress         = (void *)(&(SCB6->TX_FIFO_WR));
            desc_cfg.srcXincrement      = 1;
            desc_cfg.dstXincrement      = 0;
            desc_cfg.xCount             = dataSize;
            desc_cfg.srcYincrement      = 0;
            desc_cfg.dstYincrement      = 0;
            desc_cfg.yCount             = 1;
            desc_cfg.nextDescriptor     = NULL;
        }

        stat = Cy_DMAC_Descriptor_Init (&glCdcUartWrDmacDesc3, &desc_cfg);
        ASSERT_NON_BLOCK(stat == CY_DMAC_SUCCESS,stat);

        chan_cfg.descriptor  = &glCdcUartWrDmacDesc3;
        chan_cfg.priority    = 0;
        chan_cfg.enable      = false;
        chan_cfg.bufferable  = false;

        stat = Cy_DMAC_Channel_Init (DMAC, SCB6_TX_DMAC_CHANNEL, &chan_cfg);;
        ASSERT_NON_BLOCK(stat == CY_DMAC_SUCCESS,stat);

        /* Make sure SLOW AHB read cache is evicted before new transfer is started. */
        MAIN_REG->CTRL |= MAIN_REG_CTRL_EVICT_SLOW_AHB_RD_CACHE_Msk;
        while ((MAIN_REG->CTRL & MAIN_REG_CTRL_EVICT_SLOW_AHB_RD_CACHE_Msk) != 0);

        Cy_DMAC_Channel_SetInterruptMask(DMAC,SCB6_TX_DMAC_CHANNEL,CY_DMAC_INTR_MASK);
        Cy_DMAC_Channel_Enable(DMAC, SCB6_TX_DMAC_CHANNEL);
    }

    return;
}  /* end of function */

/**
 * \name Cy_CDC_UartDwWrite
 * \brief Function to write operation on an UART TX_FIFO using DataWire.
 * \param pBuffer Data buffer pointer
 * \param dataSize Size of data to write
 * \param scbIndex UART SCB index
 * \retval None
 */
void
Cy_CDC_UartDwWrite (uint8_t *pBuffer, uint16_t dataSize, uint8_t scbIndex)
{
    cy_stc_dma_descriptor_config_t  desc_cfg;
    cy_stc_dma_channel_config_t     chan_cfg;
    cy_en_dma_status_t              stat;

    DBG_APP_TRACE("Cy_CDC_UartDwWrite: scbIndex=%d dataSize %d\r\n", scbIndex, dataSize);
    
    /* If the DMA channel is already enabled, disable it. */
    Cy_DMA_Channel_Disable(DW0, glCdcUartWrChNum[scbIndex]);

    if (dataSize > CDC_VCOM_UART_MAX_RD_SIZE) {
        /* Data may not fit in the SCB TX FIFO. Use 2-D DMA
         * with wait for TRIGGER from SCB after each 64 bytes are
         * transferred. A second descriptor is used to send any
         * residual portion of data at the end.
         */
        if ((dataSize % CDC_VCOM_UART_MAX_RD_SIZE) != 0) {
            desc_cfg.retrigger          = CY_DMA_RETRIG_16CYC;
            desc_cfg.interruptType      = CY_DMA_DESCR_CHAIN;
            desc_cfg.triggerOutType     = CY_DMA_DESCR_CHAIN;
            desc_cfg.triggerInType      = CY_DMA_DESCR;
            desc_cfg.dataSize           = CY_DMA_BYTE;
            desc_cfg.srcTransferSize    = CY_DMA_TRANSFER_SIZE_DATA;
            desc_cfg.dstTransferSize    = CY_DMA_TRANSFER_SIZE_WORD;
            desc_cfg.descriptorType     = CY_DMA_1D_TRANSFER;
            desc_cfg.srcAddress         = (void *)(pBuffer + (dataSize & (~(CDC_VCOM_UART_MAX_RD_SIZE - 1))));
            desc_cfg.dstAddress         = (void *)(&(scbMapping[scbIndex+1])->TX_FIFO_WR);
            desc_cfg.srcXincrement      = 1;
            desc_cfg.dstXincrement      = 0;
            desc_cfg.xCount             = dataSize % CDC_VCOM_UART_MAX_RD_SIZE;
            desc_cfg.srcYincrement      = 0;
            desc_cfg.dstYincrement      = 0;
            desc_cfg.yCount             = 0;
            desc_cfg.channelState       = CY_DMA_CHANNEL_DISABLED;
            desc_cfg.nextDescriptor     = NULL;
            stat = Cy_DMA_Descriptor_Init (&glCdcUartWrDmaDesc2[scbIndex], &desc_cfg);
            ASSERT_NON_BLOCK(stat == CY_DMA_SUCCESS,stat);

            /* Descriptor 1 needs to be linked to descriptor 2. */
            desc_cfg.channelState       = CY_DMA_CHANNEL_ENABLED;
            desc_cfg.nextDescriptor     = &glCdcUartWrDmaDesc2[scbIndex];
        } else {
            desc_cfg.channelState       = CY_DMA_CHANNEL_DISABLED;
            desc_cfg.nextDescriptor     = NULL;
        }

        desc_cfg.retrigger          = CY_DMA_RETRIG_16CYC;
        desc_cfg.interruptType      = CY_DMA_DESCR_CHAIN;
        desc_cfg.triggerOutType     = CY_DMA_DESCR_CHAIN;
        desc_cfg.triggerInType      = CY_DMA_X_LOOP;
        desc_cfg.dataSize           = CY_DMA_BYTE;
        desc_cfg.srcTransferSize    = CY_DMA_TRANSFER_SIZE_DATA;
        desc_cfg.dstTransferSize    = CY_DMA_TRANSFER_SIZE_WORD;
        desc_cfg.descriptorType     = CY_DMA_2D_TRANSFER;
        desc_cfg.srcAddress         = (void *)pBuffer;
        desc_cfg.dstAddress         = (void *)(&(scbMapping[scbIndex+1])->TX_FIFO_WR);
        desc_cfg.srcXincrement      = 1;
        desc_cfg.dstXincrement      = 0;
        desc_cfg.xCount             = CDC_VCOM_UART_MAX_RD_SIZE;
        desc_cfg.srcYincrement      = CDC_VCOM_UART_MAX_RD_SIZE;
        desc_cfg.dstYincrement      = 0;
        desc_cfg.yCount             = (dataSize / CDC_VCOM_UART_MAX_RD_SIZE);
    } else {
        /* All the available data will fill in the SCB TX FIFO.
         * Use 1-D DMA to transfer the entire data in one burst.
         */
        desc_cfg.retrigger          = CY_DMA_RETRIG_4CYC;
        desc_cfg.interruptType      = CY_DMA_DESCR;
        desc_cfg.triggerOutType     = CY_DMA_DESCR;
        desc_cfg.channelState       = CY_DMA_CHANNEL_DISABLED;
        desc_cfg.triggerInType      = CY_DMA_DESCR;
        desc_cfg.dataSize           = CY_DMA_BYTE;
        desc_cfg.srcTransferSize    = CY_DMA_TRANSFER_SIZE_DATA;
        desc_cfg.dstTransferSize    = CY_DMA_TRANSFER_SIZE_WORD;
        desc_cfg.descriptorType     = CY_DMA_1D_TRANSFER;
        desc_cfg.srcAddress         = (void *)pBuffer;
        desc_cfg.dstAddress         = (void *)(&(scbMapping[scbIndex+1])->TX_FIFO_WR);
        desc_cfg.srcXincrement      = 1;
        desc_cfg.dstXincrement      = 0;
        desc_cfg.xCount             = dataSize;
        desc_cfg.srcYincrement      = 0;
        desc_cfg.dstYincrement      = 0;
        desc_cfg.yCount             = 1;
        desc_cfg.nextDescriptor     = NULL;
    }

    stat = Cy_DMA_Descriptor_Init (&glCdcUartWrDmaDesc1[scbIndex], &desc_cfg);
    ASSERT_NON_BLOCK(stat == CY_DMA_SUCCESS,stat);

    chan_cfg.descriptor  = &glCdcUartWrDmaDesc1[scbIndex];
    chan_cfg.preemptable = false;
    chan_cfg.priority    = 0;
    chan_cfg.enable      = false;
    chan_cfg.bufferable  = false;

    stat = Cy_DMA_Channel_Init (DW0, glCdcUartWrChNum[scbIndex], &chan_cfg);
    ASSERT_NON_BLOCK(stat == CY_DMA_SUCCESS,stat);

    /* Make sure SLOW AHB read cache is evicted before new transfer is started. */
    MAIN_REG->CTRL |= MAIN_REG_CTRL_EVICT_SLOW_AHB_RD_CACHE_Msk;
    while ((MAIN_REG->CTRL & MAIN_REG_CTRL_EVICT_SLOW_AHB_RD_CACHE_Msk) != 0);

    Cy_DMA_Channel_Enable(DW0, glCdcUartWrChNum[scbIndex]);
    Cy_DMA_Channel_SetInterruptMask(DW0,glCdcUartWrChNum[scbIndex],CY_DMA_INTR_MASK);

    return;
}  /* end of function */

/**
 * \name Cy_CDC_SendUartDataToUsb
 * \brief Function to send the data received from UART to the USB endpoint using High BandWidth DMA channel.
 * \param pAppCtxt User context
 * \param dataBuf_p Buffer pointer
 * \param dataLength Size of data to send
 * \param cdcIndex CDC index
 * \retval None
 */
void
Cy_CDC_SendUartDataToUsb (cy_stc_usb_app_ctxt_t *pAppCtxt,
                           uint8_t *dataBuf_p, uint32_t dataLength, uint8_t cdcIndex)
{
    cy_stc_hbdma_buff_status_t dmaBufStat;
    cy_en_hbdma_mgr_status_t   hbdma_stat;

    dmaBufStat.pBuffer = dataBuf_p;
    dmaBufStat.size    = CDC_VCOM_UART_MAX_RD_SIZE;
    dmaBufStat.count   = dataLength;
    dmaBufStat.status  = 0;

    cdcRxPktCount[cdcIndex]++;

    hbdma_stat = Cy_HBDma_Channel_CommitBuffer(pAppCtxt->pUartToUsbChn[cdcIndex], &dmaBufStat);
    if (hbdma_stat != CY_HBDMA_MGR_SUCCESS) {
        DBG_APP_ERR("Cy_CDC_SendUartDataToUsb: DMA error=%x\r\n", hbdma_stat);
        return;
    }

    pAppCtxt->cdcInfo[cdcIndex]->usbZlpReqd = false;
    pAppCtxt->cdcInfo[cdcIndex]->nxtUsbTxBufIdx++;
    if (pAppCtxt->cdcInfo[cdcIndex]->nxtUsbTxBufIdx >= pAppCtxt->cdcInfo[cdcIndex]->dmaRxBufferCount) {
        pAppCtxt->cdcInfo[cdcIndex]->nxtUsbTxBufIdx = 0;
    }

    /* Send ZLP only when this is the last occupied buffer and full-packet is transferred */
    if (dataLength == USB_HS_BULK_XFER_SIZE
        //TODO: && Add condition to send ZLP ONLY when its the last 512B transfer, not after every 512B transfer
        ) {
    #if 0
        vTaskDelay(1);
        Cy_USBD_SendEgressZLP(pAppCtxt->pUsbdCtxt, pAppCtxt->cdcInfo[cdcIndex]->vcomInEpNum);
    #else
        pAppCtxt->cdcInfo[cdcIndex]->usbZlpReqd = true;
    #endif
    }
}

/**
 * \name Cy_CDC_UartDmaWriteCompletion
 * \brief Handler for DMA transfer completion on UART TX_FIFO
 * \param pApp User context
 * \param scbIndex SCB index
 * \retval None
 */
void
Cy_CDC_UartDmaWriteCompletion(void *pApp, uint8_t scbIndex)
{
    cy_stc_usbd_app_msg_t xMsg;
    cy_stc_usb_app_ctxt_t *pAppCtxt;
    uint8_t cdcIndex = CY_APP_ERROR_CODE;

    cdcIndex =  (((scbIndex == glCdcFnScbMap[0]) && (scbIndex != CY_APP_ERROR_CODE))? 0 :
                (((scbIndex == glCdcFnScbMap[1]) && (scbIndex != CY_APP_ERROR_CODE))? 1 :
                (((scbIndex == glCdcFnScbMap[2]) && (scbIndex != CY_APP_ERROR_CODE))? 2 :
                (((scbIndex == glCdcFnScbMap[3]) && (scbIndex != CY_APP_ERROR_CODE))? 3 :
                CY_APP_ERROR_CODE))));

    if(cdcIndex == CY_APP_ERROR_CODE)
    {
        DBG_APP_ERR("Cy_CDC_UartDmaWriteCompletion: Invalid CDC Configuration. Check the DMA channel setup for CDC\r\n");
        return;
    }

    DBG_APP_TRACE("Cy_CDC_UartDmaWriteCompletion:SCB > CDC %d %d\r\n",cdcIndex, glCdcFnScbMap[0]);

    pAppCtxt = (cy_stc_usb_app_ctxt_t*)pApp;

    xMsg.type = CY_USB_UART_MSG_WRITE_COMPLETE;
    xMsg.data[0] = cdcIndex;
    Cy_CDC_PostVcomMsgFromISR(pAppCtxt, &xMsg, cdcIndex, __func__);
    return;

}  /* end of function */

/**
 * \name Cy_CDC_ClearUartDmaInterrupt
 * \brief Function to clear the pending DMA interrupt associated with an UART.
 * \param pAppCtxt User context
 * \param uart_read_write UART Read/Write Channel
 * \param scbIndex SCB index
 * \retval None
 */
void
Cy_CDC_ClearUartDmaInterrupt(cy_stc_usb_app_ctxt_t *pAppCtxt,uint8_t uart_read_write, uint8_t scbIndex)
{

    if ((pAppCtxt != NULL)) {
        if (uart_read_write == CDC_UART_WRITE) {
            if(scbIndex != SCB4_DMAC && scbIndex != SCB6_DMAC)
                Cy_DMA_Channel_ClearInterrupt(pAppCtxt->pCpuDw0Base, glCdcUartWrChNum[scbIndex]);
            else if(scbIndex == SCB4_DMAC)
                Cy_DMAC_Channel_ClearInterrupt(DMAC, SCB4_TX_DMAC_CHANNEL, CY_DMAC_INTR_MASK);
            else if(scbIndex == SCB6_DMAC)
                Cy_DMAC_Channel_ClearInterrupt(DMAC, SCB6_TX_DMAC_CHANNEL, CY_DMAC_INTR_MASK);

        } else if (uart_read_write == CDC_UART_READ) {
            if(scbIndex != SCB4_DMAC && scbIndex != SCB6_DMAC)
                Cy_DMA_Channel_ClearInterrupt(pAppCtxt->pCpuDw0Base, glCdcUartRdChNum[scbIndex]);
            else if(scbIndex == SCB4_DMAC)
                Cy_DMAC_Channel_ClearInterrupt(DMAC, SCB4_RX_DMAC_CHANNEL, CY_DMAC_INTR_MASK);
            else if(scbIndex == SCB6_DMAC)
                Cy_DMAC_Channel_ClearInterrupt(DMAC, SCB6_RX_DMAC_CHANNEL, CY_DMAC_INTR_MASK);
        } else {
            DBG_APP_ERR("Cy_CDC_ClearUartDmaInterrupt:Clear Uart Dma Intr Err\r\n");
        }
    }

    return;
}  /* end of function. */

/**
 * \name Cy_CDC_UartDmaReadCompletion
 * \brief Handler for DMA transfer completion on UART RX_FIFO
 * \param pApp User context
 * \param scbIndex SCB index
 * \retval None
 */
void
Cy_CDC_UartDmaReadCompletion(void *pApp, uint8_t scbIndex)
{
    cy_stc_usb_app_ctxt_t *pAppCtxt = (cy_stc_usb_app_ctxt_t*)pApp;
    cy_stc_usbd_app_msg_t xMsg;
    uint8_t cdcIndex = CY_APP_ERROR_CODE;

    cdcIndex =  (((scbIndex == glCdcFnScbMap[0]) && (scbIndex != CY_APP_ERROR_CODE))? 0 :
                (((scbIndex == glCdcFnScbMap[1]) && (scbIndex != CY_APP_ERROR_CODE))? 1 :
                (((scbIndex == glCdcFnScbMap[2]) && (scbIndex != CY_APP_ERROR_CODE))? 2 :
                (((scbIndex == glCdcFnScbMap[3]) && (scbIndex != CY_APP_ERROR_CODE))? 3 :
                CY_APP_ERROR_CODE))));

    if(cdcIndex == CY_APP_ERROR_CODE)
    {
        DBG_APP_ERR("Cy_CDC_UartDmaReadCompletion: Invalid CDC Configuration. Check the DMA channel setup for CDC\r\n");
        return;
    }

    /* Move to the next RX buffer for the next read. */
    pAppCtxt->cdcInfo[cdcIndex]->uartRxBufIndex++;
    if (pAppCtxt->cdcInfo[cdcIndex]->uartRxBufIndex >= pAppCtxt->cdcInfo[cdcIndex]->dmaRxBufferCount) {
        pAppCtxt->cdcInfo[cdcIndex]->uartRxBufIndex = 0;
    }
    pAppCtxt->cdcInfo[cdcIndex]->uartRxFreeBufCount--;
    if (pAppCtxt->cdcInfo[cdcIndex]->uartRxFreeBufCount != 0) {
        /* Queue read from the UART RX FIFO into the next DMA buffer. */
        Cy_CDC_QueueUartRead(pAppCtxt, USB_HS_BULK_XFER_SIZE, glCdcFnScbMap[cdcIndex], 0, false);
    }

    xMsg.type = CY_USB_UART_MSG_READ_COMPLETE;
    xMsg.data[0] = cdcIndex;
    Cy_CDC_PostVcomMsgFromISR(pAppCtxt, &xMsg, cdcIndex, __func__);
    return;
}  /* end of function */

/**
 * \name Cy_CDC_USBDmaReadCompletion
 * \brief Handler for DMA transfer completion on OUT endpoint
 * \param pApp User context
 * \param cdcIndex CDC index
 * \retval None
 */
void Cy_CDC_USBDmaReadCompletion (void *pApp, uint8_t cdcIndex, uint16_t dataLength)
{
    cy_stc_usbd_app_msg_t xMsg;
    cy_stc_usb_app_ctxt_t *pAppCtxt;

    pAppCtxt = (cy_stc_usb_app_ctxt_t*)pApp;

    xMsg.type = CY_USB_VCOM_DEVICE_MSG_READ_COMPLETE;
    xMsg.data[0] = cdcIndex;
    xMsg.data[1] = dataLength;
    Cy_CDC_PostVcomMsgFromISR(pAppCtxt, &xMsg, cdcIndex, __func__);
    return;
}   /* end of function */

/**
 * \name Cy_CDC_USBDmaWriteCompletion
 * \brief Handler for DMA transfer completion on IN endpoint
 * \param pApp User context
 * \param cdcIndex CDC index
 * \retval None
 */
void Cy_CDC_USBDmaWriteCompletion (void *pApp, uint8_t cdcIndex)
{
    cy_stc_usbd_app_msg_t xMsg;
    cy_stc_usb_app_ctxt_t *pAppCtxt;

    pAppCtxt = (cy_stc_usb_app_ctxt_t*)pApp;

    if(CDC_DATA_SOURCE_SINK(pAppCtxt->cfgTablePtr,pAppCtxt->cdcInfo[cdcIndex]->cdcFuncIndex) != CY_CDC_CPU)
    {
        pAppCtxt->cdcInfo[cdcIndex]->usbInXferPending = false;

        xMsg.type = CY_USB_VCOM_DEVICE_MSG_WRITE_COMPLETE;
        xMsg.data[0] = cdcIndex;
        Cy_CDC_PostVcomMsgFromISR(pAppCtxt, &xMsg, cdcIndex, __func__);
    }
    return;
}   /* end of function */

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
        uint16_t dataSize, uint8_t scbIndex, uint32_t pY_index, bool isY_indexStuck)
{
    cy_stc_dmac_descriptor_config_t desc_cfg;
    cy_stc_dmac_channel_config_t    chan_cfg;
    cy_en_dmac_status_t stat;

    /* First, check the Y-index to see if some data has already been loaded through 2D DMA transfer */
    uint32_t Y_index_UartDmacRead = pY_index;

    if (scbIndex==SCB4_DMAC){
        /* If the DMA channel is already enabled, disable it. */
        Cy_DMAC_Channel_Disable (DMAC, SCB4_RX_DMAC_CHANNEL);
        
        if(dataSize == USB_HS_BULK_XFER_SIZE){ /* Queue a 512 B read with a 2D transfer */
            desc_cfg.retrigger          = CY_DMAC_RETRIG_16CYC;
            desc_cfg.interruptType      = CY_DMAC_DESCR;
            desc_cfg.triggerOutType     = CY_DMAC_DESCR;
            desc_cfg.channelState       = CY_DMAC_CHANNEL_DISABLED;
            desc_cfg.triggerInType      = CY_DMAC_X_LOOP;
            desc_cfg.dataPrefetch       = false;
            desc_cfg.dataSize           = CY_DMAC_BYTE;
            desc_cfg.srcTransferSize    = CY_DMAC_TRANSFER_SIZE_WORD;
            desc_cfg.dstTransferSize    = CY_DMAC_TRANSFER_SIZE_DATA;
            desc_cfg.descriptorType     = CY_DMAC_2D_TRANSFER;
            desc_cfg.srcAddress         = (void *)(&(SCB4->RX_FIFO_RD));
            desc_cfg.dstAddress         = (void *)pHbSramBuffer;
            desc_cfg.srcXincrement      = 0u;
            desc_cfg.dstXincrement      = 1u;
            desc_cfg.xCount             = CDC_VCOM_UART_MAX_RD_SIZE;
            desc_cfg.srcYincrement      = 0u;
            desc_cfg.dstYincrement      = CDC_VCOM_UART_MAX_RD_SIZE;
            desc_cfg.yCount             = (dataSize / CDC_VCOM_UART_MAX_RD_SIZE);
            desc_cfg.nextDescriptor     = NULL;
        } else if(dataSize < USB_HS_BULK_XFER_SIZE){
            if(Y_index_UartDmacRead>0 && !isY_indexStuck){
                /* This condition is reached when some data has already been transferred through 2D transfer
                 * but there is more data to be read from the FIFO
                 *
                 * Here, we queue a 1D transfer, from the offset of where data has already been transferred to
                 */
                desc_cfg.retrigger          = CY_DMAC_RETRIG_4CYC;
                desc_cfg.interruptType      = CY_DMAC_DESCR;
                desc_cfg.triggerOutType     = CY_DMAC_DESCR;
                desc_cfg.channelState       = CY_DMAC_CHANNEL_DISABLED;
                desc_cfg.triggerInType      = CY_DMAC_DESCR_CHAIN;
                desc_cfg.dataPrefetch       = false;
                desc_cfg.dataSize           = CY_DMAC_BYTE;
                desc_cfg.srcTransferSize    = CY_DMAC_TRANSFER_SIZE_WORD;
                desc_cfg.dstTransferSize    = CY_DMAC_TRANSFER_SIZE_DATA;
                desc_cfg.descriptorType     = CY_DMAC_1D_TRANSFER;
                desc_cfg.srcAddress         = (void *)(&(SCB4->RX_FIFO_RD));
                desc_cfg.dstAddress         = (void *)(pHbSramBuffer+(CDC_VCOM_UART_MAX_RD_SIZE*(Y_index_UartDmacRead)));
                desc_cfg.srcXincrement      = 0u;
                desc_cfg.dstXincrement      = 1u;
                desc_cfg.xCount             = dataSize;
                desc_cfg.srcYincrement      = 0;
                desc_cfg.dstYincrement      = 0;
                desc_cfg.yCount             = 1;
                desc_cfg.nextDescriptor     = NULL;
            } else if(Y_index_UartDmacRead>0 && isY_indexStuck){
                /* This condition is reached when some data has already been transferred through 2D transfer
                 * and there is NO more data to be read from the FIFO
                 *
                 * Here, since we already have the data, we simply signal read completion
                 */
                Cy_CDC_UartDmaReadCompletion((void *)pAppCtxt, scbIndex);
                return;
            } else {
                /* This condition is reached when NO data has already been transferred through 2D transfer
                 * and there is some data to be read from the FIFO
                 *
                 * Here, we queue a 1D transfer into the buffer
                 */
                desc_cfg.retrigger          = CY_DMAC_RETRIG_4CYC;
                desc_cfg.interruptType      = CY_DMAC_DESCR;
                desc_cfg.triggerOutType     = CY_DMAC_DESCR;
                desc_cfg.channelState       = CY_DMAC_CHANNEL_DISABLED;
                desc_cfg.triggerInType      = CY_DMAC_DESCR_CHAIN;
                desc_cfg.dataPrefetch       = false;
                desc_cfg.dataSize           = CY_DMAC_BYTE;
                desc_cfg.srcTransferSize    = CY_DMAC_TRANSFER_SIZE_WORD;
                desc_cfg.dstTransferSize    = CY_DMAC_TRANSFER_SIZE_DATA;
                desc_cfg.descriptorType     = CY_DMAC_1D_TRANSFER;
                desc_cfg.srcAddress         = (void *)(&(SCB4->RX_FIFO_RD));
                desc_cfg.dstAddress         = (void *)pHbSramBuffer;
                desc_cfg.srcXincrement      = 0u;
                desc_cfg.dstXincrement      = 1u;
                desc_cfg.xCount             = dataSize;
                desc_cfg.srcYincrement      = 0;
                desc_cfg.dstYincrement      = 0;
                desc_cfg.yCount             = 1;
                desc_cfg.nextDescriptor     = NULL;
            }
        }

        stat = Cy_DMAC_Descriptor_Init (&glCdcUartRdDmacDesc1, &desc_cfg);
        ASSERT_NON_BLOCK(stat == CY_DMAC_SUCCESS, stat);

        chan_cfg.descriptor  = &glCdcUartRdDmacDesc1;
        chan_cfg.priority    = 0;
        chan_cfg.enable      = false;
        chan_cfg.bufferable  = false;

        stat = Cy_DMAC_Channel_Init (DMAC, SCB4_RX_DMAC_CHANNEL, &chan_cfg);
        ASSERT_NON_BLOCK(stat == CY_DMAC_SUCCESS, stat);

        Cy_DMAC_Channel_SetInterruptMask(DMAC,SCB4_RX_DMAC_CHANNEL,CY_DMA_INTR_MASK);
        Cy_DMAC_Channel_Enable (DMAC, SCB4_RX_DMAC_CHANNEL);

    } else if(scbIndex==SCB6_DMAC){
        /* If the DMA channel is already enabled, disable it. */
        Cy_DMAC_Channel_Disable(DMAC, SCB6_RX_DMAC_CHANNEL);
      
        if(dataSize == USB_HS_BULK_XFER_SIZE){ /* Queue a 512 B read with a 2D transfer */
            desc_cfg.retrigger          = CY_DMAC_RETRIG_16CYC;
            desc_cfg.interruptType      = CY_DMAC_DESCR;
            desc_cfg.triggerOutType     = CY_DMAC_DESCR;
            desc_cfg.channelState       = CY_DMAC_CHANNEL_DISABLED;
            desc_cfg.triggerInType      = CY_DMAC_X_LOOP;
            desc_cfg.dataPrefetch       = false;
            desc_cfg.dataSize           = CY_DMAC_BYTE;
            desc_cfg.srcTransferSize    = CY_DMAC_TRANSFER_SIZE_WORD;
            desc_cfg.dstTransferSize    = CY_DMAC_TRANSFER_SIZE_DATA;
            desc_cfg.descriptorType     = CY_DMAC_2D_TRANSFER;
            desc_cfg.srcAddress         = (void *)(&(SCB6->RX_FIFO_RD));
            desc_cfg.dstAddress         = (void *)pHbSramBuffer;
            desc_cfg.srcXincrement      = 0u;
            desc_cfg.dstXincrement      = 1u;
            desc_cfg.xCount             = CDC_VCOM_UART_MAX_RD_SIZE;
            desc_cfg.srcYincrement      = 0u;
            desc_cfg.dstYincrement      = CDC_VCOM_UART_MAX_RD_SIZE;
            desc_cfg.yCount             = (dataSize / CDC_VCOM_UART_MAX_RD_SIZE);
            desc_cfg.nextDescriptor     = NULL;
        } else if(dataSize < USB_HS_BULK_XFER_SIZE){
            if(Y_index_UartDmacRead>0 && !isY_indexStuck){
                /* This condition is reached when some data has already been transferred through 2D transfer
                 * but there is more data to be read from the FIFO
                 *
                 * Here, we queue a 1D transfer, from the offset of where data has already been transferred to
                 */
                desc_cfg.retrigger          = CY_DMAC_RETRIG_4CYC;
                desc_cfg.interruptType      = CY_DMAC_DESCR;
                desc_cfg.triggerOutType     = CY_DMAC_DESCR;
                desc_cfg.channelState       = CY_DMAC_CHANNEL_DISABLED;
                desc_cfg.triggerInType      = CY_DMAC_DESCR_CHAIN;
                desc_cfg.dataPrefetch       = false;
                desc_cfg.dataSize           = CY_DMAC_BYTE;
                desc_cfg.srcTransferSize    = CY_DMAC_TRANSFER_SIZE_WORD;
                desc_cfg.dstTransferSize    = CY_DMAC_TRANSFER_SIZE_DATA;
                desc_cfg.descriptorType     = CY_DMAC_1D_TRANSFER;
                desc_cfg.srcAddress         = (void *)(&(SCB6->RX_FIFO_RD));
                desc_cfg.dstAddress         = (void *)(pHbSramBuffer+(CDC_VCOM_UART_MAX_RD_SIZE*(Y_index_UartDmacRead)));
                desc_cfg.srcXincrement      = 0u;
                desc_cfg.dstXincrement      = 1u;
                desc_cfg.xCount             = dataSize;
                desc_cfg.srcYincrement      = 0;
                desc_cfg.dstYincrement      = 0;
                desc_cfg.yCount             = 1;
                desc_cfg.nextDescriptor     = NULL;
            } else if(Y_index_UartDmacRead>0 && isY_indexStuck){
                /* This condition is reached when some data has already been transferred through 2D transfer
                 * and there is NO more data to be read from the FIFO
                 *
                 * Here, since we already have the data, we simply signal read completion
                 */
                Cy_CDC_UartDmaReadCompletion((void *)pAppCtxt, scbIndex);
                return;
            } else {
                /* This condition is reached when NO data has already been transferred through 2D transfer
                 * and there is some data to be read from the FIFO
                 *
                 * Here, we queue a 1D transfer into the buffer
                 */
                desc_cfg.retrigger          = CY_DMAC_RETRIG_4CYC;
                desc_cfg.interruptType      = CY_DMAC_DESCR;
                desc_cfg.triggerOutType     = CY_DMAC_DESCR;
                desc_cfg.channelState       = CY_DMAC_CHANNEL_DISABLED;
                desc_cfg.triggerInType      = CY_DMAC_DESCR_CHAIN;
                desc_cfg.dataPrefetch       = false;
                desc_cfg.dataSize           = CY_DMAC_BYTE;
                desc_cfg.srcTransferSize    = CY_DMAC_TRANSFER_SIZE_WORD;
                desc_cfg.dstTransferSize    = CY_DMAC_TRANSFER_SIZE_DATA;
                desc_cfg.descriptorType     = CY_DMAC_1D_TRANSFER;
                desc_cfg.srcAddress         = (void *)(&(SCB6->RX_FIFO_RD));
                desc_cfg.dstAddress         = (void *)pHbSramBuffer;
                desc_cfg.srcXincrement      = 0u;
                desc_cfg.dstXincrement      = 1u;
                desc_cfg.xCount             = dataSize;
                desc_cfg.srcYincrement      = 0;
                desc_cfg.dstYincrement      = 0;
                desc_cfg.yCount             = 1;
                desc_cfg.nextDescriptor     = NULL;
            }

        }

        stat = Cy_DMAC_Descriptor_Init (&glCdcUartRdDmacDesc2, &desc_cfg);
        ASSERT_NON_BLOCK(stat == CY_DMAC_SUCCESS, stat);

        chan_cfg.descriptor  = &glCdcUartRdDmacDesc2;
        chan_cfg.priority    = 0;
        chan_cfg.enable      = false;
        chan_cfg.bufferable  = false;

        stat = Cy_DMAC_Channel_Init (DMAC, SCB6_RX_DMAC_CHANNEL, &chan_cfg);
        ASSERT_NON_BLOCK(stat == CY_DMAC_SUCCESS, stat);

        Cy_DMAC_Channel_SetInterruptMask(DMAC,SCB6_RX_DMAC_CHANNEL,CY_DMAC_INTR_MASK);
        Cy_DMAC_Channel_Enable (DMAC, SCB6_RX_DMAC_CHANNEL);
    }
    
    return;
}  /* end of function */

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
        uint16_t dataSize, uint8_t scbIndex, uint32_t pY_index, bool isY_indexStuck)
{
    cy_stc_dma_descriptor_config_t desc_cfg;
    cy_stc_dma_channel_config_t    chan_cfg;
    cy_en_dma_status_t stat;

    uint32_t Y_index_UartDwRead = pY_index;

    /* If the DMA channel is already enabled, disable it. */
    Cy_DMA_Channel_Disable (DW0, glCdcUartRdChNum[scbIndex]);
    
    if(dataSize == USB_HS_BULK_XFER_SIZE){
        desc_cfg.retrigger          = CY_DMA_RETRIG_16CYC;
        desc_cfg.interruptType      = CY_DMA_DESCR;
        desc_cfg.triggerOutType     = CY_DMA_DESCR;
        desc_cfg.channelState       = CY_DMA_CHANNEL_DISABLED;
        desc_cfg.triggerInType      = CY_DMA_X_LOOP;
        desc_cfg.dataSize           = CY_DMA_BYTE;
        desc_cfg.srcTransferSize    = CY_DMA_TRANSFER_SIZE_WORD;
        desc_cfg.dstTransferSize    = CY_DMA_TRANSFER_SIZE_DATA;
        desc_cfg.descriptorType     = CY_DMA_2D_TRANSFER;
        desc_cfg.srcAddress         = (void *)(&(scbMapping[scbIndex+1])->RX_FIFO_RD);
        desc_cfg.dstAddress         = (void *)pHbSramBuffer;
        desc_cfg.srcXincrement      = 0u;
        desc_cfg.dstXincrement      = 1u;
        desc_cfg.xCount             = CDC_VCOM_UART_MAX_RD_SIZE;
        desc_cfg.srcYincrement      = 0u;
        desc_cfg.dstYincrement      = CDC_VCOM_UART_MAX_RD_SIZE;
        desc_cfg.yCount             = (dataSize / CDC_VCOM_UART_MAX_RD_SIZE);
        desc_cfg.nextDescriptor     = NULL;
    } else if(dataSize < USB_HS_BULK_XFER_SIZE) {
        if(Y_index_UartDwRead>0 && !isY_indexStuck){
            desc_cfg.retrigger          = CY_DMA_RETRIG_4CYC;
            desc_cfg.interruptType      = CY_DMA_DESCR;
            desc_cfg.triggerOutType     = CY_DMA_DESCR;
            desc_cfg.channelState       = CY_DMA_CHANNEL_DISABLED;
            desc_cfg.triggerInType      = CY_DMA_DESCR_CHAIN;
            desc_cfg.dataSize           = CY_DMA_BYTE;
            desc_cfg.srcTransferSize    = CY_DMA_TRANSFER_SIZE_WORD;
            desc_cfg.dstTransferSize    = CY_DMA_TRANSFER_SIZE_DATA;
            desc_cfg.descriptorType     = CY_DMA_1D_TRANSFER;
            desc_cfg.srcAddress         = (void *)(&(scbMapping[scbIndex+1])->RX_FIFO_RD);
            desc_cfg.dstAddress         = (void *)(pHbSramBuffer+(CDC_VCOM_UART_MAX_RD_SIZE*(Y_index_UartDwRead)));
            desc_cfg.srcXincrement      = 0u;
            desc_cfg.dstXincrement      = 1u;
            desc_cfg.xCount             = dataSize;
            desc_cfg.srcYincrement      = 0;
            desc_cfg.dstYincrement      = 0;
            desc_cfg.yCount             = 1;
            desc_cfg.nextDescriptor     = NULL;
        } else if (Y_index_UartDwRead>0 && isY_indexStuck){
            Cy_CDC_UartDmaReadCompletion((void *)pAppCtxt, scbIndex);
            return;
        } else {
            desc_cfg.retrigger          = CY_DMA_RETRIG_4CYC;
            desc_cfg.interruptType      = CY_DMA_DESCR;
            desc_cfg.triggerOutType     = CY_DMA_DESCR;
            desc_cfg.channelState       = CY_DMA_CHANNEL_DISABLED;
            desc_cfg.triggerInType      = CY_DMA_DESCR_CHAIN;
            desc_cfg.dataSize           = CY_DMA_BYTE;
            desc_cfg.srcTransferSize    = CY_DMA_TRANSFER_SIZE_WORD;
            desc_cfg.dstTransferSize    = CY_DMA_TRANSFER_SIZE_DATA;
            desc_cfg.descriptorType     = CY_DMA_1D_TRANSFER;
            desc_cfg.srcAddress         = (void *)(&(scbMapping[scbIndex+1])->RX_FIFO_RD);
            desc_cfg.dstAddress         = (void *)pHbSramBuffer;
            desc_cfg.srcXincrement      = 0u;
            desc_cfg.dstXincrement      = 1u;
            desc_cfg.xCount             = dataSize;
            desc_cfg.srcYincrement      = 0;
            desc_cfg.dstYincrement      = 0;
            desc_cfg.yCount             = 1;
            desc_cfg.nextDescriptor     = NULL;
        }
    }

    stat = Cy_DMA_Descriptor_Init (&glCdcUartRdDmaDesc[scbIndex], &desc_cfg);
    ASSERT_NON_BLOCK(stat == CY_DMA_SUCCESS, stat);

    chan_cfg.descriptor  = &glCdcUartRdDmaDesc[scbIndex];
    chan_cfg.preemptable = false;
    chan_cfg.priority    = 0;
    chan_cfg.enable      = false;
    chan_cfg.bufferable  = false;

    stat = Cy_DMA_Channel_Init (DW0, glCdcUartRdChNum[scbIndex], &chan_cfg);
    ASSERT_NON_BLOCK(stat == CY_DMA_SUCCESS, stat);

    Cy_DMA_Channel_SetInterruptMask(DW0,glCdcUartRdChNum[scbIndex],CY_DMA_INTR_MASK);
    Cy_DMA_Channel_Enable (DW0, glCdcUartRdChNum[scbIndex]);
    
    return;
}  /* end of function */

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
                    uint8_t *pBuffer, uint16_t dataSize)
{
    cy_stc_app_endp_dma_set_t *dmaset_p;
    

    /* Null pointer checks. */
    if ((pAppCtxt == NULL) || (pAppCtxt->pUsbdCtxt == NULL) ||
        (pAppCtxt->pCpuDw0Base == NULL) || (pBuffer == NULL))
    {
        DBG_APP_ERR("QueueRead Err0\r\n");
        return;
    }
    /*
     * Verify that the selected endpoint is valid and
     * dataSize is non-zero.
     */
    dmaset_p = &(pAppCtxt->endpOutDma[endpNumber]);
    if ((dmaset_p->valid == 0) || (dataSize == 0))
    {
        DBG_APP_ERR("QueueRead Err1\r\n");
        return;
    }

    if (Cy_USBHS_App_QueueRead(dmaset_p, pBuffer, dataSize))
    {
        /* Set transfer size for the endpoint and clear NAK status to allow data to be received. */
        if (dataSize < dmaset_p->maxPktSize) {
            /* We are trying to read out data that has already been received. Force EP NAK. */
            Cy_USB_USBD_EndpSetClearNakNrdy(pAppCtxt->pUsbdCtxt, endpNumber, CY_USB_ENDP_DIR_OUT, true);
        } else {
            /* Set the NAK bit so that the IP can see the bit transition from 1->0 after XFER_CNT is set. */
            Cy_USB_USBD_EndpSetClearNakNrdy(pAppCtxt->pUsbdCtxt, endpNumber, CY_USB_ENDP_DIR_OUT, true);
            Cy_USBD_UpdateXferCount(pAppCtxt->pUsbdCtxt, endpNumber, CY_USB_ENDP_DIR_OUT, dataSize);
            Cy_USB_USBD_EndpSetClearNakNrdy(pAppCtxt->pUsbdCtxt, endpNumber, CY_USB_ENDP_DIR_OUT, false);
        }
    }
} /* end of function */

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
                         cy_stc_hbdma_buff_status_t* pbufStat, void *userCtx)
{
    cy_stc_usb_app_ctxt_t *pAppCtxt = (cy_stc_usb_app_ctxt_t *)userCtx;
    uint8_t cdcIndex = CY_APP_ERROR_CODE;
    uint8_t i =0;

    switch (type) {
    case CY_HBDMA_CB_CONS_EVENT:
        /*
         * This event means DATA is consumed and buffer is
         * available so next set of transfer can be initiated
         * from UART->HbwSRAM->USB
         */
        for(i =0 ; i< pAppCtxt->numCDCFunctions;i++)
        {
            if(pAppCtxt->pUartToUsbChn[i] == pHandle)
            {
                cdcIndex = i;
                break;
            }
        }

        if(cdcIndex == CY_APP_ERROR_CODE)
        {
            DBG_APP_ERR("Cy_CDC_HbwDmaCallback: Invalid CDC Configuration. Check the DMA channel setup for CDC\r\n");
            return;
        }

        /* Data has been consumed on USB side and buffer is free now. Send DMAWriteCompletion event. */
        Cy_CDC_USBDmaWriteCompletion((void *)pAppCtxt, cdcIndex);
        break;

    case CY_HBDMA_CB_PROD_EVENT:
        for(i = 0 ; i< pAppCtxt->numCDCFunctions;i++)
        {
            if(pAppCtxt->pUsbToUartChn[i] == pHandle)
            {
                cdcIndex = i;
                break;
            }
        }

        if(cdcIndex == CY_APP_ERROR_CODE)
        {
            DBG_APP_ERR("Cy_CDC_HbwDmaCallback: Invalid CDC Configuration. Check the DMA channel setup for CDC\r\n");
            return;
        }

        /* Generate CY_USB_VCOM_DEVICE_MSG_READ_COMPLETE event after updating the usbOutPktLen field. */
        if (pbufStat->count != 0) {
            pAppCtxt->cdcInfo[cdcIndex]->usbOutPktLen = pbufStat->count;
            Cy_CDC_USBDmaReadCompletion((void *)pAppCtxt, cdcIndex, pbufStat->count);
        }
        break;

    default:
        break;
    }
    return;
}

/**
 * \name Cy_CDC_ConfigureUartSCBClock
 * \brief Configure the clock applied to SCB.
 * \param scbIndex SCB Index
 * \param divider Clock divider
 * \retval None
 */
void Cy_CDC_ConfigureUartSCBClock(uint8_t scbIndex, uint16_t divider)
{
    uint8_t dividerNum = 0;

    dividerNum = (scbIndex == 0)? 4:5;
    if (Cy_SysClk_ClkPeriGetFrequency() == 75000000UL) {
        /* Use PERI 16-bit divider #3 to divide 75 MHz HFCLK by 9 to get 8.333 MHz. */
        Cy_SysClk_PeriphSetDivider (CY_SYSCLK_DIV_16_BIT, dividerNum, divider);
    } else {
        /* Use PERI 16-bit divider #3 to divide 50 MHz HFCLK by 6 to get 8.333 MHz. */
        Cy_SysClk_PeriphSetDivider (CY_SYSCLK_DIV_16_BIT, dividerNum, 4);
    }

    Cy_SysClk_PeriphEnableDivider(CY_SYSCLK_DIV_16_BIT, dividerNum);
    Cy_SysLib_DelayUs(10);

    /* Connect the PERI clock to the SCB input. */
    Cy_SysClk_PeriphAssignDivider(PCLK_SCB0_CLOCK + scbIndex, CY_SYSCLK_DIV_16_BIT, dividerNum);
}  /* end of function. */


/**
 * \name Cy_CDC_UartSCBInit
 * \brief Initialize SCB1 as UART.
 * \param scbIndex SCB Index
 * \retval None
 */
void Cy_CDC_UartSCBInit(uint8_t scbIndex)
{
    cy_stc_gpio_pin_config_t pinCfg;
    cy_en_trigmux_status_t trigStatus = CY_TRIGMUX_BAD_PARAM;

    /* Configure the clock input to the SCB block based on default
     * UART baud rate settings.
     */
    Cy_CDC_ConfigureUartSCBClock(scbIndex, 9);  /* 921600 baud */

    memset((void *)&pinCfg, 0, sizeof(pinCfg));

    switch(scbIndex)
    {
        case 0:
            pinCfg.driveMode = CY_GPIO_DM_STRONG_IN_OFF;
            pinCfg.hsiom     = P10_1_SCB0_UART_TX;
            Cy_GPIO_Pin_Init(P10_1_PORT, P10_1_PIN, &pinCfg);
            pinCfg.hsiom     = P10_0_SCB0_UART_RX;
            pinCfg.driveMode = CY_GPIO_DM_HIGHZ;
            Cy_GPIO_Pin_Init(P10_0_PORT, P10_0_PIN, &pinCfg);
        break;

        case 1:
            pinCfg.driveMode = CY_GPIO_DM_STRONG_IN_OFF;
            pinCfg.hsiom     = P7_7_SCB1_UART_TX;
            Cy_GPIO_Pin_Init(P7_7_PORT, P7_7_PIN, &pinCfg);
            pinCfg.hsiom     = P7_6_SCB1_UART_RX;
            pinCfg.driveMode = CY_GPIO_DM_HIGHZ;
            Cy_GPIO_Pin_Init(P7_6_PORT, P7_6_PIN, &pinCfg);
        break;

        case 4:
            pinCfg.driveMode = CY_GPIO_DM_STRONG_IN_OFF;
            pinCfg.hsiom     = P11_0_SCB4_UART_TX;
            Cy_GPIO_Pin_Init(P11_0_PORT, P11_0_PIN, &pinCfg);
            pinCfg.hsiom     = P11_1_SCB4_UART_RX;
            pinCfg.driveMode = CY_GPIO_DM_HIGHZ;
            Cy_GPIO_Pin_Init(P11_1_PORT, P11_1_PIN, &pinCfg);
        break;

        case 6:
            pinCfg.driveMode = CY_GPIO_DM_STRONG_IN_OFF;
            pinCfg.hsiom     = P6_1_SCB6_UART_TX;
            Cy_GPIO_Pin_Init(P6_1_PORT, P6_1_PIN, &pinCfg);
            pinCfg.hsiom     = P6_0_SCB6_UART_RX;
            pinCfg.driveMode = CY_GPIO_DM_HIGHZ;
            Cy_GPIO_Pin_Init(P6_0_PORT, P6_0_PIN, &pinCfg);
        break;

        default:
            DBG_APP_ERR("Invalid UART SCB \n\r");
            return ;
        break;
    }

    if(scbIndex == 0 || scbIndex == 1)
    {
        /* Configure connection from SCB TX Trigger to DW0 Channel 18 Input Trigger. */
        Cy_TrigMux_Select(TRIG_OUT_1TO1_0_SCB0_TX_TO_PDMA0_TR_IN16 + (scbIndex * 2), false, TRIGGER_TYPE_LEVEL);

        /* Configure connection from SCB RX Trigger to DW0 Channel 19 Input Trigger. */
        Cy_TrigMux_Select(TRIG_OUT_1TO1_0_SCB0_RX_TO_PDMA0_TR_IN17 + (scbIndex * 2), false, TRIGGER_TYPE_LEVEL);

    }
    else if (scbIndex == SCB4_DMAC)
    {
        /* Configure connection from SCB4 TX Trigger to DMAC Channel #2 Input Trigger. */
        trigStatus = Cy_TrigMux_Connect(TRIG_IN_MUX_5_SCB4_TX_REQ, 
            TRIG_OUT_MUX_5_MDMA_TR_IN2,
            false, TRIGGER_TYPE_LEVEL); 
        ASSERT_NON_BLOCK(CY_TRIGMUX_SUCCESS == trigStatus, trigStatus);

        /* Configure connection from SCB4 RX Trigger to DMAC Channel#3 Input Trigger. */
        trigStatus = Cy_TrigMux_Connect(TRIG_IN_MUX_5_SCB4_RX_REQ,
            TRIG_OUT_MUX_5_MDMA_TR_IN3,
            false, TRIGGER_TYPE_LEVEL);
        ASSERT_NON_BLOCK(CY_TRIGMUX_SUCCESS == trigStatus, trigStatus);

    }
    else if (scbIndex == SCB6_DMAC)
    {
        /* Configure connection from SCB6 TX Trigger to DMAC Channel #4 Input Trigger. */
        trigStatus = Cy_TrigMux_Connect(TRIG_IN_MUX_5_SCB6_TX_REQ, 
            TRIG_OUT_MUX_5_MDMA_TR_IN4,
            false, TRIGGER_TYPE_LEVEL); 
        ASSERT_NON_BLOCK(CY_TRIGMUX_SUCCESS == trigStatus, trigStatus);

        /* Configure connection from SCB6 RX Trigger to DMAC Channel #5 Input Trigger. */
        trigStatus = Cy_TrigMux_Connect(TRIG_IN_MUX_5_SCB6_RX_REQ,
            TRIG_OUT_MUX_5_MDMA_TR_IN5,
            false, TRIGGER_TYPE_LEVEL);
        ASSERT_NON_BLOCK(CY_TRIGMUX_SUCCESS == trigStatus, trigStatus);
    }

} /* end of function. */
