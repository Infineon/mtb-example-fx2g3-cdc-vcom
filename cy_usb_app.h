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



#ifndef _CY_USB_APP_H_
#define _CY_USB_APP_H_

#include "cy_pdl.h"
#include "cy_debug.h"
#include "cy_usbhs_dw_wrapper.h"
#include "vcom_config.h"

/*****************************************************************************
*                             MACROS                                          *
******************************************************************************/

#define CY_APP_ERROR_CODE                     (0xFF)
#define MAX_DMA_BUFFER_SIZE                   (65536)
#define DBG_UART_RECV_BUFFER                  (512)
#define DELAY_MICRO(us)                       Cy_SysLib_DelayUs(us)
#define DELAY_MILLI(ms)                       Cy_SysLib_Delay(ms)

#define USB_FW_DEBUG_INTERFACE(cfgTablePtr)      (cfgTablePtr->fwDbgInterface)
#define USB_LOG_LEVEL(cfgTablePtr)               (cfgTablePtr->logLevel)
#define LOGBUF_RAM_SZ                            (1024U)

#define RED                                   "\033[0;31m"
#define CYAN                                  "\033[0;36m"
#define RESET_COLOR                           "\033[0m"

#define DBG_APP_LEVEL0(str, ...)             Cy_Debug_AddToLog(0, "Info:APP:"str, ##__VA_ARGS__)
#define DBG_APP_LEVEL1(str, ...)             Cy_Debug_AddToLog(1, "Info:APP:"str, ##__VA_ARGS__)
#define DBG_APP_LEVEL2(str, ...)             Cy_Debug_AddToLog(2, "Info:APP:"str, ##__VA_ARGS__)

#define LOG_CLR(CLR, ...)                    Cy_Debug_AddToLog(1,CLR);\
                                             Cy_Debug_AddToLog(1,__VA_ARGS__); \
                                             Cy_Debug_AddToLog(1,RESET_COLOR);


#define ASSERT(condition, value)             Cy_CheckStatus(__func__, __LINE__, condition, value, true);
#define ASSERT_NON_BLOCK(condition, value)   Cy_CheckStatus(__func__, __LINE__, condition, value, false);
#define ASSERT_AND_HANDLE(condition, value, failureHandler) Cy_CheckStatusAndHandleFailure(__func__, __LINE__, condition, value, false, failureHandler);

/*MAX DMA Buffer Count : VCOM*/
#define VCOM_APP_BUFFER_CNT                      (255u)   
#define MAX_CDC_FUNCTIONS                        (4)

#define NUM_INTERFACES                           (2)
#define NUM_INTERFACES_CDC                       (2)

#define USB_HS_BULK_XFER_SIZE 512
#define USB_FS_BULK_XFER_SIZE 64

#define CY_USB_VBUS_CHANGE_INTR                     (0x0E)
#define CY_USB_VBUS_CHANGE_DEBOUNCED                (0x0F)

#define CY_USB_VCOM_DEVICE_MSG_START_DATA_XFER      (0x17)
#define CY_USB_VCOM_DEVICE_MSG_READ_COMPLETE        (0x18)
#define CY_USB_VCOM_DEVICE_MSG_SLP_OUT              (0x19)
#define CY_USB_VCOM_DEVICE_MSG_WRITE_COMPLETE       (0x1A)
#define CY_USB_VCOM_SETLINECODING_CMD               (0x1D)
#define CY_USB_UART_MSG_WRITE_COMPLETE              (0x1B)
#define CY_USB_UART_MSG_READ_COMPLETE               (0x1C)

#define CY_USB_DEVICE_MSG_QUEUE_SIZE    16
#define CY_USB_DEVICE_MSG_SIZE          (sizeof (cy_stc_usbd_app_msg_t))

#define	SET_BIT(byte, mask)		                 (byte) |= (mask)
#define CLR_BIT(byte, mask)		                 (byte) &= ~(mask)
#define CHK_BIT(byte, mask)		                 (byte) & (mask)

#define PORT0                                    (0)
#define PORT1                                    (1)

void Cy_CheckStatus(const char *function, uint32_t line, uint8_t condition, uint32_t value, uint8_t isBlocking);

