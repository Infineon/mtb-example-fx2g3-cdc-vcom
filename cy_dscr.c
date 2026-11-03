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

#include "cy_dscr.h"
#include "vcom_config.h"
#include "cy_pdl.h"
#include <string.h>
#include "cy_debug.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "cy_usbd_version.h"
#include "cy_hbdma.h"
#include "cy_usb_common.h"
#include "cy_usb_app.h"
#include "cy_usb_usbd.h"


cy_stc_usb_device_dscr_t glUsbDevDscr20;
cy_stc_usb_hs_bos_dscr_t glUsbBosDscrHs;
cy_stc_usb_devqual_dscr_t glUsbDevQualDscr;
cy_stc_usb_hsfs_config_dscr_t glUsbHsConfigDscr;
USB3_DESC_ATTRIBUTES cy_stc_usb_product_str_dscr_t glUsbProductStrDscr;
USB3_DESC_ATTRIBUTES cy_stc_usb_manufacturer_str_dscr_t glUsbManufacturerStrDscr;
USB3_DESC_ATTRIBUTES cy_stc_usb_serialnum_str_dscr_t glUsbSerialNumStrDscr;

/* Standard device descriptor for full speed (FS) */
USB3_DESC_ATTRIBUTES uint8_t CyFxUSBFSDeviceDscr[32];
/* Standard full speed configuration descriptor : full speed */
USB3_DESC_ATTRIBUTES uint8_t CyFxUSBFSConfigDscr[64];
/* Standard language ID string descriptor */
USB3_DESC_ATTRIBUTES uint8_t CyFxUSBStringLangIDDscr[16];

/* Standard manufacturer string descriptor */
USB3_DESC_ATTRIBUTES uint8_t CyFxUSBManufactureDscr[32];

/* Standard manufacturer string descriptor */
USB3_DESC_ATTRIBUTES uint8_t CyFxUSBSerialNumDscr[36];

/* Standard product string descriptor */
USB3_DESC_ATTRIBUTES uint8_t CyFxUSBProductDscr[16];

/* FS Not Supported string descriptor */
USB3_DESC_ATTRIBUTES uint8_t CyFxUSBFSDscr[64];

/* Place this buffer as the last buffer so that no other variable / code shares
 * the same cache line. Do not add any other variables / arrays in this file.
 * This will lead to variables sharing the same cache line. */
USB3_DESC_ATTRIBUTES uint8_t CyFxUsbDscrAlignBuffer[32];

USB3_DESC_ATTRIBUTES uint8_t fnStringDescr[CY_MAX_CDC_INTERFACES][256];

cy_stc_app_info_t appInfo = {
    .totalNumInterfaces = 0
};

extern uint16_t glUsbCfgDscrSizeHs;
extern uint16_t glUsbCfgDscrSizeFs;

/**
 * \name Cy_USB_GetCdcInterfaceInfo
 * \brief Function to count the CDC interfaces exposed by the application.
 * \param cfgTablePtr Pointer to config table
 * \param appInfoPtr Pointer to information structure
 * \retval None
 */
static void Cy_USB_GetCdcInterfaceInfo(cy_stc_usb_dev_config_t* cfgTablePtr, cy_stc_app_info_t* appInfoPtr)
{
    uint8_t i;
    cy_stc_fn_config_t* funcConfig = NULL;
    uint8_t interfaces = 0;

    for (i = 0; i < (cfgTablePtr->usbFnCount); i++)
    {
        funcConfig = cfgTablePtr->fnConfig[i];
        if (funcConfig == NULL)
        {
            continue;
        }

        interfaces += NUM_INTERFACES_CDC;
    }

    appInfoPtr->totalNumInterfaces = interfaces;
    appInfoPtr->totalFunctions = cfgTablePtr->usbFnCount;
}

/**
 * \name Cy_USB_PopulateDeviceDescriptors
 * \brief Function to populate device descriptor
 * \param cfgTablePtr Pointer to config table
 * \param appInfoPtr Pointer to information structure
 * \param usbSpeed USB speed
 * \retval None
 */
