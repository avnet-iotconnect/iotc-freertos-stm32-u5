/*
 * iotconnect_config.h
 *
 *  Created on: Nov 3, 2023
 *      Author: mgilhespie
 */

#ifndef CONFIG_IOTCONNECT_CONFIG_H_
#define CONFIG_IOTCONNECT_CONFIG_H_

/* @brief Enable discovery and sync instead of providing mqtt endpoint and telemetry cd settings
 */
#define IOTCONFIG_USE_DISCOVERY_SYNC
#define IOTCONFIG_ENABLE_OTA

/* Discovery host used when platform is set to AWS. */
#define IOTCONFIG_DISCOVERY_HOST_AWS    "discovery.iotconnect.io"

#endif /* CONFIG_IOTCONNECT_CONFIG_H_ */
