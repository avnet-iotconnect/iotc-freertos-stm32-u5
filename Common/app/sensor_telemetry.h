/*
 * telemetry.h
 *
 *  Created on: May 16, 2024
 *      Author: mgilhespie
 */

#ifndef APP_SENSOR_TELEMETRY_H_
#define APP_SENSOR_TELEMETRY_H_

#include <stdint.h>
#include <stdbool.h>

/* Sensor includes */
#include "b_u585i_iot02a_motion_sensors.h"
#include "b_u585i_iot02a_env_sensors.h"

typedef struct
{
    float_t fTemperature0;
    float_t fTemperature1;
    float_t fHumidity;
    float_t fBarometricPressure;
} EnvironmentalSensorData_t;

typedef struct IOTC_U5IOT_TELEMETRY {
    BSP_MOTION_SENSOR_Axes_t xAcceleroAxes, xGyroAxes, xMagnetoAxes;
    EnvironmentalSensorData_t xEnvSensorData;
    bool bMotionSensorValid;
    bool bEnvSensorDataValid;
}iotcU5IotTelemetry_t;



#endif /* APP_SENSOR_TELEMETRY_H_ */
