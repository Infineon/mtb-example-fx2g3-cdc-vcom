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

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "cy_pdl.h"
#include <string.h>
#include "cy_usb_common.h"
#include "cy_usb_usbd.h"
#include "cy_usb_app.h"
#include "cy_debug.h"
#include "cy_usbd_version.h"
#include "cy_hbdma_version.h"
#include "cy_fx_common.h"
#include "../app_version.h"
#include "timers.h"
#include "vcom_config.h"
#include "cy_dscr.h"
#include "cybsp.h"
#include "COMPONENT_CDC/cdc.h"

/* Informaion about the number of interfaces implemented by this application */
extern cy_stc_app_info_t appInfo;

/* Global variables associated with High BandWidth DMA setup. */
cy_stc_hbdma_context_t HBW_DrvCtxt;     /* High BandWidth DMA driver context. */
cy_stc_hbdma_dscr_list_t HBW_DscrList;  /* High BandWidth DMA descriptor free list. */
cy_stc_hbdma_buf_mgr_t HBW_BufMgr;      /* High BandWidth DMA buffer manager. */
cy_stc_hbdma_mgr_context_t HBW_MgrCtxt; /* High BandWidth DMA manager context. */
cy_stc_usb_usbd_ctxt_t usbdCtxt;
cy_stc_usb_app_ctxt_t appCtxt;
cy_stc_usb_cal_ctxt_t hsCalCtxt;

/* CPU DMA register pointers. */
DMAC_Type* pCpuDmacBase;
DW_Type* pCpuDw0Base;
DW_Type* pCpuDw1Base;

uint32_t hfclkFreq = BCLK__BUS_CLK__HZ;

extern void xPortPendSVHandler(void);
extern void xPortSysTickHandler(void);
extern void vPortSVCHandler(void);

void SysTickIntrWrapper (void)
{
    Cy_USBD_TickIncrement(&usbdCtxt);
    xPortSysTickHandler();
}

void vPortSetupTimerInterrupt(void)
{
    /* Register the exception vectors. */
    Cy_SysInt_SetVector(PendSV_IRQn, xPortPendSVHandler);
    Cy_SysInt_SetVector(SVCall_IRQn, vPortSVCHandler);
    Cy_SysInt_SetVector(SysTick_IRQn, SysTickIntrWrapper);

    /* Start the SysTick timer with a period of 1 ms. */
    Cy_SysTick_SetClockSource(CY_SYSTICK_CLOCK_SOURCE_CLK_CPU);
    Cy_SysTick_SetReload(hfclkFreq / 1000U);
    Cy_SysTick_Clear();
    Cy_SysTick_Enable();
}

/**
 * \name Cy_UART_SCB0Write_ISR
 * \brief Handler for DataWire transfer complete interrupts for transfers to the UART TX FIFO.
 * \retval None
 */
void Cy_UART_SCB0Write_ISR(void)
{
    Cy_CDC_ClearUartDmaInterrupt(&appCtxt, CDC_UART_WRITE, 0);
    Cy_CDC_UartDmaWriteCompletion(&appCtxt,0);
    portYIELD_FROM_ISR(true);
}  /* end of function. */

/**
 * \name Cy_UART_SCB1Write_ISR
 * \brief Handler for DataWire transfer complete interrupts for transfers to the UART TX FIFO.
 * \retval None
 */
void Cy_UART_SCB1Write_ISR(void)
{
    Cy_CDC_ClearUartDmaInterrupt(&appCtxt, CDC_UART_WRITE,1);
    Cy_CDC_UartDmaWriteCompletion(&appCtxt,1);
    portYIELD_FROM_ISR(true);
}  /* end of function. */


/**
 * \name Cy_UART_SCB4Write_ISR
 * \brief Handler for DMAC transfer complete interrupts for transfers to the UART TX FIFO.
 * \retval None
 */
