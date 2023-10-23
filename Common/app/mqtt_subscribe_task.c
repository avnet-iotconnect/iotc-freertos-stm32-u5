/*
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * http://www.FreeRTOS.org
 * http://aws.amazon.com/freertos
 */


/**
 * @brief Task to subscribe to MQTT messages on the "iot/device_id/cmd" topic.
 * Based on the original pub_sub_test_task.c
 */

/* Standard includes. */
#include <string.h>
#include <stdio.h>
#include <assert.h>

/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#define LOG_LEVEL    LOG_INFO

#include "logging.h"

/* NVM config includes */
#include "kvstore.h"

/* mqtt includes */
#include "core_mqtt.h"
#include "core_mqtt_agent.h"
#include "mqtt_agent_task.h"
#include "subscription_manager.h"

#include "iotconnect_lib.h"
#include "iotconnect_event.h"

#include "cJSON.h"

/**
 * @brief Format of topic string used to subscribe to incoming messages.
 */
#if 1
#define SUBSCRIBE_TOPIC_FORMAT   "iot/%s/cmd"
#else
#define SUBSCRIBE_TOPIC_FORMAT           "devices/%s/messages/devicebound/#"
#endif

/**
 * @brief Publish topic for device to cloud acknowledgements
 */
#define ACK_PUBLISH_TOPIC_FORMAT	"$aws/rules/msg_d2c_ack/%s/%s/2.1/6"

//AWS:   Ack: $aws/rules/msg_d2c_ack/<device_id>/<telemetry_cd>/2.1/6
//Azure: Ack: devices/<device_id>/messages/events/cd=<xxxxxxxx>&v=2.1&mt=6

/*
 * @brief Queue size of acknowledgements offloaded to the vMQTTSubscribeTask
 */
#define ACK_MSG_Q_SIZE	5

/**
 * @brief length of buffer to hold subscribe topic string containing the device id (thing_name)
 */
#define MQTT_SUBSCRIBE_TOPIC_STR_LEN           	( 256 )

/**
 * @brief length of buffer to hold publish topic string of acknowledgements
 */
#define MQTT_ACK_PUBLISH_TOPIC_STR_LEN			( 256 )

/**
 * @brief Size of statically allocated buffers for holding payloads.
 */
#define confgPAYLOAD_BUFFER_LENGTH           	( 256 )


/**
 * @brief Passed into MQTTAgent_Subscribe() as the callback to execute when
 * there is an incoming publish on the topic being subscribed to.  Its
 * implementation just logs information about the incoming publish including
 * the publish messages source topic and payload.
 *
 * See https://freertos.org/mqtt/mqtt-agent-demo.html#example_mqtt_api_call
 *
 * @param[in] pvIncomingPublishCallbackContext Context of the initial command.
 * @param[in] pxPublishInfo Deserialized publish.
 */
static void prvIncomingPublishCallback( void * pvIncomingPublishCallbackContext,
                                        MQTTPublishInfo_t * pxPublishInfo );

/**
 * @brief Subscribe to the topic the demo task will also publish to - that
 * results in all outgoing publishes being published back to the task
 * (effectively echoed back).
 *
 * @param[in] xQoS The quality of service (QoS) to use.  Can be zero or one
 * for all MQTT brokers.  Can also be QoS2 if supported by the broker.  AWS IoT
 * does not support QoS2.
 */
static MQTTStatus_t prvSubscribeToTopic( MQTTQoS_t xQoS,
                                         char * pcTopicFilter );

/**
 * @brief Task that subscribes to the iot/device_id/cmd topic.
 *
 * This task shuts down once it has subscribed. It can be extended to perform other tasks
 * in the background.
 *
 * @param pvParameters The parameters to the task.
 */
void vMQTTSubscribeTask( void * pvParameters );

/*-----------------------------------------------------------*/

/**
 * @brief The MQTT agent manages the MQTT contexts.  This set the handle to the
 * context used by this demo.
 */
extern MQTTAgentContext_t xGlobalMqttAgentContext;

/**
 * @beief Handle to MQTT agent
 */
static MQTTAgentHandle_t xMQTTAgentHandle = NULL;

/**
 *  @brief Handle to message queue of acknowledgements offloaded onto vMQTTSubscribeTask.
 */
