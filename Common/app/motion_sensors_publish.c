/*
 * Copyright (C) 2021 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 * Derived from simple_sub_pub_demo.c
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
 * https://www.FreeRTOS.org
 * https://github.com/FreeRTOS
 *
 */


#include "logging_levels.h"
/* define LOG_LEVEL here if you want to modify the logging level from the default */

//#define LOG_LEVEL    LOG_ERROR
#define LOG_LEVEL    LOG_INFO

#include "logging.h"


/* Standard includes. */
#include <string.h>
#include <stdio.h>
#include <assert.h>

/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "kvstore.h"

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
#include "iotconnect_lib.h"
#include "iotconnect_telemetry.h"

/*
 * IOT-Connect settings
 */

// CPID string
const char *cpId = "97FF86E8728645E9B89F7B07977E4JSON_FORMAT_AWS_21_WEBB15";


// Define a hardcoded telemetry "cd" variable here, overriding any set on command line
#define HARDCODED_TELEMETRY_CD  "XG4EOMA"

// Define one of these for the message format

// Handcrafted JSON, sent to to $aws/rules/msg_d2c_rpt/<device id>/<telemetry_cd>/2.1/0  (works)
//#define JSON_FORMAT_HANDCRAFTED_WORKING

// Handcrafted JSON string in iotc-c-lib format with minimal payload of "version"  (works)
//#define JSON_FORMAT_MINIMAL_VERSION_PAYLOAD)

// Handcrafted JSON string in iotc-c-lib format with telemetry data (working)
#define JSON_FORMAT_IOTC_C_LIB_HANDCRAFTED)

// Telemetry JSON created with iotc-c-lib (not working)
//#define JSON_FORMAT_IOTC_C_LIB_TELEMETRY)


#if defined (JSON_FORMAT_HANDCRAFTED_WORKING)
	#define PUB_TOPIC_FORMAT	"$aws/rules/msg_d2c_rpt/%s/%s/2.1/0"

#elif defined(JSON_FORMAT_IOTC_C_LIB_HANDCRAFTED) \
		|| defined(JSON_FORMAT_MINIMAL_VERSION_PAYLOAD) \
		|| defined(JSON_FORMAT_IOTC_C_LIB_TELEMETRY)
	#define PUB_TOPIC_FORMAT	"devices/%s/messages/events/"

#else
#error "Undefined JSON format, define one of the above formats"
#endif


#define SUB_TOPIC	"iot/%s/cmd"

//#define APP_VERSION "01.00.06"

#define APP_VERSION "01.00.06"

/**
 * @brief Size of statically allocated buffers for holding topic names and
 * payloads.
 */
#define MQTT_PUBLISH_MAX_LEN                 ( 1024 )
#define MQTT_PUBLISH_PERIOD_MS               ( 3000 )
#define MQTT_PUBLICH_TOPIC_STR_LEN           ( 256 )
#define MQTT_PUBLISH_BLOCK_TIME_MS           ( 200 )
#define MQTT_PUBLISH_NOTIFICATION_WAIT_MS    ( 1000 )
#define MQTT_NOTIFY_IDX                      ( 1 )
#define MQTT_PUBLISH_QOS                     ( MQTTQoS0 )


/*-----------------------------------------------------------*/

/**
 * @brief Defines the structure to use as the command callback context in this
 * demo.
 */
struct MQTTAgentCommandContext
{
    MQTTStatus_t xReturnStatus;
    TaskHandle_t xTaskToNotify;
    uint32_t ulNotificationValue;
};

/*-----------------------------------------------------------*/
static void prvPublishCommandCallback( MQTTAgentCommandContext_t * pxCommandContext,
                                       MQTTAgentReturnInfo_t * pxReturnInfo )
{
    TaskHandle_t xTaskHandle = ( TaskHandle_t ) pxCommandContext;

    configASSERT( pxReturnInfo != NULL );

    uint32_t ulNotifyValue = pxReturnInfo->returnCode;

    if( xTaskHandle != NULL )
    {
        /* Send the context's ulNotificationValue as the notification value so
         * the receiving task can check the value it set in the context matches
         * the value it receives in the notification. */
        ( void ) xTaskNotifyIndexed( xTaskHandle,
                                     MQTT_NOTIFY_IDX,
                                     ulNotifyValue,
                                     eSetValueWithOverwrite );
    }
}

