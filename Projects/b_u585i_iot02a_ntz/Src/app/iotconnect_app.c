//
// Copyright: Avnet 2023
// Created by Marven Gilhespie
//

#include "logging_levels.h"
/* define LOG_LEVEL here if you want to modify the logging level from the default */
#define LOG_LEVEL    LOG_INFO
#include "logging.h"

/* Standard includes. */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>

/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "kvstore.h"
#include "mbedtls_transport.h"

#include "sys_evt.h"

/* MQTT library includes. */
#include "core_mqtt.h"

/* MQTT agent include. */
#include "core_mqtt_agent.h"
#include "mqtt_agent_task.h"

/* Subscription manager header include. */
#include "subscription_manager.h"

/* Sensor includes */
#include "b_u585i_iot02a_motion_sensors.h"

//Iotconnect
#include "iotconnect.h"
#include "iotconnect_lib.h"
#include "iotconnect_telemetry.h"
#include "iotconnect_event.h"
#include <config/iotconnect_config.h>

// BSP-Specific
#include "stm32u5xx.h"
#include "b_u585i_iot02a.h"

// Constants
#define APP_VERSION 			"01.00.06"		// Version string in telemetry data
#define MQTT_PUBLISH_PERIOD_MS 	( 2000 )		// Size of statically allocated buffers for holding topic names and payloads.
#define LED_BLINK_PERIOD_MS 500  // Blink period in milliseconds


// Temperature Simulation Parameters
static bool simulateTemperature = false;
static TickType_t simStartTime;
static float presetTempF = 0.0f;  // Preset temperature
static bool targetTemperatureReached = false;
static bool grillState = false; // False means grill is off
static int evenSurface = 1; // true means grill is on an even surface

// @brief	IOTConnect configuration defined by application
static IotConnectAwsrtosConfig awsrtos_config;

// Prototypes

static BaseType_t init_sensors( void );
static char* create_telemetry_json(IotclMessageHandle msg, BSP_MOTION_SENSOR_Axes_t accel_data, BSP_MOTION_SENSOR_Axes_t gyro_data, BSP_MOTION_SENSOR_Axes_t mag_data, float simulatedTempF);
static void on_command(IotclEventData data);
static void command_status(IotclEventData data, bool status, const char *command_name, const char *message);
static float simulateTemperatureRise(void);



static float currentTempF = 65.0f; // Current temperature, initialize with start temperature

static float simulateTemperatureRise() {
    if (!simulateTemperature) {
        return currentTempF; // Return the current temperature
    }

    TickType_t currentTime = xTaskGetTickCount();
    float elapsedSec = (float)(currentTime - simStartTime) / pdMS_TO_TICKS(1000);
    float rateOfChange = 5.0f; // Rate of temperature change per second
    float tempDifference = presetTempF - currentTempF;
    float tempChange = rateOfChange * elapsedSec;

    if (fabs(tempDifference) < fabs(tempChange)) {
        currentTempF = presetTempF;
        targetTemperatureReached = true; // Target temperature reached
    } else {
        targetTemperatureReached = false; // Still transitioning
        if (tempDifference > 0) {
            currentTempF += tempChange;
        } else {
            currentTempF -= tempChange;
        }
    }

    return currentTempF;
}
/* @brief	Main IoT-Connect application task
 *
 * @param	pvParameters, argument passed by xTaskCreate
 *
 * This is started by the initialization code in app_main.c which first performs board and
 * networking initialization
 */