static QueueHandle_t mqtt_ack_queue = NULL;


/**
 *
 */
BaseType_t mqttcore_send_message(const char *buf)
{
    BaseType_t status;

    size_t msg_buf_size = strlen(buf) + 1;
    char *msg_buf = NULL;

    msg_buf = malloc(msg_buf_size);

    if (!msg_buf) {
        LogError("failed to allocate msg_buf!");
    	return -1;
    }

    strcpy(msg_buf, buf);

    status = xQueueSendToBack(mqtt_ack_queue, &msg_buf, 10);

    if(status != pdTRUE) {
        free(msg_buf);
    }

    return status;
}

/**
 *
 */
void iotconnect_sdk_send_packet(const char *data) {
    if (mqttcore_send_message(data) != pdTRUE) {
        LogInfo("IOTC: Failed to send message %s\r\n", data);
    }
}

/**
 *
 */
void command_status(IotclEventData data, bool status, const char *command_name, const char *message) {
    LogInfo ("command status");
    vTaskDelay(100);

    const char *ack = iotcl_create_ack_string_and_destroy_event(data, status, message);
    LogInfo("command: %s status=%s: %s\r\n", command_name, status ? "OK" : "Failed", message);
    LogInfo("Sent CMD ack: %s\r\n", ack);
    iotconnect_sdk_send_packet(ack);
    free((void*) ack);
}

/**
 *
 */
void on_command(IotclEventData data) {
    LogInfo ("on_command callback");
    vTaskDelay(100);

    // TODO : implement command with ack, no ack and error codes.
    // TODO: set/clear led commands
    // Perhaps return error if already set or already cleared.

	char *command = iotcl_clone_command(data);
    if (NULL != command) {
        command_status(data, false, command, "Not implemented");
        free((void*) command);
    } else {
        command_status(data, true, command, "command did something");
    }
}


/*-----------------------------------------------------------*/

static void prvIncomingPublishCallback( void * pvIncomingPublishCallbackContext, MQTTPublishInfo_t * pxPublishInfo )
{
    static char cTerminatedString[ confgPAYLOAD_BUFFER_LENGTH ];

    ( void ) pvIncomingPublishCallbackContext;

    /* Create a message that contains the incoming MQTT payload to the logger,
     * terminating the string first. */
    if( pxPublishInfo->payloadLength < confgPAYLOAD_BUFFER_LENGTH ) {
        memcpy( ( void * ) cTerminatedString, pxPublishInfo->pPayload, pxPublishInfo->payloadLength );
        cTerminatedString[ pxPublishInfo->payloadLength ] = 0x00;
    } else {
        memcpy( ( void * ) cTerminatedString, pxPublishInfo->pPayload, confgPAYLOAD_BUFFER_LENGTH );
        cTerminatedString[ confgPAYLOAD_BUFFER_LENGTH - 1 ] = 0x00;
    }

    LogInfo( ( "Received incoming publish message %s", cTerminatedString ) );

    if (! iotcl_process_event(cTerminatedString)) {
        LogError ( "Failed to process event message" );
    }
}

/*-----------------------------------------------------------*/

static MQTTStatus_t prvSubscribeToTopic( MQTTQoS_t xQoS, char *pcTopicFilter )
{
    MQTTStatus_t xMQTTStatus;

    /* Loop in case the queue used to communicate with the MQTT agent is full and
     * attempts to post to it time out.  The queue will not become full if the
     * priority of the MQTT agent task is higher than the priority of the task
     * calling this function. */
    do
    {
        xMQTTStatus = MqttAgent_SubscribeSync( xMQTTAgentHandle,
                                               pcTopicFilter,
                                               xQoS,
                                               prvIncomingPublishCallback,
                                               NULL );

        if( xMQTTStatus != MQTTSuccess ) {
            LogError( ( "Failed to SUBSCRIBE to topic with error = %u.", xMQTTStatus ) );
        } else {
            LogInfo( ( "Subscribed to topic %.*s.\n\n", strlen( pcTopicFilter ), pcTopicFilter ) );
        }
    } while( xMQTTStatus != MQTTSuccess );

    return xMQTTStatus;
}

/*-----------------------------------------------------------*/