/*-----------------------------------------------------------*/

static BaseType_t prvPublishAndWaitForAck( MQTTAgentHandle_t xAgentHandle,
                                           const char * pcTopic,
                                           const void * pvPublishData,
                                           size_t xPublishDataLen )
{
    MQTTStatus_t xStatus;
    size_t uxTopicLen = 0;

    configASSERT( pcTopic != NULL );
    configASSERT( pvPublishData != NULL );
    configASSERT( xPublishDataLen > 0 );

    uxTopicLen = strnlen( pcTopic, UINT16_MAX );

    MQTTPublishInfo_t xPublishInfo =
    {
        .qos             = MQTT_PUBLISH_QOS,
        .retain          = 0,
        .dup             = 0,
        .pTopicName      = pcTopic,
        .topicNameLength = ( uint16_t ) uxTopicLen,
        .pPayload        = pvPublishData,
        .payloadLength   = xPublishDataLen
    };

    MQTTAgentCommandInfo_t xCommandParams =
    {
        .blockTimeMs                 = MQTT_PUBLISH_BLOCK_TIME_MS,
        .cmdCompleteCallback         = prvPublishCommandCallback,
        .pCmdCompleteCallbackContext = ( void * ) xTaskGetCurrentTaskHandle(),
    };

    if( xPublishInfo.qos > MQTTQoS0 )
    {
        xCommandParams.pCmdCompleteCallbackContext = ( void * ) xTaskGetCurrentTaskHandle();
    }

    /* Clear the notification index */
    xTaskNotifyStateClearIndexed( NULL, MQTT_NOTIFY_IDX );


    xStatus = MQTTAgent_Publish( xAgentHandle,
                                 &xPublishInfo,
                                 &xCommandParams );

    if( xStatus == MQTTSuccess )
    {
        uint32_t ulNotifyValue = 0;
        BaseType_t xResult = pdFALSE;

        xResult = xTaskNotifyWaitIndexed( MQTT_NOTIFY_IDX,
                                          0xFFFFFFFF,
                                          0xFFFFFFFF,
                                          &ulNotifyValue,
                                          pdMS_TO_TICKS( MQTT_PUBLISH_NOTIFICATION_WAIT_MS ) );

        if( xResult )
        {
            xStatus = ( MQTTStatus_t ) ulNotifyValue;

            if( xStatus != MQTTSuccess )
            {
                LogError( "MQTT Agent returned error code: %d during publish operation.",
                          xStatus );
                xResult = pdFALSE;
            }
        }
        else
        {
            LogError( "Timed out while waiting for publish ACK or Sent event. xTimeout = %d",
                      pdMS_TO_TICKS( MQTT_PUBLISH_NOTIFICATION_WAIT_MS ) );
            xResult = pdFALSE;
        }
    }
    else
    {
        LogError( "MQTTAgent_Publish returned error code: %d.", xStatus );
    }

    return( xStatus == MQTTSuccess );
}

/*-----------------------------------------------------------*/
static BaseType_t xInitSensors( void )
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

// publish telemetry data to iotc
static char* publish_telemetry(IotclMessageHandle msg, BSP_MOTION_SENSOR_Axes_t accel_data, BSP_MOTION_SENSOR_Axes_t gyro_data, BSP_MOTION_SENSOR_Axes_t mag_data) {

    // Optional. The first time you create a data point, the current timestamp will be automatically added
    // TelemetryAddWith* calls are only required if sending multiple data points in one packet.
//    iotcl_telemetry_add_with_iso_time(msg, NULL);

    iotcl_telemetry_set_number(msg, "gyro_x", gyro_data.x);
    iotcl_telemetry_set_number(msg, "gyro_y", gyro_data.y);
    iotcl_telemetry_set_number(msg, "gyro_z", gyro_data.z);

    iotcl_telemetry_set_number(msg, "accelerometer_x", accel_data.x);
    iotcl_telemetry_set_number(msg, "accelerometer_y", accel_data.y);
    iotcl_telemetry_set_number(msg, "accelerometer_z", accel_data.z);


//    iotcl_telemetry_set_number(msg, "magnetometer_x", mag_data.x);
//    iotcl_telemetry_set_number(msg, "magnetometer_y", mag_data.y);
//    iotcl_telemetry_set_number(msg, "magnetometer_z", mag_data.z);

    iotcl_telemetry_set_string(msg, "version", APP_VERSION);

    LogInfo("iotcl_create_serialized_string: msg:%08x", (uint32_t)msg);


    const char* str = iotcl_create_serialized_string(msg, false);

	if (str == NULL) {
		LogInfo( "serialized_string is NULL");
	}
    ;
    iotcl_telemetry_destroy(msg);
    return (char* )str;
}