void Cy_USB_PopulateDeviceDescriptors(cy_stc_usb_dev_config_t* cfgTablePtr, cy_stc_app_info_t* appInfoPtr, cy_en_usb_speed_t usbSpeed)
{
    uint8_t deviceClass = MISC_CLASS;
    uint8_t deviceSubclass = MISC_SUBCLASS;
    uint8_t deviceProtocol = MISC_PROTOCOL;

    Cy_USB_GetCdcInterfaceInfo(cfgTablePtr, appInfoPtr);

    cy_stc_usb_device_dscr_t devDscr20 =
    {
        .length = sizeof(cy_stc_usb_device_dscr_t),
        .descriptorType = USB_DEVICE_DESCR,
        .bcdUSB = {CY_USB_GET_LSB(USB_DSCR_BCDUSB2),
                   CY_USB_GET_MSB(USB_DSCR_BCDUSB2)},

        .deviceClass = deviceClass,
        .deviceSubClass = deviceSubclass,
        .deviceProtocol = deviceProtocol,
        .maxPacketSize0 = USB_DSCR_EP0_SIZE_USB2,
        .idVendor = {USB_DSCR_VENDOR_ID_0(cfgTablePtr),USB_DSCR_VENDOR_ID_1(cfgTablePtr)},
        .idProduct = {USB_DSCR_PRODUCT_ID_0(cfgTablePtr),USB_DSCR_PRODUCT_ID_1(cfgTablePtr)},
        .bcdDevice = {CY_USB_GET_LSB(USB_DSCR_BCD_DEVICE),
                      CY_USB_GET_MSB(USB_DSCR_BCD_DEVICE)},

        .indexManufacturer = USB_DSCR_MANUFACTURER_STRING_INDEX,
        .indexProduct = USB_DSCR_PRODUCT_STRING_INDEX,
        .indexSerialNumber = USB_DSCR_SERIALNUMBER_STRING_INDEX,
        .numConfigurations = USB_DSCR_NUM_CONFIGURATIONS,
    };

    Cy_MemCopy((uint8_t*)&glUsbDevDscr20, (uint8_t*)&devDscr20, sizeof(cy_stc_usb_device_dscr_t));


    cy_stc_usb_hs_bos_dscr_t BOSDscrHs =
    {
        .length = USB_DSCR_BOS_HS_DSCR_LEN ,
        .descriptorType = USB_BOS_DESCR,
        .totalLength = {CY_USB_GET_LSB(USB_DSCR_BOS_HS_TOTAL_LENGTH),
                        CY_USB_GET_MSB(USB_DSCR_BOS_HS_TOTAL_LENGTH)},
        .numDeviceCaps = 0x01,
        .usb20Extension =
        {
            .length = sizeof(cy_stc_usb_usb2ext_dscr_t),
            .descriptorType = USB_DEVICE_CAPABILITY_DESCR,
            .devCapabilityType = 0x02,
            .bmAttributes = {0x1E, 0x64 , 0x00 , 0x00} /* Bit 1 : LPM Support - Enabled*/
        }
    };
    Cy_MemCopy((uint8_t*)&glUsbBosDscrHs, (uint8_t*)&BOSDscrHs, sizeof(cy_stc_usb_hs_bos_dscr_t));

    cy_stc_usb_devqual_dscr_t devQualityDscr =
    {
        .length = sizeof(cy_stc_usb_devqual_dscr_t),
        .descriptorType = USB_DEVICE_QUALITY_DESCR,
        .bcdUSB = {CY_USB_GET_LSB(USB_DSCR_BCDUSB2),
                   CY_USB_GET_MSB(USB_DSCR_BCDUSB2)},
        .deviceClass = deviceClass,
        .deviceSubClass = deviceSubclass,
        .deviceProtocol = deviceProtocol,
        .maxPacketSize0 = USB_DSCR_EP0_SIZE_USB2,
        .numConfigurations = USB_DSCR_NUM_CONFIGURATIONS,
        .reserved = 0x00,
    };
    /* Device qualifier descriptor */

    Cy_MemCopy((uint8_t*)&glUsbDevQualDscr, (uint8_t*)&devQualityDscr, sizeof(cy_stc_usb_devqual_dscr_t));
}

/**
 * \name Cy_USB_PopulateConfigDescriptorStart
 * \brief Function to populate Configuration descriptor start
 * \param dscr Pointer to descriptor buffer
 * \param cfgTablePtr Pointer to config table
 * \param speed USB speed
 * \param numInterfaces Number of interfaces
 * \retval uint32_t Current offset
 */
static  uint32_t Cy_USB_PopulateConfigDescriptorStart(uint8_t* dscr, cy_stc_usb_dev_config_t* cfgTablePtr,
    cy_en_usb_speed_t speed, uint8_t numInterfaces)
{

    uint8_t cfgDscrPowerConfiguration[2] = { SELF_POWERED, BUS_POWERED };
    uint8_t cfgDscrRemoteWakeupConfiguration[2] = { REMOTE_WAKEUP_DISABLED, REMOTE_WAKEUP_ENABLED };

    uint32_t offset = 0;
    cy_stc_usb_std_config_dscr_t cfgDscr =
    {
        .length = sizeof(cy_stc_usb_std_config_dscr_t),
        .descriptorType = USB_CONFIGURATION_DESCR ,
        .totalLength[0] = (CY_USB_GET_LSB(glUsbCfgDscrSizeHs)),
        .totalLength[1] = (CY_USB_GET_MSB(glUsbCfgDscrSizeHs)),
        .numInterfaces = numInterfaces,
        .configurationValue = USB_CFG_DSCR_CONFIGURATION_VAL, /* Used in SetConfiguration Request*/
        .indexConfiguration = USB_CFG_DSCR_STRING_INDEX,
        .bmAttributes = USB_CFG_DSCR_BM_ATTRIBUTES,
        .maxPower = USB_CFG_DSCR_MAX_POWER,
    };

    if (speed == CY_USBD_USB_DEV_HS)
    {
        cfgDscr.totalLength[0] = USB_CFG_DSCR_HS_TOTAL_LEN_L;
        cfgDscr.totalLength[1] = USB_CFG_DSCR_HS_TOTAL_LEN_H;

    }
    else if (speed == CY_USBD_USB_DEV_FS)
    {
        cfgDscr.totalLength[0] = USB_CFG_DSCR_FS_TOTAL_LEN_L;
        cfgDscr.totalLength[1] = USB_CFG_DSCR_FS_TOTAL_LEN_H;

    }
    Cy_MemCopy((uint8_t*)&dscr[offset], (uint8_t*)&cfgDscr, sizeof(cy_stc_usb_std_config_dscr_t));
    offset += sizeof(cy_stc_usb_std_config_dscr_t);
    return offset;
}

