# FreeRTOS STM32U5 IoT Reference for IoTConnect on AWS

## Introduction
This document guides you through building the sources into a firmware image for the STM32 U5 IoT-Conneect example for AWS.

The [Quickstart Guide](QUICKSTART.md) shows how to set up the cloud account and device settings with a pre-built firmware
image.


## Clone the Repository & Checkout the "main-iotc" branch.
_When using Windows long paths can be a problem so included in the command below is the option to create the `u5` directory as the checkout destination._

To clone using HTTPS:
```
git clone https://github.com/avnet-iotconnect/iotc-freertos-stm32-u5 -b main-iotc u5
```
Using SSH:

```
git clone git@github.com:avnet-iotconnect/iotc-freertos-stm32-u5.git -b main-iotc u5
```


Download third-party submodules with the following

```
git submodule update --init
```

Run the iotc-freertos-sdk rtosPull.sh script

```
./IoTConnect/iotc-freertos-sdk/rtosPull.sh
```

The rtosPull.sh script will ask a series of questions on which additional third-party modules to include,
select "yes" for the following list of modules and "no" for the other modules
  
  * coreHTTP
  * FreeRTOSPlus


## Install STM32CubeIDE

Download the latest version of STM32CubeIDE from the [STMicroelectronics website](https://www.st.com/en/development-tools/stm32cubeide.html).


## Import Project into STM32CubeIDE

1. Open STM32CubeIDE.

> NOTE -  when asked to open a workspace directory, you **must** select the location in which you cloned this git repository (CODE-BASE-DIRECTORY) as the workspace directory.

> If you are not asked to select a workspace when STM32CubeIDE starts, you may access this dialog via the ***File -> Switch Workspace -> Other*** menu item.


1. Click **Launch**
2. Close the **Information Center** tab if needed

3. Select ***File -> Import***.
4. In the Import dialog box, under ***Select an Import Wizard***, select ***General -> Existing Projects Into Workspace*** and click *** Next >***
5. Click **Browse** next to the *Select root directory* box and navigate to the root of this repository <CODE-BASE-DIRECTORY>.
6. Click the check box next to the *b_u585i_iot02a_ntz* project.
> Ensure that *copy projects into workspace* is not selected.
7. Click **Finish** to import the project.


#### Building
In the **Project Explorer** pane of STM32CubeIDE, Right click on the project and select **Build Project**


When the build has succeeded you should see the following lines in the console:

``` 
Finished building: b_u585i_iot02a_ntz.bin 
Finished building: b_u585i_iot02a_ntz.hex
Finished building: b_u585i_iot02a_ntz.list
```


#### Flashing the Firmware onto the Device

To flash the b_u585i_iot02a_ntz project to your STM32U5 IoT Discovery Kit:

1. Choose Run -> Run Configurations
1. Choose C/C++ Application
1. Select the Flash_ntz configuration
1. Click on the Run button


#### Configuring device and connecting to IOT-Connect

Follow the (Quickstart Guide)[QUICKSTART.md] for instructions on setting up the IpT-Connect account
and device settings using the serial port.


### Modifying telemetry update rate.

By default the sensor telemetry is sent every 3 seconds.  This can be altered by changing the
constant MQTT\_PUBLISH\_PERIOD\_MS in Src/app/iotconnect_app.c.




