/*
 * telemetry.c
 *
 *  Created on: May 16, 2024
 *      Author: mgilhespie
 */

#include "logging_levels.h"
/* define LOG_LEVEL here if you want to modify the logging level from the default */

#define LOG_LEVEL    LOG_ERROR

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

#include "iotcl_telemetry.h"
#include "iotcl_log.h"
#include "iotcl.h"

#include "app/sensor_telemetry.h"


/* @brief 	Create JSON message containing telemetry data to publish
 *
 */
void iotcApp_create_and_send_telemetry_json(
		const void *pToTelemetryStruct, size_t siz) {

    const struct IOTC_U5IOT_TELEMETRY *p = (const struct IOTC_U5IOT_TELEMETRY *)pToTelemetryStruct;
    IotclMessageHandle msg = iotcl_telemetry_create();

    if(siz != sizeof(const struct IOTC_U5IOT_TELEMETRY)) {
        IOTCL_ERROR(siz, "Expected telemetry size does not match");
        return;
    }

    // Optional. The first time you create a data poiF\Z\nt, the current timestamp will be automatically added
    // TelemetryAddWith* calls are only required if sending multiple data points in one packet.
	// FIXME: new iotc-c-lib has new API for adding timestamps
	// iotcl_telemetry_add_with_iso_time(msg, NULL);

    if (p->bMotionSensorValid) {
    	iotcl_telemetry_set_number(msg, "acc_x", (double)p->xAcceleroAxes.x);
    	iotcl_telemetry_set_number(msg, "acc_y", (double)p->xAcceleroAxes.y);
    	iotcl_telemetry_set_number(msg, "acc_z", (double)p->xAcceleroAxes.z);

    	iotcl_telemetry_set_number(msg, "gyro_x", (double)p->xGyroAxes.x);
		iotcl_telemetry_set_number(msg, "gyro_y", (double)p->xGyroAxes.y);
		iotcl_telemetry_set_number(msg, "gyro_z", (double)p->xGyroAxes.z);

		iotcl_telemetry_set_number(msg, "mgnt_x", (double)p->xMagnetoAxes.x);
		iotcl_telemetry_set_number(msg, "mgnt_y", (double)p->xMagnetoAxes.y);
		iotcl_telemetry_set_number(msg, "mgnt_Z", (double)p->xMagnetoAxes.z);
    }

    if (p->bEnvSensorDataValid) {
		iotcl_telemetry_set_number(msg, "temp_0", (double)p->xEnvSensorData.fTemperature0);
		iotcl_telemetry_set_number(msg, "temp_1", (double)p->xEnvSensorData.fTemperature1);
		iotcl_telemetry_set_number(msg, "humidity", (double)p->xEnvSensorData.fHumidity);
		iotcl_telemetry_set_number(msg, "pressure", (double)p->xEnvSensorData.fBarometricPressure);
    }

    iotcl_mqtt_send_telemetry(msg, true);
    iotcl_telemetry_destroy(msg);
}
