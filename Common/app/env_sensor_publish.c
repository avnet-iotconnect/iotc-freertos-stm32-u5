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

#define LOG_LEVEL    LOG_ERROR

#include "logging.h"

/* Standard includes. */
#include <string.h>
#include <stdio.h>

/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "kvstore.h"

/* MQTT library includes. */
#include "core_mqtt.h"
#include "core_mqtt_agent.h"
#include "sys_evt.h"

/* Subscription manager header include. */
#include "subscription_manager.h"

/* Sensor includes */
#include "b_u585i_iot02a_env_sensors.h"

#include "app/sensor_telemetry.h"

/*-----------------------------------------------------------*/

#define MQTT_PUBLISH_TIME_BETWEEN_MS 3000		/* Interval between reading environment sensors */

/*-----------------------------------------------------------*/

static BaseType_t xIsMqttConnected( void )
{
    /* Wait for MQTT to be connected */
    EventBits_t uxEvents = xEventGroupWaitBits( xSystemEvents,
                                                EVT_MASK_MQTT_CONNECTED,
                                                pdFALSE,
                                                pdTRUE,
                                                0 );

    return( ( uxEvents & EVT_MASK_MQTT_CONNECTED ) == EVT_MASK_MQTT_CONNECTED );
}

/*-----------------------------------------------------------*/

static BaseType_t xInitSensors( void )
{
    int32_t lBspError = BSP_ERROR_NONE;

    lBspError = BSP_ENV_SENSOR_Init( 0, ENV_TEMPERATURE );

    lBspError |= BSP_ENV_SENSOR_Init( 0, ENV_HUMIDITY );

    lBspError |= BSP_ENV_SENSOR_Init( 1, ENV_TEMPERATURE );

    lBspError |= BSP_ENV_SENSOR_Init( 1, ENV_PRESSURE );

    lBspError |= BSP_ENV_SENSOR_Enable( 0, ENV_TEMPERATURE );

    lBspError |= BSP_ENV_SENSOR_Enable( 0, ENV_HUMIDITY );

    lBspError |= BSP_ENV_SENSOR_Enable( 1, ENV_TEMPERATURE );

    lBspError |= BSP_ENV_SENSOR_Enable( 1, ENV_PRESSURE );

    lBspError |= BSP_ENV_SENSOR_SetOutputDataRate( 0, ENV_TEMPERATURE, 1.0f );

    lBspError |= BSP_ENV_SENSOR_SetOutputDataRate( 0, ENV_HUMIDITY, 1.0f );

    lBspError |= BSP_ENV_SENSOR_SetOutputDataRate( 1, ENV_TEMPERATURE, 1.0f );

    lBspError |= BSP_ENV_SENSOR_SetOutputDataRate( 1, ENV_PRESSURE, 1.0f );

    return( lBspError == BSP_ERROR_NONE ? pdTRUE : pdFALSE );
}

static BaseType_t xUpdateSensorData( EnvironmentalSensorData_t * pxData )
{
    int32_t lBspError = BSP_ERROR_NONE;

    lBspError = BSP_ENV_SENSOR_GetValue( 0, ENV_TEMPERATURE, &pxData->fTemperature0 );
    lBspError |= BSP_ENV_SENSOR_GetValue( 0, ENV_HUMIDITY, &pxData->fHumidity );
    lBspError |= BSP_ENV_SENSOR_GetValue( 1, ENV_TEMPERATURE, &pxData->fTemperature1 );
    lBspError |= BSP_ENV_SENSOR_GetValue( 1, ENV_PRESSURE, &pxData->fBarometricPressure );

    return( lBspError == BSP_ERROR_NONE ? pdTRUE : pdFALSE );
}

/*-----------------------------------------------------------*/

extern UBaseType_t uxRand( void );

void vEnvironmentSensorPublishTask( void * pvParameters )
{
    BaseType_t xResult = pdFALSE;
    BaseType_t xExitFlag = pdFALSE;
    MQTTAgentHandle_t xAgentHandle = NULL;
    size_t uxTopicLen = 0;

    ( void ) pvParameters;

    xResult = xInitSensors();

    if( xResult != pdTRUE )
    {
        LogError( "Error while initializing environmental sensors." );
        vTaskDelete( NULL );
    }

    vSleepUntilMQTTAgentReady();

    xAgentHandle = xGetMqttAgentHandle();

    while( xExitFlag == pdFALSE )
    {
        TickType_t xTicksToWait = pdMS_TO_TICKS( MQTT_PUBLISH_TIME_BETWEEN_MS );
        TimeOut_t xTimeOut;
        struct IOTC_U5IOT_TELEMETRY payload;

        vTaskSetTimeOutState( &xTimeOut );

        xResult = xUpdateSensorData( &payload.xEnvSensorData );

        if( xResult == pdTRUE )
        {
            payload.bMotionSensorValid = false;
            payload.bEnvSensorDataValid = true;

        	if( xIsMqttConnected() == pdTRUE )
        	{
            	iotcApp_create_and_send_telemetry_json(&payload, sizeof(payload));
        	}
        }

        /* Adjust remaining tick count */
        if( xTaskCheckForTimeOut( &xTimeOut, &xTicksToWait ) == pdFALSE )
        {
            /* Wait until its time to poll the sensors again */
            vTaskDelay( xTicksToWait );
        }
    }
}