/**
 * \name Cy_USB_AddInterfaceAssociationDescriptor
 * \brief Function to populate IAD descriptors
 * \param dscr Pointer to descriptor buffer
 * \param function Function number
 * \param firstInterface Interface number
 * \param numInterfaces Number of interfaces
 * \param functionClass Function class
 * \param functionSubClass Function sub class
 * \param functionProtocol Function protocol
 * \param prevOffset Previous offset
 * \retval uint32_t Current offset
 */
static uint32_t Cy_USB_AddInterfaceAssociationDescriptor(uint8_t* dscr, uint8_t function, uint8_t firstInterface, uint8_t numInterfaces, uint8_t functionClass,
    uint8_t functionSubClass, uint8_t functionProtocol, uint32_t prevOffset)
{
    uint32_t offset = prevOffset;
    cy_stc_usb_intfassociation_dscr_t iad =
    {
        .length = sizeof(cy_stc_usb_intfassociation_dscr_t),
        .descriptorType = INTRFC_ASSN_DESCR ,
        .firstInterface = firstInterface,
        .interfaceCount = numInterfaces,
        .functionClass = functionClass,
        .functionSubClass = functionSubClass,
        .functionProtocol = functionProtocol,
        .indexFunction = (function + USB_STRING_DESCR_OFFSET),
    };

    Cy_MemCopy((uint8_t*)&dscr[offset], (uint8_t*)&iad, sizeof(cy_stc_usb_intfassociation_dscr_t));
    offset += sizeof(cy_stc_usb_intfassociation_dscr_t);

    return offset;
}

/* CDC Descriptors Start*/
uint8_t Cy_USB_PopulateCDCInterfaceDescriptorStart[] =
{
    /* Communication Interface descriptor */
    0x09,                           /* Descriptor size */
    USB_INTF_DESCR,        /* Interface Descriptor type */
    CDC_FIRST_INTERFACE,        /* Interface number */
    0x00,                           /* Alternate setting number */
    0x01,                           /* Number of endpoints */
    0x02,                           /* Interface class : Communication interface */
    0x02,                           /* Interface sub class */
    0x01,                           /* Interface protocol code */
    0x00,                           /* Interface descriptor string index */

    /* CDC Class-specific Descriptors */
    /* Header functional Descriptor */
    0x05,                           /* Descriptors length(5) */
    0x24,                           /* Descriptor type : CS_Interface */
    0x00,                           /* DescriptorSubType : Header Functional Descriptor */
    0x10,0x01,                      /* bcdCDC : CDC Release Number */

    /* Abstract Control Management Functional Descriptor */
    0x04,                           /* Descriptors Length (4) */
    0x24,                           /* bDescriptorType: CS_INTERFACE */
    0x02,                           /* bDescriptorSubType: Abstract Control Model Functional Descriptor */
    0x02,                           /* bmCapabilities: Supports the request combination of Set_Line_Coding,
                                                          Set_Control_Line_State, Get_Line_Coding and the notification Serial_State */

    /* Union Functional Descriptor */
    0x05,                           /* Descriptors Length (5) */
    0x24,                           /* bDescriptorType: CS_INTERFACE */
    0x06,                           /* bDescriptorSubType: Union Functional Descriptor */
    CDC_FIRST_INTERFACE,        /* bMasterInterface */
    CDC_SECOND_INTERFACE,       /* bSlaveInterface */

    /* Call Management Functional Descriptor */
    0x05,                           /*  Descriptors Length (5) */
    0x24,                           /*  bDescriptorType: CS_INTERFACE */
    0x01,                           /*  bDescriptorSubType: Call Management Functional Descriptor */
    0x00,                           /*  bmCapabilities: Device sends/receives call management information
                                                            only over the Communication Class Interface. */
    CDC_SECOND_INTERFACE        /*  Interface Number of Data Class interface */
};

/**
 * \name Cy_USB_PopulateCDCInterfaceDescriptorEnd
 * \brief Function to populate CDC interface descriptors end
 * \param dscr Pointer to descriptor buffer
 * \param cfgTablePtr Pointer to configuration table
 * \param firstInterface Interface number
 * \param function Function number
 * \param prevOffset Previous offset
 * \param speedCfg USB speed
 * \retval uint32_t Current offset
 */