void Cy_UART_SCB4Write_ISR(void)
{
    Cy_CDC_ClearUartDmaInterrupt(&appCtxt, CDC_UART_WRITE,4);
    Cy_CDC_UartDmaWriteCompletion(&appCtxt,4);
    portYIELD_FROM_ISR(true);
}  /* end of function. */

/**
 * \name Cy_UART_SCB6Write_ISR
 * \brief Handler for DataWire transfer complete interrupts for transfers to the UART TX FIFO.
 * \retval None
 */
void Cy_UART_SCB6Write_ISR(void)
{
    Cy_CDC_ClearUartDmaInterrupt(&appCtxt, CDC_UART_WRITE,6);
    Cy_CDC_UartDmaWriteCompletion(&appCtxt,6);
    portYIELD_FROM_ISR(true);
}  /* end of function. */

/**
 * \name Cy_UART_SCB0Read_ISR
 * \brief Handler for DataWire transfer complete interrupts for transfers which read from the UART RX FIFO.
 * \retval None
 */
void Cy_UART_SCB0Read_ISR(void)
{
    Cy_CDC_ClearUartDmaInterrupt(&appCtxt, CDC_UART_READ,0);
    Cy_CDC_UartDmaReadCompletion(&appCtxt,0);
    portYIELD_FROM_ISR(true);
}  /* end of function. */

/**
 * \name Cy_UART_SCB1Read_ISR
 * \brief Handler for DataWire transfer complete interrupts for transfers which read from the UART RX FIFO.
 * \retval None
 */
void Cy_UART_SCB1Read_ISR(void)
{
    Cy_CDC_ClearUartDmaInterrupt(&appCtxt, CDC_UART_READ,1);
    Cy_CDC_UartDmaReadCompletion(&appCtxt,1);
    portYIELD_FROM_ISR(true);
}  /* end of function. */

/**
 * \name Cy_UART_SCB4Read_ISR
 * \brief Handler for DataWire transfer complete interrupts for transfers which read from the UART RX FIFO.
 * \retval None
 */
void Cy_UART_SCB4Read_ISR(void)
{
    Cy_CDC_ClearUartDmaInterrupt(&appCtxt, CDC_UART_READ,4);
    Cy_CDC_UartDmaReadCompletion(&appCtxt,4);
    portYIELD_FROM_ISR(true);
}  /* end of function. */

/**
 * \name Cy_UART_SCB6Read_ISR
 * \brief Handler for DataWire transfer complete interrupts for transfers which read from the UART RX FIFO.
 * \retval None
 */
void Cy_UART_SCB6Read_ISR(void)
{
    Cy_CDC_ClearUartDmaInterrupt(&appCtxt, CDC_UART_READ,6);
    Cy_CDC_UartDmaReadCompletion(&appCtxt,6);
    portYIELD_FROM_ISR(true);
}  /* end of function. */

#if !CY_CPU_CORTEX_M4
void Cy_CDC_UartDmaWrite_Combined_ISR(void)
{
    if (Cy_DMA_Channel_GetInterruptStatus(pCpuDw0Base, CDC_UART_SCB0_WRITE_DMA_CHANNEL))
    {
        Cy_UART_SCB0Write_ISR();
    }
    if (Cy_DMA_Channel_GetInterruptStatus(pCpuDw0Base, CDC_UART_SCB1_WRITE_DMA_CHANNEL))
    {
        Cy_UART_SCB1Write_ISR();
    }
    if (Cy_DMAC_Channel_GetInterruptStatus(pCpuDmacBase, SCB4_TX_DMAC_CHANNEL))
    {
        Cy_UART_SCB4Write_ISR();
    }
    if (Cy_DMAC_Channel_GetInterruptStatus(pCpuDmacBase, SCB6_TX_DMAC_CHANNEL))
    {
        Cy_UART_SCB6Write_ISR();
    }
}

