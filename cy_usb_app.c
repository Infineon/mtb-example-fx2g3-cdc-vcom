/*******************************************************************************
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

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "cy_pdl.h"
#include "cy_device.h"
#include "cy_usb_common.h"
#include "cy_usbhs_cal_drv.h"
#include "cy_hbdma.h"
#include "cy_hbdma_mgr.h"
#include "cy_usb_usbd.h"
#include "cy_usbhs_dw_wrapper.h"
#include "cy_usb_app.h"
#include "cy_debug.h"
#include "cy_dscr.h"
#include "COMPONENT_CDC/cdc.h"

/* Debug configuration */
static volatile uint32_t LogDataBuffer_test[LOGBUF_RAM_SZ / 4U];  /* RAM buffer used to hold debug log data. */

/* Debug Interface Map */
static uint8_t uartInterfaceMap[8] =
{
    CY_DEBUG_INTFCE_USBFS_CDC,
    CY_DEBUG_INTFCE_UART_SCB0,                          
    CY_DEBUG_INTFCE_UART_SCB1,                      
    CY_DEBUG_INTFCE_USBFS_CDC,         
    CY_DEBUG_INTFCE_USB_CDC,
    CY_DEBUG_INTFCE_UART_SCB4,
    0b00000000,                 /* Invalid entry */
    CY_DEBUG_INTFCE_UART_SCB6   
};

uint16_t glUsbCfgDscrSizeHs = 0;
uint16_t glUsbCfgDscrSizeFs = 0;

/* Debug Recv Buffer */
uint8_t dbgUartRcvBuffer[DBG_UART_RECV_BUFFER] = {0};

extern cy_stc_cdc_info_t glCDCInfo[MAX_CDC_FUNCTIONS];

#if DEBUG_INFRA_EN
/* Debug log related initialization */
TaskHandle_t printLogTaskHandle;