static uint32_t Cy_USB_PopulateCDCInterfaceDescriptorEnd(uint8_t* dscr, cy_stc_usb_dev_config_t* cfgTablePtr, uint8_t firstInterface, uint8_t function,
    uint32_t prevOffset, uint8_t speedCfg)
{
    /* Add endpoint descriptors for streaming interface */
    uint32_t offset = prevOffset;

    /* HS/FS interrupt endpoint descriptor (no SS companion) */
    cy_stc_usb_ep_dscr_t streamingEP =
    {
        .streamingEP =
        {
            .length = sizeof(cy_stc_usb_std_ep_dscr_t),
            .descriptorType = USB_ENDP_DESCR,
            .endpointAddress = CDC_INTR_EP_NUM(cfgTablePtr,function),
            .bmAttributes = CDC_EP_INTR_TYPE(cfgTablePtr,function),
            .maxPacketSize = {CY_USB_GET_LSB(CDC_INTR_EP_MAXPKTSIZE(cfgTablePtr,function)),
                              CY_USB_GET_MSB(CDC_INTR_EP_MAXPKTSIZE(cfgTablePtr,function))},
            .interval = CDC_INTR_EP_INTERVAL(cfgTablePtr,function),
        },
    };
    Cy_MemCopy(&dscr[offset], (uint8_t*)&streamingEP, sizeof(cy_stc_usb_ep_dscr_t));
    offset += sizeof(cy_stc_usb_ep_dscr_t);

    cy_stc_cdc_data_intf_dscr_t dataInterface =
    {
        .length = sizeof(cy_stc_cdc_data_intf_dscr_t),
        .descriptorType = USB_INTF_DESCR,
        .interfaceNumber = firstInterface,
        .alternateSetting = 0,
        .noOfEndpoints = 0x02,
        .interfaceClass = 0x0A,
        .interfaceSubClass = 0x00,
        .interfaceProtocol = 0x00,
        .interfaceStringIndex = 0x00,
    };

    Cy_MemCopy(&dscr[offset], (uint8_t*)&dataInterface, sizeof(cy_stc_cdc_data_intf_dscr_t));
    offset += sizeof(cy_stc_cdc_data_intf_dscr_t);

    /* HS/FS data endpoint descriptors (no SS companion) */
    cy_stc_usb_ep_dscr_t dataEPHSIn =
    {
        .streamingEP =
            {
                .length = sizeof(cy_stc_usb_std_ep_dscr_t),
                .descriptorType = USB_ENDP_DESCR,
                .endpointAddress = CDC_DATA_IN_EP_NUM(cfgTablePtr,function),
                .bmAttributes = CDC_DATA_IN_EP_TYPE(cfgTablePtr,function),
                .maxPacketSize = {CY_USB_GET_LSB((CDC_DATA_IN_EP_TYPE(cfgTablePtr,function) == CY_USB_ENDP_TYPE_BULK) ? 512 : CDC_DATA_IN_EP_MAXPKTSIZE(cfgTablePtr,function)),
                                  CY_USB_GET_MSB((CDC_DATA_IN_EP_TYPE(cfgTablePtr,function) == CY_USB_ENDP_TYPE_BULK) ? 512 : CDC_DATA_IN_EP_MAXPKTSIZE(cfgTablePtr,function))},
                .interval = CDC_DATA_IN_EP_INTERVAL(cfgTablePtr,function),
            },
        };

        cy_stc_usb_ep_dscr_t dataEPHSOut =
        {
           .streamingEP =
            {
                .length = sizeof(cy_stc_usb_std_ep_dscr_t),
                .descriptorType = USB_ENDP_DESCR,
                .endpointAddress = CDC_DATA_OUT_EP_NUM(cfgTablePtr,function),
                .bmAttributes = CDC_DATA_OUT_EP_TYPE(cfgTablePtr,function),
                .maxPacketSize = {CY_USB_GET_LSB((CDC_DATA_OUT_EP_TYPE(cfgTablePtr,function) == CY_USB_ENDP_TYPE_BULK) ? 512 : CDC_DATA_OUT_EP_MAXPKTSIZE(cfgTablePtr,function)),
                                  CY_USB_GET_MSB((CDC_DATA_OUT_EP_TYPE(cfgTablePtr,function) == CY_USB_ENDP_TYPE_BULK) ? 512 : CDC_DATA_OUT_EP_MAXPKTSIZE(cfgTablePtr,function))},
                .interval = CDC_DATA_OUT_EP_INTERVAL(cfgTablePtr,function),
            },
        };
        Cy_MemCopy(&dscr[offset], (uint8_t*)&dataEPHSIn, sizeof(cy_stc_usb_ep_dscr_t));
        offset += sizeof(cy_stc_usb_ep_dscr_t);
        Cy_MemCopy(&dscr[offset], (uint8_t*)&dataEPHSOut, sizeof(cy_stc_usb_ep_dscr_t));
        offset += sizeof(cy_stc_usb_ep_dscr_t);

    return offset;
}

/**
 * \name Cy_USB_AddCDCDescriptors
 * \brief Function to populate CDC interface descriptors
 * \param dscr Pointer to descriptor buffer
 * \param cfgTablePtr Pointer to configuration table
 * \param prevOffset Previous offset
 * \param firstInterface Interface number
 * \param function Function number
 * \param speed USB speed
 * \retval uint32_t Current offset
 */