void Cy_CDC_UartDmaRead_Combined_ISR(void)
{
    if (Cy_DMA_Channel_GetInterruptStatus(pCpuDw0Base, CDC_UART_SCB0_READ_DMA_CHANNEL))
    {
        Cy_UART_SCB0Read_ISR();
    }
    if (Cy_DMA_Channel_GetInterruptStatus(pCpuDw0Base, CDC_UART_SCB1_READ_DMA_CHANNEL))
    {
        Cy_UART_SCB1Read_ISR();
    }
    if (Cy_DMAC_Channel_GetInterruptStatus(pCpuDmacBase, SCB4_RX_DMAC_CHANNEL))
    {
        Cy_UART_SCB4Read_ISR();
    }
    if (Cy_DMAC_Channel_GetInterruptStatus(pCpuDmacBase, SCB6_RX_DMAC_CHANNEL))
    {
        Cy_UART_SCB6Read_ISR();
    }
}
#endif

/**
 * \name Cy_CDC_HbDma_DW0_ISR
 * \brief Combined ISR for HBDMA DW0 (OUT endpoint) transfer completion.
 * \details Routes to the HBDMA manager which handles all active DW0 channels.
 * \retval None
 */
void Cy_CDC_HbDma_DW0_ISR(void)
{
    Cy_HBDma_Mgr_HandleDW0Interrupt(appCtxt.pHbDmaMgrCtxt);
    portYIELD_FROM_ISR(true);
}

/**
 * \name Cy_CDC_HbDma_DW1_ISR
 * \brief Combined ISR for HBDMA DW1 (IN endpoint) transfer completion.
 * \details Routes to the HBDMA manager which handles all active DW1 channels.
 * \retval None
 */
void Cy_CDC_HbDma_DW1_ISR(void)
{
    Cy_HBDma_Mgr_HandleDW1Interrupt(appCtxt.pHbDmaMgrCtxt);
    portYIELD_FROM_ISR(true);
}

/**
 * \name Cy_CDC_Ch0OutEpDma_ISR
 * \brief Handler for DMA transfer completion on OUT endpoint.
 * \retval None
 */
void Cy_CDC_Ch0OutEpDma_ISR (void)
{
    Cy_USB_AppClearDmaInterrupt(&appCtxt, appCtxt.cdcInfo[0]->vcomOutEpNum, CY_USB_ENDP_DIR_OUT);
    portYIELD_FROM_ISR(true);
}  /* end of function. */

/**
 * \name Cy_CDC_Ch1OutEpDma_ISR
 * \brief Handler for DMA transfer completion on OUT endpoint.
 * \retval None
 */
void Cy_CDC_Ch1OutEpDma_ISR (void)
{
    Cy_USB_AppClearDmaInterrupt(&appCtxt, appCtxt.cdcInfo[1]->vcomOutEpNum, CY_USB_ENDP_DIR_OUT);
    portYIELD_FROM_ISR(true);
}  /* end of function. */

/**
 * \name Cy_CDC_Ch2OutEpDma_ISR
 * \brief Handler for DMA transfer completion on OUT endpoint.
 * \retval None
 */
void Cy_CDC_Ch2OutEpDma_ISR (void)
{
    Cy_USB_AppClearDmaInterrupt(&appCtxt, appCtxt.cdcInfo[2]->vcomOutEpNum, CY_USB_ENDP_DIR_OUT);
    portYIELD_FROM_ISR(true);
}  /* end of function. */

/**
 * \name Cy_CDC_Ch3OutEpDma_ISR
 * \brief Handler for DMA transfer completion on OUT endpoint.
 * \retval None
 */
void Cy_CDC_Ch3OutEpDma_ISR (void)
{
    Cy_USB_AppClearDmaInterrupt(&appCtxt, appCtxt.cdcInfo[3]->vcomOutEpNum, CY_USB_ENDP_DIR_OUT);
    portYIELD_FROM_ISR(true);
}  /* end of function. */

/**
 * \name Cy_CDC_Ch0InEpDma_ISR
 * \brief Handler for DMA transfer completion on IN endpoint.
 * \retval None
 */