void Cy_PrintTaskHandler(void* pTaskParam)
{
    while (1)
    {
        /* Print any pending logs to the output console. */
        Cy_Debug_PrintLog();

        /* Put the thread to sleep for 5 ms */
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}
#endif /* DEBUG_INFRA_EN */

/**
 * \name Cy_DebugInit
 * \brief Function to initialize debug logs
 * \param appCtxt User context
 * \retval None
 */
void Cy_DebugInit(cy_stc_usb_app_ctxt_t *appCtxt)
{
    cy_stc_debug_config_t    dbgCfg;
    cy_stc_usb_dev_config_t *cfgTablePtr = appCtxt->cfgTablePtr;
    
    if(uartInterfaceMap[USB_FW_DEBUG_INTERFACE(cfgTablePtr)] == CY_DEBUG_INTFCE_UART_SCB0 )
        InitUart(CY_DEBUG_INTFCE_UART_SCB0);
    else if(uartInterfaceMap[USB_FW_DEBUG_INTERFACE(cfgTablePtr)] == CY_DEBUG_INTFCE_UART_SCB1)
        InitUart(CY_DEBUG_INTFCE_UART_SCB1);
    else if (uartInterfaceMap[USB_FW_DEBUG_INTERFACE(cfgTablePtr)] == CY_DEBUG_INTFCE_UART_SCB4)
        InitUart(CY_DEBUG_INTFCE_UART_SCB4);

    if(USB_FW_DEBUG_INTERFACE(cfgTablePtr) != CY_DBGLOG_TGT_CDC )
    {
        dbgCfg.pBuffer   = (uint8_t *)LogDataBuffer_test;
        dbgCfg.traceLvl  = USB_LOG_LEVEL(cfgTablePtr);
        dbgCfg.bufSize   = LOGBUF_RAM_SZ;
        dbgCfg.dbgIntfce = uartInterfaceMap[(uint8_t)USB_FW_DEBUG_INTERFACE(cfgTablePtr)]; /* CY_DEBUG_INTFCE_UART_SCB0; */
        dbgCfg.printNow  = true; /* Delay output of messages till logging task runs. */
        Cy_Debug_LogInit(&dbgCfg);
    }
}

/**
 * \name Cy_USB_RecvEndp0Data
 * \brief Wrapper function for non blocking Receive EP0 Data
 * \param pUsbdCtxt USBD Context pointer
 * \param pBuffer Buffer pointer
 * \param bufferSize Buffer size
 * \retval cy_en_usbd_ret_code_t Status
 */
cy_en_usbd_ret_code_t Cy_USB_RecvEndp0Data(cy_stc_usb_usbd_ctxt_t *pUsbdCtxt, uint8_t *pBuffer, uint32_t bufferSize)
{
    uint16_t loopCnt = 250u;
    cy_en_usbd_ret_code_t retStatus = CY_USBD_STATUS_SUCCESS;

    retStatus = Cy_USB_USBD_RecvEndp0Data(pUsbdCtxt, (uint8_t *)pBuffer, bufferSize);
    if(retStatus == CY_USBD_STATUS_SUCCESS)
    {
        /* Wait until receive DMA transfer has been completed. */
        while ((!Cy_USBD_IsEp0ReceiveDone(pUsbdCtxt)) && (loopCnt--)) 
        {
            Cy_SysLib_DelayUs(10);
        }
        if (!Cy_USBD_IsEp0ReceiveDone(pUsbdCtxt)) {
            DBG_APP_ERR("SET_CUR data timed out\r\n");
            Cy_USB_USBD_RetireRecvEndp0Data(pUsbdCtxt);
            Cy_USB_USBD_EndpSetClearStall(pUsbdCtxt, 0x00, CY_USB_ENDP_DIR_IN, TRUE);
            return CY_USBD_STATUS_TIMEOUT;
        }
    }
    return retStatus;
}

/**
 * \name Cy_USB_DisableEnpDma
 * \brief Wrapper function to de-init all active USB DMA channels
 * \param pAppCtxt Pointer to application context
 * \param channelHandle Pointer to channel handler
 * \param endpNum Endpoint number
 * \param endpDirection Endpoint direction
 * \retval None
 */
static void Cy_USB_DisableEnpDma(cy_stc_usb_app_ctxt_t* pAppCtxt ,cy_stc_hbdma_channel_t *channelHandle, uint32_t endpNum, cy_en_usb_endp_dir_t endpDirection)
{

    Cy_HBDma_Channel_Disable(channelHandle);
    Cy_HBDma_Channel_Destroy(channelHandle);
}

/**
 * \name Cy_USB_AppDisableEndpDma
 * \brief This function de-inits all active USB DMA channels as part of USB disconnect process.
 * \param pAppCtxt Pointer to application context
 * \retval None
 */
void
Cy_USB_AppDisableEndpDma(cy_stc_usb_app_ctxt_t* pAppCtxt)
{
    uint8_t i;
    pAppCtxt->appInit = 0;

    /* Disable and reset the DataWire channels. */
    for (i = 1; i < CY_USB_MAX_ENDP_NUMBER; i++) {
        if (pAppCtxt->endpInDma[i].valid) {
            /* DeInit the DMA channel and disconnect the triggers. */
            Cy_USBHS_App_DisableEpDmaSet(&(pAppCtxt->endpInDma[i]));
        }

        if (pAppCtxt->endpOutDma[i].valid) {
            /* DeInit the DMA channel and disconnect the triggers. */
            Cy_USBHS_App_DisableEpDmaSet(&(pAppCtxt->endpOutDma[i]));
        }
    }

    for( i =0; i < pAppCtxt->numCDCFunctions;i++)
    {
        /* Abort any ongoing transfers from/to the UART FIFOs and then disable the SCB-UART. */
        pAppCtxt->cdcInfo[i]->blockUartReads = true;
        if(pAppCtxt->cdcDebugEnabled == 1)
        {
            Cy_Debug_LogDeInit();
            continue;
        }

        if(glCdcFnScbMap[i] != CY_APP_ERROR_CODE)
        {
            Cy_DMA_Channel_Disable(pAppCtxt->pCpuDw0Base, glCdcUartWrChNum[glCdcFnScbMap[i]]);
            Cy_CDC_ClearUartDmaInterrupt(pAppCtxt, CDC_UART_WRITE,glCdcFnScbMap[i]);
            Cy_DMA_Channel_Disable(pAppCtxt->pCpuDw0Base, glCdcUartRdChNum[glCdcFnScbMap[i]]);
            Cy_CDC_ClearUartDmaInterrupt(pAppCtxt, CDC_UART_READ,glCdcFnScbMap[i]);
            Cy_SCB_ClearRxFifo(scbMapping[(glCdcFnScbMap[i]+1)]);
            Cy_SCB_UART_DeInit(scbMapping[(glCdcFnScbMap[i]+1)]);
        }

        /* Disable and destroy the High BandWidth DMA channels. */
        if (pAppCtxt->pUsbToUartChn[i] != NULL) 
        {
            Cy_USB_DisableEnpDma(pAppCtxt ,pAppCtxt->pUsbToUartChn[i], pAppCtxt->cdcInfo[i]->vcomOutEpNum, CY_USB_ENDP_DIR_OUT);
            pAppCtxt->pUsbToUartChn[i] = NULL;
        }

        if (pAppCtxt->pUartToUsbChn[i] != NULL) 
        {
            Cy_USB_DisableEnpDma(pAppCtxt ,pAppCtxt->pUartToUsbChn[i],pAppCtxt->cdcInfo[i]->vcomInEpNum, CY_USB_ENDP_DIR_IN);
            pAppCtxt->pUartToUsbChn[i] = NULL;
        }
    }
}

/**
 * \name Cy_USB_DeviceTaskHandler
 * \brief This function handles application related event handling.
 * \param pTaskParam Task param
 * \retval None
 */
void Cy_USB_DeviceTaskHandler(void* pTaskParam)
{
    cy_stc_usb_app_ctxt_t* pAppCtxt = (cy_stc_usb_app_ctxt_t*)pTaskParam;
    cy_stc_usbd_app_msg_t queueMsg;
    BaseType_t xStatus;
    cy_stc_usb_dev_config_t *cfgTablePtr = pAppCtxt->cfgTablePtr; 
    

    DBG_APP_INFO("USB Device Thread Created (app) \r\n");

    /* If VBus is present, enable the USB connection. */
    pAppCtxt->vbusPresent = (Cy_GPIO_Read(VBUS_DETECT_GPIO_PORT, VBUS_DETECT_GPIO_PIN) == VBUS_DETECT_STATE(cfgTablePtr));
    if (pAppCtxt->vbusPresent) {
        Cy_USB_EnableUsbHSConnection(pAppCtxt);
        vTaskDelay(500);
        Cy_USBD_LpmDisable(pAppCtxt->pUsbdCtxt);
    }

    for (;;)
    {
        /*
         * Wait until some data is received from the queue.
         * Timeout after 100 ms.
         */
        xStatus = xQueueReceive(pAppCtxt->appMsgQueue, &queueMsg, 500);
        if (xStatus != pdPASS) {
            continue;
        }

        switch (queueMsg.type) {
        case CY_USB_VBUS_CHANGE_INTR:
            /* Start the debounce timer. */
            xTimerStart(pAppCtxt->vbusDebounceTimer, 0);
            break;

        case CY_USB_VBUS_CHANGE_DEBOUNCED:
            /* Check whether VBus state has changed. */
            pAppCtxt->vbusPresent = (Cy_GPIO_Read(VBUS_DETECT_GPIO_PORT, VBUS_DETECT_GPIO_PIN) == VBUS_DETECT_STATE(cfgTablePtr));

            if (pAppCtxt->vbusPresent) {
                if (!pAppCtxt->usbConnected) {
                    DBG_APP_INFO("Enabling USB connection due to VBus detect\r\n");
                    Cy_USB_EnableUsbHSConnection(pAppCtxt);
                }
                } else {
                if (pAppCtxt->usbConnected) {
                    Cy_USB_AppDisableEndpDma(pAppCtxt);
                    DBG_APP_INFO("Disabling USB connection due to VBus removal\r\n");
                    Cy_USB_DisableUsbHSConnection(pAppCtxt);
                }
            }
            break;

        case CY_USB_PRINT_LOGS:
                Cy_PrintInitLogs();
        break;

        default:
            break;
        }
    }
}

/**
 * \name Cy_USB_VbusDebounceTimerCb
 * \brief Timer used to do debounce on VBus changed interrupt notification.
 * \param xTimer Timer handle
 * \retval None
 */
void
Cy_USB_VbusDebounceTimerCb(TimerHandle_t xTimer)
{
    cy_stc_usb_app_ctxt_t* pAppCtxt = (cy_stc_usb_app_ctxt_t*)pvTimerGetTimerID(xTimer);
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    cy_stc_usbd_app_msg_t xMsg;

    DBG_APP_INFO("VbusDebounce_CB\r\n");
    if (pAppCtxt->vbusChangeIntr) {
        /* Notify the VCOM task that VBus debounce is complete. */
        xMsg.type = CY_USB_VBUS_CHANGE_DEBOUNCED;
        xQueueSendFromISR(pAppCtxt->appMsgQueue, &(xMsg), &(xHigherPriorityTaskWoken));

        /* Clear and re-enable the interrupt. */
        pAppCtxt->vbusChangeIntr = false;
        Cy_GPIO_ClearInterrupt(VBUS_DETECT_GPIO_PORT, VBUS_DETECT_GPIO_PIN);
        Cy_GPIO_SetInterruptMask(VBUS_DETECT_GPIO_PORT, VBUS_DETECT_GPIO_PIN, 1);
    }
}   /* end of function  */

/**
 * \name Cy_ApplicationInit
 * \brief Function to initialize application context
 * \param pAppCtxt Pointer to application context
 * \retval None
 */
void Cy_ApplicationInit(cy_stc_usb_app_ctxt_t* pAppCtxt)
{
    if(pAppCtxt != NULL)
    {
        uint8_t index = 0;
        pAppCtxt->numOfHbdmaChannel = 0;

        /* If any CDC functions are active, reset them*/
        for(index = 0;index <pAppCtxt->numCDCFunctions;index++)
        {
            memset(pAppCtxt->cdcInfo[index],0,sizeof(cy_stc_cdc_info_t));
        }

        pAppCtxt->numCDCFunctions = 0;

    }
}

/**
 * \name Cy_USB_AppInit
 * \brief Initializes application related data structures.
 * \details This function initializes application related data structures,
 * registers callbacks and creates queue and task for device function.
 * \param pAppCtxt Pointer to application context
 * \param pUsbdCtxt Pointer to USBD context
 * \param pCpuDmacBase Pointer to DMAC base
 * \param pCpuDw0Base Pointer to DW0 base
 * \param pCpuDw1Base Pointer to DW1 base
 * \param pHbDmaMgrCtxt Pointer to HBDMA manager
 * \retval None
 */
void Cy_USB_AppInit(cy_stc_usb_app_ctxt_t* pAppCtxt,
    cy_stc_usb_usbd_ctxt_t* pUsbdCtxt, DMAC_Type* pCpuDmacBase,
    DW_Type* pCpuDw0Base, DW_Type* pCpuDw1Base,
    cy_stc_hbdma_mgr_context_t* pHbDmaMgrCtxt)
{
    uint32_t index;
    BaseType_t status = pdFALSE;
    cy_stc_app_endp_dma_set_t* pEndpInDma;
    cy_stc_app_endp_dma_set_t* pEndpOutDma;


    pAppCtxt->devState = CY_USB_DEVICE_STATE_DISABLE;
    pAppCtxt->prevDevState = CY_USB_DEVICE_STATE_DISABLE;
    pAppCtxt->devSpeed = CY_USBD_USB_DEV_FS;
    pAppCtxt->devAddr = 0x00;
    pAppCtxt->activeCfgNum = 0x00;
    pAppCtxt->prevAltSetting = 0x00;
    pAppCtxt->enumMethod = CY_USB_ENUM_METHOD_FAST;
    pAppCtxt->pHbDmaMgrCtxt = pHbDmaMgrCtxt;

    for (index = 0x00; index < CY_USB_MAX_ENDP_NUMBER; index++)
    {
        pEndpInDma = &(pAppCtxt->endpInDma[index]);
        memset((void*)pEndpInDma, 0, sizeof(cy_stc_app_endp_dma_set_t));

        pEndpOutDma = &(pAppCtxt->endpOutDma[index]);
        memset((void*)pEndpOutDma, 0, sizeof(cy_stc_app_endp_dma_set_t));
    }

    pAppCtxt->pCpuDmacBase = pCpuDmacBase;
    pAppCtxt->pCpuDw0Base = pCpuDw0Base;
    pAppCtxt->pCpuDw1Base = pCpuDw1Base;
    pAppCtxt->pUsbdCtxt = pUsbdCtxt;

    /*
     * Callbacks registered with USBD layer. These callbacks will be called
     * based on appropriate event.
     */
    Cy_USB_AppRegisterCallback(pAppCtxt);
    
    if (!(pAppCtxt->firstInitDone))
    {
        pAppCtxt->vbusChangeIntr = false;
        pAppCtxt->vbusPresent = false;
        pAppCtxt->usbConnected = false;

#if DEBUG_INFRA_EN
        Cy_SysLib_Delay(500); 
        xTaskCreate(Cy_PrintTaskHandler, "PrintLogTask", 512, NULL, 5, &printLogTaskHandle);
#endif /* DEBUG_INFRA_EN */

        /* Create the message queue and register it with the kernel. */
        pAppCtxt->appMsgQueue = xQueueCreate(CY_USB_DEVICE_MSG_QUEUE_SIZE,
            CY_USB_DEVICE_MSG_SIZE);
        if (pAppCtxt->appMsgQueue == NULL) {
            DBG_APP_ERR("QueuecreateFail\r\n");
            return;
        }

        vQueueAddToRegistry(pAppCtxt->appMsgQueue, "AppMsgQueue");

        pAppCtxt->vbusDebounceTimer = xTimerCreate("VbusDebounceTimer", 200, pdFALSE,
            (void*)pAppCtxt, Cy_USB_VbusDebounceTimerCb);
        if (pAppCtxt->vbusDebounceTimer == NULL) {
            DBG_APP_ERR("TimerCreateFail\r\n");
            return;
        }

        DBG_APP_TRACE("Creating USB Device Thread\r\n");
        /* Create task and check status to confirm task created properly. */
        status = xTaskCreate(Cy_USB_DeviceTaskHandler, "USBDeviceTask", 2048,
            (void*)pAppCtxt, 5, &(pAppCtxt->usbDevicetaskHandle));
        if (status != pdPASS)
        {
            DBG_APP_ERR("TaskcreateFail\r\n");
            return;
        }
        pAppCtxt->firstInitDone = 0x01;
    }

    /* Zero out the EP0 test buffer. */
    return;
} /* end of function. */

/**
 * \name Cy_USB_AppSetAddressCallback
 * \brief This function will be called by USBD layer when a USB address has been assigned to the device.
 * \param pCtxt Application layer context pointer
 * \param pUsbdCtxt USBD layer context pointer
 * \param pMsg USB message
 * \retval None
 */
void
Cy_USB_AppSetAddressCallback(void* pCtxt, cy_stc_usb_usbd_ctxt_t* pUsbdCtxt,
    cy_stc_usb_cal_msg_t* pMsg)
{
    cy_stc_usb_app_ctxt_t* pAppCtxt = (cy_stc_usb_app_ctxt_t*)pCtxt;

    /* Update the state variables. */
    pAppCtxt->devState = CY_USB_DEVICE_STATE_ADDRESS;
    pAppCtxt->prevDevState = CY_USB_DEVICE_STATE_DEFAULT;
    pAppCtxt->devAddr = pUsbdCtxt->devAddr;
    pAppCtxt->devSpeed = Cy_USBD_GetDeviceSpeed(pUsbdCtxt);

    if(pAppCtxt->appInit == 1u)
    {
        Cy_USB_AppDisableEndpDma(pAppCtxt);
    }
    
    Cy_ApplicationInit(pAppCtxt);

    Cy_USBD_LpmDisable(pUsbdCtxt);

    if(!(pAppCtxt->descriptorPopulated))
    {
        /* Populate descriptors based on config table*/
        DBG_APP_INFO("USB: Populate descriptors  for USB Speed %d\r\n",pAppCtxt->devSpeed);
        Cy_USB_PopulateDescriptors(pAppCtxt,pAppCtxt->devSpeed);
        pAppCtxt->descriptorPopulated = 1;

        /* Check the type of USB connection and register appropriate descriptors. */
        Cy_USB_RegisterUsbDescriptors(pAppCtxt, pAppCtxt->devSpeed);

        if(pAppCtxt->appInfoPtr->cdcCount >= 1)
        {
            uint8_t function = 0;
            uint8_t i = 0;
            
            /* Get function num for CDC */
            for(function = 0; (function < pAppCtxt->appInfoPtr->totalFunctions) && (i < pAppCtxt->appInfoPtr->cdcCount); function++)
            { 
                i++;
                /* Initialize SCB for CDC and create task*/
                if((CDC_DATA_SOURCE_SINK(pAppCtxt->cfgTablePtr,function) == CY_CDC_UART_SCB0) && (pAppCtxt->uartSCB0Enabled != 1))
                {
                    Cy_CDC_UartSCBInit(CY_DEBUG_INTFCE_UART_SCB0);       
                    Cy_CDC_UartInitDmaIntr(CDC_UART_WRITE,0);
                    Cy_CDC_UartInitDmaIntr(CDC_UART_READ,0);
                    pAppCtxt->uartSCB0Enabled = 1;     
                }
                
                if((CDC_DATA_SOURCE_SINK(pAppCtxt->cfgTablePtr,function) == CY_CDC_UART_SCB1) && (pAppCtxt->uartSCB1Enabled != 1))
                {
                    Cy_CDC_UartSCBInit(CY_DEBUG_INTFCE_UART_SCB1);       
                    Cy_CDC_UartInitDmaIntr(CDC_UART_WRITE,1);
                    Cy_CDC_UartInitDmaIntr(CDC_UART_READ,1);
                    pAppCtxt->uartSCB1Enabled = 1;
                }

                if((CDC_DATA_SOURCE_SINK(pAppCtxt->cfgTablePtr,function) == CY_CDC_UART_SCB4) && (pAppCtxt->uartSCB4Enabled != 1))
                {
                    Cy_CDC_UartSCBInit(CY_DEBUG_INTFCE_UART_SCB4);       
                    Cy_CDC_UartInitDmaIntr(CDC_UART_WRITE,4);
                    Cy_CDC_UartInitDmaIntr(CDC_UART_READ,4);
                    pAppCtxt->uartSCB4Enabled = 1;
                }

                if((CDC_DATA_SOURCE_SINK(pAppCtxt->cfgTablePtr,function) == CY_CDC_UART_SCB6) && (pAppCtxt->uartSCB6Enabled != 1))
                {
                    Cy_CDC_UartSCBInit(CY_DEBUG_INTFCE_UART_SCB6);       
                    Cy_CDC_UartInitDmaIntr(CDC_UART_WRITE,6);
                    Cy_CDC_UartInitDmaIntr(CDC_UART_READ,6);
                    pAppCtxt->uartSCB6Enabled = 1;
                }
            }

            Cy_CDC_TaskCreate(pAppCtxt);
        }
    }
}

/**
 * \name Cy_USB_AppRegisterCallback
 * \brief This function will register all callbacks with USBD layer.
 * \param pAppCtxt Application layer context pointer
 * \retval None
 */
void Cy_USB_AppRegisterCallback(cy_stc_usb_app_ctxt_t* pAppCtxt)
{
    cy_stc_usb_usbd_ctxt_t* pUsbdCtxt = pAppCtxt->pUsbdCtxt;

    Cy_USBD_RegisterCallback(pUsbdCtxt, CY_USB_USBD_CB_RESET,
        Cy_USB_AppBusResetCallback);
    Cy_USBD_RegisterCallback(pUsbdCtxt, CY_USB_USBD_CB_RESET_DONE,
        Cy_USB_AppBusResetDoneCallback);
    Cy_USBD_RegisterCallback(pUsbdCtxt, CY_USB_USBD_CB_BUS_SPEED,
        Cy_USB_AppBusSpeedCallback);
    Cy_USBD_RegisterCallback(pUsbdCtxt, CY_USB_USBD_CB_SETUP,
        Cy_USB_AppSetupCallback);
    Cy_USBD_RegisterCallback(pUsbdCtxt, CY_USB_USBD_CB_SUSPEND,
        Cy_USB_AppSuspendCallback);
    Cy_USBD_RegisterCallback(pUsbdCtxt, CY_USB_USBD_CB_RESUME,
        Cy_USB_AppResumeCallback);
    Cy_USBD_RegisterCallback(pUsbdCtxt, CY_USB_USBD_CB_SET_CONFIG,
        Cy_USB_AppSetCfgCallback);
    Cy_USBD_RegisterCallback(pUsbdCtxt, CY_USB_USBD_CB_SET_INTF,
        Cy_USB_AppSetIntfCallback);
    Cy_USBD_RegisterCallback(pUsbdCtxt, CY_USB_USBD_CB_L1_SLEEP,
        Cy_USB_AppL1SleepCallback);
    Cy_USBD_RegisterCallback(pUsbdCtxt, CY_USB_USBD_CB_L1_RESUME,
        Cy_USB_AppL1ResumeCallback);
    Cy_USBD_RegisterCallback(pUsbdCtxt, CY_USB_USBD_CB_ZLP,
        Cy_USB_AppZlpCallback);
    Cy_USBD_RegisterCallback(pUsbdCtxt, CY_USB_USBD_CB_SLP,
        Cy_USB_AppSlpCallback);
    Cy_USBD_RegisterCallback(pUsbdCtxt, CY_USB_USBD_CB_SETADDR,
        Cy_USB_AppSetAddressCallback);
    return;
} /* end of function. */

/**
 * \name Cy_Debug_DataRecvCb
 * \brief Callback function which handles data received through the debug UART.
 * \param pBuf Pointer to read data buffer
 * \param size Size of data received
 * \param pAppCtxt Pointer to application context
 * \retval None
 */
void Cy_Debug_DataRecvCb (uint8_t *pBuf, uint16_t size, void *pAppCtxt)
{
    cy_stc_usb_app_ctxt_t *pApp = (cy_stc_usb_app_ctxt_t*)pAppCtxt;
    DBG_APP_INFO("Received %d bytes from the debug UART\r\n", size);
    Cy_Debug_QueueDataRead(dbgUartRcvBuffer, 1, Cy_Debug_DataRecvCb, pApp);
}


/**
 * \name Cy_USB_CreateCDCChannel
 * \brief Function to create CDC DMA channel
 * \param pAppCtxt Pointer to application context
 * \param dmaConfig Pointer to DMA configuration
 * \param dir Endpoint direction
 * \param function Function number
 * \param endpNumber Endpoint number
 * \param maxPktSize Endpoint max packet size
 * \param burstSize Endpoint burst size
 * \retval True if channel created successfully else false
 */
bool Cy_USB_CreateCDCChannel(cy_stc_usb_app_ctxt_t *pAppCtxt, cy_stc_hbdma_chn_config_t *dmaConfig, uint8_t dir, uint8_t function, 
                            uint32_t endpNumber, uint16_t maxPktSize, uint8_t burstSize)
{
    bool channelValid = false;
    cy_en_hbdma_mgr_status_t mgrStat;
    cy_stc_usb_dev_config_t *cfgTablePtr = pAppCtxt->cfgTablePtr;
    uint32_t bufSize;
    uint8_t  bufCount;

    if(dir != 0)
    {
        /* The size of DMA buffer used needs to be CDC_VCOM_UART_MAX_RD_SIZE bytes.
         * Adjust the number of buffers based on user selection.
         */
        bufSize = CDC_DMA_BUFFER_SIZE(cfgTablePtr,function);
        bufCount = CDC_DMA_BUFFER_COUNT(cfgTablePtr,function);

        /* Create channel which will move data from SRAM to USBHS IN endpoint. */
        dmaConfig->size           = bufSize;                                           /* DMA Buffer size in bytes */
        dmaConfig->count          = bufCount;                                          /* DMA Buffer Count */
        dmaConfig->prodBufSize    = bufSize;
        dmaConfig->chType         = CY_HBDMA_TYPE_MEM_TO_IP;
        dmaConfig->prodSck[0]     = CY_HBDMA_VIRT_SOCKET_WR;
        dmaConfig->consSck[0]     = (cy_hbdma_socket_id_t)(CY_HBDMA_USBHS_IN_EP_00 + endpNumber);
        dmaConfig->usbMaxPktSize  = maxPktSize;
        dmaConfig->cb             = Cy_CDC_HbwDmaCallback; /* HB-DMA callback */
        dmaConfig->intrEnable     = USB32DEV_ADAPTER_DMA_SCK_INTR_CONSUME_EVENT_Msk;

        mgrStat = Cy_HBDma_Channel_Create(pAppCtxt->pUsbdCtxt->pHBDmaMgr,
                                                    &(pAppCtxt->endpInDma[endpNumber].hbDmaChannel),
                                                    dmaConfig);
        if (mgrStat != CY_HBDMA_MGR_SUCCESS) {
            /* Not able to create Channel then return. */
            DBG_APP_ERR("CDC: EP BULK-IN: channel create failed 0x%x\r\n", mgrStat);
            return false;
        }
        else
        {
            DBG_APP_TRACE("CDC: Bulk-IN: channel created\r\n");
            channelValid = true;
        }

        Cy_HBDma_Channel_Enable(&(pAppCtxt->endpInDma[endpNumber].hbDmaChannel), 0);

        /* Register the DW1 ISR for this IN endpoint for intrrupt on transfer completion */
        Cy_USB_AppInitDmaIntr(endpNumber, CY_USB_ENDP_DIR_IN, Cy_CDC_HbDma_DW1_ISR);

        /* Store the pointer to the DMA channel. */
        pAppCtxt->pUartToUsbChn[pAppCtxt->numCDCFunctions] = &(pAppCtxt->endpInDma[endpNumber].hbDmaChannel);
        pAppCtxt->numOfHbdmaChannel++;
        pAppCtxt->cdcInfo[pAppCtxt->numCDCFunctions]->vcomInEpNum   = endpNumber;
        pAppCtxt->cdcInfo[pAppCtxt->numCDCFunctions]->dmaRxBufferCount = bufCount;
        pAppCtxt->endpInDma[endpNumber].maxPktSize = maxPktSize;
        pAppCtxt->endpInDma[endpNumber].valid      = 0x01;

        /* Get the list of DMA buffers to be used for UART to USB transfers. */
        mgrStat = Cy_HBDma_Channel_GetBufferInfo(pAppCtxt->pUartToUsbChn[pAppCtxt->numCDCFunctions], pAppCtxt->cdcInfo[pAppCtxt->numCDCFunctions]->pUartRxBuffer, bufCount);
        if (mgrStat != CY_HBDMA_MGR_SUCCESS)
            DBG_APP_ERR("CDC: Get buffer failed 0x%x\r\n", mgrStat);
            
        pAppCtxt->cdcInfo[pAppCtxt->numCDCFunctions]->uartRxBufIndex     = 0;
        pAppCtxt->cdcInfo[pAppCtxt->numCDCFunctions]->uartRxFreeBufCount = bufCount;
        pAppCtxt->cdcInfo[pAppCtxt->numCDCFunctions]->nxtUsbTxBufIdx     = 0;
        pAppCtxt->cdcInfo[pAppCtxt->numCDCFunctions]->usbInXferPending   = false;
    }
    else
    {
        /* Create channel which moves data from USBHS OUT endpoint into HBW SRAM. */
        dmaConfig->size        = maxPktSize * burstSize;    /* DMA Buffer Size in bytes */
        dmaConfig->count       = 4;                         /* Always use 4 buffers on USB to UART path. */
        dmaConfig->prodBufSize = maxPktSize * burstSize;
        dmaConfig->chType      = CY_HBDMA_TYPE_IP_TO_MEM;
        dmaConfig->prodSck[0]  = (cy_hbdma_socket_id_t)(CY_HBDMA_USBHS_OUT_EP_00 + endpNumber);
        dmaConfig->consSck[0]  = CY_HBDMA_VIRT_SOCKET_RD;
        dmaConfig->usbMaxPktSize = maxPktSize;
        dmaConfig->cb          = Cy_CDC_HbwDmaCallback;     /* HB-DMA callback */
        dmaConfig->intrEnable  = USB32DEV_ADAPTER_DMA_SCK_INTR_PRODUCE_EVENT_Msk;

        mgrStat = Cy_HBDma_Channel_Create(pAppCtxt->pUsbdCtxt->pHBDmaMgr,
                                                &(pAppCtxt->endpOutDma[endpNumber].hbDmaChannel),
                                                dmaConfig);
        if (mgrStat != CY_HBDMA_MGR_SUCCESS) {
            /* Not able to create Channel then return. */
            DBG_APP_ERR("CDC: EP BULK-OUT: channel create failed 0x%x\r\n", mgrStat);
            return false;
        }
        else
        {
            DBG_APP_TRACE("CDC: BULK-OUT: channel created\r\n");
            channelValid = true;
        }

        Cy_HBDma_Channel_Enable(&(pAppCtxt->endpOutDma[endpNumber].hbDmaChannel), 0);

        /* Register the DW0 ISR for this OUT endpoint for intrrupt on transfer completion */
        Cy_USB_AppInitDmaIntr(endpNumber, CY_USB_ENDP_DIR_OUT, Cy_CDC_HbDma_DW0_ISR);

        /* Store the DMA channel handle. */
        pAppCtxt->pUsbToUartChn[pAppCtxt->numCDCFunctions] = &(pAppCtxt->endpOutDma[endpNumber].hbDmaChannel);
        pAppCtxt->numOfHbdmaChannel++;
        pAppCtxt->cdcInfo[pAppCtxt->numCDCFunctions]->vcomOutEpNum  = (uint8_t)endpNumber;
        pAppCtxt->cdcInfo[pAppCtxt->numCDCFunctions]->dmaTxBufferCount = 4;
        pAppCtxt->endpInDma[endpNumber].maxPktSize = maxPktSize;
        pAppCtxt->endpInDma[endpNumber].valid      = 0x01;

        /* Get the list of DMA buffers to be used for UART to USB transfers. */
        mgrStat = Cy_HBDma_Channel_GetBufferInfo(pAppCtxt->pUsbToUartChn[pAppCtxt->numCDCFunctions], pAppCtxt->cdcInfo[pAppCtxt->numCDCFunctions]->pUsbRxBuffer, 4);
        if (mgrStat != CY_HBDMA_MGR_SUCCESS)
            DBG_APP_ERR("CDC: Get buffer failed 0x%x\r\n", mgrStat);

        pAppCtxt->cdcInfo[pAppCtxt->numCDCFunctions]->nxtUsbRxBufIdx    = 0;
        pAppCtxt->cdcInfo[pAppCtxt->numCDCFunctions]->uartTxBufIndex    = 0;
        pAppCtxt->cdcInfo[pAppCtxt->numCDCFunctions]->usbRxBusyBufCount = 0;          
    }
    return channelValid;
}

/**
 * \name Cy_USB_AppSetupEndpDmaParamsInternal
 * \brief Perform high bandwidth DMA initialization associated with a USB endpoint.
 * \param pAppCtxt Pointer to application context structure
 * \param pEndpDscr Endpoint descriptor
 * \param function Function number
 * \param functionType Function type
 * \retval None
 */
static void
Cy_USB_AppSetupEndpDmaParamsInternal (cy_stc_usb_app_ctxt_t *pAppCtxt,
                                uint8_t *pEndpDscr,uint8_t function,uint8_t functionType)
{
    cy_stc_hbdma_chn_config_t dmaConfig;
    bool channelValid = 0;
    uint32_t endpNumber, dir;
    uint16_t maxPktSize;
    uint8_t burstSize = 1;
    uint8_t bmAttribute = 0;
    cy_stc_usb_dev_config_t *cfgTablePtr = pAppCtxt->cfgTablePtr;
    static cy_stc_debug_config_t pDbgCfg;

    if(CHECK_FN_CONFIG(cfgTablePtr,function) == NULL)
    {
        DBG_APP_ERR("AppSetupEndpDmaParams Function check failed\r\n");
        return;
    }

    Cy_USBD_GetEndpNumMaxPktDir(pEndpDscr, &endpNumber, &maxPktSize, &dir);

    bmAttribute = bmAttribute & 0x0F;

    DBG_APP_TRACE("AppSetupEndpDmaParams: functionIndex: %dfunctionType %d, endpNum:0x%x maxPktSize:0x%x "
                    "dir:0x%x burstSize:0x%x bmAttribute:0x%x\r\n",
                    function,functionType, endpNumber, maxPktSize,
                    dir, burstSize, bmAttribute);

    if((endpNumber != (CDC_INTR_EP_NUM(cfgTablePtr,function) & 0x0F)) || (dir == CY_USB_ENDP_DIR_OUT))
    {
        static uint8_t cdcPrevFn = 0;
        bool stat =0;

        if((CDC_DMA_BUFFER_SIZE(cfgTablePtr,function) > (MAX_DMA_BUFFER_SIZE)))
        {
            DBG_APP_ERR("CDC: Invalid DMA Buffer Size!\r\n");
            return;
        }
        pAppCtxt->cdcInfo[pAppCtxt->numCDCFunctions] = &glCDCInfo[pAppCtxt->numCDCFunctions];
        pAppCtxt->cdcInfo[pAppCtxt->numCDCFunctions]->cdcFuncIndex = function;

        /* If CDC function is for debug logs*/
        if(CDC_DATA_SOURCE_SINK(cfgTablePtr,function) == CY_CDC_CPU)
        {  
            if((endpNumber == (CDC_DATA_IN_EP_NUM(cfgTablePtr,function) & 0xF)) && (dir != 0) )
            {
                pDbgCfg.pUsbCtxt  = pAppCtxt->pUsbdCtxt;
                pDbgCfg.cdcEpIn   = (CDC_DATA_IN_EP_NUM(cfgTablePtr,function) & 0xF);
                pDbgCfg.traceLvl  = USB_LOG_LEVEL(cfgTablePtr);
                pDbgCfg.bufSize   = CDC_DMA_BUFFER_SIZE(cfgTablePtr,function);
                pDbgCfg.bufCount  = CDC_DMA_BUFFER_COUNT(cfgTablePtr,function);
                pDbgCfg.dbgIntfce = CY_DEBUG_INTFCE_USB_CDC;
                pDbgCfg.printNow  = true;                /* Delay output of messages till logging task runs. */
                cdcPrevFn++;                                     
            }
            else if((endpNumber == (CDC_DATA_OUT_EP_NUM(cfgTablePtr,function) & 0xF))  && (dir == CY_USB_ENDP_DIR_OUT) && (CDC_DATA_RECEIVE_ENABLED(cfgTablePtr,function)==1u))
            {
                pDbgCfg.recvEnabled = 1;
                pDbgCfg.pCpuDw0Base = pAppCtxt->pCpuDw0Base;
                pDbgCfg.pCpuDw1Base = pAppCtxt->pCpuDw1Base;
                pDbgCfg.cdcEpOut   = (CDC_DATA_OUT_EP_NUM(cfgTablePtr,function) & 0xF);
                cdcPrevFn++;
            }
        }
        /* CDC for UART <> CDC*/
        else if (CDC_DATA_SOURCE_SINK(cfgTablePtr,function) != CY_CDC_CPU)
        {
            /* The High BandWidth Channel Creation code needs to be run in all cases
            * as the DMA buffers are allocated as part of it. */
           
            dmaConfig.bufferMode     = false;                      /* DMA buffer mode disabled */
            dmaConfig.prodHdrSize    = 0;                          /* No header being added. */
            dmaConfig.prodSckCount   = 1;                          /* No. of producer sockets */
            dmaConfig.consSckCount   = 1;                          /* No. of consumer Sockets */
            dmaConfig.prodSck[1]     = (cy_hbdma_socket_id_t)0;    /* Producer Socket ID: None */
            dmaConfig.consSck[1]     = (cy_hbdma_socket_id_t)0;    /* Consumer Socket ID: None */
            dmaConfig.eventEnable    = false;                      /* Firmware intervention required for transfers. */
            dmaConfig.userCtx        = (void *)(pAppCtxt);         /* Pass the application context as user context. */

            if(CDC_DATA_SOURCE_SINK(cfgTablePtr,function) == CY_CDC_UART_SCB0)
            {
                glCdcFnScbMap[pAppCtxt->numCDCFunctions] = CY_DEBUG_INTFCE_UART_SCB0 ;
            }
            else if(CDC_DATA_SOURCE_SINK(cfgTablePtr,function) == CY_CDC_UART_SCB1)
            {
                glCdcFnScbMap[pAppCtxt->numCDCFunctions] = CY_DEBUG_INTFCE_UART_SCB1 ;
            }
            else if(CDC_DATA_SOURCE_SINK(cfgTablePtr,function) == CY_CDC_UART_SCB4)
            {
                glCdcFnScbMap[pAppCtxt->numCDCFunctions] = CY_DEBUG_INTFCE_UART_SCB4;
            }
            else if(CDC_DATA_SOURCE_SINK(cfgTablePtr,function) == CY_CDC_UART_SCB6)
            {
                glCdcFnScbMap[pAppCtxt->numCDCFunctions] = CY_DEBUG_INTFCE_UART_SCB6;
            }
            else
            {
                glCdcFnScbMap[pAppCtxt->numCDCFunctions] = CY_APP_ERROR_CODE ;
            }

            if((endpNumber == (CDC_DATA_IN_EP_NUM(cfgTablePtr,function) & 0x0F)) && (dir != CY_USB_ENDP_DIR_OUT) )
            {   
                cdcPrevFn++;
                /*
                * Its IN endpoint which means DATA flow from
                * UART->DMA->HBWRAM->EPM->Host
                */
                if (pAppCtxt->pUartToUsbChn[pAppCtxt->numCDCFunctions] != NULL)
                {
                    DBG_APP_ERR("CDC: BULK-IN channel already created\r\n");
                    return;
                }

                channelValid = Cy_USB_CreateCDCChannel(pAppCtxt, &dmaConfig, CY_USB_ENDP_DIR_IN, function, endpNumber, maxPktSize, burstSize);
            }
            else if ((endpNumber == CDC_DATA_OUT_EP_NUM(cfgTablePtr,function)) && (CDC_DATA_RECEIVE_ENABLED(cfgTablePtr,function) == 1u)) 
            {
                cdcPrevFn++;
                /* Only one OUT endpoint is expected. Ensure that OUT channel has not been created. */
                if (pAppCtxt->pUsbToUartChn[pAppCtxt->numCDCFunctions] != NULL) {
                    DBG_APP_ERR("CDC: BULK-OUT channel already created\r\n");
                    return;
                }

                channelValid = Cy_USB_CreateCDCChannel(pAppCtxt, &dmaConfig, CY_USB_ENDP_DIR_OUT, function, endpNumber, maxPktSize, burstSize);
                if(channelValid)
                {
                    pAppCtxt->endpInDma[endpNumber].maxPktSize = maxPktSize;
                    pAppCtxt->endpInDma[endpNumber].valid      = 0x01;

                }
            }
        }
        /* Update num of CDC functions when channels for both the endpoints is created*/
        if(((cdcPrevFn == 2) && (CDC_DATA_RECEIVE_ENABLED(cfgTablePtr,function) == 1u)) || ((cdcPrevFn == 1) && (CDC_DATA_RECEIVE_ENABLED(cfgTablePtr,function) != 1u)))
        {
            /*Updta only if function Number Changes*/

            if(CDC_DATA_SOURCE_SINK(cfgTablePtr,function) == CY_CDC_CPU)
            {
                Cy_Debug_LogInit(&pDbgCfg);
                DBG_APP_TRACE("CDC: Debug Init: %d\n\r",stat);

                pAppCtxt->cdcDebugEnabled = 1;

                if(pDbgCfg.recvEnabled == 1)
                {
                    stat = Cy_Debug_QueueDataRead(dbgUartRcvBuffer, 1, Cy_Debug_DataRecvCb, pAppCtxt);
                    DBG_APP_TRACE("CDC: Debug Recv Init: %d\n\r",stat);
                }
            }

            pAppCtxt->numCDCFunctions++;
            cdcPrevFn=0;
        }
    }
} /* end of function  */

/**
 * \name Cy_USB_AppSetupEndpDmaParamsHs
 * \brief Perform high bandwidth DMA initialization associated with a USB endpoint.
 * \param pAppCtxt Pointer to application context structure
 * \param pEndpDscr Endpoint descriptor
 * \retval None
 */
void
Cy_USB_AppSetupEndpDmaParamsHs(cy_stc_usb_app_ctxt_t *pAppCtxt,
                               uint8_t *pEndpDscr)
{
    cy_stc_app_endp_dma_set_t* pEndpDmaSet;
    DW_Type* pDW;
    bool stat;
    uint32_t endpNumber;
    uint16_t maxPktSize = 0x00;
    cy_en_usb_endp_dir_t endpDirection;

    endpNumber = ((*(pEndpDscr + CY_USB_ENDP_DSCR_OFFSET_ADDRESS)) & 0x7F);
    Cy_USBD_GetEndpMaxPktSize(pEndpDscr, &maxPktSize);

    if (*(pEndpDscr + CY_USB_ENDP_DSCR_OFFSET_ADDRESS) & 0x80)
    {
        endpDirection = CY_USB_ENDP_DIR_IN;
        pEndpDmaSet = &(pAppCtxt->endpInDma[endpNumber]);
        pDW = pAppCtxt->pCpuDw1Base;
    }
    else
    {
        endpDirection = CY_USB_ENDP_DIR_OUT;
        pEndpDmaSet = &(pAppCtxt->endpOutDma[endpNumber]);
        pDW = pAppCtxt->pCpuDw0Base;
    }

    stat = Cy_USBHS_App_EnableEpDmaSet(pEndpDmaSet, pDW, endpNumber, endpNumber, endpDirection, maxPktSize);
    DBG_APP_TRACE("Enable EPDmaSet: endp=%x dir=%x stat=%x\r\n", endpNumber, endpDirection, stat);
} /* end of function  */

/**
 * \name Cy_USB_AppSetupEndpDmaParams
 * \brief This function will setup Endpoint and DMA related parameters before transfer initiated.
 * \param pAppCtxt Pointer to application context structure
 * \param pEndpDscr Endpoint descriptor
 * \param function Function number
 * \param functionType Function type
 * \retval None
 */
void Cy_USB_AppSetupEndpDmaParams (cy_stc_usb_app_ctxt_t *pAppCtxt,
                                   uint8_t *pEndpDscr, uint8_t function, uint8_t functionType)
{
    /* We always need to set up HBDMA channels for buffer management. */
    Cy_USB_AppSetupEndpDmaParamsInternal(pAppCtxt, pEndpDscr,function,functionType);

    /* If a valid SCB is configured for the CD interface, set up the endpoint dma */
    if(CDC_DATA_SOURCE_SINK(pAppCtxt->cfgTablePtr,function) != CY_CDC_CPU)
    {
        Cy_USB_AppSetupEndpDmaParamsHs(pAppCtxt, pEndpDscr);
    }
}

/**
 * \name Cy_USB_AppConfigureEndp
 * \brief Configure all endpoints used by application (except EP0)
 * \param pUsbdCtxt USBD layer context pointer
 * \param pEndpDscr Endpoint descriptor pointer
 * \retval None
 */
void Cy_USB_AppConfigureEndp(cy_stc_usb_usbd_ctxt_t* pUsbdCtxt, uint8_t* pEndpDscr)
{
    cy_stc_usb_endp_config_t endpConfig;
    cy_en_usb_endp_dir_t endpDirection;
    bool valid;
    uint32_t endpType;
    uint32_t endpNumber, dir;
    uint16_t maxPktSize;
    uint8_t isoPkts = 0x00;
    uint8_t burstSize = 0x00;
    uint8_t maxStream = 0x00;
    uint8_t interval = 0x00;
    cy_en_usbd_ret_code_t usbdRetCode;

    /* If it is not endpoint descriptor then return */
    if (!Cy_USBD_EndpDscrValid(pEndpDscr))
    {
        return;
    }
    Cy_USBD_GetEndpNumMaxPktDir(pEndpDscr, &endpNumber, &maxPktSize, &dir);

    if (dir)
    {
        endpDirection = CY_USB_ENDP_DIR_IN;
    }
    else
    {
        endpDirection = CY_USB_ENDP_DIR_OUT;
    }
    Cy_USBD_GetEndpType(pEndpDscr, &endpType);

    if ((CY_USB_ENDP_TYPE_ISO == endpType) || (CY_USB_ENDP_TYPE_INTR == endpType)) {
        if(endpDirection == CY_USB_ENDP_DIR_IN)
        {
            /* The ISOINPKS setting in the USBHS register is the actual packets per microframe value. */
            /* Enable short packet on ISO IN ep. */
            USB32DEV->USB32DEV_PROT.PROT_EPI_ENHANCE[endpNumber] |= (USB32DEV_PROT_EPI_ENHANCE_EN_EOB_SHORT_Msk);
        }

        isoPkts = (
                (*((uint8_t *)(pEndpDscr + CY_USB_ENDP_DSCR_OFFSET_MAX_PKT + 1)) & CY_USB_ENDP_ADDL_XN_MASK)
                >> CY_USB_ENDP_ADDL_XN_POS) + 1;
    }

    valid = 0x01;

    /* Fetch the endpoint service interval. */
    Cy_USBD_GetEndpInterval(pEndpDscr, &interval);

    /* Prepare endpointConfig parameter. */
    endpConfig.endpType = (cy_en_usb_endp_type_t)endpType;
    endpConfig.endpDirection = endpDirection;
    endpConfig.valid = valid;
    endpConfig.endpNumber = endpNumber;
    endpConfig.maxPktSize = (uint32_t)maxPktSize;
    endpConfig.isoPkts = (uint32_t)isoPkts;
    endpConfig.burstSize = burstSize;
    endpConfig.interval = interval;
    endpConfig.streamID = maxStream;
    endpConfig.allowNakTillDmaRdy = (endpConfig.endpType != CY_USB_ENDP_TYPE_ISO);
    usbdRetCode = Cy_USB_USBD_EndpConfig(pUsbdCtxt, endpConfig);

    /* Print status of the endpoint configuration to help debug. */
    DBG_APP_TRACE("#ENDPCFG: %d, %d, %d, %d, %d, %d, %d, %x\r\n", endpNumber, endpConfig.valid,endpConfig.maxPktSize,
                                                            endpConfig.isoPkts,endpConfig.burstSize,endpConfig.interval,
                                                            endpConfig.allowNakTillDmaRdy,usbdRetCode);

    return;
} /* end of function */

/**
 * \name Cy_USB_AppSetCfgCallback
 * \brief Callback function will be invoked by USBD when set configuration is received
 * \param pCtxt Application layer context pointer
 * \param pUsbdCtxt USBD layer context pointer
 * \param pMsg USB Message
 * \retval None
 */
void Cy_USB_AppSetCfgCallback(void* pCtxt, cy_stc_usb_usbd_ctxt_t* pUsbdCtxt,
                              cy_stc_usb_cal_msg_t *pMsg)
{
    cy_stc_usb_app_ctxt_t *pAppCtxt;
    uint8_t *pActiveCfg, *pIntfDscr, *pEndpDscr;
    uint8_t index, numOfIntf, numOfEndp;
    uint8_t functionNum = 0;
    uint8_t functionType = 0;
    uint8_t i = 0;
    BaseType_t xHigherPriorityTaskWoken;
    cy_stc_usbd_app_msg_t xMsg;
    DBG_APP_TRACE("AppSetCfgCbStart\r\n");

    pAppCtxt = (cy_stc_usb_app_ctxt_t*)pCtxt;
    cy_stc_app_info_t *appInfoPtr = pAppCtxt->appInfoPtr;
    pAppCtxt->devSpeed = Cy_USBD_GetDeviceSpeed(pUsbdCtxt);
    pAppCtxt->appInit = 1;

    /* Disable optional Low Power Mode transitions. */
    Cy_USBD_LpmDisable(pUsbdCtxt);

    /*
     * Based on type of application as well as how data flows,
     * data wire can be used so initialize datawire.
     */
    Cy_DMA_Enable(pAppCtxt->pCpuDw0Base);
    Cy_DMA_Enable(pAppCtxt->pCpuDw1Base);

    pActiveCfg = Cy_USB_USBD_GetActiveCfgDscr(pUsbdCtxt);
    if (!pActiveCfg)
    {
        /* Set config should be called when active config value > 0x00. */
        return;
    }
    numOfIntf = Cy_USBD_FindNumOfIntf(pActiveCfg);
    if (numOfIntf == 0x00)
    {
        return;
    }

    for (index = 0x00; index < numOfIntf; index++)
    {
        functionNum = INFO_FN_NUM(appInfoPtr,index);
        DBG_APP_TRACE("TotalAltSetting:%d\r\n",pUsbdCtxt->numOfAltSettings[index]);
        
        for(i =0; i<pUsbdCtxt->numOfAltSettings[index];i++)
        {
            DBG_APP_TRACE("AltSetting:%d total %d\r\n",i, pUsbdCtxt->numOfAltSettings[index]);
            /* During Set Config command always altSetting 0 will be active. */
            pIntfDscr = Cy_USBD_GetIntfDscr(pUsbdCtxt, index, i);
            if (pIntfDscr == NULL)
            {
                DBG_APP_TRACE("pIntfDscrNull\r\n");
                return;
            }

            numOfEndp = Cy_USBD_FindNumOfEndp(pIntfDscr);
            if (numOfEndp == 0x00)
            {
                DBG_APP_TRACE("numOfEndp 0\r\n");
                continue;
            }

            pEndpDscr = Cy_USBD_GetEndpDscr(pUsbdCtxt, pIntfDscr);
            while (numOfEndp != 0x00)
            {
                Cy_USB_AppConfigureEndp(pUsbdCtxt, pEndpDscr);
                Cy_USB_AppSetupEndpDmaParams(pAppCtxt, pEndpDscr, functionNum, functionType);

                numOfEndp--;
                pEndpDscr = (pEndpDscr + (*(pEndpDscr + CY_USB_DSCR_OFFSET_LEN)));
            }
        }
    }

    pAppCtxt->prevDevState = CY_USB_DEVICE_STATE_CONFIGURED;
    pAppCtxt->devState = CY_USB_DEVICE_STATE_CONFIGURED;


    if(pAppCtxt->appInfoPtr->cdcCount >= 1 )        
    {
        for(i = 0; i < pAppCtxt->numCDCFunctions; i++)
        {
            if(CDC_DATA_SOURCE_SINK(pAppCtxt->cfgTablePtr,pAppCtxt->cdcInfo[i]->cdcFuncIndex) != CY_CDC_CPU )
            {
                Cy_CDC_InitializeTimer(pAppCtxt, i);
                xTimerReset(pAppCtxt->vcomTimerHandle[i], 0);
                if(CDC_DATA_RECEIVE_ENABLED(pAppCtxt->cfgTablePtr,pAppCtxt->cdcInfo[i]->cdcFuncIndex) == 1u) 
                {
                    xMsg.type = CY_USB_VCOM_DEVICE_MSG_START_DATA_XFER;
                    xMsg.data[0] = i;
                    xQueueSendFromISR(pAppCtxt->vcomDeviceQueue, &(xMsg), &(xHigherPriorityTaskWoken));
                }
            }
        }
    }
    
    xMsg.type = CY_USB_PRINT_LOGS;
    xQueueSendFromISR(pAppCtxt->appMsgQueue, &(xMsg), &(xHigherPriorityTaskWoken));
    DBG_APP_INFO("AppSetCfgCbEnd Done\r\n");
    return;
} /* end of function */

/**
 * \name Cy_USB_HBDMAChannelDestroy
 * \brief Function to destroy all the enabled HBDMA channels
 * \param pAppCtxt Application layer context pointer
 * \retval None
 */
void Cy_USB_HBDMAChannelDestroy(cy_stc_usb_app_ctxt_t *pAppCtxt)
{
    /* Destroy HBDMA channel used for CDC-debug interface*/
    if(pAppCtxt->cdcDebugEnabled)
    {
        Cy_Debug_LogDeInit();
    }
}

/**
 * \name Cy_USB_AppBusResetCallback
 * \brief Callback function will be invoked by USBD when bus detects RESET
 * \param pCtxt Application layer context pointer
 * \param pUsbdCtxt USBD layer context pointer
 * \param pMsg USB Message
 * \retval None
 */
void Cy_USB_AppBusResetCallback(void* pCtxt, cy_stc_usb_usbd_ctxt_t* pUsbdCtxt,
                                cy_stc_usb_cal_msg_t *pMsg)
{
    cy_stc_usb_app_ctxt_t *pAppCtxt;
    pAppCtxt = (cy_stc_usb_app_ctxt_t*)pCtxt;

    DBG_APP_INFO("AppBusResetCallback\r\n");

    Cy_USB_AppDisableEndpDma(pAppCtxt);

    /*
     * USBD layer takes care of reseting its own data structure as well as
     * takes care of calling CAL reset APIs. Application needs to take care
     * of reseting its own data structure as well as "device function".
     */
    Cy_USB_AppInit(pAppCtxt, pUsbdCtxt, pAppCtxt->pCpuDmacBase,
                   pAppCtxt->pCpuDw0Base, pAppCtxt->pCpuDw1Base,
                   pAppCtxt->pHbDmaMgrCtxt);
    pAppCtxt->devState = CY_USB_DEVICE_STATE_RESET;
    pAppCtxt->prevDevState = CY_USB_DEVICE_STATE_RESET;
    pAppCtxt->isApplnActive = false;

    return;
} /* end of function. */

/**
 * \name Cy_USB_AppBusResetDoneCallback
 * \brief Callback function will be invoked by USBD when RESET is completed
 * \param pCtxt Application layer context pointer
 * \param pUsbdCtxt USBD layer context pointer
 * \param pMsg USB Message
 * \retval None
 */
void Cy_USB_AppBusResetDoneCallback(void* pCtxt,
                                    cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                                    cy_stc_usb_cal_msg_t *pMsg)
{
    cy_stc_usb_app_ctxt_t *pAppCtxt;

    pAppCtxt = (cy_stc_usb_app_ctxt_t*)pCtxt;
    pAppCtxt->devState = CY_USB_DEVICE_STATE_DEFAULT;
    pAppCtxt->prevDevState = pAppCtxt->devState;
    return;
} /* end of function. */

/**
 * \name Cy_USB_AppBusSpeedCallback
 * \brief Callback function will be invoked by USBD when speed is identified or speed change is detected
 * \param pCtxt Application layer context pointer
 * \param pUsbdCtxt USBD context
 * \param pMsg USB Message
 * \retval None
 */
void Cy_USB_AppBusSpeedCallback(void* pCtxt, cy_stc_usb_usbd_ctxt_t* pUsbdCtxt,
                                cy_stc_usb_cal_msg_t *pMsg)
{
    cy_stc_usb_app_ctxt_t *pAppCtxt;

    pAppCtxt = (cy_stc_usb_app_ctxt_t*)pCtxt;
    pAppCtxt->devState = CY_USB_DEVICE_STATE_DEFAULT;
    pAppCtxt->devSpeed = Cy_USBD_GetDeviceSpeed(pUsbdCtxt);

    return;
} /* end of function. */

/**
 * \name Cy_USB_AppSetupCallback
 * \brief Callback function will be invoked by USBD when SETUP packet is received
 * \param pCtxt Application layer context pointer
 * \param pUsbdCtxt USBD context
 * \param pMsg USB Message
 * \retval None
 */
void Cy_USB_AppSetupCallback(void *pCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                             cy_stc_usb_cal_msg_t *pMsg)
{
    cy_stc_usb_app_ctxt_t *pAppCtxt;
    pAppCtxt = (cy_stc_usb_app_ctxt_t *)pCtxt;
    uint8_t bRequest, bReqType;
    uint8_t bType, bTarget;
    uint16_t wValue, wIndex, wLength;
    bool isReqHandled = false;
    cy_en_usbd_ret_code_t retStatus = CY_USBD_STATUS_SUCCESS;
    cy_en_usb_endp_dir_t epDir = CY_USB_ENDP_DIR_INVALID;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    cy_stc_usbd_app_msg_t xMsg;
    uint8_t functionNum =0;
	
    cy_stc_app_info_t *appInfoPtr = pAppCtxt->appInfoPtr;

    DBG_APP_TRACE("AppSetupCallback\r\n");

    /* Fast enumeration is used. Only requests addressed to the interface, class,
     * vendor and unknown control requests are received by this function. */

    /* Decode the fields from the setup request. */
    bReqType = pUsbdCtxt->setupReq.bmRequest;
    bType = ((bReqType & CY_USB_CTRL_REQ_TYPE_MASK) >> CY_USB_CTRL_REQ_TYPE_POS);
    bTarget = (bReqType & CY_USB_CTRL_REQ_RECIPENT_OTHERS);
    bRequest = pUsbdCtxt->setupReq.bRequest;
    wValue = pUsbdCtxt->setupReq.wValue;
    wIndex = pUsbdCtxt->setupReq.wIndex;
    wLength = pUsbdCtxt->setupReq.wLength;


    DBG_APP_TRACE("CTRLRQ: %x %x %x %x %x \r\n", bReqType, bRequest, wValue, wIndex, wLength);

    if (bType == CY_USB_CTRL_REQ_STD)
    {
        DBG_APP_TRACE("CY_USB_CTRL_REQ_STD\r\n");
        if (bRequest == CY_USB_SC_SET_FEATURE)
        {
            DBG_APP_INFO("CY_USB_SC_SET_FEATURE: wIndex %x\r\n");
            /* Function Suspend: ACK the request if the device is in configured state in an USB 3.x connection. */
            if ((bTarget == CY_USB_CTRL_REQ_RECIPENT_INTF) && (wValue == 0))
            {

                Cy_USB_USBD_EndpSetClearStall(pUsbdCtxt, 0x00, CY_USB_ENDP_DIR_IN, TRUE);
                isReqHandled = true;
            }

            if ((bTarget == CY_USB_CTRL_REQ_RECIPENT_ENDP) && (bRequest == CY_USB_SC_SET_FEATURE))
            {
                epDir = ((wIndex & 0x80UL) ? (CY_USB_ENDP_DIR_IN) : (CY_USB_ENDP_DIR_OUT));
                isReqHandled = true;
            }

            if ((bTarget == CY_USB_CTRL_REQ_RECIPENT_DEVICE) &&
                    ((wValue == CY_USB_FEATURE_U1_ENABLE) || (wValue == CY_USB_FEATURE_U2_ENABLE))) {
                /* Set U1/U2 enable. Just ACK the request. */
                Cy_USBD_SendACkSetupDataStatusStage(pUsbdCtxt);
                isReqHandled = true;
            }
        }

        if (bRequest == CY_USB_SC_CLEAR_FEATURE)
        {
            DBG_APP_INFO("CY_USB_SC_CLEAR_FEATURE wIndex %x\r\n",wIndex);

            if ((bTarget == CY_USB_CTRL_REQ_RECIPENT_ENDP) && (wValue == CY_USB_FEATURE_ENDP_HALT))
            {              
                DBG_APP_TRACE("CY_USB_CTRL_REQ_RECIPENT_ENDP && CY_USB_FEATURE_ENDP_HALT\r\n");
                epDir = ((wIndex & 0x80UL) ? (CY_USB_ENDP_DIR_IN) : (CY_USB_ENDP_DIR_OUT));

                Cy_USBD_FlushEndp(pUsbdCtxt, ((uint32_t)wIndex & 0x7FUL), epDir);
                Cy_USBD_ResetEndp(pUsbdCtxt, ((uint32_t)wIndex & 0x7FUL), epDir,false);
                Cy_USB_USBD_EndpSetClearStall(pUsbdCtxt, ((uint32_t)wIndex & 0x7FUL),
                                epDir, false);
                Cy_USBD_SendAckSetupDataStatusStage(pUsbdCtxt);
                
                isReqHandled = true;
            }

            if ((bTarget == CY_USB_CTRL_REQ_RECIPENT_DEVICE) &&
                    ((wValue == CY_USB_FEATURE_U1_ENABLE) || (wValue == CY_USB_FEATURE_U2_ENABLE))) {
                /* Set U1/U2 disable. Just ACK the request. */
                Cy_USBD_SendACkSetupDataStatusStage(pUsbdCtxt);
                isReqHandled = true;
            }
        }

        /* Handle Microsoft OS String Descriptor request. */
        if ((bTarget == CY_USB_CTRL_REQ_RECIPENT_DEVICE) &&
            (bRequest == CY_USB_SC_GET_DESCRIPTOR) &&
            (wValue == ((CY_USB_STRING_DSCR << 8) | 0xEE))) {

            /* Make sure we do not send more data than requested. */
            if (wLength > glOsString[0]) {
                wLength = glOsString[0];
            }
            DBG_APP_INFO("OSString\r\n");
            retStatus = Cy_USB_USBD_SendEndp0Data(pAppCtxt->pUsbdCtxt,
                                                    glOsString, wLength);
            if (retStatus != CY_USBD_STATUS_SUCCESS) {
                DBG_APP_ERR("SendEp0Fail\r\n");
            } else {
                isReqHandled = true;
                return;
            }
        }
    }

    uint8_t interface = CY_USB_GET_LSB(wIndex);
    functionNum = INFO_FN_NUM(appInfoPtr,interface);

    /* Check for Class Requests */
    if (bType == CY_USB_CTRL_REQ_CLASS)
    {
        DBG_APP_TRACE("USB Class Requests: function %d %d %d %d \r\n", functionNum, bReqType, interface, functionNum);
        switch(bReqType)
        {
            case  CY_USB_CLASS_GET_REQ_TYPE:
            case  CY_USB_CLASS_SET_REQ_TYPE:
            {
                /* Handle requests addressed to the  interface. */
                DBG_APP_TRACE("CDC Class Requests\r\n");
                isReqHandled = true;
                if (bRequest == CDC_SET_LINE_CODING)
                {
                    xMsg.type    = CY_USB_VCOM_SETLINECODING_CMD;
                    xMsg.data[0] = wLength;
                    xMsg.data[1] = functionNum;
                    xQueueSendFromISR(pAppCtxt->vcomDeviceQueue, &(xMsg), &(xHigherPriorityTaskWoken));
                }
                else
                {
                    isReqHandled = Cy_USB_HandleCDCRequests(pAppCtxt,bRequest,wLength,wValue);
                }
            }
            break;
        }
        /* Don't try to stall the endpoint if we have already attempted data transfer. */
    }

    /*
     * If Request is not handled by the callback, Stall the command.
     */
    if (!isReqHandled)
    {
        DBG_APP_TRACE("STALL\n\r");
        Cy_USB_USBD_EndpSetClearStall(pUsbdCtxt, 0x00,
                                      CY_USB_ENDP_DIR_IN, TRUE);
    }
} /* end of function. */

/**
 * \name Cy_USB_AppSuspendCallback
 * \brief Callback function will be invoked by USBD when Suspend signal/message is detected
 * \param pCtxt Application layer context pointer
 * \param pUsbdCtxt USBD context
 * \param pMsg USB Message
 * \retval None
 */
void Cy_USB_AppSuspendCallback(void* pCtxt, cy_stc_usb_usbd_ctxt_t* pUsbdCtxt,
                               cy_stc_usb_cal_msg_t *pMsg)
{
    cy_stc_usb_app_ctxt_t *pAppCtxt;

    pAppCtxt = (cy_stc_usb_app_ctxt_t*)pCtxt;
    pAppCtxt->prevDevState = pAppCtxt->devState;
    pAppCtxt->devState = CY_USB_DEVICE_STATE_SUSPEND;
} /* end of function. */

/**
 * \name Cy_USB_AppResumeCallback
 * \brief Callback function will be invoked by USBD when Resume signal/message is detected
 * \param pCtxt Application layer context pointer
 * \param pUsbdCtxt USBD context
 * \param pMsg USB Message
 * \retval None
 */
void Cy_USB_AppResumeCallback(void *pCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                              cy_stc_usb_cal_msg_t *pMsg)
{
    cy_stc_usb_app_ctxt_t *pAppCtxt;
    cy_en_usb_device_state_t tempState;

    pAppCtxt = (cy_stc_usb_app_ctxt_t *)pCtxt;

    tempState = pAppCtxt->devState;
    pAppCtxt->devState = pAppCtxt->prevDevState;
    pAppCtxt->prevDevState = tempState;
    return;
} /* end of function. */

/**
 * \name Cy_USB_AppSetIntfCallback
 * \brief Callback function will be invoked by USBD when SET_INTERFACE is received
 * \param pAppCtxt Application layer context pointer
 * \param pUsbdCtxt USBD context
 * \param pMsg USB Message
 * \retval None
 */
void Cy_USB_AppSetIntfCallback(void* pAppCtxt, cy_stc_usb_usbd_ctxt_t* pUsbdCtxt,
    cy_stc_usb_cal_msg_t* pMsg)
{
    cy_stc_usb_setup_req_t* pSetupReq;
    uint8_t intfNum, altSetting;
    uint8_t functionNum;
    cy_stc_app_info_t *appInfoPtr = ((cy_stc_usb_app_ctxt_t*)pAppCtxt)->appInfoPtr;

    pSetupReq = &(pUsbdCtxt->setupReq);
    intfNum = pSetupReq->wIndex;
    altSetting = pSetupReq->wValue;

    uint8_t interface = CY_USB_GET_LSB(pSetupReq->wIndex);

    functionNum = INFO_FN_NUM(appInfoPtr,interface);
    DBG_APP_TRACE("SetIntf(%d) on interface %d functionNum:%d \r\n", altSetting, intfNum, functionNum);

    /* Interface does not support multiple alternate settings: Stall the SET_INTERFACE request. */
    DBG_APP_TRACE("SetIntf(%d) on interface %d: Stalled\r\n", altSetting, intfNum);
    Cy_USB_USBD_EndpSetClearStall(pUsbdCtxt, 0x00, CY_USB_ENDP_DIR_IN, true);
    DBG_APP_INFO("AppSetIntfCallback done\r\n");
} /* end of function. */

/**
 * \name Cy_USB_AppZlpCallback
 * \brief This function will be called by USBD layer when ZLP message comes
 * \param pAppCtxt Application layer context pointer
 * \param pUsbdCtxt USBD context
 * \param pMsg USB Message
 * \retval None
 */
void Cy_USB_AppZlpCallback(void *pAppCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                           cy_stc_usb_cal_msg_t *pMsg)
{
   DBG_APP_TRACE("AppZlpCb\r\n");

    return;
} /* end of function. */

/**
 * \name Cy_USB_AppL1SleepCallback
 * \brief This function will be called by USBD layer when L1 Sleep message comes.
 * \param pAppCtxt Application layer context pointer
 * \param pUsbdCtxt USBD context
 * \param pMsg USB Message
 * \retval None
 */
void Cy_USB_AppL1SleepCallback(void *pAppCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                               cy_stc_usb_cal_msg_t *pMsg)
{
    DBG_APP_INFO("AppL1SleepCb\r\n");
    return;
} /* end of function. */

/**
 * \name Cy_USB_AppL1ResumeCallback
 * \brief This function will be called by USBD layer when L1 Resume message comes.
 * \param pAppCtxt Application layer context pointer
 * \param pUsbdCtxt USBD context
 * \param pMsg USB Message
 * \retval None
 */
void Cy_USB_AppL1ResumeCallback(void *pAppCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                                cy_stc_usb_cal_msg_t *pMsg)
{
    DBG_APP_INFO("AppL1ResumeCb\r\n");
    return;
} /* end of function. */

/**
 * \name Cy_USB_AppSlpCallback
 * \brief This function will be called by USBD layer when SLP message comes.
 * \param pAppCtxt Application layer context pointer
 * \param pUsbdCtxt USBD context
 * \param pMsg USB Message
 * \retval None
 */
void Cy_USB_AppSlpCallback(void *pAppCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                           cy_stc_usb_cal_msg_t *pMsg)
{
    DBG_APP_TRACE("AppSlpCb\r\n");
    cy_stc_usb_app_ctxt_t *usbApp;
    usbApp = (cy_stc_usb_app_ctxt_t *)pAppCtxt;
    uint8_t i = 0;
    uint8_t cdcIndex = CY_APP_ERROR_CODE;

    /*Handle SLP for CDC OUT endpoint*/
    if(usbApp->appInfoPtr->cdcCount >= 1)
    {
        if (pMsg->type == CY_USB_CAL_MSG_OUT_SLP) 
        {
            for(i=0;i<usbApp->numCDCFunctions ;i++)
            {
                if(usbApp->cdcInfo[i]->vcomOutEpNum == pMsg->data[0])
                {
                    cdcIndex = i;
                }
            }
            
            if(cdcIndex == CY_APP_ERROR_CODE)
                return;

             /*
             * Notify the HBDMA manager about the short packet so it can complete
             * the pending DW read with the actual received byte count.
             * The DW completion ISR will then raise a PROD_EVENT callback.
             */
            Cy_HBDma_Mgr_HandleUsbShortInterrupt(usbApp->pHbDmaMgrCtxt,
                    pMsg->data[0], pMsg->data[1]);
        }
    }
    return;
} /* end of function. */

/**
 * \name Cy_USB_AppSetFeatureCallback
 * \brief This function will be called by USBD layer when set feature message comes.
 * \param pAppCtxt Application layer context pointer
 * \param pUsbdCtxt USBD context
 * \param pMsg USB Message
 * \retval None
 */
void Cy_USB_AppSetFeatureCallback(void *pAppCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                                  cy_stc_usb_cal_msg_t *pMsg)
{
    DBG_APP_INFO("AppSetFeatureCb\r\n");
    return;
} /* end of function. */

/**
 * \name Cy_USB_AppClearFeatureCallback
 * \brief This function will be called by USBD layer when clear feature message comes.
 * \param pAppCtxt Application layer context pointer
 * \param pUsbdCtxt USBD context
 * \param pMsg USB Message
 * \retval None
 */
void Cy_USB_AppClearFeatureCallback(void *pAppCtxt,
                                    cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                                    cy_stc_usb_cal_msg_t *pMsg)
{
    DBG_APP_INFO("AppClearFeatureCb\r\n");
    return;
} /* end of function. */

/**
 * \name Cy_USB_AppQueueWrite
 * \brief Queue USBHS Write on the USB endpoint
 * \param pAppCtxt Application layer context pointer
 * \param endpNumber Endpoint number
 * \param pBuffer Data Buffer Pointer
 * \param dataSize DataSize to send on USB bus
 * \retval None
 */
void Cy_USB_AppQueueWrite(cy_stc_usb_app_ctxt_t* pAppCtxt, uint8_t endpNumber,
    uint8_t* pBuffer, uint16_t dataSize)
{
    cy_stc_app_endp_dma_set_t* dmaset_p;
    
    /* Null pointer checks. */
    if ((pAppCtxt == NULL) || (pAppCtxt->pUsbdCtxt == NULL) ||
        (pAppCtxt->pCpuDw1Base == NULL) || (pBuffer == NULL)) {
        DBG_APP_ERR("QueueWrite Err0\r\n");
        return;
    }

    /*
     * Verify that the selected endpoint is valid and the dataSize
     * is non-zero.
     */
    dmaset_p = &(pAppCtxt->endpInDma[endpNumber]);
    if ((dmaset_p->valid == 0) || (dataSize == 0)) {
        DBG_APP_ERR("QueueWrite Err1\r\n");
        return;
    }
    
    Cy_USBHS_App_QueueWrite(dmaset_p, pBuffer, dataSize);
} /* end of function */

/**
 * \name Cy_USB_AppInitDmaIntr
 * \brief Function to register an ISR for the DMA channel associated with an endpoint
 * \param endpNumber USB endpoint number
 * \param endpDirection Endpoint direction
 * \param userIsr ISR function pointer. Can be NULL if interrupt is to be disabled.
 * \retval None
 */
void Cy_USB_AppInitDmaIntr(uint32_t endpNumber, cy_en_usb_endp_dir_t endpDirection,
    cy_israddress userIsr)
{
    cy_stc_sysint_t intrCfg;
    if ((endpNumber > 0) && (endpNumber < CY_USB_MAX_ENDP_NUMBER))
    {
#if (!CY_CPU_CORTEX_M4)
        intrCfg.intrPriority = 3;
        if (endpDirection == CY_USB_ENDP_DIR_IN)
        {
            intrCfg.intrSrc = NvicMux1_IRQn;
            /* DW1 channels 0 onwards are used for IN endpoints. */
            intrCfg.cm0pSrc = (cy_en_intr_t)(cpuss_interrupts_dw1_0_IRQn + endpNumber);
        }
        else
        {
            intrCfg.intrSrc = NvicMux4_IRQn;
            /* DW0 channels 0 onwards are used for OUT endpoints. */
            intrCfg.cm0pSrc = (cy_en_intr_t)(cpuss_interrupts_dw0_0_IRQn + endpNumber);
        }
#else
        intrCfg.intrPriority = 5;
        if (endpDirection == CY_USB_ENDP_DIR_IN)
        {
            /* DW1 channels 0 onwards are used for IN endpoints. */
            intrCfg.intrSrc =
                (IRQn_Type)(cpuss_interrupts_dw1_0_IRQn + endpNumber);
        }
        else
        {
            /* DW0 channels 0 onwards are used for OUT endpoints. */
            intrCfg.intrSrc =
                (IRQn_Type)(cpuss_interrupts_dw0_0_IRQn + endpNumber);
        }
#endif /* (!CY_CPU_CORTEX_M4) */

        if (userIsr != NULL)
        {
            /* If an ISR is provided, register it and enable the interrupt. */
            Cy_SysInt_Init(&intrCfg, userIsr);
            NVIC_EnableIRQ(intrCfg.intrSrc);
        }
        else
        {
            /* ISR is NULL. Disable the interrupt. */
            NVIC_DisableIRQ(intrCfg.intrSrc);
        }
    }
} /* end of function. */

/**
 * \name Cy_USB_AppClearDmaInterrupt
 * \brief Clear DMA Interrupt
 * \param pAppCtxt Application layer context pointer
 * \param endpNumber Endpoint number
 * \param endpDirection Endpoint direction
 * \retval None
 */
void Cy_USB_AppClearDmaInterrupt(cy_stc_usb_app_ctxt_t* pAppCtxt,
    uint32_t endpNumber, cy_en_usb_endp_dir_t endpDirection)
{
    if ((pAppCtxt != NULL) && (endpNumber > 0) &&
        (endpNumber < CY_USB_MAX_ENDP_NUMBER)) {
        if (endpDirection == CY_USB_ENDP_DIR_IN) {
            Cy_USBHS_App_ClearDmaInterrupt(&(pAppCtxt->endpInDma[endpNumber]));
        }
        else {
            Cy_USBHS_App_ClearDmaInterrupt(&(pAppCtxt->endpOutDma[endpNumber]));
        }
    }
} /* end of function. */

/**
 * \name Cy_CheckStatus
 * \brief Function that handles prints error log
 * \param function Pointer to function
 * \param line Line number where error is seen
 * \param condition Condition of failure
 * \param value Error code
 * \param isBlocking Blocking function
 * \retval None
 */
void Cy_CheckStatus(const char *function, uint32_t line, uint8_t condition, uint32_t value, uint8_t isBlocking)
{
    if (!condition)
    {
        /* Application failed with the error code status */
        Cy_Debug_AddToLog(1, RED);
        Cy_Debug_AddToLog(1, "Function %s failed at line %d with status = 0x%x\r\n", function, line, value);
        Cy_Debug_AddToLog(1, RESET_COLOR);
        if (isBlocking)
        {
            /* Loop indefinitely */
            for (;;)
            {
            }
        }
    }
}

/**
 * \name Cy_CheckStatusAndHandleFailure
 * \brief Function that handles prints error log
 * \param function Pointer to function
 * \param line Line number where error is seen
 * \param condition Condition of failure
 * \param value Error code
 * \param isBlocking Blocking function
 * \param failureHandler Failure handler function
 * \retval None
 */
void Cy_CheckStatusAndHandleFailure(const char *function, uint32_t line, uint8_t condition, uint32_t value, uint8_t isBlocking, void (*failureHandler)(void))
{
    if (!condition)
    {
        /* Application failed with the error code status */
        Cy_Debug_AddToLog(1, RED);
        Cy_Debug_AddToLog(1, "Function %s failed at line %d with status = 0x%x\r\n", function, line, value);
        Cy_Debug_AddToLog(1, RESET_COLOR);

        if(failureHandler != NULL)
        {
            (*failureHandler)();
        }


        if (isBlocking)
        {
            /* Loop indefinitely */
            for (;;)
            {
            }
        }
    }
}

/**
 * \name Cy_FailHandler
 * \brief Error Handler
 * \retval None
 */
void Cy_FailHandler(void)
{
    Cy_Debug_AddToLog(1, RED);
    Cy_Debug_AddToLog(1, "Failure Handler\r\n");
    Cy_Debug_AddToLog(1, RESET_COLOR);
}

/* [EOF] */