static uint32_t Cy_USB_AddCDCDescriptors(uint8_t* dscr, cy_stc_usb_dev_config_t* cfgTablePtr, uint32_t prevOffset, uint8_t firstInterface, uint8_t function, uint8_t speed)
{
    uint32_t offset = prevOffset;
    uint8_t secondInterface = firstInterface + 1;

    Cy_USB_PopulateCDCInterfaceDescriptorStart[CDC_FIRST_INTERFACE_INDEX_1] = firstInterface;
    Cy_USB_PopulateCDCInterfaceDescriptorStart[CDC_FIRST_INTERFACE_INDEX_2] = firstInterface;
    Cy_USB_PopulateCDCInterfaceDescriptorStart[CDC_SECOND_INTERFACE_INDEX_1] = secondInterface;
    Cy_USB_PopulateCDCInterfaceDescriptorStart[CDC_SECOND_INTERFACE_INDEX_2] = secondInterface;

    Cy_USB_PopulateCDCInterfaceDescriptorStart[8] = (function + USB_STRING_DESCR_OFFSET);

    Cy_MemCopy(&dscr[offset], (uint8_t*)&Cy_USB_PopulateCDCInterfaceDescriptorStart, sizeof(Cy_USB_PopulateCDCInterfaceDescriptorStart));
    offset += sizeof(Cy_USB_PopulateCDCInterfaceDescriptorStart);

    offset = Cy_USB_PopulateCDCInterfaceDescriptorEnd(dscr, cfgTablePtr, secondInterface, function,
        offset, speed);
    return offset;
}

/**
 * \name Cy_USB_InitSerialNoStringDesc
 * \brief Function to generate serial number string descriptor
 * \param serialNoDesc Pointer to serial number descriptor
 * \retval None
 */
void Cy_USB_InitSerialNoStringDesc(uint8_t *serialNoDesc)
{
    uint8_t i = 0;
    uint64_t serialNo;
    serialNo = Cy_SysLib_GetUniqueId();
    while(i < 64)
    {
        serialNoDesc[(i / 2)] = CHARMAP(((serialNo >> (60 - i) & (0xF))));
        serialNoDesc[1 +(i / 2)] = 0x00;
        i += 4; 
    }
}

/**
 * \name Cy_USB_PopulateConfigDescriptors
 * \brief Function to populate configuration descriptors based on config table
 * \param cfgTablePtr Pointer to config table
 * \param appInfoPtr Pointer to application info structure
 * \param pUsbSpeed USB speed
 * \retval None
 */
static void Cy_USB_PopulateConfigDescriptors(cy_stc_usb_dev_config_t *cfgTablePtr, cy_stc_app_info_t* appInfoPtr, cy_en_usb_speed_t pUsbSpeed)
{
    uint32_t offsetHS = 0;
    uint8_t numInterfaceTypes = 0;
    uint8_t firstInterface = 0;
    uint8_t i = 0;
    uint8_t function = 0;
    uint8_t index = 0;

    /*Find number of interface types are present in the current configuration type*/
    numInterfaceTypes =  cfgTablePtr->usbFnCount;

    /* Add Standard Configuration Descriptor for HS */
    offsetHS = Cy_USB_PopulateConfigDescriptorStart((uint8_t*)&glUsbHsConfigDscr, cfgTablePtr,
        CY_USBD_USB_DEV_HS, appInfoPtr->totalNumInterfaces);

    /* Add descriptors for all interfaces based on the type of interface */
    for (i = 0; i < numInterfaceTypes; i++)
    {
        function = i;

        if(CHECK_FN_CONFIG(cfgTablePtr,function) != NULL)
        {
            DBG_APP_TRACE("CDC %d\n\r",firstInterface);
            /* HS Config Descriptors */
            offsetHS = Cy_USB_AddInterfaceAssociationDescriptor((uint8_t*)&glUsbHsConfigDscr,
                function,
                firstInterface,
                NUM_INTERFACES_CDC, CDC_CTRL_CLASS,
                CDC_SUBCLASS, MISC_PROTOCOL, offsetHS);

            offsetHS = Cy_USB_AddCDCDescriptors((uint8_t *)&glUsbHsConfigDscr,cfgTablePtr,offsetHS, firstInterface, function, CY_USBD_USB_DEV_HS);

            for (index = 0; index < NUM_INTERFACES_CDC; index++)
            {
                appInfoPtr->usbInterface[firstInterface + index].functionNum =  function;
            }
            appInfoPtr->cdcCount ++;
            firstInterface += (NUM_INTERFACES_CDC);
        }
    }
    glUsbCfgDscrSizeHs += offsetHS;
    glUsbHsConfigDscr.config.totalLength[0] = CY_USB_GET_LSB(glUsbCfgDscrSizeHs);
    glUsbHsConfigDscr.config.totalLength[1] = CY_USB_GET_MSB(glUsbCfgDscrSizeHs);
}

/**
 * \name Cy_USB_PopulateStringDescriptors
 * \brief Function to populate string descriptors based on config table
 * \param cfgTablePtr Pointer to config table
 * \retval None
 */
