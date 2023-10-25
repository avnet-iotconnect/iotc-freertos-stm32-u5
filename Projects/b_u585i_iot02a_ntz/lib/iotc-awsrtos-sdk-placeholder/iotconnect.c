/*
 * iotconnect.c
 *
 *  Created on: Oct 23, 2023
 *      Author: mgilhespie
 */

#include "logging_levels.h"

#define LOG_LEVEL    LOG_DEBUG

#include "logging.h"

/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "sys_evt.h"
#include "stm32u5xx.h"
#include "kvstore.h"
#include "hw_defs.h"
#include <string.h>

#include "lfs.h"
#include "fs/lfs_port.h"
#include "stm32u5xx_ll_rng.h"

/* MQTT library includes. */
#include "core_mqtt.h"

/* MQTT agent include. */
#include "core_mqtt_agent.h"
#include "mqtt_agent_task.h"

#include "iotc_awsmqtt_client.h"

//Iotconnect
#include "iotconnect.h"
#include "iotconnect_lib.h"
#include "iotconnect_telemetry.h"
#include "iotconnect_event.h"

/*
 * @brief	IOTConnect internal configuration (copy)
 */
static IotConnectClientConfig config = { 0 };
static IotclConfig lib_config = { 0 };
static IotConnectAwsrtosConfig awsrtos_config = { 0 };
static IotclSyncResult last_sync_result = IOTCL_SR_UNKNOWN_DEVICE_STATUS;


/*	@brief	Pre-initialization of SDK's configuration and return pointer to it.
 *
 */
IotConnectClientConfig* iotconnect_sdk_init_and_get_config() {
    memset(&config, 0, sizeof(config));
    return &config;
}


/* @brief	This the Initialization os IoTConnect SDK
 *
 */
int iotconnect_sdk_init(IotConnectAwsrtosConfig *ac) {
	int ret;
	IotConnectAWSMQTTConfig awsmqtt_config;

	memcpy(&awsrtos_config, ac, sizeof(awsrtos_config));
    memset(&awsmqtt_config, 0, sizeof(awsmqtt_config));

    last_sync_result = IOTCL_SR_UNKNOWN_DEVICE_STATUS;

#if 1
    // Get mqtt endpoint device id, telemetry_cd from the CLI

#else
	iotcl_discovery_free_discovery_response(discovery_response);
	iotcl_discovery_free_sync_response(sync_response);
	discovery_response = NULL;
	sync_response = NULL;
    LogInfo("IOTC: Performing discovery...\r\n");
    discovery_response = run_http_discovery(config.cpid, config.env);
    if (NULL == discovery_response) {
        // get_base_url will print the error
        return -1;
    }
    LogInfo("IOTC: Discovery response parsing successful. Performing sync...\r\n");

    sync_response = run_http_sync(config.cpid, config.duid);
    if (NULL == sync_response) {
        // Sync_call will print the error
        return -2;
    }
    LogInfo("IOTC: Sync response parsing successful.\r\n");
#endif

    // We want to print only first 5 characters of cpid. %.5s doesn't seem to work with prink
#if 0
    char cpid_buff[6];
    strncpy(cpid_buff, sync_response->cpid, 5);
    cpid_buff[5] = 0;

    LogInfo("IOTC: CPID: %s***\r\n", cpid_buff);
#endif

    LogInfo("IOTC: ENV:  %s\r\n", config.env);

#if 0
    awsmqtt_config.device_name = sync_response->broker.client_id;
    awsmqtt_config.host = sync_response->broker.host;
    awsmqtt_config.auth = &config.auth;
    awsmqtt_config.status_cb = on_iotconnect_status;
#endif

    lib_config.device.env = config.env;
    lib_config.device.cpid = config.cpid;
    lib_config.device.duid = config.duid;

    /*
     * FIXME: The telemetry cd and dtg values are passed by the IotConnectAwsrtosConfig.  These may be
     * removed and obtained by the to be implemented discovery/sync code.
     */
	lib_config.telemetry.cd = ac->telemetry_cd;
	lib_config.telemetry.dtg = ac->telemetry_dtg;

    lib_config.event_functions.ota_cb = config.ota_cb;
    lib_config.event_functions.cmd_cb = config.cmd_cb;
    lib_config.event_functions.msg_cb = config.msg_cb;

    // Initialize the iotc-c-lib for awsformat2.1
    if (!iotcl_init_v2(&lib_config)) {
        LogError("IOTC: Failed to initialize the IoTConnect C Lib");
        return -1;
    }

    LogInfo("IOTC: Initializing the mqtt connection");
    ret = awsmqtt_client_init(&awsmqtt_config, &awsrtos_config);
    if (ret) {
        LogError("IOTC: Failed to connect to mqtt server");
    	return ret;
    }

    return ret;
}


/**
 * @brief	Publish a message on the event topic
 */
void iotconnect_sdk_send_packet(const char *data) {
    if (awsmqtt_send_message(data) != 0) {
        LogError("IOTC: Failed to send message %s\r\n", data);
    }
}