void Cy_CDC_Ch0InEpDma_ISR (void)
{
    Cy_USB_AppClearDmaInterrupt(&appCtxt, appCtxt.cdcInfo[0]->vcomInEpNum, CY_USB_ENDP_DIR_IN);
    portYIELD_FROM_ISR(true);
}  /* end of function. */

/**
 * \name Cy_CDC_Ch1InEpDma_ISR
 * \brief Handler for DMA transfer completion on IN endpoint.
 * \retval None
 */
void Cy_CDC_Ch1InEpDma_ISR (void)
{
    Cy_USB_AppClearDmaInterrupt(&appCtxt, appCtxt.cdcInfo[1]->vcomInEpNum, CY_USB_ENDP_DIR_IN);
    portYIELD_FROM_ISR(true);
}  /* end of function. */

/**
 * \name Cy_CDC_Ch2InEpDma_ISR
 * \brief Handler for DMA transfer completion on IN endpoint.
 * \retval None
 */
void Cy_CDC_Ch2InEpDma_ISR (void)
{
    Cy_USB_AppClearDmaInterrupt(&appCtxt, appCtxt.cdcInfo[2]->vcomInEpNum, CY_USB_ENDP_DIR_IN);
    portYIELD_FROM_ISR(true);
}  /* end of function. */

/**
 * \name Cy_CDC_Ch3InEpDma_ISR
 * \brief Handler for DMA transfer completion on IN endpoint.
 * \retval None
 */
void Cy_CDC_Ch3InEpDma_ISR (void)
{
    Cy_USB_AppClearDmaInterrupt(&appCtxt, appCtxt.cdcInfo[3]->vcomInEpNum, CY_USB_ENDP_DIR_IN);
    portYIELD_FROM_ISR(true);
}  /* end of function. */

/**
 * \name Cy_USBHS_ISR
 * \brief Handler for USB-HS Interrupts.
 * \retval None
 */
void Cy_USBHS_ISR(void)
{
    if (Cy_USBHS_Cal_IntrHandler(&hsCalCtxt))
    {
        portYIELD_FROM_ISR(true);
    }
}

/**
 * \name Cy_VbusDetGpio_ISR
 * \brief Interrupt handler for the Vbus detect GPIO transition detection.
 * \retval None
 */
