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

/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "kvstore.h"
#include "mbedtls_transport.h"

#include "sys_evt.h"
#include "ota_pal.h"

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
#include "iotcl.h"
#include "iotcl_c2d.h"
#include "iotcl_certs.h"
#include "iotcl_cfg.h"
#include "iotcl_log.h"
#include "iotcl_telemetry.h"
#include "iotcl_util.h"

#include <config/iotconnect_config.h>

// BSP-Specific
#include "stm32u5xx.h"
#include "b_u585i_iot02a.h"

// Constants
#define APP_VERSION 			"05.09.14"		// Version string
#define MQTT_PUBLISH_PERIOD_MS 	( 3000 )		// Size of statically allocated buffers for holding topic names and payloads.

// @brief	IOTConnect configuration defined by application
#if !defined(IOTCONFIG_USE_DISCOVERY_SYNC)
static IotConnectCustomMQTTConfig custom_mqtt_config;
#endif

static bool is_downloading = false;



// Prototypes
static BaseType_t init_sensors( void );
static int create_and_send_telemetry(BSP_MOTION_SENSOR_Axes_t accel_data,
		BSP_MOTION_SENSOR_Axes_t gyro_data, BSP_MOTION_SENSOR_Axes_t mag_data);
static void on_command(IotclC2dEventData data);
static void on_ota(IotclC2dEventData data);
static bool is_ota_agent_file_initialized(void);
static int split_url(const char *url, char **host_name, char**resource);
static bool is_app_version_same_as_ota(const char *version);
static bool app_needs_ota_update(const char *version);
static int start_ota(char *url);


/* @brief	Main IoT-Connect application task
 *
 * @param	pvParameters, argument passed by xTaskCreate
 *
 * This is started by the initialization code in app_main.c which first performs board and
 * networking initialization
 */