#define CY_USB_CLASS_SET_REQ_TYPE                   (uint8_t)(0x21)    /* Class specific Interface SET request type */
#define CY_USB_CLASS_GET_REQ_TYPE                   (uint8_t)(0xA1)    /* Class specific Interface GET request type */

#define CY_USB_PRINT_LOGS                          (0x25)
#define MS_VENDOR_CODE                             (0xF0)


extern uint8_t glOsString[];


/* P4.0 is used for VBus detect functionality. */
#define VBUS_DETECT_GPIO_PORT                       (P4_0_PORT)
#define VBUS_DETECT_GPIO_PIN                        (P4_0_PIN)
#define VBUS_DETECT_GPIO_INTR                       (ioss_interrupts_gpio_dpslp_4_IRQn)
#define VBUS_DETECT_STATE(cfgTablePtr)              (cfgTablePtr->vbusDetectPolarity)



/*****************************************************************************
*                             Structures                                      *
******************************************************************************/
typedef struct cy_stc_usb_app_ctxt_ cy_stc_usb_app_ctxt_t;

/* USBD layer return code shared between USBD layer and Application layer. */
typedef enum cy_en_usb_app_ret_code_ {
    CY_USB_APP_STATUS_SUCCESS=0,
    CY_USB_APP_STATUS_FAILURE,
}cy_en_usb_app_ret_code_t;

typedef struct 
{
    uint8_t functionNum;
}cy_stc_app_interface_details_t;


typedef struct __attribute__((aligned (4)))
{
    uint8_t totalNumInterfaces; /* For bNumInterfaces in Standard Configuration Descriptor*/
    uint8_t totalFunctions;
    uint8_t cdcCount;
    cy_stc_app_interface_details_t usbInterface[CY_MAX_CDC_INTERFACES * NUM_INTERFACES];
}cy_stc_app_info_t;

/* CDC Info structure */
typedef struct cy_stc_cdc_info_t
{
    uint8_t  cdcFuncIndex;
    uint8_t  dmaRxBufferCount;                                      /** Number of buffers used to receive from UART */
    uint8_t  vcomOutEpNum;                                          /** Index of OUT endpoint used for VCOM i/f. */
    uint8_t  vcomInEpNum;                                           /** Index of IN endpoint used for VCOM i/f. */
    uint8_t *pUartRxBuffer[VCOM_APP_BUFFER_CNT];                    /** Pointer of buffers to read UART data into. */
    uint16_t uartRxDataLen[VCOM_APP_BUFFER_CNT];                    /** Amount of data in each UART RX buffer. */
    uint8_t  dmaTxBufferCount;                                      /** Number of buffers used to send data to UART */
    uint8_t  uartRxBufIndex;                                        /** Index of current UART read buffer. */
    uint8_t  nxtUsbTxBufIdx;                                        /** Index of next buffer to be sent on USB EP. */
    uint8_t  uartRxFreeBufCount;                                    /** Number of free UART RX buffers. */
    bool     usbInXferPending;                                      /** Whether a write has been queued on IN EP. */

    uint8_t *pUsbRxBuffer[VCOM_APP_BUFFER_CNT];                     /** Pointer of buffers to read USB data into. */
    uint16_t usbRxDataLen[VCOM_APP_BUFFER_CNT];                     /** Amount of data in each USB RX buffer. */
    uint16_t usbOutPktLen;                                          /** Length of current USB OUT packet. */
    uint8_t  nxtUsbRxBufIdx;                                        /** Index of next USB read buffer. */
    uint8_t  uartTxBufIndex;                                        /** Index of current UART write buffer. */
    uint8_t  usbRxBusyBufCount;                                     /** Number of used USB RX buffers. */
    bool     blockUartReads;                                        /** Whether read from UART should be blocked. */
    bool     usbZlpReqd;                                        /** Whether a ZLP needs to be sent on USB. */
}cy_stc_cdc_info_t;

/*
 * USB application data structure which is bridge between USB system and device
 * functionality.
 * It maintains some usb system information which comes from USBD and it also
 * maintains info about functionality.
 */
