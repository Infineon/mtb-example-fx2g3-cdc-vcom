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


#ifndef INCLUDED_CY_DSCR_H
#define INCLUDED_CY_DSCR_H

#include "vcom_config.h"
#include "cy_usbd_version.h"
#include "cy_hbdma.h"
#include "cy_usb_common.h"
#include "cy_usb_app.h"


/* Descriptor helpers used across the CDC-only build. */
#define USB3_DESC_ATTRIBUTES __attribute__ ((section(".descSection"), used))__attribute__ ((aligned (32)))

#define CY_USB_GET_LSB(w)           ((uint8_t)((w) & UINT8_MAX))
#define CY_USB_GET_MSB(w)           ((uint8_t)((w) >> 8))
#define CHARMAP(c)                  (((c) >= 10) ? ('A' + (c) - 10) : ('0' + (c)))

#define USB_DEVICE_DESCR                                    (0x01)
#define USB_CONFIGURATION_DESCR                             (0x02)
#define USB_STRING_DESCR                                    (0x03)
#define USB_INTF_DESCR                                      (0x04)
#define USB_ENDP_DESCR                                      (0x05)
#define USB_DEVICE_QUALITY_DESCR                            (0x06)
#define USB_BOS_DESCR                                       (0x0F)
#define USB_DEVICE_CAPABILITY_DESCR                         (0x10)
#define USB_SSEP_COMPN_DESCR                                (0x30)
#define INTRFC_ASSN_DESCR                                   (11)

#define USB_MAX_PROD_STRING_LEN                             (0xFF)
#define USB_MAX_MANUFACTURER_STRING_LEN                     (0xFF)
#define USB_MAX_SERIAL_NUM_STRING_LEN                       (0xFF)

#define USB_DSCR_BCDUSB2                                    (0x0210)
#define USB_DSCR_EP0_SIZE_USB2                              (0x40)
#define USB_DSCR_BCD_DEVICE                                 (0x0000)
#define USB_DSCR_MANUFACTURER_STRING_INDEX                  (0x01)
#define USB_DSCR_PRODUCT_STRING_INDEX                       (0x02)
#define USB_DSCR_SERIALNUMBER_STRING_INDEX                  (0x03)
#define USB_DSCR_NUM_CONFIGURATIONS                         (0x01)

#define USB_DSCR_BOS_DSCR_LEN                               (5)
#define USB_DSCR_BOS_HS_DSCR_LEN                            (sizeof(cy_stc_usb_hs_bos_dscr_t) - sizeof(cy_stc_usb_usb2ext_dscr_t))
#define USB_DSCR_BOS_HS_TOTAL_LENGTH                        (sizeof(cy_stc_usb_hs_bos_dscr_t))
#define USB_CFG_DSCR_MAX_POWER                              (0x32)
#define USB_DEV_CAP                                         (2)

#define USB_DSCR_VENDOR_ID_0(cfgTablePtr)                   ((uint8_t)(cfgTablePtr->usbVid & 0xFF))
#define USB_DSCR_VENDOR_ID_1(cfgTablePtr)                   ((uint8_t)((cfgTablePtr->usbVid & 0xFF00) >> 8))
#define USB_DSCR_PRODUCT_ID_0(cfgTablePtr)                  ((uint8_t)(cfgTablePtr->usbPid & 0xFF))
#define USB_DSCR_PRODUCT_ID_1(cfgTablePtr)                  ((uint8_t)((cfgTablePtr->usbPid & 0xFF00) >> 8))

#define GET_MANUFACTURER_STRING_LEN(cfgTablePtr)            (cfgTablePtr->mfgString[0])
#define CHECK_PRODUCT_STRING(cfgTablePtr)                   (cfgTablePtr->prodString)
#define CHECK_MANUFACTURER_STRING(cfgTablePtr)              (cfgTablePtr->mfgString)
#define GET_PRODUCT_STRING_LEN(cfgTablePtr)                 (cfgTablePtr->prodString[0])
#define GET_MANUFACTURER_STRING_DSCR_ADDR(cfgTablePtr)      (cfgTablePtr->mfgString)
#define GET_PRODUCT_STRING_DSCR_ADDR(cfgTablePtr)           (cfgTablePtr->prodString)
#define CHECK_AUTO_GENERATE_SERIAL_NUM(cfgTablePtr)         (cfgTablePtr->serialGenEnabled)
#define GET_SERIAL_NUM_STRING_LEN(cfgTablePtr)              (cfgTablePtr->serialNumberString[0])
#define GET_SERIAL_NUM_STRING_DSCR_ADDR(cfgTablePtr)        (cfgTablePtr->serialNumberString)
#define CHECK_SERIAL_NUM_STRING(cfgTablePtr)                (cfgTablePtr->serialNumberString)