void iotconnect_app( void * )
{
    BaseType_t result = pdFALSE;

    LogInfo( " ***** STARTING APP VERSION %s *****", APP_VERSION );

    result = init_sensors();

    if(result != pdTRUE) {
        LogError( "Error while initializing motion sensors." );
        vTaskDelete( NULL );
    }

    // Get some settings from non-volatile storage.  These can be set on the command line
    // using the conf command.

    char *device_id = KVStore_getStringHeap(CS_CORE_THING_NAME, NULL);   // Device ID
    char *cpid = KVStore_getStringHeap(CS_IOTC_CPID, NULL);
    char *iotc_env = KVStore_getStringHeap(CS_IOTC_ENV, NULL);

    if (device_id == NULL || cpid == NULL || iotc_env == NULL) {
    	LogError("IOTC configuration, thing_name, cpid or env are not set\n");
		vTaskDelete(NULL);
    }

    // IoT-Connect configuration setup

    IotConnectClientConfig *config = iotconnect_sdk_init_and_get_config();
    config->cpid = cpid;
    config->env = iotc_env;
    config->duid = device_id;
    config->cmd_cb = on_command;

#ifdef IOTCONFIG_ENABLE_OTA
    config->ota_cb = on_ota;
#else
    config->ota_cb = NULL;
#endif

    config->status_cb = NULL;
    config->auth_info.type = IOTC_X509;
    config->auth_info.mqtt_root_ca               = xPkiObjectFromLabel( TLS_MQTT_ROOT_CA_CERT_LABEL );
    config->auth_info.data.cert_info.device_cert = xPkiObjectFromLabel( TLS_CERT_LABEL );
    config->auth_info.data.cert_info.device_key  = xPkiObjectFromLabel( TLS_KEY_PRV_LABEL );;

#if defined(IOTCONFIG_USE_DISCOVERY_SYNC)
    // Get MQTT configuration from discovery and sync
    iotconnect_sdk_init(NULL);
#else
    // Not using Discovery and Sync so some additional settings, are obtained from the CLI,
    char *mqtt_endpoint_url = KVStore_getStringHeap(CS_CORE_MQTT_ENDPOINT, NULL);

    if (mqtt_endpoint_url == NULL) {
    	LogError ("IOTC configuration, mqtt_endpoint not set");
    	vTaskDelete( NULL );
    }

    custom_mqtt_config.host = mqtt_endpoint_url;
    iotconnect_sdk_init(&custom_mqtt_config);
#endif

    while (1) {
        /* Interpret sensor data */
        int32_t sensor_error = BSP_ERROR_NONE;
        BSP_MOTION_SENSOR_Axes_t xAcceleroAxes, xGyroAxes, xMagnetoAxes;

        sensor_error = BSP_MOTION_SENSOR_GetAxes( 0, MOTION_GYRO, &xGyroAxes );
        sensor_error |= BSP_MOTION_SENSOR_GetAxes( 0, MOTION_ACCELERO, &xAcceleroAxes );
        sensor_error |= BSP_MOTION_SENSOR_GetAxes( 1, MOTION_MAGNETO, &xMagnetoAxes );

        if (sensor_error == BSP_ERROR_NONE) {
			create_and_send_telemetry(xAcceleroAxes, xGyroAxes, xMagnetoAxes);
        }

        vTaskDelay( pdMS_TO_TICKS( MQTT_PUBLISH_PERIOD_MS ) );
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

    return( lBspError == BSP_ERROR_NONE ? pdTRUE : pdFALSE );
}


/* @brief 	Create JSON message containing telemetry data to publish
 *
 */
static int create_and_send_telemetry(BSP_MOTION_SENSOR_Axes_t accel_data,
		BSP_MOTION_SENSOR_Axes_t gyro_data, BSP_MOTION_SENSOR_Axes_t mag_data) {

    IotclMessageHandle msg = iotcl_telemetry_create();

    iotcl_telemetry_set_number(msg, "gyro_x", gyro_data.xval);
    iotcl_telemetry_set_number(msg, "gyro_y", gyro_data.yval);
    iotcl_telemetry_set_number(msg, "gyro_z", gyro_data.zval);

    iotcl_telemetry_set_number(msg, "accelerometer_x", accel_data.xval);
    iotcl_telemetry_set_number(msg, "accelerometer_y", accel_data.yval);
    iotcl_telemetry_set_number(msg, "accelerometer_z", accel_data.zval);

    iotcl_telemetry_set_number(msg, "magnetometer_x", mag_data.xval);
    iotcl_telemetry_set_number(msg, "magnetometer_y", mag_data.yval);
    iotcl_telemetry_set_number(msg, "magnetometer_z", mag_data.zval);

    iotcl_telemetry_set_string(msg, "version", APP_VERSION);

    iotcl_mqtt_send_telemetry(msg, true);
    iotcl_telemetry_destroy(msg);
}


/* @brief	Callback when a a cloud-to-device command is received on the subscribed MQTT topic
 */
static void on_command(IotclC2dEventData data) {
    const char *command = iotcl_c2d_get_command(data);
    const char *ack_id = iotcl_c2d_get_ack_id(data);
    if (command) {
        IOTCL_INFO("Command %s received with %s ACK ID\n", command,
                        ack_id ? ack_id : "no");

        if(NULL != strstr(command, "led-red") ) {
            if (NULL != strstr(command, "on")) {
                BSP_LED_On(LED_RED);
            } else {
                BSP_LED_Off(LED_RED);
            }
        } else if(NULL != strstr(command, "led-green") ) {
            if (NULL != strstr(command, "on")) {
                BSP_LED_On(LED_GREEN);
            } else {
                BSP_LED_Off(LED_GREEN);
            }
        }

        // could be a command without acknowledgement, so ackID can be null
        if (ack_id) {
            iotcl_mqtt_send_cmd_ack(ack_id, IOTCL_C2D_EVT_CMD_FAILED,
	                            "Not implemented");
        }
    } else {
        IOTCL_ERROR(0, "No command, internal error");
        // could be a command without acknowledgement, so ackID can be null
        if (ack_id) {
            iotcl_mqtt_send_cmd_ack(ack_id, IOTCL_C2D_EVT_CMD_FAILED,
 	                            "Internal error");
        }
    }
}

#ifdef IOTCONFIG_ENABLE_OTA
static void on_ota(IotclC2dEventData data) {
	const char *message = NULL;
	const char *url = iotcl_c2d_get_ota_url(data, 0);
	const char *ack_id = iotcl_c2d_get_ack_id(data);
    bool success = false;
    int needs_ota_commit = false;

    LogInfo("\n\nOTA command received\n");

    if (NULL != url) {
    	LogInfo("Download URL is: %s\r\n", url);
		const char *version = iotcl_c2d_get_ota_sw_version(data);
        if (!version) {
            success = true;
            message = "Failed to parse message";
        } else {
            // ignore wrong app versions in this application
            success = true;
            if (is_app_version_same_as_ota(version)) {
            	IOTCL_WARN(0, "OTA request for same version %s. Sending successn", version);
            } else if (app_needs_ota_update(version)) {
            	IOTCL_WARN(0, "OTA update is required for version %s.", version);
            }  else {
            	IOTCL_WARN(0, "Device firmware version %s is newer than OTA version %s. Sending failuren", APP_VERSION,
                        version);
                // Not sure what to do here. The app version is better than OTA version.
                // Probably a development version, so return failure?
                // The user should decide here.
            }

            is_downloading = true;

            if (start_ota(url) == 0) {
                needs_ota_commit = true;
                success = true;
            }

            is_downloading = false; // we should reset soon
        }

        free((void*) url);
        free((void*) version);
    } else {
        IOTCL_ERROR(0, "OTA has no URL");
        success = false;
    }

    iotcl_mqtt_send_ota_ack(ack_id,
            (success ?
                        IOTCL_C2D_EVT_OTA_SUCCESS :
                        IOTCL_C2D_EVT_OTA_DOWNLOAD_FAILED), message);

    if (needs_ota_commit) {
        // 5 second Delay to allow OTA ack to be sent
    	IOTCL_INFO("wait 5 seconds to commit OTA");
        vTaskDelay( pdMS_TO_TICKS( 5000 ) );
        IOTCL_INFO("committing OTA...");
        iotc_ota_fw_apply();
    }
}


/* @brief	Parse the OTA download URL into host and resource strings
 *
 * Note: The host and resource strings will be malloced, ensure to
 * free the two pointers on success
 */
static int split_url(const char *url, char **host_name, char**resource) {
    size_t host_name_start = 0;
    size_t url_len = strlen(url);

    if (!host_name || !resource) {
    	IOTCL_ERROR(0, "split_url: Invalid usage");
        return -1;
    }
    *host_name = NULL;
    *resource = NULL;
    int slash_count = 0;
    for (size_t i = 0; i < url_len; i++) {
        if (url[i] == '/') {
            slash_count++;
            if (slash_count == 2) {
                host_name_start = i + 1;
            } else if (slash_count == 3) {
                const size_t slash_start = i;
                const size_t host_name_len = i - host_name_start;
                const size_t resource_len = url_len - i;
                *host_name = malloc(host_name_len + 1); //+1 for null
                if (NULL == *host_name) {
                    return -2;
                }
                memcpy(*host_name, &url[host_name_start], host_name_len);
                (*host_name)[host_name_len] = 0; // terminate the string

                *resource = malloc(resource_len + 1); //+1 for null
                if (NULL == *resource) {
                    free(*host_name);
                    return -3;
                }
                memcpy(*resource, &url[slash_start], resource_len);
                (*resource)[resource_len] = 0; // terminate the string

                return 0;
            }
        }
    }
    return -4; // URL could not be parsed
}

static int start_ota(char *url)
{
    char *host_name;
    char *resource;
    int status;

    IOTCL_INFO("start_ota: %s", url);

    status = split_url(url, &host_name, &resource);
    if (status) {
        IOTCL_ERROR(status, "start_ota: Error while splitting the URL, code: 0x%x", status);
        return status;
    }

    status = iotc_ota_fw_download(host_name, resource);

    free(host_name);
    free(resource);

    return status;
}

static bool is_app_version_same_as_ota(const char *version) {
    return strcmp(APP_VERSION, version) == 0;
}

static bool app_needs_ota_update(const char *version) {
    return strcmp(APP_VERSION, version) < 0;
}

#endif


