# EZ-USB&trade; FX2G3: USB communications device class application

This code example demonstrates a quad-channel USB communications device class (CDC) virtual COM port application for Infineon's EZ-USB&trade; FX2G3 device. Data received on each virtual COM port OUT endpoint is forwarded to the SCB UART interface assigned to that port, while data received on the corresponding SCB UART interface is transmitted to the USB host through the associated virtual COM port IN endpoint.

[View this README on GitHub.](https://github.com/Infineon/mtb-example-fx2g3-cdc-vcom)

[Provide feedback on this code example.](https://yourvoice.infineon.com/jfe/form/SV_1NTns53sK2yiljn?Q_EED=eyJVbmlxdWUgRG9jIElkIjoiQ0UyNDIwMDkiLCJTcGVjIE51bWJlciI6IjAwMi00MjAwOSIsIkRvYyBUaXRsZSI6IkVaLVVTQiZ0cmFkZTsgRlgyRzM6IFVTQiBjb21tdW5pY2F0aW9ucyBkZXZpY2UgY2xhc3MgYXBwbGljYXRpb24iLCJyaWQiOiJzdW1pdC5rdW1hckBpbmZpbmVvbi5jb20iLCJEb2MgdmVyc2lvbiI6IjEuMC4wIiwiRG9jIExhbmd1YWdlIjoiRW5nbGlzaCIsIkRvYyBEaXZpc2lvbiI6Ik1DRCIsIkRvYyBCVSI6IldJUkVEIiwiRG9jIEZhbWlseSI6IlNTX1VTQiJ9)


## Requirements

- [ModusToolbox&trade;](https://www.infineon.com/modustoolbox) v3.5 or later (tested with v3.5)
- Board support package (BSP) minimum required version: 4.3.3
- Programming language: C


## Supported toolchains (make variable 'TOOLCHAIN')

- GNU Arm&reg; Embedded Compiler v11.3.1 (`GCC_ARM`) – Default value of `TOOLCHAIN`
- Arm&reg; Compiler v6.22 (`ARM`)


## Supported kits (make variable 'TARGET')

- [EZ-USB&trade; FX2G3 DVK](https://www.infineon.com/cms/en/product/promopages/ez-usb-fx2g3) (`KIT_FX2G3_104LGA`) – Default value of `TARGET`


## Hardware setup

This example uses the board's default configuration. See the kit user guide to ensure that the board is configured correctly.


## Software setup

See the [ModusToolbox&trade; tools package installation guide](https://www.infineon.com/ModusToolboxInstallguide) for information about installing and configuring the tools package.

Install a terminal emulator if you do not have one. Instructions in this document use [Tera Term](https://teratermproject.github.io/index-en.html).

Install the [EZ-USB&trade; FX Control Center](https://softwaretools.infineon.com/tools/com.ifx.tb.tool.ezusbfxcontrolcenter) application for assistance with device programming and testing.


## Using the code example


### Create the project

The ModusToolbox&trade; tools package provides the Project Creator as both a GUI tool and a command line tool.

<details><summary><b>Use Project Creator GUI</b></summary>

1. Open the Project Creator GUI tool

   There are several ways to do this, including launching it from the dashboard or from inside the Eclipse IDE. For more details, see the [Project Creator user guide](https://www.infineon.com/ModusToolboxProjectCreator) (locally available at *{ModusToolbox&trade; install directory}/tools_{version}/project-creator/docs/project-creator.pdf*)

2. On the **Choose Board Support Package (BSP)** page, select a kit supported by this code example. See [Supported kits](#supported-kits-make-variable-target)

   > **Note:** To use this code example for a kit not listed here, you may need to update the source files. If the kit does not have the required resources, the application may not work

3. On the **Select Application** page:

   a. Select the **Applications(s) Root Path** and the **Target IDE**

      > **Note:** Depending on how you open the Project Creator tool, these fields may be pre-selected for you

   b. Select this code example from the list by enabling its check box

      > **Note:** You can narrow the list of displayed examples by typing in the filter box

   c. (Optional) Change the suggested **New Application Name** and **New BSP Name**

   d. Click **Create** to complete the application creation process

</details>


<details><summary><b>Use Project Creator CLI</b></summary>

The 'project-creator-cli' tool can be used to create applications from a CLI terminal or from within batch files or shell scripts. This tool is available in the *{ModusToolbox&trade; install directory}/tools_{version}/project-creator/* directory.

Use a CLI terminal to invoke the 'project-creator-cli' tool. On Windows, use the command-line 'modus-shell' program provided in the ModusToolbox&trade; installation instead of a standard Windows command-line application. This shell provides access to all ModusToolbox&trade; tools. You can access it by typing "modus-shell" in the search box in the Windows menu. In Linux and macOS, you can use any terminal application.

The following example clones the "mtb-example-fx2g3-cdc-vcom" example with the desired name "FX2G3_CDC_VCOM" configured for the *KIT_FX2G3_104LGA* BSP into the specified working directory, *C:/mtb_projects*:

   ```
   project-creator-cli --board-id KIT_FX2G3_104LGA --app-id mtb-example-fx2g3-cdc-vcom --user-app-name FX2G3_CDC_VCOM --target-dir "C:/mtb_projects"
   ```

The 'project-creator-cli' tool has the following arguments:

Argument | Description | Required/optional
---------|-------------|-----------
`--board-id` | Defined in the <id> field of the [BSP](https://github.com/Infineon?q=bsp-manifest&type=&language=&sort=) manifest | Required
`--app-id`   | Defined in the <id> field of the [CE](https://github.com/Infineon?q=ce-manifest&type=&language=&sort=) manifest | Required
`--target-dir`| Specify the directory in which the application is to be created if you prefer not to use the default current working directory | Optional
`--user-app-name`| Specify the name of the application if you prefer to have a name other than the example's default name | Optional

<br>

> **Note:** The project-creator-cli tool uses the `git clone` and `make getlibs` commands to fetch the repository and import the required libraries. For details, see the "Project creator tools" section of the [ModusToolbox&trade; tools package user guide](https://www.infineon.com/ModusToolboxUserGuide) (locally available at {ModusToolbox&trade; install directory}/docs_{version}/mtb_user_guide.pdf).

</details>


### Open the project

After the project has been created, you can open it in your preferred development environment.


<details><summary><b>Eclipse IDE</b></summary>

If you opened the Project Creator tool from the included Eclipse IDE, the project will open in Eclipse automatically.

For more details, see the [Eclipse IDE for ModusToolbox&trade; user guide](https://www.infineon.com/MTBEclipseIDEUserGuide) (locally available at *{ModusToolbox&trade; install directory}/docs_{version}/mt_ide_user_guide.pdf*).

</details>


<details><summary><b>Visual Studio (VS) Code</b></summary>

Launch VS Code manually, and then open the generated *{project-name}.code-workspace* file located in the project directory.

For more details, see the [Visual Studio Code for ModusToolbox&trade; user guide](https://www.infineon.com/MTBVSCodeUserGuide) (locally available at *{ModusToolbox&trade; install directory}/docs_{version}/mt_vscode_user_guide.pdf*).

</details>


<details><summary><b>Command line</b></summary>

If you prefer to use the CLI, open the appropriate terminal, and navigate to the project directory. On Windows, use the command-line 'modus-shell' program; on Linux and macOS, you can use any terminal application. From there, you can run various `make` commands.

For more details, see the [ModusToolbox&trade; tools package user guide](https://www.infineon.com/ModusToolboxUserGuide) (locally available at *{ModusToolbox&trade; install directory}/docs_{version}/mtb_user_guide.pdf*).

</details>


### Using this code example with specific products

By default, the code example builds for the `CYUSB2318-BF104AXI` product.


#### List of supported products

- `CYUSB2318-BF104AXI`

- `CYUSB2317-BF104AXI`

- `CYUSB2316-BF104AXI`

- `CYUSB2315-BF104AXI`

#### Maximum VCOM-UART channels supported for each of the devices:

**Table 1. Maximum VCOM-UART channels and their supported devices**

| Product | Maximum VCOM-UART channels |
|:-------:|:--------------------------:|
| `CYUSB2318-BF104AXI` |       4       |
| `CYUSB2317-BF104AXI` |       4       |
| `CYUSB2316-BF104AXI` |       2       |
| `CYUSB2315-BF104AXI` |       1       |

<br>

> **Note:** For each of the products, the `VCOM_UART_CHANNELS` Makefile variable must be set equal to or less than the above supported maximum number of channels.

#### Setup for a different product

Perform the following steps to build this code example for a different, supported product:

1. Launch the BSP assistant tool:

   a. **Eclipse IDE:** Launch the **BSP Assistant** tool by navigating to **Quick Panel** > **Tools**

   b. **Visual Studio Code:** Select the ModusToolbox&trade; extension from the menu bar, and launch the **BSP Assistant** tool, available in the **Application** menu of the **MODUSTOOLBOX TOOLS** section from the left pane

2. In **BSP Assistant**, select **Devices** from the tree view on the left

3. Choose `CYUSB231x-BF104AXI` from the drop-down menu, on the right

4. Click **Save**

   This closes the **BSP Assistant** tool

5. Navigate the IDE's **Explorer** and delete the *GeneratedSource* folder (if available) at *`<bsp-root-folder>`/bsps/TARGET_APP_KIT_FX2G3_104LGA/*

   > **Note:** For products `CYUSB2315-BF104AXI` and `CYUSB2316-BF104AXI`, additionally delete the `*.cyqspi` file from the *config* directory

6. Launch the **Device Configurator** tool

   a. **Eclipse IDE:** Select your project in the project explorer, and launch the **Device Configurator** tool by navigating to **Quick Panel** > **Tools**

   b. **Visual Studio Code:** Select the ModusToolbox&trade; extension from the left menu bar, and launch the **Device Configurator** tool, available in the **BSP** menu of the **MODUSTOOLBOX TOOLS** section from the left pane

7. Correct the issues (if any) specified in the **Errors** section on the bottom

   a. For a switch from the `CYUSB2318-BF104AXI` product to any other, a new upper limit of 100 MHz is imposed on the desired frequency that can originate from the PLL. Select this issue and change the desired frequency from 150 MHz to 75 MHz

   b. The `CLK_PERI` clock, which is derived from this new source frequency, is also affected. To restore it to its original frequency, go to the **System Clocks** tab, select `CLK_PERI`, and set its divider to '1' (instead of '2')


## Compile-time configurations

Application functionality can be customized by setting variables in *Makefile* or by configuring them through `make` CLI arguments.

- Run the `make build` command or build the project in your IDE to compile the application and generate a USB bootloader-compatible binary. This binary can be programmed onto the EZ-USB&trade; FX2G3 device using the EZ-USB&trade; FX Control Center application

- Run the `make build CORE=CM0P` command or set the variable in *Makefile* to compile and generate the binary for the Cortex&reg; M0+ core. By default, `CORE` is set as `CM4` and the binary is compiled and generated for the Cortex&reg; M4 core

- Choose between the **Arm&reg; Compiler** or the **GNU Arm&reg; Embedded Compiler** build toolchains by setting the `TOOLCHAIN` variable in *Makefile* to `ARM` or `GCC_ARM` respectively. If you set it to `ARM`, ensure to set `CY_COMPILER_ARM_DIR` as a make variable or environment variable, pointing to the path of the compiler's root directory

Additional settings can be configured through macros specified by the `DEFINES` variable in *Makefile*:

**Table 2. Macro description**

Option name           | Description | Allowed values
:-------------        | :---------- | :-------------
`VCOM_UART_CHANNELS`  | Select the number of CDC-UART bridges enabled by the firmware through the `DEFINES` setting in *Makefile* | `1`, `2`, `3`, `4`


## Operation

> **Note:** This code example has been tested with Windows, Linux, and macOS hosts.

1. Connect the board (J2) to your PC using the provided USB cable

2. Connect the USBFS port (J7) on the board to the PC for debug logs

3. Open a terminal program and select the Serial COM port. Set the serial port parameters to 8N1 and 921600 baud

4. Follow these steps to program the board using the [**EZ-USB&trade; FX Control Center**](https://softwaretools.infineon.com/tools/com.ifx.tb.tool.ezusbfxcontrolcenter) (Alpha) application

   1. Perform the following steps to enter into the **Bootloader** mode:

      a. Press and hold the **PMODE** (**SW1**) switch<br>
      b. Press and release the **RESET** switch<br>
      c. Release the **PMODE** switch<br>

   2. Open **EZ-USB&trade; FX Control Center** application
      The **EZ-USB&trade; FX2G3** device displays as **EZ-USB&trade; FX BOOTLOADER**

   3. Select the **EZ-USB&trade; FX BOOTLOADER** device in **EZ-USB&trade; FX Control Center**

   4. Navigate to **Program** > **Internal Flash**

   5. Navigate to the *<CE Title>/build/APP_KIT_FX2G3_104LGA/Release* folder within the CE directory and locate the *.hex* file and program

   6. Confirm if the programming is successful in the log window of the **EZ-USB&trade; FX Control Center** application

   7. Select the **FX Bootloader** device in **EZ-USB&trade; FX Control Center** and navigate to **Program** > **Internal Flash**

   8. Navigate to the */build/APP_KIT_FX2G3_104LGA/Release/* folder within the CE directory and locate the *.hex* file, and program <br>

   9. Confirm if the programming is successful in the log window of **EZ-USB&trade; FX Control Center**

5. After programming, the application starts automatically and enumerates the configured CDC interfaces on the USB high-speed port

6. Open a separate Tera Term instance for each enumerated VCOM port. Configure the serial port (8N1) as 8 data bits, no parity, and 1 stop bit. Use `3686400` baud for operation at 4M baud, or select one of the supported baud rates given below:

    Supported baud rates:
   - 9600
   - 14400
   - 19200
   - 38400
   - 57600
   - 115200
   - 230400
   - 460800
   - 921600
   - 3686400

7. Data written to each VCOM port is forwarded to its assigned SCB UART interface, and data received on that UART interface is returned to the host through the corresponding VCOM IN endpoint. This assignment is captured in the `vcom_config.c` file, as shown in **Table 3**:

   **Table 3: CDC-function-to-SCB mapping**
   | CDC function | CDC function structure | UART block |
   | :---: | :---: | :---: |
   | CDC Function 0 | `cdcFn0` | SCB0 |
   | CDC Function 1 | `cdcFn1` | SCB4 |
   | CDC Function 2 | `cdcFn2` | SCB6 |
   | CDC Function 3 | `cdcFn3` | SCB1 |


## Debugging


### Using the Arm&reg; debug port

If you have access to a MiniProg or KitProg3 device, you can debug the example to step through the code.


<details><summary><b>In Eclipse IDE</b></summary>

Use the **\<Application Name> Debug (KitProg3_MiniProg4)** configuration in the **Quick Panel**. For details, see the "Program and debug" section in the [Eclipse IDE for ModusToolbox&trade; user guide](https://www.infineon.com/MTBEclipseIDEUserGuide).

</details>


<details><summary><b>In other IDEs</b></summary>

Follow the instructions in your preferred IDE.

</details>


### Log messages

The USBFS port is enabled for debug logs.

The verbosity of the debug log output can be modified by setting the `DEBUG_LEVEL` macro in the *main.c* file using the values shown in **Table 4**.

**Table 4. Debug values**
 Macro value  | Description
 :--------    | :-------------
 1u           | Enable only error messages
 2u           | Enable error and warning messages
 3u           | Enable error, warning, and info messages
 4u           | Enable all message types
<br>


## Design and implementation

### Overview

This application supports up to four USB-to-UART channels using SCB0, SCB1, SCB4, and SCB6. The number of channels can be set by configuring the `VCOM_UART_CHANNELS` macro (see **Table 2**). Active channels can be configured by selecting the appropriate `fnConfig` entries in `appConfig`, defined in *vcom_config.c*.

### Application workflow

The application flow involves three main steps:

- [Initialization](#initialization)
- [USB device enumeration](#usb-device-enumeration)
- [CDC data transfer](#cdc-data-transfer)


#### Initialization

During initialization, the following steps are performed:

1. The application context, USB descriptors, DMA resources, and CDC data structures are initialized

2. The USBD and CAL layers are initialized for the FX2G3 USB high-speed device block

3. The application registers the descriptors and callback functions required for CDC enumeration and control handling

4. The CDC task, timers, and interrupt handlers used for UART and USB data movement are created

5. The USB device is connected to the host and attempts enumeration


#### USB device enumeration

1. During USB device enumeration, the host requests the descriptors that were registered during initialization

2. The host sends `CY_USB_SC_SET_CONFIGURATION` and CDC class-specific commands, such as `CDC_GET_LINE_CODING`, `CDC_SET_LINE_CODING`, and `CDC_SET_CONTROL_LINE_STATE`

3. After `CDC_SET_CONTROL_LINE_STATE` is received, the firmware enables the data paths for the targeted CDC function


#### CDC data transfer

Each CDC function exposes three USB endpoints:

- **Interrupt IN** – CDC class notifications
- **Bulk IN** – device-to-host data
- **Bulk OUT** – host-to-device data


##### USB OUT to UART TX (host to device)

- Incoming USB Bulk OUT packets land in HBDMA-backed endpoint buffers (four 512-byte buffers per CDC function)
- On each received packet, the HBDMA OUT channel completion callback posts a `CY_USB_VCOM_DEVICE_MSG_READ_COMPLETE` message to the CDC task (`Cy_CDC_DeviceTaskHandler`)
- The CDC task configures a DMA transfer to drain the pre-filled endpoint buffer into the SCB UART transmit FIFO:
  - SCB0 and SCB1 use DataWire (DW0)
  - SCB4 and SCB6 use DMAC
  - For payloads > 64 bytes: a 2D descriptor outputs the data in 64-byte bursts, each gated by the SCB TX FIFO threshold trigger. A chained 1D descriptor handles any non-aligned (i.e, not a chunk of 64B) residual bytes
  - For payloads ≤ 64 bytes: a single 1D descriptor completes the transfer in one burst
- On DMA completion, an ISR posts a `CY_USB_UART_MSG_WRITE_COMPLETE` message to the CDC task, which discards the consumed HBDMA buffer to free it for the next USB OUT packet

##### UART RX to USB IN (device to host)

- A 2D DMA descriptor accumulates incoming UART RX data into a 512-byte RAM buffer:
  - Each X-loop reads 64 bytes from the SCB RX FIFO, triggered by the RX FIFO threshold
  - 8 X-loops fill the 512-byte buffer
  - SCB0 and SCB1 use DataWire (DW0)
  - SCB4 and SCB6 use DMAC
- On buffer fill, a DMA completion ISR posts a `CY_USB_UART_MSG_READ_COMPLETE` message to the CDC task (`Cy_CDC_DeviceTaskHandler`)
- The CDC task commits the filled buffer to the HBDMA USB IN channel via `Cy_HBDma_Channel_CommitBuffer()` which is delivered to the host through the CDC Bulk IN endpoint
- A zero-length packet is appended automatically when the committed length equals the 512-byte bulk packet size (full packet) and no further data is immediately ready to be sent, to indicate the end of transfer in USBHS
- Once the host consumes the data from the IN endpoint, an ISR posts `CY_USB_VCOM_DEVICE_MSG_WRITE_COMPLETE` to the CDC task, which queues the next UART read if a buffer is free, sends any pending data through the next write to the IN endpoint, and sends a ZLP if the previous transfer was a full-packet (512-bytes)
- When UART input falls below the 64-byte RX threshold, data stagnates in the FIFO and the SCB RX FIFO trigger is not asserted. A 1-second periodic timer detects this by comparing RX FIFO depths across two consecutive ticks. If stagnant data is found, the DMA descriptor is reconfigured for a partial 1D read at the current Y-index offset in the buffer, and the SCB RX trigger is asserted manually to flush the residual bytes

## Application files

**Table 5. Application file description**

File | Description
:-------- | :------------
*cy_usb_app.c* | Initializes the USB application, registers callbacks, and manages USB endpoint and DMA state
*cy_usb_app.h* | Declares application context structures, macros, and USB callback interfaces
*vcom_config.c* | Defines the USB device settings, string descriptors, endpoint configuration, and CDC function mapping
*vcom_config.h* | Defines configuration data types and constants for the CDC functions and USB device
*cy_dscr.c* | Builds and registers the USB descriptors used by the configured CDC functions
*cy_dscr.h* | Declares descriptor structures and helper macros
*COMPONENT_CDC/cdc.c* | Implements CDC task handling, UART and DMA setup, line-coding processing, and timer callbacks
*COMPONENT_CDC/cdc.h* | Declares CDC data structures, UART and DMA interfaces, and helper functions
*main.c* | Implements device initialization, debug setup, RTOS startup, and top-level interrupt support
*cm0_code.c* | Startup image used when the Cortex&reg; M0+ core needs to release the Cortex&reg; M4 core
*Makefile* | GNU make compliant build script for compiling this example
<br>


## Related resources

Resources  | Links
-----------|----------------------------------
Board support package | [KIT_FX2G3_104LGA BSP API Reference Manual](https://infineon.github.io/TARGET_KIT_FX2G3_104LGA/html/modules.html)
Code examples  | [Using ModusToolbox&trade;](https://github.com/Infineon/Code-Examples-for-ModusToolbox-Software) on GitHub
Device documentation | [EZ-USB&trade; FX2G3 documentation](https://www.infineon.com/cms/en/product/promopages/ez-usb-fx2g3)
Development kits | Select your kits from the [Evaluation board finder](https://www.infineon.com/cms/en/design-support/finder-selection-tools/product-finder/evaluation-board)
Libraries on GitHub | [mtb-pdl-cat1](https://github.com/Infineon/mtb-pdl-cat1) – Peripheral Driver Library (PDL)
Middleware on GitHub  | [usbfxstack](https://github.com/Infineon/usbfxstack) – USBFX Stack middleware library and documents
Tools  | [ModusToolbox&trade;](https://www.infineon.com/modustoolbox) – ModusToolbox&trade; software is a collection of easy-to-use libraries and tools enabling rapid development with Infineon MCUs for applications ranging from wireless and cloud-connected systems, edge AI/ML, embedded sense and control, to wired USB connectivity using PSOC&trade; Industrial/IoT MCUs, AIROC&trade; Wi-Fi and Bluetooth&reg; connectivity devices, XMC&trade; Industrial MCUs, and EZ-USB&trade;/EZ-PD&trade; wired connectivity controllers. ModusToolbox&trade; incorporates a comprehensive set of BSPs, HAL, libraries, configuration tools, and provides support for industry-standard IDEs to fast-track your embedded application development

<br>


## Other resources

Infineon provides a wealth of data at [www.infineon.com](https://www.infineon.com) to help you select the right device, and quickly and effectively integrate it into your design.


## Document history

Document title: *CE242009* – *EZ-USB&trade; FX2G3: USB communications device class application*

 Version | Description of change
 ------- | ---------------------
 1.0.0   | New code example
<br>



All referenced product or service names and trademarks are the property of their respective owners.

The Bluetooth&reg; word mark and logos are registered trademarks owned by Bluetooth SIG, Inc., and any use of such marks by Infineon is under license.

PSOC&trade;, formerly known as PSoC&trade;, is a trademark of Infineon Technologies. Any references to PSoC&trade; in this document or others shall be deemed to refer to PSOC&trade;.

---------------------------------------------------------

(c) 2026, Infineon Technologies AG, or an affiliate of Infineon Technologies AG. All rights reserved.
This software, associated documentation and materials ("Software") is owned by Infineon Technologies AG or one of its affiliates ("Infineon") and is protected by and subject to worldwide patent protection, worldwide copyright laws, and international treaty provisions. Therefore, you may use this Software only as provided in the license agreement accompanying the software package from which you obtained this Software. If no license agreement applies, then any use, reproduction, modification, translation, or compilation of this Software is prohibited without the express written permission of Infineon.
<br>
Disclaimer: UNLESS OTHERWISE EXPRESSLY AGREED WITH INFINEON, THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, ALL WARRANTIES OF NON-INFRINGEMENT OF THIRD-PARTY RIGHTS AND IMPLIED WARRANTIES SUCH AS WARRANTIES OF FITNESS FOR A SPECIFIC USE/PURPOSE OR MERCHANTABILITY. Infineon reserves the right to make changes to the Software without notice. You are responsible for properly designing, programming, and testing the functionality and safety of your intended application of the Software, as well as complying with any legal requirements related to its use. Infineon does not guarantee that the Software will be free from intrusion, data theft or loss, or other breaches (“Security Breaches”), and Infineon shall have no liability arising out of any Security Breaches. Unless otherwise explicitly approved by Infineon, the Software may not be used in any application where a failure of the Product or any consequences of the use thereof can reasonably be expected to result in personal injury.