#define GET_POWER_CONFIG                                    ((cfgTablePtr->isSelfPowered) ? 0 : 1)
#define USB_CFG_DSCR_CONFIGURATION_VAL                      (1)
#define USB_CFG_DSCR_STRING_INDEX                           (3)
#define USB_CFG_DSCR_BM_ATTRIBUTES                          (RESERVED | USB_CFG_DSCR_POWER_CONFIG | USB_CFG_DSCR_REMOTEWAKEUP_CONFIG)
#define USB_CFG_DSCR_POWER_CONFIG                           (cfgDscrPowerConfiguration[GET_POWER_CONFIG])
#define USB_CFG_DSCR_REMOTEWAKEUP_CONFIG                    (cfgDscrRemoteWakeupConfiguration[0])

#define USB_CFG_DSCR_HS_TOTAL_LEN_L                         (CY_USB_GET_LSB(glUsbCfgDscrSizeHs))
#define USB_CFG_DSCR_HS_TOTAL_LEN_H                         (CY_USB_GET_MSB(glUsbCfgDscrSizeHs))
#define USB_CFG_DSCR_FS_TOTAL_LEN_L                         (CY_USB_GET_LSB(glUsbCfgDscrSizeFs))
#define USB_CFG_DSCR_FS_TOTAL_LEN_H                         (CY_USB_GET_MSB(glUsbCfgDscrSizeFs))

#define CHECK_FN_CONFIG(cfgTablePtr, function)              (cfgTablePtr->fnConfig[function]->fnConfig)
#define FN_TYPE(cfgTablePtr, function)                      (cfgTablePtr->fnConfig[function]->fnType)
#define INFO_FN_NUM(appInfoPtr, i)                          (appInfoPtr->usbInterface[i].functionNum)

#define CDC_FIRST_INTERFACE                                 (0x00)
#define CDC_SECOND_INTERFACE                                (CDC_FIRST_INTERFACE + 1)
#define CDC_MAX_DATA_EP                                     (0x02)
#define CDC_INTERFACE_CLASS                                 (0x0A)
#define CDC_DATA_IN                                         (1)
#define CDC_DATA_OUT                                        (2)
#define USB_STRING_DESCR_OFFSET                             (4)

#define CDC_INTR_EP_NUM(cfgTablePtr, function)                 (((cy_stc_cdc_fn_config_t*)cfgTablePtr->fnConfig[function]->fnConfig)->cdcEpConfig[0]->epNumber)
#define CDC_EP_INTR_TYPE(cfgTablePtr, function)                (((cy_stc_cdc_fn_config_t*)cfgTablePtr->fnConfig[function]->fnConfig)->cdcEpConfig[0]->epType)
#define CDC_INTR_EP_MAXPKTSIZE(cfgTablePtr, function)          (((cy_stc_cdc_fn_config_t*)cfgTablePtr->fnConfig[function]->fnConfig)->cdcEpConfig[0]->epMaxPktSize)
#define CDC_INTR_EP_INTERVAL(cfgTablePtr, function)            (((cy_stc_cdc_fn_config_t*)cfgTablePtr->fnConfig[function]->fnConfig)->cdcEpConfig[0]->epServiceInterval)
#define CDC_INTR_EP_BURST(cfgTablePtr, function)               ((((cy_stc_cdc_fn_config_t*)cfgTablePtr->fnConfig[function]->fnConfig)->cdcEpConfig[0]->epBurst) - 1)
#define CDC_INTR_EP_MULT(cfgTablePtr, function)                (((cy_stc_cdc_fn_config_t*)cfgTablePtr->fnConfig[function]->fnConfig)->cdcEpConfig[0]->epMult)
#define CDC_INTR_EP_BYTESPERINTERVAL(cfgTablePtr, function)    (CDC_INTR_EP_MAXPKTSIZE(cfgTablePtr, function) * (CDC_INTR_EP_MULT(cfgTablePtr, function) + 1) * CDC_INTR_EP_BURST(cfgTablePtr, function))