void iotconnect_app( void * pvParameters )
{
    BaseType_t result = pdFALSE;
    static TickType_t lastBlinkTime = 0;
    static bool ledState = false;
    result = init_sensors();

    if(result != pdTRUE) {
        LogError( "Error while initializing motion sensors." );
        vTaskDelete( NULL );
    }

    char *device_id = KVStore_getStringHeap(CS_CORE_THING_NAME, NULL);   // Device ID
    char *cpid = KVStore_getStringHeap(CS_IOTC_CPID, NULL);
    char *iotc_env = KVStore_getStringHeap(CS_IOTC_ENV, NULL);

    if (device_id == NULL || cpid == NULL || iotc_env == NULL) {
        LogError("IOTC configuration, thing_name, cpid or env are not set\n");
        vTaskDelete(NULL);
    }

    IotConnectClientConfig *config = iotconnect_sdk_init_and_get_config();
    config->cpid = cpid;
    config->env = iotc_env;
    config->duid = device_id;
    config->cmd_cb = on_command;
    config->ota_cb = NULL;
    config->status_cb = NULL;
    config->auth_info.type = IOTC_X509;
    config->auth_info.https_root_ca = xPkiObjectFromLabel( TLS_HTTPS_ROOT_CA_CERT_LABEL );
    config->auth_info.mqtt_root_ca = xPkiObjectFromLabel( TLS_MQTT_ROOT_CA_CERT_LABEL );
    config->auth_info.data.cert_info.device_cert = xPkiObjectFromLabel( TLS_CERT_LABEL );
    config->auth_info.data.cert_info.device_key = xPkiObjectFromLabel( TLS_KEY_PRV_LABEL );

#if defined(IOTCONFIG_USE_DISCOVERY_SYNC)
    iotconnect_sdk_init(NULL);
#else
    char *mqtt_endpoint_url = KVStore_getStringHeap(CS_CORE_MQTT_ENDPOINT, NULL);
    char *telemetry_cd = KVStore_getStringHeap(CS_IOTC_TELEMETRY_CD, NULL);

    if (mqtt_endpoint_url == NULL || telemetry_cd == NULL) {
        LogError ("IOTC configuration, mqtt_endpoint, telemetry_cd not set");
        vTaskDelete(NULL);
    }

    awsrtos_config.host = mqtt_endpoint_url;
    awsrtos_config.telemetry_cd = telemetry_cd;
    iotconnect_sdk_init(&awsrtos_config);
#endif

    while (1) {
        int32_t sensor_error = BSP_ERROR_NONE;
        BSP_MOTION_SENSOR_Axes_t xAcceleroAxes, xGyroAxes, xMagnetoAxes;

        sensor_error = BSP_MOTION_SENSOR_GetAxes(0, MOTION_GYRO, &xGyroAxes);
        sensor_error |= BSP_MOTION_SENSOR_GetAxes(0, MOTION_ACCELERO, &xAcceleroAxes);
        sensor_error |= BSP_MOTION_SENSOR_GetAxes(1, MOTION_MAGNETO, &xMagnetoAxes);

        if (xAcceleroAxes.x > 100 || xAcceleroAxes.x < -100) {
            grillState = false;  // Turn off the grill
            presetTempF = 65.0f; // Reset temperature
            evenSurface = 0; // Set even-surface telemetry to false
            BSP_LED_Off(LED_GREEN);
            // Log and handle this event
            LogInfo("Unsafe accelerometer reading detected. Grill turned off for safety.");
        } else {
            evenSurface = 1; // Set even-surface to true after consecutive safe readings
             }

        if (sensor_error == BSP_ERROR_NONE) {
            float simulatedTempF = simulateTemperatureRise();  // Get simulated temperature

            IotclMessageHandle message = iotcl_telemetry_create();
            char* json_message = create_telemetry_json(message, xAcceleroAxes, xGyroAxes, xMagnetoAxes, simulatedTempF);

            if (json_message == NULL) {
                LogError("Could not create telemetry data\n");
                vTaskDelete(NULL);
            }

            iotconnect_sdk_send_packet(json_message);  // Send telemetry data
            iotcl_destroy_serialized(json_message);
        }

        TickType_t currentTime = xTaskGetTickCount();
        if (currentTempF == 65.0f) {
                    BSP_LED_Off(LED_RED); // Turn off the red LED
        } else if (!targetTemperatureReached) {
            if (currentTime - lastBlinkTime >= pdMS_TO_TICKS(LED_BLINK_PERIOD_MS)) {
                lastBlinkTime = currentTime; // Correctly update the last blink time
                ledState = !ledState;        // Toggle the LED state
                if (ledState) {
                    BSP_LED_On(LED_RED);
                } else {
                    BSP_LED_Off(LED_RED);
                }
            }
        } else {
            BSP_LED_On(LED_RED); // Keep the LED on when the target temperature is reached
        }

        vTaskDelay(pdMS_TO_TICKS(MQTT_PUBLISH_PERIOD_MS));
    }
}



