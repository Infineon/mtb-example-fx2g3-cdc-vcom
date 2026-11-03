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

#include "vcom_config.h"

/* Serial Number string descriptor: "0" */
const uint8_t serialNumberString[] ={
0x4, 0x03, 
'0', 0x00
};

/* Manufacturer string descriptor : "Infineon" */
const uint8_t mfgString[] ={
0x12, 0x03, 
'I', 0x00, 'n', 0x00, 'f', 0x00, 'i', 0x00, 'n', 0x00, 'e', 0x00, 'o', 0x00, 'n', 0x00
};

/* Product string descriptor: "FX2G3" */
const uint8_t prodString[] ={
0xC, 0x03, 
'F', 0x00, 'X', 0x00, '2', 0x00, 'G', 0x00, '3', 0x00
};

/* Function  string: "cdc-0" */
const uint8_t cdc0String[] ={
0xA, 0x03, 
'C', 0x00, 'D', 0x00, 'C', 0x00, '-', 0x00, '0'
};

/* Function  string: "cdc-1" */
const uint8_t cdc1String[] ={
0xA, 0x03, 
'C', 0x00, 'D', 0x00, 'C', 0x00, '-', 0x00, '1'
};

/* Function  string: "cdc-2" */
const uint8_t cdc2String[] ={
0xA, 0x03, 
'C', 0x00, 'D', 0x00, 'C', 0x00, '-', 0x00, '2'
};

/* Function  string: "cdc-3" */
const uint8_t cdc3String[] ={
0xA, 0x03, 
'C', 0x00, 'D', 0x00, 'C', 0x00, '-', 0x00, '3'
};

/*  CDC Intr IN endpoint Settings */
cy_stc_usb_ep_config_t cdc0IntrInEpConfig ={
    .epNumber            = 0x81,  /* Endpoint Address */
    .epType              = CY_USB_ENDP_TYPE_INTR,  /* Endpoint Type */
    .epBurst             = 0,  /* Bursts per transfer */
    .epMaxPktSize        = 1024,
    .epMult              = 0,
    .epServiceInterval   = 4  /* Interval for servicing the endpoint for data transfers. Expressed in 125 µs units. */
};

/* CDC Bulk IN endpoint settings */
cy_stc_usb_ep_config_t cdc0BlkInEpConfig ={
    .epNumber            = 0x82,  /* Endpoint Address */
    .epType              = CY_USB_ENDP_TYPE_BULK,  /* Endpoint Type */
    .epBurst             = 0,  /* Bursts per transfer */
    .epMaxPktSize        = 512,
    .epMult              = 0,
    .epServiceInterval   = 0  /* Interval for servicing the endpoint for data transfers. Expressed in 125 µs units */
};

/* CDC Bulk OUT endpoint settings */
cy_stc_usb_ep_config_t cdc0BlkOutEpConfig ={
    .epNumber            = 0x01,  /* Endpoint Address */
    .epType              = CY_USB_ENDP_TYPE_BULK,  /* Endpoint Type */
    .epBurst             = 0,  /* Bursts per transfer */
    .epMaxPktSize        = 512,
    .epMult              = 0,
    .epServiceInterval   = 0  /* Interval for servicing the endpoint for data transfers. Expressed in 125 µs units */
};

/* CDC 0 function configuration */
cy_stc_cdc_fn_config_t cdc0FnConfig ={
    .dataReceiveEnabled  = true,  /* Enable / Disable receiving data on the CDC OUT endpoint */
    .cdcDataSourceSink   = CY_CDC_UART_SCB0,
    .dmaBufferSize       = 512,  /* Choose the size (in bytes) for each DMA buffer (should be a multiple of 32) */
    .dmaBufferCount      = 8,  /* Choose the number of DMA buffers for this interface */
    .cdcEpConfig         = {&cdc0IntrInEpConfig,&cdc0BlkInEpConfig,&cdc0BlkOutEpConfig},  /* 0: Contol interface ep in, 1: data interface ep IN 2: data interface ep out */
    .cdcFnString         = cdc0String
};

/*  CDC Function */
cy_stc_fn_config_t cdc0Fn ={
    .fnConfig            = (void*)&cdc0FnConfig  
};

/*  CDC endpoint Settings */
cy_stc_usb_ep_config_t cdc1IntrInEpConfig ={
    .epNumber            = 0x83,  /* Endpoint Address */
    .epType              = CY_USB_ENDP_TYPE_INTR,  /* Endpoint Type */
    .epBurst             = 0,  /* Bursts per transfer */
    .epMaxPktSize        = 1024,  
    .epMult              = 0,  
    .epServiceInterval   = 4  /* Interval for servicing the endpoint for data transfers. Expressed in 125 µs units. */
};