#define CDC_DATA_IN_EP_NUM(cfgTablePtr, function)              (((cy_stc_cdc_fn_config_t*)cfgTablePtr->fnConfig[function]->fnConfig)->cdcEpConfig[1]->epNumber)
#define CDC_DATA_IN_EP_TYPE(cfgTablePtr, function)             (((cy_stc_cdc_fn_config_t*)cfgTablePtr->fnConfig[function]->fnConfig)->cdcEpConfig[1]->epType)
#define CDC_DATA_IN_EP_MAXPKTSIZE(cfgTablePtr, function)       (((cy_stc_cdc_fn_config_t*)cfgTablePtr->fnConfig[function]->fnConfig)->cdcEpConfig[1]->epMaxPktSize)
#define CDC_DATA_IN_EP_INTERVAL(cfgTablePtr, function)         (((cy_stc_cdc_fn_config_t*)cfgTablePtr->fnConfig[function]->fnConfig)->cdcEpConfig[1]->epServiceInterval)
#define CDC_DATA_IN_EP_BURST(cfgTablePtr, function)            ((((cy_stc_cdc_fn_config_t*)cfgTablePtr->fnConfig[function]->fnConfig)->cdcEpConfig[1]->epBurst) - 1)
#define CDC_DATA_IN_EP_MULT(cfgTablePtr, function)             (((cy_stc_cdc_fn_config_t*)cfgTablePtr->fnConfig[function]->fnConfig)->cdcEpConfig[1]->epMult)
#define CDC_DATA_IN_EP_BYTESPERINTERVAL(cfgTablePtr, function) (CDC_DATA_IN_EP_MAXPKTSIZE(cfgTablePtr, function) * (CDC_DATA_IN_EP_MULT(cfgTablePtr, function) + 1) * CDC_DATA_IN_EP_BURST(cfgTablePtr, function))

#define CDC_DATA_OUT_EP_NUM(cfgTablePtr, function)              (((cy_stc_cdc_fn_config_t*)cfgTablePtr->fnConfig[function]->fnConfig)->cdcEpConfig[2]->epNumber)
#define CDC_DATA_OUT_EP_TYPE(cfgTablePtr, function)             (((cy_stc_cdc_fn_config_t*)cfgTablePtr->fnConfig[function]->fnConfig)->cdcEpConfig[2]->epType)
#define CDC_DATA_OUT_EP_MAXPKTSIZE(cfgTablePtr, function)       (((cy_stc_cdc_fn_config_t*)cfgTablePtr->fnConfig[function]->fnConfig)->cdcEpConfig[2]->epMaxPktSize)
#define CDC_DATA_OUT_EP_INTERVAL(cfgTablePtr, function)         (((cy_stc_cdc_fn_config_t*)cfgTablePtr->fnConfig[function]->fnConfig)->cdcEpConfig[2]->epServiceInterval)
#define CDC_DATA_OUT_EP_BURST(cfgTablePtr, function)            ((((cy_stc_cdc_fn_config_t*)cfgTablePtr->fnConfig[function]->fnConfig)->cdcEpConfig[2]->epBurst) - 1)
#define CDC_DATA_OUT_EP_MULT(cfgTablePtr, function)             (((cy_stc_cdc_fn_config_t*)cfgTablePtr->fnConfig[function]->fnConfig)->cdcEpConfig[2]->epMult)
#define CDC_DATA_OUT_EP_BYTESPERINTERVAL(cfgTablePtr, function) (CDC_DATA_OUT_EP_MAXPKTSIZE(cfgTablePtr, function) * (CDC_DATA_OUT_EP_MULT(cfgTablePtr, function) + 1) * CDC_DATA_OUT_EP_BURST(cfgTablePtr, function))
#define CDC_INTERFACE_STRING(cfgTablePtr, function)             (((cy_stc_cdc_fn_config_t*)cfgTablePtr->fnConfig[function]->fnConfig)->cdcFnString)

typedef enum cy_en_usb_bdevice_class_t
{
    CDC_CTRL_CLASS = 2,
    CDC_DATA_CLASS = 0x0A,
    MISC_CLASS = 0xEF
} cy_en_usb_bdevice_class_t;

typedef enum cy_en_usb_bdevice_subclass_t
{
    MISC_SUBCLASS = 0x02,
    CDC_SUBCLASS = 0x02
} cy_en_usb_bdevice_subclass_t;

typedef enum cy_en_usb_bdevice_protocol_t
{
    MISC_PROTOCOL = 0x01
} cy_en_usb_bdevice_protocol_t;