/* @brief	Initialize the dev board's accelerometer, gyro and magnetometer sensors
 */
static BaseType_t init_sensors( void )
{
    int32_t lBspError = BSP_ERROR_NONE;

    /* Gyro + Accelerometer*/
    lBspError = BSP_MOTION_SENSOR_Init( 0, MOTION_GYRO | MOTION_ACCELERO );
    lBspError |= BSP_MOTION_SENSOR_Enable( 0, MOTION_GYRO );
    lBspError |= BSP_MOTION_SENSOR_Enable( 0, MOTION_ACCELERO );
    lBspError |= BSP_MOTION_SENSOR_SetOutputDataRate( 0, MOTION_GYRO, 1.0f );
    lBspError |= BSP_MOTION_SENSOR_SetOutputDataRate( 0, MOTION_ACCELERO, 1.0f );

    /* Magnetometer */
    lBspError |= BSP_MOTION_SENSOR_Init( 1, MOTION_MAGNETO );
    lBspError |= BSP_MOTION_SENSOR_Enable( 1, MOTION_MAGNETO );
    lBspError |= BSP_MOTION_SENSOR_SetOutputDataRate( 1, MOTION_MAGNETO, 1.0f );

    BSP_LED_Off(LED_GREEN); // Ensure the grill switch is off at startup

    return( lBspError == BSP_ERROR_NONE ? pdTRUE : pdFALSE );
}

/* @brief 	Create JSON message containing telemetry data to publish
 *
 */
static char *create_telemetry_json(IotclMessageHandle msg, BSP_MOTION_SENSOR_Axes_t accel_data,
								BSP_MOTION_SENSOR_Axes_t gyro_data, BSP_MOTION_SENSOR_Axes_t mag_data, float simulatedTempF) {

    // Optional. The first time you create a data point, the current timestamp will be automatically added
    // TelemetryAddWith* calls are only required if sending multiple data points in one packet.
    iotcl_telemetry_add_with_iso_time(msg, NULL);

    iotcl_telemetry_set_number(msg, "gyro_x", gyro_data.x);
    iotcl_telemetry_set_number(msg, "gyro_y", gyro_data.y);
    iotcl_telemetry_set_number(msg, "gyro_z", gyro_data.z);

    iotcl_telemetry_set_number(msg, "accelerometer_x", accel_data.x);
    iotcl_telemetry_set_number(msg, "accelerometer_y", accel_data.y);
    iotcl_telemetry_set_number(msg, "accelerometer_z", accel_data.z);

#if 0
    iotcl_telemetry_set_number(msg, "magnetometer_x", mag_data.x);
    iotcl_telemetry_set_number(msg, "magnetometer_y", mag_data.y);
    iotcl_telemetry_set_number(msg, "magnetometer_z", mag_data.z);
#endif

    iotcl_telemetry_set_string(msg, "version", APP_VERSION);

    // Add the simulated temperature to the telemetry data
    iotcl_telemetry_set_number(msg, "simulated_temp", simulatedTempF);
    iotcl_telemetry_set_number(msg, "set_temp", presetTempF);
    iotcl_telemetry_set_bool(msg, "grill_state", grillState);
    iotcl_telemetry_set_number(msg, "even_surface", evenSurface);

    const char* str = iotcl_create_serialized_string(msg, false);

	if (str == NULL) {
		LogInfo( "serialized_string is NULL");
	}

	iotcl_telemetry_destroy(msg);
    return (char* )str;
}


