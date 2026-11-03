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


#include <stdint.h>
#include <stdbool.h>
#include "cy_usb_common.h"

#ifndef __CONFIG_H__
#define __CONFIG_H__

/* Macro definitions */
#define CY_MAX_CDC_INTERFACES             (4u)        /**< Max no. of CDC functions supported */
#define CY_CDC_MAX_ENDPOINTS              (3u)        /**< No. of endpoints per CDC interface */

/**
 * @typedef cy_en_usb_vbus_polarity_t
 * @brief Enum providing Vbus detect polarity configuration. 
 *        P4.0 pin is always used as Vbus detect indication.
 */
typedef enum {
    CY_VBUS_DETECT_LOW = 0,
    CY_VBUS_DETECT_HIGH = 1
}cy_en_usb_vbus_polarity_t;

/**
 * @typedef cy_en_dbg_interface_t
 * @brief Selection of interface through which firmware logs are output.
 */
typedef enum {
    CY_DBGLOG_TGT_UART_SCB0 = 1,                      /**< Log data through SCB0 configured as UART. */
    CY_DBGLOG_TGT_UART_SCB1,                          /**< Log data through SCB1 configured as UART. */
    CY_DBGLOG_TGT_USBFS_CDC,                          /**< Log data through CDC interface on USBFS port. */
    CY_DBGLOG_TGT_CDC,                                /**< Log data through CDC interface on USBSS port. */
    CY_DBGLOG_TGT_UART_SCB4,                          /**< Log data through SCB4 configured as UART. */
} cy_en_dbg_interface_t;

/**
 * @typedef cy_stc_usb_ep_config_t
 * @brief Specifies USB Endpoint Configuration.
 */
typedef struct cy_stc_usb_ep_config_t {

    cy_en_usb_endp_type_t       epType;                 /**< Endpoint Type */
    uint8_t                     epNumber;               /**< Endpoint Number */
    uint8_t                     epBurst;                /**< Endpoint Burst */
    uint8_t                     epMult;                 /**< Bursts per Interval: Valid for ISO & INTR EP */                                  
    uint8_t                     epServiceInterval;      /**< Service interval for ISO & Interrupt EP */
    uint16_t                    epMaxPktSize;           /**< Maximum Packet Size */
} cy_stc_usb_ep_config_t;

/******************************************************************************/
/*                            CDC Interface                                   */
/******************************************************************************/
/**
 * @typedef cy_en_cdc_sourcesink_t
 * @brief List of Data Source/Sink types.
 */
typedef enum {
    CY_CDC_UART_SCB0 = 1,                           /**< UART SCB 0 */
    CY_CDC_UART_SCB1,                               /**< UART SCB 1 */
    CY_CDC_CPU,                                     /**< CPU: For just sending data to CPU */
    CY_CDC_UART_SCB4,                               /**< UART SCB 4 */
    CY_CDC_UART_SCB6                                /**< UART SCB 6 */
} cy_en_cdc_sourcesink_t;


/**
 * @typedef cy_stc_cdc_fn_config_t
 * @brief Structure providing configuration information for a USB CDC function supported
 * by firmware application.
 * Channels: UART > USB  & USB > UART or CPU>USB & USB >CPU (debugging)
 */
typedef struct {
    uint32_t                            dmaBufferSize;                                  /**< DMA Buffer Size for DMA channel*/
    uint8_t                             dataReceiveEnabled;                             /**< Whether data receive on CDC OUT endpoint is enabled. */
    uint8_t                             dmaBufferCount;                                 /**< DMA Buffer Count for DMA channel*/
    cy_en_cdc_sourcesink_t              cdcDataSourceSink;                              /**< Source / sink type for cdc interface */
    cy_stc_usb_ep_config_t             *cdcEpConfig[CY_CDC_MAX_ENDPOINTS] ;             /**< 0: Contol interface ep in, 1: data interface ep IN 2: data interface ep out */
    const uint8_t                      *cdcFnString;
} cy_stc_cdc_fn_config_t;

/**
 * @typedef cy_stc_fn_config_t
 * @brief Structure providing Functions information
 */
typedef struct {
    void                        *fnConfig;
} cy_stc_fn_config_t;

/**
 * @typedef cy_stc_usb_dev_config_t
 * @brief Structure providing complete configuration information for the template
 * firmware application.
 */
typedef struct {
    uint16_t                                    usbVid;                                 /**< USB Vendor ID to be used. */
    uint16_t                                    usbPid;                                 /**< USB Product ID to be used. */
    uint8_t                                     isSelfPowered;                          /**< Whether device is self powered (true) or bus powered (false). */
    uint8_t                                     serialGenEnabled;                       /**< Whether unique serial number generation is enabled. */
    uint8_t                                     logLevel;                               /**< Verbosity level of log messages. */
    cy_en_dbg_interface_t                       fwDbgInterface;                         /**< Selection of interface used for firmware debug logs. */
    uint8_t                                     usbFnCount;                             /**< Number of USB functions enabled. */
    cy_en_usb_vbus_polarity_t                   vbusDetectPolarity;                     /**< Vbus detect polarity */
    const uint8_t                               *mfgString;                             /**< Pointer to manufacturer string descriptor in the standard
                                                                                            USB format. Non-NULL value required. */
    const uint8_t                               *prodString;                            /**< Pointer to product string descriptor in the standard
                                                                                            USB format. Non-NULL value required. */
    const uint8_t                               *serialNumberString;                    /**< Pointer to serial number string descriptor in the standard
                                                                                            USB format. Can be NULL. */

    cy_stc_fn_config_t                          *fnConfig[CY_MAX_CDC_INTERFACES];       /**< List of USB functions enabled */
} cy_stc_usb_dev_config_t;


#endif /* __CONFIG_H__ */
/*[] */