typedef enum cy_en_usb_wspeed_code_t
{
    LS_SUPPORT = 0x01,
    FS_SUPPORT = 0x02,
    HS_SUPPORT = 0x04,
} cy_en_usb_wspeed_code_t;

typedef enum cy_en_usb_bmattribute_scfg_t
{
    RESERVED = 0x80,
    SELF_POWERED = 0x40,
    BUS_POWERED = 0x00,
    REMOTE_WAKEUP_ENABLED = 0x20,
    REMOTE_WAKEUP_DISABLED = 0x00
} cy_en_usb_bmattribute_scfg_t;

typedef struct cy_stc_usb_device_dscr_t
{
    uint8_t length;
    uint8_t descriptorType;
    uint8_t bcdUSB[2];
    uint8_t deviceClass;
    uint8_t deviceSubClass;
    uint8_t deviceProtocol;
    uint8_t maxPacketSize0;
    uint8_t idVendor[2];
    uint8_t idProduct[2];
    uint8_t bcdDevice[2];
    uint8_t indexManufacturer;
    uint8_t indexProduct;
    uint8_t indexSerialNumber;
    uint8_t numConfigurations;
} cy_stc_usb_device_dscr_t __attribute__((aligned(32)));

typedef struct cy_stc_usb_usb2ext_dscr_t
{
    uint8_t length;
    uint8_t descriptorType;
    uint8_t devCapabilityType;
    uint8_t bmAttributes[4];
} cy_stc_usb_usb2ext_dscr_t;

typedef struct cy_stc_usb_hs_bos_dscr_t
{
    uint8_t length;
    uint8_t descriptorType;
    uint8_t totalLength[2];
    uint8_t numDeviceCaps;
    cy_stc_usb_usb2ext_dscr_t usb20Extension;
} cy_stc_usb_hs_bos_dscr_t __attribute__((aligned(32)));

typedef struct cy_stc_usb_devqual_dscr_t
{
    uint8_t length;
    uint8_t descriptorType;
    uint8_t bcdUSB[2];
    uint8_t deviceClass;
    uint8_t deviceSubClass;
    uint8_t deviceProtocol;
    uint8_t maxPacketSize0;
    uint8_t numConfigurations;
    uint8_t reserved;
} cy_stc_usb_devqual_dscr_t __attribute__((aligned(32)));

typedef struct cy_stc_usb_std_config_dscr_t
{
    uint8_t length;
    uint8_t descriptorType;
    uint8_t totalLength[2];
    uint8_t numInterfaces;
    uint8_t configurationValue;
    uint8_t indexConfiguration;
    uint8_t bmAttributes;
    uint8_t maxPower;
} cy_stc_usb_std_config_dscr_t;

typedef struct cy_stc_usb_std_intf_dscr_t
{
    uint8_t length;
    uint8_t descriptorType;
    uint8_t interfaceNumber;
    uint8_t alternateSetting;
    uint8_t numEndpoints;
    uint8_t interfaceClass;
    uint8_t interfaceSubClass;
    uint8_t interfaceProtocol;
    uint8_t indexInterface;
} cy_stc_usb_std_intf_dscr_t;

typedef struct cy_stc_usb_std_ep_dscr_t
{
    uint8_t length;
    uint8_t descriptorType;
    uint8_t endpointAddress;
    uint8_t bmAttributes;
    uint8_t maxPacketSize[2];
    uint8_t interval;
} cy_stc_usb_std_ep_dscr_t;



typedef struct cy_stc_usb_product_str_dscr_t
{
    uint8_t length;
    uint8_t descrType;
    uint8_t productString[USB_MAX_PROD_STRING_LEN];
} cy_stc_usb_product_str_dscr_t __attribute__((aligned(32)));

typedef struct cy_stc_usb_manufacturer_str_dscr_t
{
    uint8_t length;
    uint8_t descrType;
    uint8_t manufacturerString[USB_MAX_MANUFACTURER_STRING_LEN];
} cy_stc_usb_manufacturer_str_dscr_t __attribute__((aligned(32)));

typedef struct cy_stc_usb_serialnum_str_dscr_t
{
    uint8_t length;
    uint8_t descrType;
    uint8_t serialNumString[USB_MAX_SERIAL_NUM_STRING_LEN];
} cy_stc_usb_serialnum_str_dscr_t __attribute__((aligned(32)));