struct cy_stc_usb_app_ctxt_
{
    uint8_t firstInitDone;
    uint8_t appInit;
    uint8_t descriptorPopulated;
    uint8_t devAddr;
    uint8_t activeCfgNum;
    uint8_t prevAltSetting;
    cy_en_usb_device_state_t devState;
    cy_en_usb_device_state_t prevDevState;
    cy_en_usb_speed_t devSpeed;
    cy_en_usb_enum_method_t enumMethod;

    bool usbConnectDone;
    bool vbusChangeIntr;                        /** VBus change interrupt received flag. */
    bool vbusPresent;                           /** VBus presence indicator flag. */
    bool usbConnected;                          /** Whether USB connection is enabled. */
    TimerHandle_t vbusDebounceTimer;            /** VBus change debounce timer handle. */

    cy_stc_app_endp_dma_set_t endpInDma[CY_USB_MAX_ENDP_NUMBER];
    cy_stc_app_endp_dma_set_t endpOutDma[CY_USB_MAX_ENDP_NUMBER];

    DMAC_Type *pCpuDmacBase;
    DW_Type *pCpuDw0Base;
    DW_Type *pCpuDw1Base;

    uint8_t isApplnActive;

    /* DMA channel Specific Fields*/
    uint8_t numOfHbdmaChannel;
    cy_stc_hbdma_mgr_context_t *pHbDmaMgrCtxt;
    cy_stc_usb_usbd_ctxt_t *pUsbdCtxt;

    cy_stc_scb_uart_context_t *pUartCtxt[MAX_CDC_FUNCTIONS];            /** SCB-UART context pointer. */

    /* Global Task handles */
    TaskHandle_t usbDevicetaskHandle;           /** USB device application task. */
    QueueHandle_t appMsgQueue;                  /** Message queue used to send messages to the device task. */

    uint8_t cdcDebugEnabled;
    uint8_t numCDCFunctions;
    uint8_t uartSCB0Enabled;
    uint8_t uartSCB1Enabled;
    uint8_t uartSCB4Enabled;
    uint8_t uartSCB6Enabled;
    uint32_t vcomtimerExpiry[MAX_CDC_FUNCTIONS];                    /** VCOM timer duration in milliseconds. */
    TaskHandle_t vcomDevicetaskHandle;                              /** VCOM application task handle. */
    QueueHandle_t vcomDeviceQueue;                                  /** VCOM application message queue. */
    TimerHandle_t vcomTimerHandle[MAX_CDC_FUNCTIONS];               /** VCOM application timer handle. */

    cy_stc_hbdma_channel_t *pUartToUsbChn[MAX_CDC_FUNCTIONS];       /** UART to USB DMA channel handle. */
    cy_stc_hbdma_channel_t *pUsbToUartChn[MAX_CDC_FUNCTIONS];       /** USB to UART DMA channel handle. */

    cy_stc_cdc_info_t *cdcInfo[MAX_CDC_FUNCTIONS];
    cy_stc_usb_dev_config_t *cfgTablePtr;      /** Configuration Table*/
    cy_stc_app_info_t       *appInfoPtr;
};

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
void Cy_USB_AppInit(cy_stc_usb_app_ctxt_t *pAppCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                    DMAC_Type *pCpuDmacBase, DW_Type *pCpuDw0Base,
                    DW_Type *pCpuDw1Base, cy_stc_hbdma_mgr_context_t *pHbDmaMgrCtxt);

/**
 * \name Cy_USB_AppRegisterCallback
 * \brief This function will register all callbacks with USBD layer.
 * \param pAppCtxt Application layer context pointer
 * \retval None
 */
void Cy_USB_AppRegisterCallback(cy_stc_usb_app_ctxt_t *pAppCtxt);


/**
 * \name Cy_USB_AppSetCfgCallback
 * \brief Callback function will be invoked by USBD when set configuration is received
 * \param pCtxt Application layer context pointer
 * \param pUsbdCtxt USBD layer context pointer
 * \param pMsg USB Message
 * \retval None
 */
void Cy_USB_AppSetCfgCallback(void *pAppCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                                    cy_stc_usb_cal_msg_t *pMsg);

