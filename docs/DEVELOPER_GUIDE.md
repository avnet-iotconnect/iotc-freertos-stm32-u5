# FreeRTOS STM32U5 IoT Reference for /IOTCONNECT

## Introduction
This guide walks through cloning, building, and flashing the STM32U5 /IOTCONNECT example from source.

If you want to use a pre-built firmware image instead of building from source, use the [Quickstart Guide](QUICKSTART.md).

## Clone the Repository and Checkout `main-iotc`
_When using Windows, long paths can be a problem. The commands below clone into a short `u5` directory._

Clone with HTTPS:
```bash
git clone https://github.com/avnet-iotconnect/iotc-freertos-stm32-u5 -b main-iotc u5
cd u5
```

Clone with SSH:
```bash
git clone git@github.com:avnet-iotconnect/iotc-freertos-stm32-u5.git -b main-iotc u5
cd u5
```

Initialize repository submodules (run from the repository root):
```bash
git submodule update --init
```

Run the /IOTCONNECT RTOS pull script:
```bash
./IoTConnect/iotc-freertos-sdk/rtosPull.sh
```

When prompted by `rtosPull.sh`, select:
- `yes` for `FreeRTOS-Plus`
- `yes` for `coreHTTP`
- `no` for all other modules

## Install STM32CubeIDE
Download and install STM32CubeIDE from the [STMicroelectronics website](https://www.st.com/en/development-tools/stm32cubeide.html).

## Import Project into STM32CubeIDE
1. Open STM32CubeIDE.
1. If prompted for workspace, select the repository root directory (`CODE-BASE-DIRECTORY`).
1. If workspace is not prompted at startup, use `File -> Switch Workspace -> Other`.
1. Click **Launch**.
1. Close the **Information Center** tab if needed.
1. Select `File -> Import`.
1. In the import wizard, select `General -> Existing Projects Into Workspace` and click **Next**.
1. Click **Browse** next to `Select root directory` and choose the repository root (`CODE-BASE-DIRECTORY`).
1. Select project `b_u585i_iot02a_ntz`.
1. Ensure `Copy projects into workspace` is not selected.
1. Click **Finish**.

## Build
In the **Project Explorer** pane, right click `b_u585i_iot02a_ntz` and select **Build Project**.

When the build succeeds, the console includes:
```text
Finished building: b_u585i_iot02a_ntz.bin
Finished building: b_u585i_iot02a_ntz.hex
Finished building: b_u585i_iot02a_ntz.list
```

Build artifacts are generated in:
`Projects/b_u585i_iot02a_ntz/Debug/`

## Known Build Warnings
This project links with minimal syscalls (`--specs=nosys.specs`). As a result, linker warnings about `_read`, `_write`, `_open`, `_close`, `_fstat`, `_isatty`, `_lseek`, `_getpid`, and `_kill` may appear.

This project may also emit:
- `warning: b_u585i_iot02a_ntz.elf has a LOAD segment with RWX permissions`
- linker `note:` lines such as `the message above does not take linker garbage collection into account`

What we observed:
- STM32CubeIDE can show a summary such as `Build Failed. 9 errors, 10 warnings` even when linking completed and output artifacts were generated.
- In this case, treat `Finished building target: b_u585i_iot02a_ntz.elf` plus generated `.bin`, `.hex`, and `.list` files as the success signal.

Optional cleanup (if you want a quieter build):
1. Keep default behavior and ignore these messages (recommended for quick bring-up).
1. Add syscall retarget implementations (`_write`, `_read`, `_open`, `_close`, `_fstat`, `_isatty`, `_lseek`, `_getpid`, `_kill`) in your application layer if you need proper libc I/O behavior.
1. Suppress the RWX linker warning by adding linker option `-Wl,--no-warn-rwx-segments` in STM32CubeIDE (`Project Properties -> C/C++ Build -> Settings -> MCU GCC Linker -> Miscellaneous`).

## Flash the Firmware onto the Device
To flash the `b_u585i_iot02a_ntz` project to STM32U5 IoT Discovery Kit:

1. Choose `Run -> Run Configurations`.
1. Choose `C/C++ Application`.
1. Select configuration `Flash_ntz`.
1. Click **Run**.

## Configure Device and Connect to /IOTCONNECT
Follow the [Quickstart Guide](QUICKSTART.md) for setting up your /IOTCONNECT account and device settings over the serial port.

## Modify Telemetry Update Rate
By default, sensor telemetry is sent every 3 seconds.

To change motion sensor telemetry interval, edit `MQTT_PUBLISH_PERIOD_MS` in:
`Common/app/motion_sensors_publish.c`

To change environmental sensor telemetry interval, edit `MQTT_PUBLISH_TIME_BETWEEN_MS` in:
`Common/app/env_sensor_publish.c`
