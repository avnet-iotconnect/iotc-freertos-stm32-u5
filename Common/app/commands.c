/*
 * commands.c
 *
 *  Created on: May 16, 2024
 *      Author: mgilhespie
 */


// BSP-Specific
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "stm32u5xx.h"
#include "b_u585i_iot02a.h"

#include "iotconnect.h"
#include "iotcl.h"
#include "iotcl_c2d.h"

#include "iotconnect_app.h"

/*
 *
 */
int iotc_process_cmd_str(IotclC2dEventData data, char* command)
{
	int status = 0;

    LogInfo("Received command: %s", command);

    if(NULL != strcasestr(command, IOTC_CMD_PING)){
        LogInfo("Ping Command Received!\n");
    } else if(NULL != strcasestr(command, IOTC_CMD_LED_RED)){
        if (NULL != strcasestr(command, "on")) {
            LogInfo("led-red on");
            set_led_red(true);
        } else if (NULL != strcasestr(command, "off")) {
            LogInfo("led-red off");
            set_led_red(false);
        } else {
            LogWarn("Invalid led-red command received: %s", command);
            status = -1;
        }
    } else if(NULL != strcasestr(command, IOTC_CMD_LED_GREEN)) {
        if (NULL != strcasestr(command, "on")) {
            LogInfo("led-green on");
            set_led_green(true);
        } else if (NULL != strcasestr(command, "off")) {
            LogInfo("led-green off");
            set_led_green(false);
        } else {
            LogWarn("Invalid led-green command received: %s", command);
            status = -1;
        }
    } else {
        LogInfo("Command not recognized: %s", command);
        status = -1;
    }
    return 0;
}


/*
 *
 */
void set_led_red(bool on)
{
	if (on) {
		BSP_LED_On(LED_RED);
	} else {
		BSP_LED_Off(LED_RED);
	}
}


/*
 *
 */
void set_led_green(bool on)
{
	if (on) {
		BSP_LED_On(LED_GREEN);
	} else {
		BSP_LED_Off(LED_GREEN);
	}
}


