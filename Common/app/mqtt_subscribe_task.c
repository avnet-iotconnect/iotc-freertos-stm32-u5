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

/* NVM config includes */
#include "kvstore.h"

/* mqtt includes */
#include "core_mqtt.h"
#include "core_mqtt_agent.h"
#include "mqtt_agent_task.h"
#include "subscription_manager.h"

/**
 * @brief Format of topic string used to subscribe to incoming messages.
 */
#if 1
#define SUBSCRIBE_TOPIC_FORMAT   "iot/%s/cmd"
#else
#define SUBSCRIBE_TOPIC_FORMAT           "devices/%s/messages/devicebound/#"
#endif

/**
 * @brief length of buffer to hold subscribe topic string containing the device id (thing_name)
 */
#define MQTT_SUBSCRIBE_TOPIC_STR_LEN           	( 256 )

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


/*-----------------------------------------------------------*/

static void prvIncomingPublishCallback( void * pvIncomingPublishCallbackContext,
                                        MQTTPublishInfo_t * pxPublishInfo )
{
    static char cTerminatedString[ confgPAYLOAD_BUFFER_LENGTH ];

    ( void ) pvIncomingPublishCallbackContext;

    /* Create a message that contains the incoming MQTT payload to the logger,
     * terminating the string first. */
    if( pxPublishInfo->payloadLength < confgPAYLOAD_BUFFER_LENGTH )
    {
        memcpy( ( void * ) cTerminatedString, pxPublishInfo->pPayload, pxPublishInfo->payloadLength );
        cTerminatedString[ pxPublishInfo->payloadLength ] = 0x00;
    }
    else
    {
        memcpy( ( void * ) cTerminatedString, pxPublishInfo->pPayload, confgPAYLOAD_BUFFER_LENGTH );
        cTerminatedString[ confgPAYLOAD_BUFFER_LENGTH - 1 ] = 0x00;
    }

    LogInfo( ( "Received incoming publish message %s", cTerminatedString ) );
}

/*-----------------------------------------------------------*/

static MQTTStatus_t prvSubscribeToTopic( MQTTQoS_t xQoS,
                                         char * pcTopicFilter )
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

        if( xMQTTStatus != MQTTSuccess )
        {
            LogError( ( "Failed to SUBSCRIBE to topic with error = %u.", xMQTTStatus ) );
        }
        else
        {
            LogInfo( ( "Subscribed to topic %.*s.\n\n", strlen( pcTopicFilter ), pcTopicFilter ) );
        }
    } while( xMQTTStatus != MQTTSuccess );

    return xMQTTStatus;
}

/*-----------------------------------------------------------*/

void vMQTTSubscribeTask( void * pvParameters )
{
    BaseType_t xStatus = pdPASS;
    MQTTStatus_t xMQTTStatus;

    char pcSubTopicString[ MQTT_SUBSCRIBE_TOPIC_STR_LEN ] = { 0 };
    char * pcDeviceId = NULL;
    int lSubTopicLen = 0;

    ( void ) pvParameters;

    vSleepUntilMQTTAgentReady();

    xMQTTAgentHandle = xGetMqttAgentHandle();
    configASSERT( xMQTTAgentHandle != NULL );

    vSleepUntilMQTTAgentConnected();

    LogInfo( ( "MQTT Agent is connected. Starting the subscribe task. " ) );

    if( xStatus == pdPASS )
    {
        pcDeviceId = KVStore_getStringHeap( CS_CORE_THING_NAME, NULL );

        if (pcDeviceId == NULL) {
            LogError( "Error getting the thing_name setting." );
            vTaskDelete( NULL );
        } else {
        	lSubTopicLen = snprintf( pcSubTopicString, ( size_t ) MQTT_SUBSCRIBE_TOPIC_STR_LEN, SUBSCRIBE_TOPIC_FORMAT, pcDeviceId);
        }

        if( ( lSubTopicLen <= 0 ) || ( lSubTopicLen > MQTT_SUBSCRIBE_TOPIC_STR_LEN) )
        {
            LogError( "Error while constructing subscribe topic string." );
            vTaskDelete( NULL );
        }

        xMQTTStatus = prvSubscribeToTopic( MQTTQoS1, pcSubTopicString);

        if( xMQTTStatus != MQTTSuccess )
        {
            LogError( "Failed to subscribe to topic: %s.", pcSubTopicString );
            xStatus = pdFAIL;
        }
    }

    LogInfo ( "Subscribed to: %s", pcSubTopicString );
    vTaskDelete( NULL );
}
