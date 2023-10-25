# FreeRTOS STM32U5 IoT Reference for IoTConnect
## Introduction
This project demonstrates how to integrate modular [ FreeRTOS kernel ](https://www.freertos.org/RTOS.html) and [ libraries ](https://www.freertos.org/libraries/categories.html) with hardware enforced security to build more secure updatable cloud connected applications. The project is pre-configured to run on the [ STM32U585 IoT Discovery Kit ](https://www.st.com/en/evaluation-tools/b-u585i-iot02a.html) which includes an kit which includes an [ STM32U5 ](https://www.st.com/en/microcontrollers-microprocessors/stm32u5-series.html) microcontroller.

The *Projects* directory consists of a [Non-TrustZone](Projects/b_u585i_iot02a_ntz) and a [Trusted-Firmware-M-Enabled](Projects/b_u585i_iot02a_tfm) project which both demonstrate connecting to AWS IoT Core and utilizing many of the services available via the MQTT protocol.

The demo projects both connect to AWS IoT core via the included Wi-Fi module and use the [CoreMQTT-Agent](https://github.com/FreeRTOS/coreMQTT-Agent) library to share a single MQTT connection among multiple tasks. These tasks publish environemnt and motion sensor data from a subset of the sensor available on the development board, and demonstrate use of the AWS IoT Device Shadow and Device Defender services.
For more details on the feature, see the [ ST Featured IoT Reference Integration ](https://www.freertos.org/STM32U5/) page on FreeRTOS.org.

## AWS IoT Core Demo Tasks
* MQTT Agent
* IoT Defender
* OTA Update
* Environment Sensor Publishing
* Motion Sensor Publishing

## Key Software Components
### LWIP TCP/IP Stack
See [ lwIP ](https://github.com/lwip-tcpip/lwip) for details.

### Mbedtls 3.1.0 TLS and Cryptography library
See [ MbedTLS ](https://github.com/Mbed-TLS/mbedtls/tree/d65aeb37349ad1a50e0f6c9b694d4b5290d60e49) for details.

### Command Line Interface (CLI)
The CLI interface located in the Common/cli directory is used to provision the device. It also provides other Unix-like utilities. See [Common/cli](Common/cli/ReadMe.md) for details.

### Key-Value Store
The key-value store located in the Common/kvstore directory is used to store runtime configuration values in non-volatile flash memory.
See [Common/kvstore](Common/kvstore/ReadMe.md) for details.

### PkiObject API
The PkiObject API takes care of some of the mundane tasks in converting between different representations of cryptographic objects such as public keys, private keys, and certificates. See [Common/crypto](Common/crypto/ReadMe.md) for details.

### Mbedtls Transport
The *Common/net/mbedtls_transport.c* file contains a transport layer implementation for coreMQTT and coreHTTP which uses mbedtls to encrypt the connection in a way supported by AWS IoT Core.

Optionally, client key / certificate authentication may be used with the mbedtls transport or this parameter may be set to NULL if not needed.
### Cloning the Repository
To clone using HTTPS:
```
git clone https://github.com/avnet-iotconnect/iotc-freertos-stm32-u5 --recurse-submodules
```
Using SSH:
```
git clone git@github.com:avnet-iotconnect/iotc-freertos-stm32-u5.git --recurse-submodules
```
If you have downloaded the repo without using the `--recurse-submodules` argument, you should run:
```
git submodule update --init --recursive
```
## Running the demos
To get started running demos, see the steps below. 
### Step 1: Install STM32CubeIDE
Download the latest version of STM32CubeIDE from the [STMicroelectronics website](https://www.st.com/en/development-tools/stm32cubeide.html).

Note that the projects in this repository have been verified with versions 1.8.0 and 1.9.0 of STM32CubeIDE.

### Step 2: Import Projects into STM32CubeIDE
1. Open STM32CubeIDE.
> NOTE -  when asked to open a workspace directory, you **must** select the location in which you cloned this git repository (CODE-BASE-DIRECTORY) as the workspace directory.
>
> If you are not asked to select a workspace when STM32CubeIDE starts, you may access this dialog via the ***File -> Switch Workspace -> Other*** menu item.

1. Click **Launch**
2. Close the **Information Center** tab if needed

3. Select ***File -> Import***.
4. In the Import dialog box, under ***Select an Import Wizard***, select ***General -> Existing Projects Into Workspace*** and click *** Next >***
5. Click **Browse** next to the *Select root directory* box and navigate to the root of this repository <CODE-BASE-DIRECTORY>.
6. Click the check box next to both the *b_u585i_iot02a_ntz* and *b_u585i_iot02a_tfm* projects
> Ensure that *copy projects into workspace* is not selected
7. Click **Finish** to import the projects.

### Step 3: Build Firmware image and Flash your development board
After importing the two demo projects into STM32CubeIDE, decide which one you will build and deploy first and follow the instructions below to do so.

#### Building
In the **Project Explorer** pane of STM32CubeIDE, Right click on the project and select **Build Project**

>Notes:

You must set the **workspace** directory as mentioned in **Step 2** to the directory of this git otherwise this project will not build.

When building there may still be errors reported in the **"Problems"** tab or the **"Console"** tab.
Errors such as  "_fstat is not implemented and will always fail" are warnings and can be ignored.

If the build has succeeded you should see lines similar to the following:

```
arm-none-eabi-size   b_u585i_iot02a_ntz.elf 
arm-none-eabi-objdump -h -S  b_u585i_iot02a_ntz.elf  > "b_u585i_iot02a_ntz.list"
   text	   data	    bss	    dec	    hex	filename
 527780	   2140	 514712	1044632	  ff098	b_u585i_iot02a_ntz.elf
arm-none-eabi-objcopy  -O ihex  b_u585i_iot02a_ntz.elf  "b_u585i_iot02a_ntz.hex"
arm-none-eabi-objcopy  -O binary  b_u585i_iot02a_ntz.elf  "b_u585i_iot02a_ntz.bin"
Finished building: default.size.stdout
 
Finished building: b_u585i_iot02a_ntz.bin 
Finished building: b_u585i_iot02a_ntz.hex
Finished building: b_u585i_iot02a_ntz.list

13:01:31 Build Failed. 9 errors, 1145 warnings. (took 8s.144ms)
```


#### Non-TrustZone Project (NTZ)
Review the README.md file for the [Non TrustZone](Projects/b_u585i_iot02a_ntz) project for more information on the setup and limitations of this demo project.

To flash the b_u585i_iot02a_ntz project to your STM32U5 IoT Discovery Kit:

1. Choose Run -> Run Configurations
1. Choose C/C++ Application
1. Select the Flash_ntz configuration
1. Click on the Run button

> Note: There might be some errors when building, as long as it generates the output firmware files.

### Step 4: Configure Your Board
Open the target board's serial port with your favorite serial terminal. Some common options are terraterm, putty, screen, minicom, and picocom.

#### Thing Name

First, configure the desired thing name / mqtt device identifier:
```
> conf set thing_name my_thing_name
thing_name="my_thing_name"
```
#### WiFi SSID and Passphrase
Next, configure you WiFi network details:
```
> conf set wifi_ssid ssidGoesHere
wifi_ssid="ssidGoesHere"
> conf set wifi_credential MyWifiPassword
wifi_credential="MyWifiPassword"
```

#### Commit Configuration Changes
Commit the staged configuration changes to non-volatile memory.
```
> conf commit
Configuration saved to NVM.
```

#### Import the Amazon Root CA Certificate
Use the *pki import cert root_ca_cert* command to import the Root CA Certificate.
For this demo, we recommend you use the ["Starfield Services Root Certificate Authority - G2](https://www.amazontrust.com/repository/SFSRootCAG2.pem) Root CA Certificate which has signed all four available Amazon Trust Services Root CA certificates.

Copy/Paste the contents of [SFSRootCAG2.pem](https://www.amazontrust.com/repository/SFSRootCAG2.pem) into your serial terminal after issuing the ```pki import cert``` command.
```
> pki import cert root_ca_cert
-----BEGIN CERTIFICATE-----
XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
-----END CERTIFICATE-----
```

#### Generate a private key
Use the *pki generate key* command to generate a new ECDSA device key pair. The resulting public key will be printed to the console.
```
> pki generate key
SUCCESS: Key pair generated and stored in
Private Key Label: tls_key_priv
Public Key Label: tls_key_pub
-----BEGIN PUBLIC KEY-----
XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX=
-----END PUBLIC KEY-----
```
#### Generate a self-signed certificate
Next, use the *pki generate cert* command to generate a new self-signed certificate:
```
> pki generate cert
-----BEGIN CERTIFICATE-----
XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX==
-----END CERTIFICATE-----
```

Save the resulting certificate to a new file, name it devicecert.pem.


### Step 5: Register the device with IoTConnect-AWS
1. Upload the certificate that you saved from the terminal, devicecert.pem at https://awspoc.iotconnect.io/certificate (CA Certificate Individual)
2. Create a template using **CA certificate Individual** as "Auth Type".
3. Create a device and select the certicate that you uploaded in "Certificate Authority" and upload the same certificate again in "Device Certificate".

> Note: We will change the device registration method in future.

In the template add attributes for the following, set their types as integers:

```
accelerometer_x
accelerometer_y
accelerometer_z
gyro_x
gyro_y
gyro_z
```

#### MQTT Endpoint
Next, set the mqtt endpoint to the endpoint for your account:

This can be found by going to the newly created device's page in IoT-Connect and clicking the "connection info" link. 
The mqtt endpoint is listed as **# host**.

```
> conf set mqtt_endpoint xxxxxxxxxxxxxx-ats.iot.us-west-2.amazonaws.com
mqtt_endpoint="xxxxxxxxxxxxxx-ats.iot.us-west-2.amazonaws.com"
```

#### Telemetry CD
Next, set the telemetry_cd for this device, this is a unique 7 or 8 digit alphanumeric code for each device:

```
conf set telemetry_cd XG4EOMA
telemetry_cd="XG4EOMA"
```

This can be found by going your newly created device's page in IoT-Connect and clicking the "connection info" link
The **Telemetry CD** is found within the mqtt **#pubTopics** and is the value shown as **XG4EOMA** in the
example below:

```
 $aws/rules/msg_d2c_rpt/your_device_name/XG4EOMA/2.1/0 
```

#### Commit Updated Configuration Changes
Finally, commit the staged configuration changes to non-volatile memory.
```
> conf commit
Configuration saved to NVM.
```

#### Reset the target device

Reset the device and it shall automatically connect to the WiFi router and AWS MQTT broker based on the configuration set earlier. 
```
> reset
Resetting device.
```

When connected the following lines should appear on the CLI

```
<INF>     9574 [MQTTAgent ] Network connection 0x20025538: TLS handshake successful. (mbedtls_transport.c:1367)
<INF>     9574 [MQTTAgent ] Network connection 0x20025538: Connection to xxxxxxxx-ats.iot.us-east-1.amazonaws.com:8883 established. (mbedtls_transport.c:1374)
<INF>     9864 [MQTTAgent ] Starting a clean MQTT Session. (mqtt_agent_task.c:1169)
<INF>    10732 [lwIP      ] Time set to: 2023-10-09T11:56:59.000Z! (time.c:68)
<INF>    10839 [sntp      ] Time received from NTP. Time now: 2023-10-09T11:56:59.000Z! (time.c:100)
```

Observe telemetry data is received on the IoT-Connect website in the Device's **Live Data** section.

#### Reset settings to defaults

The settings can be erased with the following command:

```
> erase
Erasing QSPI NVM, will reset afterwards.
```

#### Modifying telemetry update rate.

By default the sensor telemetry is sent every 3 seconds.  This can be altered by changing the
constant MQTT\_PUBLISH\_PERIOD\_MS in ./Common/app/motion\_sensors\_publish.c.


## Contribution
See [CONTRIBUTING](https://github.com/FreeRTOS/iot-reference-stm32u5/blob/main/CONTRIBUTING.md) for more information.

## License
Source code located in the *Projects*, *Common*, *Middleware/AWS*, and *Middleware/FreeRTOS* directories are available under the terms of the MIT License. See the LICENSE file for more details.

Other libraries located in the *Drivers* and *Middleware* directories are available under the terms specified in each source file.