/* @brief	Callback when a a cloud-to-device command is received on the subscribed MQTT topic
 */
static void on_command(IotclEventData data) {
	if (data == NULL ) {
		LogWarn("on_command called with data = NULL");
		return;
	}

	char *command = iotcl_clone_command(data);

    if (NULL != command) {
    	LogInfo("Received command: %s", command);

    	if(NULL != strstr(command, "led-red") ) {
			if (NULL != strstr(command, "on")) {
				LogInfo("led-red on");
				BSP_LED_On(LED_RED);
			} else {
				LogInfo("led-red off");
				BSP_LED_Off(LED_RED);
			}
			command_status(data, true, command, "OK");
		} else if(NULL != strstr(command, "led-green") ) {
			if (NULL != strstr(command, "on")) {
				BSP_LED_On(LED_GREEN);
			} else {
				BSP_LED_Off(LED_GREEN);
			}
			command_status(data, true, command, "OK");
		} else if (NULL != strstr(command, "set-temp")) {
			 if (grillState) {  // Check if grill is on
			                float tempVal;
			                if (sscanf(command, "set-temp %f", &tempVal) == 1) {
			                    presetTempF = tempVal;
			                    simulateTemperature = true;  // Start simulation
			                    simStartTime = xTaskGetTickCount();
			                    LogInfo("Temperature set to %.2f", presetTempF);
			                    command_status(data, true, command, "Temperature set");
			                } else {
			                    command_status(data, false, command, "Invalid temperature value");
			                }
			            } else {
			                // Handle the case when grill is off
			                LogInfo("Grill is off. Cannot set temperature.");
			                command_status(data, false, command, "Grill is off. Cannot set temperature.");
			            }
		} else if (strstr(command, "grill_switch")) {
		            if (strstr(command, "on")) {
		                grillState = true;
		                BSP_LED_On(LED_GREEN);  // Turn on green LED
		                command_status(data, true, "grill_switch", "Grill is ON");
		            } else if (strstr(command, "off")) {
		                grillState = false;
		                BSP_LED_Off(LED_GREEN);  // Turn off green LED
		                command_status(data, true, "grill_switch", "Grill is OFF");
		            } else {
		                command_status(data, false, "grill_switch", "Invalid command value");
		            }
		} else {
			LogInfo("command not recognized");
			command_status(data, false, command, "Not implemented");
		}
        free((void*) command);
    } else {
		LogInfo("No command, internal error");
        command_status(data, false, "?", "Internal error");
    }
}


/* @brief	Generate a command acknowledgement message and publish it on the events topic
 *
 */
static void command_status(IotclEventData data, bool status, const char *command_name, const char *message) {
    const char *ack = iotcl_create_ack_string_and_destroy_event(data, status, message);				// defined in iotc-c-lib iotconnect_evvent.c

    if (ack == NULL) {
    	LogInfo("command: no ack required");
    	return;
    }

	LogInfo("command: %s status=%s: %s\r\n", command_name, status ? "OK" : "Failed", message);
	LogInfo("Sent CMD ack: %s\r\n", ack);
	vTaskDelay(100);
	iotconnect_sdk_send_packet(ack);
	free((void*) ack);
}

/*static float simulateTemperatureRise() {
    if (!simulateTemperature) {
        return startTempF;
    }

    TickType_t currentTime = xTaskGetTickCount();
    float elapsedSec = (float)(currentTime - simStartTime) / pdMS_TO_TICKS(1000);

    // Check if the elapsed time is less than the simulation duration
    if (elapsedSec < simDurationSec) {
        // Simulate the temperature rise
        float tempRise = (presetTempF - startTempF) * (elapsedSec / simDurationSec);
        return startTempF + tempRise;
    } else {
        // Keep the temperature at the preset value
        return presetTempF;
    }
}
*/