static void Cy_USB_PopulateStringDescriptors(cy_stc_usb_dev_config_t* cfgTablePtr)
{
    if((CHECK_PRODUCT_STRING(cfgTablePtr) != NULL ) && (CHECK_MANUFACTURER_STRING(cfgTablePtr) != NULL))
    {

        Cy_MemCopy((uint8_t*)&glUsbProductStrDscr,
            GET_PRODUCT_STRING_DSCR_ADDR(cfgTablePtr),
            GET_PRODUCT_STRING_LEN(cfgTablePtr) );

        Cy_MemCopy((uint8_t*)&glUsbManufacturerStrDscr,
            GET_MANUFACTURER_STRING_DSCR_ADDR(cfgTablePtr),
           GET_MANUFACTURER_STRING_LEN(cfgTablePtr) );
    }

    if(CHECK_AUTO_GENERATE_SERIAL_NUM(cfgTablePtr))
    {
        glUsbSerialNumStrDscr.length = 34;/*2 = size of len + dscrtype*/
        glUsbSerialNumStrDscr.descrType = USB_DSCR_SERIALNUMBER_STRING_INDEX;
       memset(glUsbSerialNumStrDscr.serialNumString,0,32);
       Cy_USB_InitSerialNoStringDesc(glUsbSerialNumStrDscr.serialNumString);
    }
    else
    {
        if(CHECK_SERIAL_NUM_STRING(cfgTablePtr)!=NULL && GET_SERIAL_NUM_STRING_LEN(cfgTablePtr) !=0 )
        {
            Cy_MemCopy((uint8_t*)&glUsbSerialNumStrDscr,
                     GET_SERIAL_NUM_STRING_DSCR_ADDR(cfgTablePtr),
                     GET_SERIAL_NUM_STRING_LEN(cfgTablePtr));
        }
    }

}

/**
 * \name PrintString
 * \brief Function to print a string
 * \param string Pointer to string
 * \param length Length of string
 * \retval None
 */
void PrintString(const char *string, uint8_t length)
{
    uint8_t index;

    for (index = 0; index < length; index++)
    {
        while (Cy_SCB_UART_GetNumInTxFifo (SCB1) >= 16)
        {
            Cy_SysLib_DelayUs(1);
        }

        Cy_SCB_WriteTxFifo(SCB1, (uint32_t)string[index]);
    }
}

#define CHARMAP(c)      (((c) >= 10) ? ('A' + (c) - 10) : ('0' + (c)))

/**
 * \name PrintByte
 * \brief Function to print one byte of data
 * \param data 1-byte data to print
 * \retval None
 */
void PrintByte(uint8_t data)
{
    while (Cy_SCB_UART_GetNumInTxFifo (SCB1) >= 16);
    Cy_SCB_WriteTxFifo (SCB1, CHARMAP(data >> 4));
    Cy_SCB_WriteTxFifo (SCB1, CHARMAP(data & 0xF));
    Cy_SCB_WriteTxFifo (SCB1, ' ');
}

/**
 * \name PrintBuffer
 * \brief Function to print the values from buffer
 * \param buf_p Buffer pointer
 * \param len Size of buffer
 * \retval None
 */
void PrintBuffer(uint8_t *buf_p, uint16_t len)
{
    uint16_t i;
    PrintString("BUF:", 4);
    PrintByte((len&0xFF));
    PrintByte(((len>>8)&0xFF));
    Cy_SCB_WriteTxFifo (SCB1, ':');
    for (i = 0; i < len; i++)
        PrintByte(buf_p[i]);
    PrintString("\r\n", 2);
}

/**
 * \name hexDump
 * \brief Function to print the hex values from buffer
 * \param buffer Buffer pointer
 * \param len Size of buffer
 * \retval None
 */
void hexDump(uint8_t* buffer, const uint16_t len)
{
    uint16_t index = 0;

    DBG_APP_TRACE("\r\nPrinting %d items\r\n", len);
    for (index = 0; index < len; index++)
    {
        /* Group 8 Bytes */
        if (!(index % 8))
        {
            DBG_APP_INFO("\r\n");
        }
        /*Print as 0x0_ if num < 0x10 */
        if (buffer[index] < 0x10)
        {
            DBG_APP_INFO("0%x ", buffer[index]);
        }
        else
        {
            DBG_APP_INFO("%x ", buffer[index]);
        }
    }
    DBG_APP_INFO("\r\n\r\n");
}

/**
 * \name Cy_USB_PopulateDescriptors
 * \brief Function to parse function configuration and populate descriptor based on USB speed
 * \param appCtx Pointer to application context
 * \param pUsbSpeed USB speed
 * \retval None
 */
void Cy_USB_PopulateDescriptors(cy_stc_usb_app_ctxt_t *appCtx, cy_en_usb_speed_t pUsbSpeed)
{

    cy_stc_usb_dev_config_t *cfgTablePtr = appCtx->cfgTablePtr;
    cy_stc_app_info_t *appInfoPtr = appCtx->appInfoPtr;
    /*Create device descriptors based on config table */
    Cy_USB_PopulateDeviceDescriptors(cfgTablePtr, appInfoPtr,pUsbSpeed);
    /*Create config descriptors based on config table */
    Cy_USB_PopulateConfigDescriptors(cfgTablePtr, appInfoPtr, pUsbSpeed);

    Cy_USB_PopulateStringDescriptors(cfgTablePtr);

#if PRINT_DSCR

    DBG_APP_INFO("Device Descriptors 2.0 \n\r");
    PrintBuffer((uint8_t*)&glUsbDevDscr20, sizeof(glUsbDevDscr20));
    DBG_APP_INFO("Dev Qual Descriptors \n\r");
    PrintBuffer((uint8_t*)&glUsbDevQualDscr, sizeof(glUsbDevQualDscr));
    DBG_APP_INFO("**String Descriptors** \n\r");
    PrintBuffer((uint8_t*)&glUsbManufacturerStrDscr, glUsbManufacturerStrDscr.length);
    PrintBuffer((uint8_t*)&glUsbProductStrDscr, glUsbProductStrDscr.length);
    PrintBuffer((uint8_t*)&glUsbSerialNumStrDscr, glUsbSerialNumStrDscr.length);
    DBG_APP_INFO("Config Descriptors \n\r");

    PrintBuffer((uint8_t*)&glUsbHsConfigDscr, glUsbCfgDscrSizeHs);

#endif
}