/**
 * \name Cy_USB_AppBusResetCallback
 * \brief Callback function will be invoked by USBD when bus detects RESET
 * \param pCtxt Application layer context pointer
 * \param pUsbdCtxt USBD layer context pointer
 * \param pMsg USB Message
 * \retval None
 */
void Cy_USB_AppBusResetCallback(void *pAppCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                                cy_stc_usb_cal_msg_t *pMsg);

/**
 * \name Cy_USB_AppBusResetDoneCallback
 * \brief Callback function will be invoked by USBD when RESET is completed
 * \param pCtxt Application layer context pointer
 * \param pUsbdCtxt USBD layer context pointer
 * \param pMsg USB Message
 * \retval None
 */
void Cy_USB_AppBusResetDoneCallback(void *pAppCtxt,cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                                    cy_stc_usb_cal_msg_t *pMsg);

/**
 * \name Cy_USB_AppBusSpeedCallback
 * \brief Callback function will be invoked by USBD when speed is identified or speed change is detected
 * \param pCtxt Application layer context pointer
 * \param pUsbdCtxt USBD context
 * \param pMsg USB Message
 * \retval None
 */
void Cy_USB_AppBusSpeedCallback(void *pAppCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                                cy_stc_usb_cal_msg_t *pMsg);

/**
 * \name Cy_USB_AppSetupCallback
 * \brief Callback function will be invoked by USBD when SETUP packet is received
 * \param pCtxt Application layer context pointer
 * \param pUsbdCtxt USBD context
 * \param pMsg USB Message
 * \retval None
 */
void Cy_USB_AppSetupCallback(void *pAppCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                             cy_stc_usb_cal_msg_t *pMsg);

/**
 * \name Cy_USB_AppSuspendCallback
 * \brief Callback function will be invoked by USBD when Suspend signal/message is detected
 * \param pAppCtxt Application layer context pointer
 * \param pUsbdCtxt USBD context
 * \param pMsg USB Message
 * \retval None
 */
void Cy_USB_AppSuspendCallback(void *pAppCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                               cy_stc_usb_cal_msg_t *pMsg);

/**
 * \name Cy_USB_AppResumeCallback
 * \brief Callback function will be invoked by USBD when Resume signal/message is detected
 * \param pAppCtxt Application layer context pointer
 * \param pUsbdCtxt USBD context
 * \param pMsg USB Message
 * \retval None
 */
void Cy_USB_AppResumeCallback (void *pAppCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                               cy_stc_usb_cal_msg_t *pMsg);

/**
 * \name Cy_USB_AppSetIntfCallback
 * \brief Callback function will be invoked by USBD when SET_INTERFACE is received
 * \param pAppCtxt Application layer context pointer
 * \param pUsbdCtxt USBD context
 * \param pMsg USB Message
 * \retval None
 */
void Cy_USB_AppSetIntfCallback(void *pAppCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                               cy_stc_usb_cal_msg_t *pMsg);

/**
 * \name Cy_USB_AppL1SleepCallback
 * \brief This function will be called by USBD layer when L1 Sleep message comes.
 * \param pAppCtxt Application layer context pointer
 * \param pUsbdCtxt USBD context
 * \param pMsg USB Message
 * \retval None
 */
void Cy_USB_AppL1SleepCallback(void *pAppCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                               cy_stc_usb_cal_msg_t *pMsg);

/**
 * \name Cy_USB_AppL1ResumeCallback
 * \brief This function will be called by USBD layer when L1 Resume message comes.
 * \param pAppCtxt Application layer context pointer
 * \param pUsbdCtxt USBD context
 * \param pMsg USB Message
 * \retval None
 */
void Cy_USB_AppL1ResumeCallback(void *pAppCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                                cy_stc_usb_cal_msg_t *pMsg);

/**
 * \name Cy_USB_AppZlpCallback
 * \brief This function will be called by USBD layer when ZLP message comes
 * \param pAppCtxt Application layer context pointer
 * \param pUsbdCtxt USBD context
 * \param pMsg USB Message
 * \retval None
 */
void Cy_USB_AppZlpCallback(void *pAppCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                           cy_stc_usb_cal_msg_t *pMsg);

