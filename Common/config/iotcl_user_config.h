/*
 * iotcl_user_config.h
 *
 *  Created on: May 8, 2024
 *      Author: mgilhespie
 */

#ifndef COMMON_CONFIG_IOTCL_USER_CONFIG_H_
#define COMMON_CONFIG_IOTCL_USER_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <logging.h>


/* @brief 	Enable or Disable iot-connect logging using the IOTCL_ macros
 *
 * Remove the leading "//" comment marker from the lines below to disable logging
 * for that log level.
 */

// #define IOTCL_ERROR(err_code, ...)
// #define IOTCL_WARN(err_code, ...)
// #define IOTCL_INFO(...)

/* ------------------------------------------------------------------------ */

#ifndef IOTCL_ERROR
#define IOTCL_ERROR(err_code, ...) \
    do { \
        vLoggingPrintf2("ERR", __NAME_ARG__,__LINE__, err_code, __VA_ARGS__); \
    } while(0)
#endif

#ifndef IOTCL_WARN
#define IOTCL_WARN(err_code, ...) \
    do { \
        vLoggingPrintf2("WRN", __NAME_ARG__,__LINE__, err_code, __VA_ARGS__); \
    } while(0)
#endif

#ifndef IOTCL_INFO
#define IOTCL_INFO(...) \
    do { \
        vLoggingPrintf("INF", __NAME_ARG__,__LINE__, __VA_ARGS__); \
    } while(0)
#endif


#ifdef __cplusplus
}
#endif

#endif /* COMMON_CONFIG_IOTCL_USER_CONFIG_H_ */