/* CDC Bulk IN endpoint settings */
cy_stc_usb_ep_config_t cdc1BlkInEpConfig ={
    .epNumber            = 0x84,  /* Endpoint Address */
    .epType              = CY_USB_ENDP_TYPE_BULK,  /* Endpoint Type */
    .epBurst             = 0,  /* Bursts per transfer */
    .epMaxPktSize        = 512,  
    .epMult              = 0,  
    .epServiceInterval   = 0  /* Interval for servicing the endpoint for data transfers. Expressed in 125 µs units */
};

/* CDC Bulk OUT endpoint settings */
cy_stc_usb_ep_config_t cdc1BlkOutEpConfig ={
    .epNumber            = 0x02,  /* Endpoint Address */
    .epType              = CY_USB_ENDP_TYPE_BULK,  /* Endpoint Type */
    .epBurst             = 0,  /* Bursts per transfer */
    .epMaxPktSize        = 512,  
    .epMult              = 0,  
    .epServiceInterval   = 0  /* Interval for servicing the endpoint for data transfers. Expressed in 125 µs units */
};

/* CDC 1 function configuration */
cy_stc_cdc_fn_config_t cdc1FnConfig ={
    .dataReceiveEnabled  = true,  /* Enable / Disable receiving data on the CDC OUT endpoint */
    .cdcDataSourceSink   = CY_CDC_UART_SCB4,  
    .dmaBufferSize       = 512,  /* Choose the size (in bytes) for each DMA buffer (should be a multiple of 32) */
    .dmaBufferCount      = 8,  /* Choose the number of DMA buffers for this interface */
    .cdcEpConfig         = {&cdc1IntrInEpConfig,&cdc1BlkInEpConfig,&cdc1BlkOutEpConfig},  /* 0: Contol interface ep in, 1: data interface ep IN 2: data interface ep out */
    .cdcFnString         = cdc1String  
};

/*  CDC Function */
cy_stc_fn_config_t cdc1Fn ={
    .fnConfig            = (void*)&cdc1FnConfig  
};

/*  CDC endpoint Settings */
cy_stc_usb_ep_config_t cdc2IntrInEpConfig ={
    .epNumber            = 0x85,  /* Endpoint Address */
    .epType              = CY_USB_ENDP_TYPE_INTR,  /* Endpoint Type */
    .epBurst             = 0,  /* Bursts per transfer */
    .epMaxPktSize        = 1024,  
    .epMult              = 0,  
    .epServiceInterval   = 4  /* Interval for servicing the endpoint for data transfers. Expressed in 125 µs units. */
};

/* CDC Bulk IN endpoint settings */
cy_stc_usb_ep_config_t cdc2BlkInEpConfig ={
    .epNumber            = 0x86,  /* Endpoint Address */
    .epType              = CY_USB_ENDP_TYPE_BULK,  /* Endpoint Type */
    .epBurst             = 0,  /* Bursts per transfer */
    .epMaxPktSize        = 512,  
    .epMult              = 0,  
    .epServiceInterval   = 0  /* Interval for servicing the endpoint for data transfers. Expressed in 125 µs units */
};

/* CDC Bulk OUT endpoint settings */
cy_stc_usb_ep_config_t cdc2BlkOutEpConfig ={
    .epNumber            = 0x03,  /* Endpoint Address */
    .epType              = CY_USB_ENDP_TYPE_BULK,  /* Endpoint Type */
    .epBurst             = 0,  /* Bursts per transfer */
    .epMaxPktSize        = 512,  
    .epMult              = 0,  
    .epServiceInterval   = 0  /* Interval for servicing the endpoint for data transfers. Expressed in 125 µs units */
};

/* CDC 2 function configuration */
cy_stc_cdc_fn_config_t cdc2FnConfig ={
    .dataReceiveEnabled  = true,  /* Enable / Disable receiving data on the CDC OUT endpoint */
    .cdcDataSourceSink   = CY_CDC_UART_SCB6,  
    .dmaBufferSize       = 512,  /* Choose the size (in bytes) for each DMA buffer (should be a multiple of 32) */
    .dmaBufferCount      = 8,  /* Choose the number of DMA buffers for this interface */
    .cdcEpConfig         = {&cdc2IntrInEpConfig,&cdc2BlkInEpConfig,&cdc2BlkOutEpConfig},  /* 0: Contol interface ep in, 1: data interface ep IN 2: data interface ep out */
    .cdcFnString         = cdc2String  
};