/*-----------------------------------------------------------*/
void vMotionSensorsPublish( void * pvParameters )
{
    ( void ) pvParameters;
    BaseType_t xResult = pdFALSE;
    BaseType_t xExitFlag = pdFALSE;

    MQTTAgentHandle_t xAgentHandle = NULL;
    char pcPayloadBuf[ MQTT_PUBLISH_MAX_LEN ];
    char pcTopicString[ MQTT_PUBLICH_TOPIC_STR_LEN ] = { 0 };
    char * pcDeviceId = NULL;
    char * pcTelemetryCd = NULL;
    int lTopicLen = 0;

    xResult = xInitSensors();

    if( xResult != pdTRUE )
    {
        LogError( "Error while initializing motion sensors." );
        vTaskDelete( NULL );
    }

    pcDeviceId = KVStore_getStringHeap( CS_CORE_THING_NAME, NULL );

#if defined (HARDCODED_TELEMETRY_CD)
    pcTelemetryCd = HARDCODED_TELEMETRY_CD;
#else
    pcTelemetryCd = KVStore_getStringHeap( CS_IOTC_TELEMETRY_CD, NULL );
#endif

    if( pcDeviceId == NULL || pcTelemetryCd == NULL)
    {
        xExitFlag = pdTRUE;
    }
    else
    {

#if defined (JSON_FORMAT_HANDCRAFTED_WORKING)
    	lTopicLen = snprintf( pcTopicString, ( size_t ) MQTT_PUBLICH_TOPIC_STR_LEN, PUB_TOPIC_FORMAT, pcDeviceId, pcTelemetryCd );
#elif  defined(JSON_FORMAT_IOTC_C_LIB_HANDCRAFTED) \
		|| defined(JSON_FORMAT_MINIMAL_VERSION_PAYLOAD) \
		|| defined(JSON_FORMAT_IOTC_C_LIB_TELEMETRY)

    	lTopicLen = snprintf( pcTopicString, ( size_t ) MQTT_PUBLICH_TOPIC_STR_LEN, PUB_TOPIC_FORMAT, pcDeviceId);
#endif

    	LogInfo("TopicString: %s", pcTopicString);
    }

    if( ( lTopicLen <= 0 ) || ( lTopicLen > MQTT_PUBLICH_TOPIC_STR_LEN ) )
    {
        LogError( "Error while constructing topic string." );
        xExitFlag = pdTRUE;
    }

    vSleepUntilMQTTAgentReady();

    xAgentHandle = xGetMqttAgentHandle();

    IotclConfig iot_config;
    memset (&iot_config, 0, sizeof iot_config);

    iot_config.device.cpid = cpId;
    iot_config.device.duid = pcDeviceId;
    iot_config.device.env = "poc";
    iot_config.telemetry.cd = pcTelemetryCd;
    iot_config.telemetry.dtg = NULL;
    iotcl_init_v2(&iot_config);

    while (1) {
        if( xIsMqttAgentConnected() == pdTRUE ) {
        	break;
        }
    }

    while (1)
    {
        /* Interpret sensor data */
        int32_t lBspError = BSP_ERROR_NONE;
        BSP_MOTION_SENSOR_Axes_t xAcceleroAxes, xGyroAxes, xMagnetoAxes;

        lBspError = BSP_MOTION_SENSOR_GetAxes( 0, MOTION_GYRO, &xGyroAxes );
        lBspError |= BSP_MOTION_SENSOR_GetAxes( 0, MOTION_ACCELERO, &xAcceleroAxes );
        lBspError |= BSP_MOTION_SENSOR_GetAxes( 1, MOTION_MAGNETO, &xMagnetoAxes );

        if( lBspError == BSP_ERROR_NONE )
        {
#if defined(JSON_FORMAT_HANDCRAFTED_WORKING)
      	 int lbytesWritten = snprintf( pcPayloadBuf,
                                          MQTT_PUBLISH_MAX_LEN,
										  " {"
  								      	  " \"cd\": \"%s\","
								      	  " \"mt\": 0,"
										  "  \"d\": [{"
										  "    \"d\": {"
										  "     \"accelerometer_x\":%d,"
										  "     \"accelerometer_y\":%d,"
										  "     \"accelerometer_z\":%d,"
										  "     \"gyro_x\":%d,"
										  "     \"gyro_y\":%d,"
										  "     \"gyro_z\":%d"
										  "    }"
										  "  }]"
										  " }",
										  pcTelemetryCd,
										  xAcceleroAxes.x, xAcceleroAxes.y, xAcceleroAxes.z,
                                          xGyroAxes.x, xGyroAxes.y, xGyroAxes.z);


#elif defined(JSON_FORMAT_IOTC_C_LIB_HANDCRAFTED)
      	 int lbytesWritten = snprintf( pcPayloadBuf,
                                          MQTT_PUBLISH_MAX_LEN,
										  "{"
										    "\"cd\": \"%s\","
										    "\"d\": {"
										      "\"d\": ["
										        "{"
										          "\"d\": {"
										            "\"gyro_x\": %d,"
										            "\"gyro_y\": %d,"
										  	  	  	"\"gyro_z\": %d,"
										  	  	  	"\"accelerometer_x\": %d,"
										  	  	    "\"accelerometer_y\": %d,"
										  	  	  	"\"accelerometer_z\": %d,"
										  	  	  	"\"version\": \"2023-10-03T14:51:55.000Z\""
										          "}"
										        "}"
										      "]"
										    "},"
										    "\"mt\": 0"
										  "}",
										  pcTelemetryCd,
                                          xGyroAxes.x,
										  xGyroAxes.y,
										  xGyroAxes.z,
										  xAcceleroAxes.x,
										  xAcceleroAxes.y,
										  xAcceleroAxes.z);


#elif defined(JSON_FORMAT_MINIMAL_VERSION_PAYLOAD)
      	 int lbytesWritten = snprintf( pcPayloadBuf,
                                          MQTT_PUBLISH_MAX_LEN,
										 "{"
										  "\"d\": {"
											"\"d\": [{"
												"\"d\": {"
												  "\"version\": \"APP-1.0\""
												"}"
											"}]"
										  "}"
										  "\"mt\": 0,"
										  "\"cd\": \"%s\""
										"}",
										pcTelemetryCd
										  );

#elif defined (JSON_FORMAT_IOTC_C_LIB_TELEMETRY)
        	vTaskDelay(1000);

            IotclMessageHandle message = iotcl_telemetry_create();
            char* data = publish_telemetry(message, xAcceleroAxes, xGyroAxes, xMagnetoAxes);

            if (data == NULL) {
            	LogInfo("data is NULL...\n");
            	return;
            }

            int lbytesWritten = snprintf(pcPayloadBuf, MQTT_PUBLISH_MAX_LEN, "%s", data);
            iotcl_destroy_serialized(data);

#endif
        	LogInfo( "PAYLOAD is %s.", pcPayloadBuf);

            if( (xIsMqttAgentConnected() == pdTRUE ) )
            {
            	LogInfo( "PUB TOPIC is %s", pcTopicString);
                xResult = prvPublishAndWaitForAck( xAgentHandle,
                                                   pcTopicString,
                                                   pcPayloadBuf,
                                                   ( size_t ) lbytesWritten );

                if( xResult != pdPASS )
                {
                    LogError( "Failed to publish motion sensor data" );
                }
            }
        }

        vTaskDelay( pdMS_TO_TICKS( MQTT_PUBLISH_PERIOD_MS ) );
    }

    vPortFree( pcDeviceId );
}