/* Full speed Device descriptor */
const uint8_t FullSpeedDeviceDescr[] = {
    0x12,                           /* Descriptor size */
    0x01,                           /* Device descriptor type */
    0x10,0x02,                      /* USB 2.1 */
    0x00,                           /* Device class: Defined in interface */
    0x00,                           /* Device Sub-class:  Defined in interface */
    0x00,                           /* Device protocol: Defined in interface */
    0x40,                           /* Maxpacket size for EP0 */
    0xB4,0x04,                      /* Vendor ID */
    0xFB,0x00,                      /* Product ID: Use PID that maps to devices certified with CyUsb3 driver. */
    0x00,0x00,                      /* Device release number */
    0x01,                           /* Manufacture string index */
    0x02,                           /* Product string index */
    0x03,                           /* Serial number string index */
    0x01                            /* Number of configurations */
};

/* Full speed Configuration descriptor */
const uint8_t FullSpeedConfigDescr[] = {
    /* Configuration descriptor */
    0x09,                           /* Descriptor size */
    0x02,                           /* Configuration descriptor type */
    0x12,0x00,                      /* Length of this descriptor and all sub descriptors */
    0x01,                           /* Number of interfaces */
    0x01,                           /* Configuration number */
    0x03,                           /* Configuration string index: CyFxUSBFSDscr */
    0x80,                           /* Config characteristics - bus powered */
    0x32,                           /* Max power consumption of device (in 2mA unit) : 100mA */

    /* interface descriptor */
    0x09,                           /* Descriptor size */
    0x04,                           /* Interface descriptor type */
    0x00,                           /* Interface number */
    0x00,                           /* Alternate setting number */
    0x00,                           /* Number of end points */
    0xFF,                           /* Interface class: Vendor defined */
    0x00,                           /* Interface sub class: Vendor defined */
    0x00,                           /* Interface protocol: Vendor defined */
    0x00                            /* Interface descriptor string index */
};

/* Language ID descriptor */
USB3_DESC_ATTRIBUTES uint8_t CyFxLangString[16] =
{
    0x04,
    0x03,
    0x09,
    0x04
};

/* Full speed string descriptor */
USB3_DESC_ATTRIBUTES uint8_t FullSpeedStringDescr[] = {
    0x22,        /* Descriptor Size */
    0x03,        /* Device descriptor type */
    'F',0x00,
    'S',0x00,
    ' ',0x00,
    'N',0x00,
    'O',0x00,
    'T',0x00,
    ' ',0x00,
    'S',0x00,
    'U',0x00,
    'P',0x00,
    'P',0x00,
    'O',0x00,
    'R',0x00,
    'T',0x00,
    'E',0x00,
    'D',0x00
};


/* MS OS String Descriptor */
USB3_DESC_ATTRIBUTES uint8_t glOsString[] __attribute__ ((aligned (32))) =
{
    0x12, /* Length. */
    0x03, /* Type - string. */
    'M', 0x00, 'S', 0x00, 'F', 0x00, 'T', 0x00, '1', 0x00, '0', 0x00, '0', 0x00, /* Signature. */
    MS_VENDOR_CODE, /* MS vendor code. */
    0x00 /* Padding. */
};

/**
 * \name Cy_USB_RegisterFnString
 * \brief Function to register function string
 * \param pAppCtxt Pointer to application context
 * \retval None
 */
void Cy_USB_RegisterFnString(cy_stc_usb_app_ctxt_t* pAppCtxt)
{
    uint8_t index = 0;
    cy_stc_usb_dev_config_t* cfgTablePtr = pAppCtxt->cfgTablePtr;
    for(index =0; index < cfgTablePtr->usbFnCount; index++)
    {
        if(CHECK_FN_CONFIG(cfgTablePtr,index) != NULL)
        {

            if(CDC_INTERFACE_STRING(cfgTablePtr,index) != NULL)
            {
                Cy_MemCopy(fnStringDescr[index],(uint8_t*)CDC_INTERFACE_STRING(cfgTablePtr,index),CDC_INTERFACE_STRING(cfgTablePtr,index)[0]);
                Cy_USBD_SetDscr(pAppCtxt->pUsbdCtxt, CY_USB_SET_STRING_DSCR, index + USB_STRING_DESCR_OFFSET, (uint8_t*)fnStringDescr[index]);
            }
        }
    }
}