/**
 * \name Cy_USB_AppSlpCallback
 * \brief This function will be called by USBD layer when SLP message comes.
 * \param pAppCtxt Application layer context pointer
 * \param pUsbdCtxt USBD context
 * \param pMsg USB Message
 * \retval None
 */       
void Cy_USB_AppSlpCallback(void *pAppCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                           cy_stc_usb_cal_msg_t *pMsg);

/**
 * \name Cy_USB_AppSetFeatureCallback
 * \brief This function will be called by USBD layer when set feature message comes.
 * \param pAppCtxt Application layer context pointer
 * \param pUsbdCtxt USBD context
 * \param pMsg USB Message
 * \retval None
 */
void Cy_USB_AppSetFeatureCallback(void *pAppCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                                  cy_stc_usb_cal_msg_t *pMsg);

/**
 * \name Cy_USB_AppClearFeatureCallback
 * \brief This function will be called by USBD layer when clear feature message comes.
 * \param pAppCtxt Application layer context pointer
 * \param pUsbdCtxt USBD context
 * \param pMsg USB Message
 * \retval None
 */
void Cy_USB_AppClearFeatureCallback(void *pAppCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                                  cy_stc_usb_cal_msg_t *pMsg);

/**
 * \name Cy_USB_RegisterUsbDescriptors
 * \brief Function to register USB Descriptor to USBD
 * \param pAppCtxt Pointer to application context
 * \param usbSpeed USB speed
 * \retval None
 */
void Cy_USB_RegisterUsbDescriptors(cy_stc_usb_app_ctxt_t *pAppCtxt,
                                    cy_en_usb_speed_t usbSpeed);

/**
 * \name Cy_USB_AppQueueRead
 * \brief Function to queue read operation on an OUT endpoint.
 * \param pAppCtxt User context
 * \param endpNumber Endpoint number
 * \param pBuffer Buffer pointer
 * \param dataSize Data size to read
 * \retval None
 */
void Cy_USB_AppQueueRead (cy_stc_usb_app_ctxt_t *pAppCtxt, uint8_t endpNumber, 
                          uint8_t *pBuffer, uint16_t dataSize);

/**
 * \name Cy_USB_AppQueueWrite
 * \brief Queue USBHS Write on the USB endpoint
 * \param pAppCtxt Application layer context pointer
 * \param endpNumber Endpoint number
 * \param pBuffer Data Buffer Pointer
 * \param dataSize DataSize to send on USB bus
 * \retval None
 */
void Cy_USB_AppQueueWrite (cy_stc_usb_app_ctxt_t *pAppCtxt, uint8_t endpNumber, 
                            uint8_t *pBuffer, uint16_t dataSize);

/**
 * \name Cy_USB_AppInitDmaIntr
 * \brief Function to register an ISR for the DMA channel associated with an endpoint
 * \param endpNumber USB endpoint number
 * \param endpDirection Endpoint direction
 * \param userIsr ISR function pointer. Can be NULL if interrupt is to be disabled.
 * \retval None
 */
void Cy_USB_AppInitDmaIntr(uint32_t endpNumber,cy_en_usb_endp_dir_t endpDirection,
                           cy_israddress userIsr);

/**
 * \name Cy_USB_AppClearDmaInterrupt
 * \brief Clear DMA Interrupt
 * \param pAppCtxt Application layer context pointer
 * \param endpNumber Endpoint number
 * \param endpDirection Endpoint direction
 * \retval None
 */
void Cy_USB_AppClearDmaInterrupt(cy_stc_usb_app_ctxt_t *pAppCtxt, uint32_t endpNumber, 
                                cy_en_usb_endp_dir_t endpDirection);

/**
 * \name Cy_USB_AppDisableEndpDma
 * \brief This function de-inits all active USB DMA channels as part of USB disconnect process.
 * \param pAppCtxt Pointer to application context
 * \retval None
 */
void Cy_USB_AppDisableEndpDma(cy_stc_usb_app_ctxt_t* pAppCtxt);

/* Function to print a string to UART console. */
void PrintString(const char *string, uint8_t length);

/**
 * \name Cy_USB_EnableUsbHSConnection
 * \brief Enable USBHS connection.
 * \param pAppCtxt Pointer to application context structure
 * \retval bool True if connection enabled successfully
 */