typedef struct cy_stc_usb_intfassociation_dscr_t
{
    uint8_t length;
    uint8_t descriptorType;
    uint8_t firstInterface;
    uint8_t interfaceCount;
    uint8_t functionClass;
    uint8_t functionSubClass;
    uint8_t functionProtocol;
    uint8_t indexFunction;
} cy_stc_usb_intfassociation_dscr_t;

typedef struct cy_stc_usb_ep_dscr_t
{
    cy_stc_usb_std_ep_dscr_t streamingEP;
} cy_stc_usb_ep_dscr_t;

#define MAX_INTERFACE_SIZE_CDC_HS                             (75U * MAX_CDC_FUNCTIONS)
#define MAX_INTERFACE_SIZE_HS                                 (MAX_INTERFACE_SIZE_CDC_HS)

typedef struct cy_stc_usb_hsfs_config_dscr_t
{
    cy_stc_usb_std_config_dscr_t config;
    uint8_t interfaceCollection[MAX_INTERFACE_SIZE_HS];
} cy_stc_usb_hsfs_config_dscr_t __attribute__((aligned(32)));

typedef struct cy_stc_cdc_data_intf_dscr_t
{
    uint8_t length;
    uint8_t descriptorType;
    uint8_t interfaceNumber;
    uint8_t alternateSetting;
    uint8_t noOfEndpoints;
    uint8_t interfaceClass;
    uint8_t interfaceSubClass;
    uint8_t interfaceProtocol;
    uint8_t interfaceStringIndex;
} cy_stc_cdc_data_intf_dscr_t;

typedef enum cy_en_cdc_intf_index_t
{
    CDC_FIRST_INTERFACE_INDEX_1 = 2,
    CDC_FIRST_INTERFACE_INDEX_2 = 21,
    CDC_SECOND_INTERFACE_INDEX_1 = 22,
    CDC_SECOND_INTERFACE_INDEX_2 = 27
} cy_en_cdc_intf_index_t;

/**
 * \name Cy_USB_PopulateDescriptors
 * \brief Function to parse function configuration and populate descriptor based on USB speed
 * \param pAppCtxt Pointer to application context
 * \param usbSpeed USB speed
 * \retval None
 */
void Cy_USB_PopulateDescriptors(cy_stc_usb_app_ctxt_t *pAppCtxt,cy_en_usb_speed_t usbSpeed);

/**
 * \name Cy_USB_PopulateDeviceDescriptors
 * \brief Function to populate device descriptor
 * \param cfgTablePtr Pointer to config table
 * \param appInfoPtr Pointer to information structure
 * \param usbSpeed USB speed
 * \retval None
 */
void Cy_USB_PopulateDeviceDescriptors(cy_stc_usb_dev_config_t* cfgTablePtr, 
                cy_stc_app_info_t* appInfoPtr,cy_en_usb_speed_t usbSpeed );

/**
 * \name Cy_USB_RegisterUsbDeviceDescriptors
 * \brief Function to register USB Device Descriptor to USBD
 * \param pUsbdCtxt USBD context
 * \retval None
 */
void Cy_USB_RegisterUsbDeviceDescriptors(cy_stc_usb_usbd_ctxt_t* pUsbdCtxt);

/**
 * \name Cy_USB_RegisterUsbDescriptors
 * \brief Function to register USB Descriptor to USBD
 * \param pAppCtxt Pointer to application context
 * \param usbSpeed USB speed
 * \retval None
 */
void Cy_USB_RegisterUsbDescriptors(cy_stc_usb_app_ctxt_t* pAppCtxt, cy_en_usb_speed_t usbSpeed);

/*****************************************************************************
*                               Variables                                    *
******************************************************************************/
extern cy_stc_usb_dev_config_t appConfig;
extern cy_stc_usb_device_dscr_t glUsbDevDscr20;
extern cy_stc_usb_hs_bos_dscr_t glUsbBosDscrHs;
extern cy_stc_usb_devqual_dscr_t glUsbDevQualDscr;
extern cy_stc_usb_hsfs_config_dscr_t glUsbHsConfigDscr;
extern cy_stc_usb_product_str_dscr_t glUsbProductStrDscr;
extern cy_stc_usb_manufacturer_str_dscr_t glUsbManufacturerStrDscr;
extern cy_stc_usb_serialnum_str_dscr_t glUsbSerialNumStrDscr;

#endif /*INCLUDED_CY_DSCR_H*/