/*  CDC Function */
cy_stc_fn_config_t cdc2Fn ={
    .fnConfig            = (void*)&cdc2FnConfig  
};

/*  CDC endpoint Settings */
cy_stc_usb_ep_config_t cdc3IntrInEpConfig ={
    .epNumber            = 0x87,  /* Endpoint Address */
    .epType              = CY_USB_ENDP_TYPE_INTR,  /* Endpoint Type */
    .epBurst             = 0,  /* Bursts per transfer */
    .epMaxPktSize        = 1024,  
    .epMult              = 0,  
    .epServiceInterval   = 4  /* Interval for servicing the endpoint for data transfers. Expressed in 125 µs units. */
};

/* CDC Bulk IN endpoint settings */
cy_stc_usb_ep_config_t cdc3BlkInEpConfig ={
    .epNumber            = 0x88,  /* Endpoint Address */
    .epType              = CY_USB_ENDP_TYPE_BULK,  /* Endpoint Type */
    .epBurst             = 0,  /* Bursts per transfer */
    .epMaxPktSize        = 512,  
    .epMult              = 0,  
    .epServiceInterval   = 0  /* Interval for servicing the endpoint for data transfers. Expressed in 125 µs units */
};

/* CDC Bulk OUT endpoint settings */
cy_stc_usb_ep_config_t cdc3BlkOutEpConfig ={
    .epNumber            = 0x04,  /* Endpoint Address */
    .epType              = CY_USB_ENDP_TYPE_BULK,  /* Endpoint Type */
    .epBurst             = 0,  /* Bursts per transfer */
    .epMaxPktSize        = 512,  
    .epMult              = 0,  
    .epServiceInterval   = 0  /* Interval for servicing the endpoint for data transfers. Expressed in 125 µs units */
};

/* CDC 3 function configuration */
cy_stc_cdc_fn_config_t cdc3FnConfig ={
    .dataReceiveEnabled  = true,  /* Enable / Disable receiving data on the CDC OUT endpoint */
    .cdcDataSourceSink   = CY_CDC_UART_SCB1,  
    .dmaBufferSize       = 512,  /* Choose the size (in bytes) for each DMA buffer (should be a multiple of 32) */
    .dmaBufferCount      = 8,  /* Choose the number of DMA buffers for this interface */
    .cdcEpConfig         = {&cdc3IntrInEpConfig,&cdc3BlkInEpConfig,&cdc3BlkOutEpConfig},  /* 0: Contol interface ep in, 1: data interface ep IN 2: data interface ep out */
    .cdcFnString         = cdc3String  
};

/* CDC Function */
cy_stc_fn_config_t cdc3Fn ={
    .fnConfig            = (void*)&cdc3FnConfig  
};

/* FX2G3 CDC device configuration structure */
cy_stc_usb_dev_config_t appConfig ={
    .usbVid              = 0x04B4,  /* Vendor ID */
    .usbPid              = 0x00EF,  /* Product ID */
    .fwDbgInterface      = CY_DBGLOG_TGT_USBFS_CDC,  /* Interface for logs */
    .vbusDetectPolarity  = CY_VBUS_DETECT_LOW,  /* Active Low: USB connection when VBUS DETECT GPIO goes low                 Active High: USB connection when VBUS DETECT GPIO goes high */
    .logLevel            = 3,  /* Debug log level */
    .isSelfPowered       = 0,  /* Self Powered Disabled */
    .serialGenEnabled    = 0,  /* Firmware will generate serial number string. */
    .mfgString           = mfgString,  /* Manufacturer String */
    .prodString          = prodString,  /* Product String */
    .serialNumberString  = serialNumberString,  /* Serial number not required as FW generates it. */
    .usbFnCount          = VCOM_UART_CHANNELS,
#if VCOM_UART_CHANNELS==1
    .fnConfig            = {&cdc0Fn,},
#elif VCOM_UART_CHANNELS==2
    .fnConfig            = {&cdc0Fn,&cdc1Fn},
#elif VCOM_UART_CHANNELS==3
    .fnConfig            = {&cdc0Fn,&cdc1Fn,&cdc2Fn},
#elif VCOM_UART_CHANNELS==4
    .fnConfig            = {&cdc0Fn,&cdc1Fn,&cdc2Fn,&cdc3Fn},
#endif
};

/* [EOF] */