/**
 * \name Cy_USB_RegisterUsbDescriptors
 * \brief Function to register USB Descriptor to USBD
 * \param pAppCtxt Pointer to application context
 * \param usbSpeed USB speed
 * \retval None
 */
void Cy_USB_RegisterUsbDescriptors(cy_stc_usb_app_ctxt_t* pAppCtxt, cy_en_usb_speed_t usbSpeed)
{
    
    if ((pAppCtxt != NULL) && (pAppCtxt->pUsbdCtxt != NULL)) {

        /* Register USB descriptors with the stack for USB-HS */
        if (usbSpeed == CY_USBD_USB_DEV_HS)
        {
            DBG_APP_TRACE("Register HS Descriptor\r\n");
            if (pAppCtxt->devState >= CY_USB_DEVICE_STATE_ADDRESS)
                Cy_USBD_SetDscr(pAppCtxt->pUsbdCtxt, CY_USB_SET_HS_DEVICE_DSCR, 0, (uint8_t*)&glUsbDevDscr20);
            else
                Cy_USBD_SetDscr(pAppCtxt->pUsbdCtxt, CY_USB_SET_HS_DEVICE_DSCR, 0, (uint8_t*)FullSpeedDeviceDescr);

            Cy_USBD_SetDscr(pAppCtxt->pUsbdCtxt, CY_USB_SET_HS_CONFIG_DSCR, 0, (uint8_t*)&glUsbHsConfigDscr);
            Cy_USBD_SetDscr(pAppCtxt->pUsbdCtxt, CY_USB_SET_FS_CONFIG_DSCR, 0, (uint8_t*)FullSpeedConfigDescr);
            Cy_USBD_SetDscr(pAppCtxt->pUsbdCtxt, CY_USB_SET_STRING_DSCR, 0, (uint8_t*)CyFxLangString);
            Cy_USBD_SetDscr(pAppCtxt->pUsbdCtxt, CY_USB_SET_STRING_DSCR, 1, (uint8_t*)&glUsbManufacturerStrDscr);
            Cy_USBD_SetDscr(pAppCtxt->pUsbdCtxt, CY_USB_SET_STRING_DSCR, 2, (uint8_t*)&glUsbProductStrDscr);
            Cy_USBD_SetDscr(pAppCtxt->pUsbdCtxt, CY_USB_SET_STRING_DSCR, 3, (uint8_t*)&glUsbSerialNumStrDscr);
            Cy_USBD_SetDscr(pAppCtxt->pUsbdCtxt, CY_USB_SET_DEVICE_QUAL_DSCR, 0, (uint8_t*)&glUsbDevQualDscr);

            /*Register function/interface string*/
            Cy_USB_RegisterFnString(pAppCtxt);

            Cy_USBD_SetDscr(pAppCtxt->pUsbdCtxt, CY_USB_SET_HS_BOS_DSCR, 0, (uint8_t*)&glUsbBosDscrHs);

        }
        else {
            /* FOR USB FS */
            DBG_APP_TRACE("Register FS Descriptor\r\n");
            Cy_USBD_SetDscr(pAppCtxt->pUsbdCtxt, CY_USB_SET_HS_DEVICE_DSCR, 0, (uint8_t*)CyFxUSBFSDeviceDscr);
            Cy_USBD_SetDscr(pAppCtxt->pUsbdCtxt, CY_USB_SET_FS_CONFIG_DSCR, 0, (uint8_t*)CyFxUSBFSConfigDscr);
            Cy_USBD_SetDscr(pAppCtxt->pUsbdCtxt, CY_USB_SET_STRING_DSCR, 0, (uint8_t*)CyFxUSBStringLangIDDscr);
            Cy_USBD_SetDscr(pAppCtxt->pUsbdCtxt, CY_USB_SET_STRING_DSCR, 1, (uint8_t*)CyFxUSBManufactureDscr);
            Cy_USBD_SetDscr(pAppCtxt->pUsbdCtxt, CY_USB_SET_STRING_DSCR, 2, (uint8_t*)CyFxUSBProductDscr);
            Cy_USBD_SetDscr(pAppCtxt->pUsbdCtxt, CY_USB_SET_STRING_DSCR, 3, (uint8_t*)CyFxUSBFSDscr);
            Cy_USBD_SetDscr(pAppCtxt->pUsbdCtxt, CY_USB_SET_HS_BOS_DSCR, 0, (uint8_t*)&glUsbBosDscrHs);
        }
    }
}

/**
 * \name Cy_USB_RegisterUsbDeviceDescriptors
 * \brief Function to register USB Device Descriptor to USBD
 * \param pUsbdCtxt USBD context
 * \retval None
 */
void Cy_USB_RegisterUsbDeviceDescriptors(cy_stc_usb_usbd_ctxt_t* pUsbdCtxt)
{
    if ( (pUsbdCtxt != NULL)) {
        DBG_APP_TRACE("Register Device Descriptor\r\n");

        /* Register USB descriptors with the stack for USB HS */
        Cy_USBD_SetDscr(pUsbdCtxt, CY_USB_SET_HS_DEVICE_DSCR, 0, (uint8_t*)&glUsbDevDscr20);
        
    }
} /* end of function */