void vMQTTSubscribeTask( void * pvParameters )
{
    ( void ) pvParameters;

    IotclConfig *iot_config;
	BaseType_t xStatus = pdPASS;
    MQTTStatus_t xMQTTStatus;
    char pcSubTopicString[ MQTT_SUBSCRIBE_TOPIC_STR_LEN ] = { 0 };
    char * pcDeviceId = NULL;
    char * pcTelemetryCd = NULL;
    int lSubTopicLen = 0;
    int lAckPubTopicLen = 0;
    int ret;
    char *pcAckMsgBuf;
    char pcAckPubTopicString[ MQTT_ACK_PUBLISH_TOPIC_STR_LEN ] = { 0 };;
    size_t lAckMsgLen;

    vSleepUntilMQTTAgentReady();
    xMQTTAgentHandle = xGetMqttAgentHandle();
    configASSERT( xMQTTAgentHandle != NULL );
    vSleepUntilMQTTAgentConnected();

    LogInfo( ( "MQTT Agent is connected. Starting the subscribe task. " ) );

    // message queue init
    mqtt_ack_queue = xQueueCreate(ACK_MSG_Q_SIZE, sizeof(char *));
    if (mqtt_ack_queue == NULL) {
        LogError("Failed to create Ack message queue");
		vTaskDelete( NULL );
    }


    iot_config = iotcl_get_config();

	pcDeviceId = iot_config->device.duid;
	pcTelemetryCd = iot_config->telemetry.cd;

	if (pcDeviceId == NULL) {
		LogError( "Error getting the thing_name setting." );
		vTaskDelete( NULL );
	} else {
		lSubTopicLen = snprintf( pcSubTopicString, ( size_t ) MQTT_SUBSCRIBE_TOPIC_STR_LEN, SUBSCRIBE_TOPIC_FORMAT, pcDeviceId);
	}

	if( ( lSubTopicLen <= 0 ) || ( lSubTopicLen > MQTT_SUBSCRIBE_TOPIC_STR_LEN) ) {
		LogError( "Error while constructing subscribe topic string." );
		vTaskDelete( NULL );
	}

	if (pcTelemetryCd == NULL) {
		LogError( "Error getting the telemetry_cd setting." );
		vTaskDelete( NULL );
	} else {
		lAckPubTopicLen = snprintf( pcAckPubTopicString, ( size_t ) MQTT_ACK_PUBLISH_TOPIC_STR_LEN, ACK_PUBLISH_TOPIC_FORMAT, pcDeviceId, pcTelemetryCd);
	}

	if( ( lAckPubTopicLen <= 0 ) || ( lAckPubTopicLen > MQTT_ACK_PUBLISH_TOPIC_STR_LEN) ) {
		LogError( "Error while constructing ack publsh topic string, len: %d.", lAckPubTopicLen );
		vTaskDelete( NULL );
	}

	xMQTTStatus = prvSubscribeToTopic( MQTTQoS1, pcSubTopicString);

	if( xMQTTStatus != MQTTSuccess ) {
		LogError( "Failed to subscribe to topic: %s.", pcSubTopicString );
		xStatus = pdFAIL;
	}

    LogInfo ( "Subscribed to: %s", pcSubTopicString );
    LogInfo ( "Ack Publish to: %s", pcSubTopicString );


    // message queue init
    while (1) {
        xStatus = xQueueReceive(mqtt_ack_queue, &pcAckMsgBuf, portMAX_DELAY);
        if (xStatus != pdPASS) {
            LogError("[%s] Q recv error (%d)\r\n", __func__, xStatus);
            break;
        }

        LogInfo ("Received something in ack queue");

        if (pcAckMsgBuf) {
        	LogInfo ("Publishing command: %s", pcAckMsgBuf);

        	lAckMsgLen = strlen(pcAckMsgBuf);

        	ret = prvPublishAndWaitForAck(xMQTTAgentHandle,
        								  pcAckPubTopicString,
        	                              pcAckMsgBuf,
        	                              lAckMsgLen);

            if (ret != 0) {
                LogError("Sending a message failed\n");
            }

            // allocated in _mqtt_send_to_q()
            free(pcAckMsgBuf);
        } else {
            LogError("[%s] can't send a NULL user_msg_buf\r\n", __func__);
        }
    }

    vTaskDelete( NULL );
}