static void Cy_VbusDetGpio_ISR(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    cy_stc_usbd_app_msg_t xMsg;

    /* Send VBus changed message to the task thread. */
    xMsg.type = CY_USB_VBUS_CHANGE_INTR;
    xQueueSendFromISR(appCtxt.appMsgQueue, &(xMsg), &(xHigherPriorityTaskWoken));

    /* Remember that VBus change has happened and disable the interrupt. */
    appCtxt.vbusChangeIntr = true;
    Cy_GPIO_SetInterruptMask(VBUS_DETECT_GPIO_PORT, VBUS_DETECT_GPIO_PIN, 0);

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/**
 * \name Cy_Fx2g3_InitPeripheralClocks
 * \brief Function used to enable clocks to different peripherals on the FX2G3 device.
 * \param adcClkEnable Whether to enable clock to the ADC in the USBSS block
 * \param usbfsClkEnable Whether to enable bus reset detect clock input to the USBFS block
 * \retval None
 */
void Cy_Fx3g2_InitPeripheralClocks (
        bool adcClkEnable,
        bool usbfsClkEnable)
{
    if (adcClkEnable) {
        /* Divide PERI clock at 75 MHz by 75 to get 1 MHz clock using 16-bit divider #1. */
        Cy_SysClk_PeriphSetDivider(CY_SYSCLK_DIV_16_BIT, 1, 74);
        Cy_SysClk_PeriphEnableDivider(CY_SYSCLK_DIV_16_BIT, 1);
        Cy_SysLib_DelayUs(10U);
        Cy_SysClk_PeriphAssignDivider(PCLK_LVDS2USB32SS_CLOCK_SAR, CY_SYSCLK_DIV_16_BIT, 1);
    }

    if (usbfsClkEnable) {
        /* Divide PERI clock at 75 MHz by 750 to get 100 KHz clock using 16-bit divider #2. */
        Cy_SysClk_PeriphSetDivider(CY_SYSCLK_DIV_16_BIT, 2, 749);
        Cy_SysClk_PeriphEnableDivider(CY_SYSCLK_DIV_16_BIT, 2);
        Cy_SysLib_DelayUs(10U);
        Cy_SysClk_PeriphAssignDivider(PCLK_USB_CLOCK_DEV_BRS, CY_SYSCLK_DIV_16_BIT, 2);
    }
}

/**
 * \name Cy_PrintVersionInfo
 * \brief Function to print version information to UART console.
 * \param type Type of version string
 * \param version Version number including major, minor, patch and build number
 * \retval None
 */
void Cy_PrintVersionInfo(const char* type, uint32_t version)
{
    char tString[32];
    uint16_t vBuild;
    uint8_t vMajor, vMinor, vPatch;
    uint8_t typeLen = strlen(type);

    vMajor = (version >> 28U);
    vMinor = ((version >> 24U) & 0x0FU);
    vPatch = ((version >> 16U) & 0xFFU);
    vBuild = (uint16_t)(version & 0xFFFFUL);

    memcpy(tString, type, typeLen);
    tString[typeLen++] = '0' + (vMajor / 10);
    tString[typeLen++] = '0' + (vMajor % 10);
    tString[typeLen++] = '.';
    tString[typeLen++] = '0' + (vMinor / 10);
    tString[typeLen++] = '0' + (vMinor % 10);
    tString[typeLen++] = '.';
    tString[typeLen++] = '0' + (vPatch / 10);
    tString[typeLen++] = '0' + (vPatch % 10);
    tString[typeLen++] = '.';
    tString[typeLen++] = '0' + (vBuild / 1000);
    tString[typeLen++] = '0' + ((vBuild % 1000) / 100);
    tString[typeLen++] = '0' + ((vBuild % 100) / 10);
    tString[typeLen++] = '0' + (vBuild % 10);
    tString[typeLen++] = '\r';
    tString[typeLen++] = '\n';
    tString[typeLen] = 0;

    Cy_Debug_AddToLog(1, "%s", tString);
}

/**
 * \name Cy_MemCopy
 * \brief Wrapper function for memcpy
 * \param dest Destination address
 * \param src Source address
 * \param count Number of bytes to copy from source to destination
 * \retval None
 */
void Cy_MemCopy (uint8_t  *dest, const uint8_t  *src, uint32_t  count)
{
    memcpy((uint8_t *)dest,(uint8_t *)src,count);
}

/**
 * \name Cy_USB_HSInit
 * \brief Initialize USBHS block and attempt device enumeration.
 * \retval None
 */
void Cy_USB_HSInit(void)
{
    cy_stc_gpio_pin_config_t pinCfg;
    cy_stc_sysint_t intrCfg;

    memset((void *)&pinCfg, 0, sizeof(pinCfg));

    /* Configure VBus detect GPIO. */
    pinCfg.driveMode = CY_GPIO_DM_HIGHZ;
    pinCfg.hsiom = HSIOM_SEL_GPIO;
    pinCfg.intEdge = CY_GPIO_INTR_BOTH;
    pinCfg.intMask = 0x01UL;
    Cy_GPIO_Pin_Init(VBUS_DETECT_GPIO_PORT, VBUS_DETECT_GPIO_PIN, &pinCfg);

    /* Register edge detect interrupt for Vbus detect GPIO.*/ 
     /* Register edge detect interrupt for Vbus detect GPIO. */
#if CY_CPU_CORTEX_M4
    intrCfg.intrSrc = VBUS_DETECT_GPIO_INTR;
    intrCfg.intrPriority = 7;
#else
    intrCfg.cm0pSrc = VBUS_DETECT_GPIO_INTR;
    intrCfg.intrSrc = NvicMux2_IRQn;
    intrCfg.intrPriority = 3;
#endif
    Cy_SysInt_Init(&intrCfg, Cy_VbusDetGpio_ISR);
    NVIC_EnableIRQ(intrCfg.intrSrc);


    /*
     * Enable the DMA buffer RAM
     * need to be enabled before we try to do High BandWidth DMA initialization.
     */
    MAIN_REG->CTRL |= MAIN_REG_CTRL_IP_ENABLED_Msk;

    /* Register the USBHS ISR and enable the interrupt. */
#if CY_CPU_CORTEX_M4
    intrCfg.intrSrc      = usbhsdev_interrupt_u2d_active_o_IRQn;
    intrCfg.intrPriority = 4;
#else
    intrCfg.cm0pSrc      = usbhsdev_interrupt_u2d_active_o_IRQn;
    intrCfg.intrSrc      = NvicMux5_IRQn;
    intrCfg.intrPriority = 2;
#endif /* CY_CPU_CORTEX_M4 */
    Cy_SysInt_Init(&intrCfg, Cy_USBHS_ISR);
    NVIC_EnableIRQ(intrCfg.intrSrc);

    /* Register ISR for and enable USBHS Interrupt. */
#if CY_CPU_CORTEX_M4
    intrCfg.intrSrc      = usbhsdev_interrupt_u2d_dpslp_o_IRQn;
    intrCfg.intrPriority = 4;
#else
    intrCfg.cm0pSrc      = usbhsdev_interrupt_u2d_dpslp_o_IRQn;
    intrCfg.intrSrc      = NvicMux5_IRQn;
    intrCfg.intrPriority = 2;
#endif /* CY_CPU_CORTEX_M4 */
    Cy_SysInt_Init(&intrCfg, Cy_USBHS_ISR);
    NVIC_EnableIRQ(intrCfg.intrSrc);
}

/* Initialize HBDMA*/
bool Cy_HBDMA_Init(void)
{
    cy_en_hbdma_status_t drvstat;
    cy_en_hbdma_mgr_status_t mgrstat;

    /* Initialize the HBW DMA driver layer. */
    drvstat = Cy_HBDma_Init(LVDSSS_LVDS, USB32DEV, &HBW_DrvCtxt, 0, 0);
    if (drvstat != CY_HBDMA_SUCCESS)
    {
        return false;
    }

    /* Setup a HBW DMA descriptor list. */
    mgrstat = Cy_HBDma_DscrList_Create(&HBW_DscrList, 256U);
    if (mgrstat != CY_HBDMA_MGR_SUCCESS)
    {
        return false;
    }

    /* Initialize the DMA buffer manager. We will use 800 KB of space from 0x1C030000 onwards. */
    mgrstat = Cy_HBDma_BufMgr_Create(&HBW_BufMgr, (uint32_t*)0x1C030000UL, 0x80000UL);
    if (mgrstat != CY_HBDMA_MGR_SUCCESS)
    {
        return false;
    }

    /* Initialize the HBW DMA channel manager. */
    mgrstat = Cy_HBDma_Mgr_Init(&HBW_MgrCtxt, &HBW_DrvCtxt, &HBW_DscrList, &HBW_BufMgr);
    if (mgrstat != CY_HBDMA_MGR_SUCCESS)
    {
        return false;
    }

    /* Register the USB stack context with the HBW DMA manager.
     * Required for HBDMA channels that use USBHS endpoint sockets. */
    Cy_HBDma_Mgr_RegisterUsbContext(&HBW_MgrCtxt, &usbdCtxt);

    return true;
}

/**
 * \name Cy_USB_DisableUsbBlock
 * \brief Function to disable the USB32DEV IP block after terminating current connection.
 * \retval None
 */
void Cy_USB_DisableUsbBlock(void)
{

    Cy_HBDma_DeInit(&HBW_DrvCtxt);
    DBG_APP_INFO("Disabled HBWSS DMA adapters\r\n");
}

/**
 * \name Cy_USB_EnableUsbBlock
 * \brief Function to enable the USB32DEV IP block before enabling a new USB connection.
 * \retval None
 */
void Cy_USB_EnableUsbBlock(void)
{
    /* Enable the USB DMA adapters and respective interrupts. */
    Cy_HBDma_Init(NULL, USB32DEV, &HBW_DrvCtxt, 0, 0);
}

/**
 * \name Cy_USB_EnableUsbHSConnection
 * \brief Enable USBHS connection.
 * \param pAppCtxt Pointer to application context structure
 * \retval bool True if connection enabled successfully
 */
bool Cy_USB_EnableUsbHSConnection(cy_stc_usb_app_ctxt_t* pAppCtxt)
{
	LOG_CLR(CYAN,"Connect USB HS\r\n");
    Cy_USBD_ConnectDevice(pAppCtxt->pUsbdCtxt, CY_USBD_USB_DEV_HS);
    pAppCtxt->usbConnected = true;

    return true;
}

/**
 * \name Cy_USB_DisableUsbHSConnection
 * \brief Disable USBHS connection.
 * \param pAppCtxt Pointer to application context structure
 * \retval None
 */
void Cy_USB_DisableUsbHSConnection(cy_stc_usb_app_ctxt_t* pAppCtxt)
{
    Cy_USBD_DisconnectDevice(pAppCtxt->pUsbdCtxt);
    pAppCtxt->usbConnected = false;
    pAppCtxt->devState = CY_USB_DEVICE_STATE_DISABLE;
    pAppCtxt->prevDevState = CY_USB_DEVICE_STATE_DISABLE;
}

/**
 * \name Cy_PrintInitLogs
 * \brief Print initialization logs.
 * \retval None
 */
void Cy_PrintInitLogs(void)
{
    Cy_SysLib_Delay(100);
    Cy_Debug_AddToLog(1,"\n\r###############################\n\r");
    Cy_Debug_AddToLog(1,"\n\r####### CONFIG DETAILS ########\n\r");
    Cy_Debug_AddToLog(1,"\n\r###############################\n\r");
    
    /* Print application, USBD stack and HBDMA version information. */
    Cy_PrintVersionInfo("   APP_VERSION: ", APP_VERSION_NUM);
    Cy_PrintVersionInfo("   USBD_VERSION: ", USBD_VERSION_NUM);
    Cy_PrintVersionInfo("   HBDMA_VERSION: ", HBDMA_VERSION_NUM);

    Cy_SysLib_Delay(10);
    Cy_Debug_AddToLog(1,"\n\r### USB FN DETAILS ###\n\r");
    Cy_Debug_AddToLog(1,"   Function Enabled: CDC: %d\n\r", appCtxt.appInfoPtr->cdcCount);

    Cy_Debug_AddToLog(1,"\n\r### DMA CHANNEL CONFIG ###\n\r");
    Cy_Debug_AddToLog(1,"   No. of DMA channels: %d \r\n", appCtxt.numOfHbdmaChannel); 
    Cy_SysLib_Delay(10);

    Cy_Debug_AddToLog(1,"\n\r### COMMUNICATION BLOCKS ###\n\r");
    Cy_Debug_AddToLog(1,"   USBHS/SS CDC Debug Enabled: %s \r\n",(appCtxt.cdcDebugEnabled)?"YES":"NO");
    Cy_Debug_AddToLog(
                        1," UART SCB Enabled:\
                            \r\n                SCB0: %d\
                            \r\n                SCB1: %d\
                            \r\n                SCB4: %d\
                            \r\n                SCB6: %d\
                            \r\n",  appCtxt.uartSCB0Enabled,
                                    appCtxt.uartSCB1Enabled,
                                    appCtxt.uartSCB4Enabled,
                                    appCtxt.uartSCB6Enabled
                                );
    
    Cy_Debug_AddToLog(1,"\n\r###############################\n\r");
    Cy_Debug_AddToLog(1,"\n\r#### END OF CONFIG DETAILS ####\n\r");
    Cy_Debug_AddToLog(1,"\n\r###############################\n\r");
    Cy_SysLib_Delay(500);
}

/**
 * \name main
 * \brief Entry to the application.
 * \retval int Does not return
 */
int main(void)
{
    pCpuDmacBase = ((DMAC_Type*)DMAC_BASE);
    pCpuDw0Base = ((DW_Type*)DW0_BASE);
    pCpuDw1Base = ((DW_Type*)DW1_BASE);

    /* Initialize the PDL driver library and set the clock variables. */
    Cy_PDL_Init(&cy_deviceIpBlockCfgFX3G2);

    /* Initialize the device and board peripherals */
    cybsp_init();
    
    /* Initialize the PDL and register ISR for USB block. */
    Cy_USB_HSInit();

    Cy_Fx3g2_InitPeripheralClocks(true, true);

#if CY_CPU_CORTEX_M4
    hfclkFreq = Cy_SysClk_ClkFastGetFrequency();
#else
    hfclkFreq = Cy_SysClk_ClkSlowGetFrequency();
#endif

    /* Unlock and then disable the watchdog. */
    Cy_WDT_Unlock();
    Cy_WDT_Disable();
    
    /* Enable interrupts. */
    __enable_irq();

    memset((uint8_t*)&appCtxt, 0, sizeof(appCtxt));
    memset((uint8_t*)&hsCalCtxt, 0, sizeof(hsCalCtxt));
    memset((uint8_t*)&usbdCtxt, 0, sizeof(usbdCtxt));

    /* Get Configuration Table*/
    appCtxt.cfgTablePtr = &appConfig;
    CY_ASSERT(appCtxt.cfgTablePtr != NULL);
    appCtxt.appInfoPtr = &appInfo;

    /* Enable debug prints */
#if DEBUG_INFRA_EN
       Cy_DebugInit(&appCtxt);
#endif /* DEBUG_INFRA_EN */
    
    /* Initialize the HbDma IP and DMA Manager */
    Cy_HBDMA_Init();
    DBG_APP_INFO("Cy_HBDMA_Init done \r\n");

    /* Store IP base address in CAL context. */
    hsCalCtxt.pCalBase = MXS40USBHSDEV_USBHSDEV;
    hsCalCtxt.pPhyBase = MXS40USBHSDEV_USBHSPHY;

    Cy_SysLib_Delay(500);

    /* Initialize the USBD layer */
    Cy_USB_USBD_Init(&appCtxt, &usbdCtxt, pCpuDmacBase, &hsCalCtxt, NULL, &HBW_MgrCtxt);
    DBG_APP_INFO("USBD_Init done\r\n");

    /* Enable stall cycles between back-to-back AHB accesses to high bandwidth RAM. */
    MAIN_REG->CTRL = (MAIN_REG->CTRL & 0xF00FFFFFUL) | 0x09900000UL;

    /* Register Device Descriptors to avoid failure in USB HS*/
    Cy_USB_PopulateDeviceDescriptors(appCtxt.cfgTablePtr,appCtxt.appInfoPtr, CY_USBD_USB_DEV_HS);
    Cy_USB_RegisterUsbDeviceDescriptors(&usbdCtxt);

    /* Initialize the application and create device thread. */
    Cy_USB_AppInit(&appCtxt, &usbdCtxt, pCpuDmacBase, pCpuDw0Base, pCpuDw1Base, &HBW_MgrCtxt);

    /* Invokes scheduler: Not expected to return. */
    vTaskStartScheduler();
    while (1);

    return 0;
}

void Cy_OnResetUser(void)
{
    Cy_UsbFx_OnResetInit();
}


/* [] END OF FILE */