bool Cy_USB_EnableUsbHSConnection(cy_stc_usb_app_ctxt_t *pAppCtxt);

/**
 * \name Cy_USB_DisableUsbHSConnection
 * \brief Disable USBHS connection.
 * \param pAppCtxt Pointer to application context structure
 * \retval None
 */
void Cy_USB_DisableUsbHSConnection(cy_stc_usb_app_ctxt_t *pAppCtxt);

/**
 * \name Cy_USB_AppConfigureEndp
 * \brief Configure all endpoints used by application (except EP0)
 * \param pUsbdCtxt USBD layer context pointer
 * \param pEndpDscr Endpoint descriptor pointer
 * \retval None
 */
void Cy_USB_AppConfigureEndp(cy_stc_usb_usbd_ctxt_t *pUsbdCtxt, uint8_t *pEndpDscr);

/**
 * \name Cy_USB_AppSetupEndpDmaParams
 * \brief This function will setup Endpoint and DMA related parameters before transfer initiated.
 * \param pAppCtxt Pointer to application context structure
 * \param pEndpDscr Endpoint descriptor
 * \param functionNum Function number
 * \param functionType Function type
 * \retval None
 */
void Cy_USB_AppSetupEndpDmaParams(cy_stc_usb_app_ctxt_t *pAppCtxt, uint8_t *pEndpDscr,uint8_t functionNum, uint8_t functionType);

/**
 * \name Cy_MemCopy
 * \brief Wrapper function for memcpy
 * \param dest Destination address
 * \param src Source address
 * \param count Number of bytes to copy from source to destination
 * \retval None
 */
void Cy_MemCopy (uint8_t  *dest, const uint8_t  *src, uint32_t  count);

/**
 * \name Cy_PrintInitLogs
 * \brief Print initialization logs.
 * \retval None
 */
void Cy_PrintInitLogs(void);

/**
 * \name Cy_USB_RecvEndp0Data
 * \brief Wrapper function for non blocking Receive EP0 Data
 * \param pUsbdCtxt USBD Context pointer
 * \param pBuffer Buffer pointer
 * \param bufferSize Buffer size
 * \retval cy_en_usbd_ret_code_t Status
 */
cy_en_usbd_ret_code_t Cy_USB_RecvEndp0Data(cy_stc_usb_usbd_ctxt_t *pUsbdCtxt, uint8_t *pBuffer, uint32_t bufferSize);

/**
 * \name Cy_USB_AppSetupEndpDmaParamsHs
 * \brief Perform high bandwidth DMA initialization associated with a USB endpoint.
 * \param pAppCtxt Pointer to application context structure
 * \param pEndpDscr Endpoint descriptor
 * \retval None
 */
void
Cy_USB_AppSetupEndpDmaParamsHs(cy_stc_usb_app_ctxt_t *pAppCtxt,uint8_t *pEndpDscr);

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
void Cy_CheckStatus(const char *function, uint32_t line, uint8_t condition, uint32_t value, uint8_t isBlocking);

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
void Cy_CheckStatusAndHandleFailure(const char *function, uint32_t line, uint8_t condition, uint32_t value, uint8_t isBlocking, void (*failureHandler)());

/**
 * \name Cy_FailHandler
 * \brief Error Handler
 * \retval None
 */
void Cy_FailHandler(void);

/**
 * \name Cy_USB_DeviceTaskHandler
 * \brief This function handles application related event handling.
 * \param pTaskParam Task param
 * \retval None
 */
void Cy_USB_DeviceTaskHandler(void *pTaskParam);

/* Combined ISR handlers for HBDMA DW0 (OUT) and DW1 (IN) transfer completion. */
void Cy_CDC_HbDma_DW0_ISR(void);
void Cy_CDC_HbDma_DW1_ISR(void);

/**
 * \name Cy_DebugInit
 * \brief Function to initialize debug logs
 * \param appCtxt User context
 * \retval None
 */
void Cy_DebugInit(cy_stc_usb_app_ctxt_t *appCtxt);

#endif /* _CY_USB_APP_H_ */

