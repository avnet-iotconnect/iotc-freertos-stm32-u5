/*
 * iotconnect.c
 *
 *  Created on: Oct 23, 2023
 *      Author: mgilhespie
 */

#include "logging_levels.h"
#define LOG_LEVEL	LOG_DEBUG
#include "logging.h"

/* Standard library includes */

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

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

#include "core_http_client.h"
#include "transport_interface.h"

/* Transport interface implementation include header for TLS. */
#include "mbedtls_transport.h"

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
#include "iotconnect_discovery.h"
#include "iotconnect_certs.h"


/* Constants */
#define HTTPS_PORT				443
//#define ROOT_CA_CERT_PATH    	"certificates/AmazonRootCA1.crt"
#define democonfigDISABLE_SNI                ( pdFALSE )

#define DISCOVERY_SERVER_HOST	"awsdiscovery.iotconnect.io"


/* Variables */
static char *ca_certs = CERT_GODADDY_ROOT_CA;

static HTTPResponse_t response = { 0 };
static TransportInterface_t transportInterface = { 0 };
static HTTPRequestHeaders_t requestHeaders = { 0 };
static HTTPClient_ResponseHeaderParsingCallback_t headerParsingCallback = { 0 };
char discovery_method_path[256];
char identity_method_path[256];

// Buffers for HTTP request and response
char request_buffer[4096] = {0};
char response_buffer[4096] = {0};		// FIXME: Response size has fluctuated, giving errors, insufficient space, increased to 4KB.

// IoT Connect configuration
static IotConnectClientConfig config = { 0 };
static IotclConfig lib_config = { 0 };
static IotConnectAwsrtosConfig awsrtos_config = { 0 };
static IotclSyncResult last_sync_result = IOTCL_SR_UNKNOWN_DEVICE_STATUS;

static uint32_t ulGlobalEntryTimeTicks;			// Timer epoch in ticks since start of a HTTP_Send


// Prototypes
#ifndef pdTICKS_TO_MS
    #define pdTICKS_TO_MS( xTicks )       ( ( TickType_t ) ( ( uint64_t ) ( xTicks ) * 1000 / configTICK_RATE_HZ ) )
#endif

static int iotconnect_discovery_and_identity(const char *cpid, const char *env, const char *device_id);
int32_t send_http_request( const char *server_host, int port,
		                        const char *method, const char *path,
								char *response_buffer, size_t response_buffer_sz, char **ppBody);
static BaseType_t prvConnectToServer( NetworkContext_t * pxNetworkContext, const char *base_url);
static NetworkContext_t *configure_transport(void);
static uint32_t prvGetTimeMs( void );


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

    LogInfo("iotconnect_sdk_init");
    vTaskDelay(200);

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

    LogInfo(("IOTC: ENV:  %s\r\n", config.env));

#if 0
    awsmqtt_config.device_name = sync_response->broker.client_id;
    awsmqtt_config.host = sync_response->broker.host;
    awsmqtt_config.auth = &config.auth;
    awsmqtt_config.status_cb = on_iotconnect_status;
#endif

#if 0
    lib_config.device.env = config.env;
    lib_config.device.cpid = config.cpid;
    lib_config.device.duid = config.duid;
#else
    lib_config.device.env = "poc";
    lib_config.device.cpid = "97FF86E8728645E9B89F7B07977E4B15";
    lib_config.device.duid = "mgilhdev02caci";
#endif

    LogInfo("discovery_and_identity");
    vTaskDelay(200);

    iotconnect_discovery_and_identity(lib_config.device.cpid, lib_config.device.env,  lib_config.device.duid);

    /*
     * FIXME: The telemetry cd and dtg values are passed by the IotConnectAwsrtosConfig.  These may be
     * removed and obtained by the to be implemented discovery/sync code.
     */
	lib_config.telemetry.cd = ac->telemetry_cd;
	lib_config.telemetry.dtg = ac->telemetry_dtg;

    lib_config.event_functions.ota_cb = config.ota_cb;
    lib_config.event_functions.cmd_cb = config.cmd_cb;
    lib_config.event_functions.msg_cb = config.msg_cb;

    LogInfo("iotcl_init_v2");
    vTaskDelay(200);

    // Initialize the iotc-c-lib for awsformat2.1
    if (!iotcl_init_v2(&lib_config)) {
        LogError(("IOTC: Failed to initialize the IoTConnect C Lib"));
        return -1;
    }

    LogInfo(("IOTC: Initializing the mqtt connection"));
    ret = awsmqtt_client_init(&awsmqtt_config, &awsrtos_config);
    if (ret) {
        LogError(("IOTC: Failed to connect to mqtt server"));
    	return ret;
    }

    return ret;
}


/**
 * @brief	Publish a message on the event topic
 */
void iotconnect_sdk_send_packet(const char *data) {
    if (awsmqtt_send_message(data) != 0) {
        LogError(("IOTC: Failed to send message %s\r\n", data));
    }
}


/* @brief	Send a discovery and identity HTTP Get request to populate config fields.
 *
 * SEE SOURCES    https://github.com/aws/aws-iot-device-sdk-embedded-C/blob/main/demos/http/http_demo_plaintext/http_demo_plaintext.c
 *
 * Or split into two functions as azure-rtos does
 */
static int iotconnect_discovery_and_identity(const char *cpid, const char *env, const char *device_id)
{
	HTTPStatus_t returnStatus = HTTPSuccess;
	char *response_body = NULL;

	LogInfo ("iotconnect_discovery_and_identity");
	LogInfo("cpid=%s", cpid);
	LogInfo("env=%s", env);
	LogInfo("device_id=%s", device_id);

    const char *discovery_server_host = DISCOVERY_SERVER_HOST;

    snprintf (discovery_method_path, sizeof discovery_method_path, "/api/v2.1/dsdk/cpId/%s/env/%s", cpid, env);

	returnStatus = send_http_request( discovery_server_host, HTTPS_PORT,
			                        "GET", discovery_method_path,
									response_buffer, sizeof response_buffer, &response_body);

    if (returnStatus != HTTPSuccess) {
    	LogError(("Failed the discovery HTTP Get request"));
		return -1;
    }

    IotclDiscoveryResponse *discovery_ret = iotcl_discovery_parse_discovery_response(response_body);

    if (discovery_ret == NULL) {
    	LogError(("Failed to parse discovery response"));
    	return -1;
    }

    if (discovery_ret->host == NULL || discovery_ret->path == NULL) {
    	LogError(("Discovery response did not return host or method path"));
    	return -1;
    }

    snprintf (identity_method_path, sizeof identity_method_path, "%s/uid/%s", discovery_ret->path, device_id);

	returnStatus = send_http_request(discovery_ret->host, HTTPS_PORT,
			                        "GET", identity_method_path,
									response_buffer, sizeof response_buffer, &response_body);

	IotclSyncResponse *sync_ret = iotcl_discovery_parse_sync_response(response_body);

	if (sync_ret == NULL) {
		LogError(("Failed to parse sync response"));
		return -1;
	}

	if (sync_ret->broker.host != NULL) {
		LogInfo ("response : host: %s", sync_ret->broker.host);
	} else {
		LogError ("response no broker.host");
	}

	LogInfo ("response : port: %d", sync_ret->broker.port);

	if (sync_ret->cd != NULL) {
		LogInfo ("response : telemetry cd: %s", sync_ret->cd);
	} else {
		LogError ("response no telemetry cd");
	}

    // save telemetry "cd"
    // Save "h" or "un"
	// Save port


	// Save "c2d" address for subscribe
    // Still need to hard-code publish

	return 0;
}


/*
 *
 */
void vSetGlobalEntryTimeInTicks(void)
{
	ulGlobalEntryTimeTicks = ( uint32_t )  xTaskGetTickCount();
}


/*
 *
 */
static uint32_t prvGetTimeMs( void )
{
    uint32_t ulTimeMs = 0UL;

    /* Determine the elapsed time in the application */
    ulTimeMs = ( uint32_t ) pdTICKS_TO_MS( (uint32_t)xTaskGetTickCount() - ulGlobalEntryTimeTicks);

    return ulTimeMs;
}


/*
 *
 */
int32_t send_http_request( const char *server_host, int port,
		                        const char *method, const char *path,
								char *user_buffer, size_t user_buffer_len, char **ppBody)
{
//	LogInfo ("send_http_request, %s : %s : %s\n", server_host, method, path);
//	vTaskDelay(400);

	int32_t returnStatus = EXIT_SUCCESS;

    /* Network connection context. This is an opaque object that actually points to an mbedTLS structure that
     * manages the TLS connection */
    NetworkContext_t *pNetworkContext;
    /* Configurations of the initial request headers that are passed to #HTTPClient_InitializeRequestHeaders. */
    HTTPRequestInfo_t requestInfo;
    /* Represents a response returned from an HTTP server. */
    HTTPResponse_t response;
    /* Represents header data that will be sent in an HTTP request. */
    HTTPRequestHeaders_t requestHeaders;

    /* Return value of all methods from the HTTP Client library API. */
    HTTPStatus_t httpStatus = HTTPSuccess;

    assert( method != NULL );
    assert( path != NULL );

    pNetworkContext = configure_transport();

    if (pNetworkContext == NULL) {
    	LogError("failed to configure network context");
    	vTaskDelay(200);
    	return EXIT_FAILURE;
    }

    returnStatus = prvConnectToServer( pNetworkContext, server_host );

    if (returnStatus != pdPASS) {
    	LogError("Failed to connect to HTTPS server :5s", server_host);
    	return EXIT_FAILURE;
    }

    /* Initialize all HTTP Client library API structs to 0. */
    ( void ) memset( &transportInterface, 0, sizeof( transportInterface ) );
    ( void ) memset( &requestInfo, 0, sizeof( requestInfo ) );
    ( void ) memset( &requestHeaders, 0, sizeof( requestHeaders ) );
    ( void ) memset( &response, 0, sizeof( response ) );

    transportInterface.recv = mbedtls_transport_recv;
    transportInterface.send = mbedtls_transport_send;
    transportInterface.writev = NULL;
    transportInterface.pNetworkContext = pNetworkContext;

    /* Initialize the request object. */
    requestInfo.pHost = server_host;
    requestInfo.hostLen = strlen(server_host);

    requestInfo.pMethod = method;
    requestInfo.methodLen = strlen(method);
    requestInfo.pPath = path;
    requestInfo.pathLen = strlen(path);

    /* Set "Connection" HTTP header to "keep-alive" so that multiple requests
     * can be sent over the same established TCP connection. */
    requestInfo.reqFlags = HTTP_REQUEST_KEEP_ALIVE_FLAG;

    /* Set the buffer used for storing request headers. */
    requestHeaders.pBuffer = request_buffer;
    requestHeaders.bufferLen = sizeof request_buffer -1;

    // We save the current time here so that timeouts in HTTP_Send are relative to it.
    vSetGlobalEntryTimeInTicks();

    /* Set time function for retry timeout on receiving the response. */
    response.getTime = prvGetTimeMs;
	response.pBuffer = (uint8_t *)response_buffer;
    response.bufferLen = sizeof response_buffer -1;

    httpStatus = HTTPClient_InitializeRequestHeaders( &requestHeaders,
                                                      &requestInfo );

    if( httpStatus == HTTPSuccess )
    {
        LogInfo( ("Sending HTTPS %s request to %s %s...",
                   requestInfo.pMethod,
                   server_host,
                   requestInfo.pPath) );

        LogInfo("requestHeaders: %s", requestHeaders.pBuffer);

        /* Send the request and receive the response. */
        httpStatus = HTTPClient_Send( &transportInterface,
                                      &requestHeaders,
                                      ( uint8_t * ) "\r\n",
                                      2,
                                      &response,
                                      0 );


        LogInfo("HTTPClient_Send complete");
        vTaskDelay(300);

    }
    else
    {
        LogError( ( "Failed to initialize HTTP request headers: Error=%s.",
                    HTTPClient_strerror( httpStatus ) ) );
    }

    if( httpStatus == HTTPSuccess )
    {
    	*ppBody = response.pBody;

    	LogInfo("\n\n... printing HTTP response ...\r\n");
        LogInfo( ("Received HTTP response from %s %s...\r\n", server_host, requestInfo.pPath));
        LogInfo( ("Response Headers:\r\n%s\r\n", response.pHeaders));
        LogInfo( ("Response Status:\r\n%u\r\n", response.statusCode));
    	LogInfo( ("Response Body ptr: %08x\n", (uint32_t)response.pBody));
        LogInfo( ("Response Body:\n%s\n", response.pBody));
    	LogInfo("\n\n... finished printing HTTP response ...\n\n");
    }
    else
    {
    	*ppBody = NULL;

        LogError(( "Failed to send HTTP %s request to %s %s: Error=%s.",
                    requestInfo.pMethod,
                    server_host,
                    requestInfo.pPath,
                    HTTPClient_strerror( httpStatus)) );
    }

    if( httpStatus != HTTPSuccess )
    {
        returnStatus = EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}


/*
 *
 */
static BaseType_t prvConnectToServer( NetworkContext_t *pxNetworkContext, const char *server_addr)
{
    BaseType_t xStatus = pdPASS;

    TlsTransportStatus_t xNetworkStatus;

    LogInfo("prvConnectToServer : %s", server_addr);
    vTaskDelay(100);

    xNetworkStatus = mbedtls_transport_connect( pxNetworkContext,
                                            server_addr,
                                            HTTPS_PORT,
                                            0, 0 );

    xStatus = ( xNetworkStatus == TLS_TRANSPORT_SUCCESS ) ? pdPASS : pdFAIL;

    return xStatus;
}


/*
 *
 */
static NetworkContext_t *configure_transport(void)
{
    MQTTStatus_t xMQTTStatus = MQTTSuccess;
    TlsTransportStatus_t xTlsStatus = TLS_TRANSPORT_CONNECT_FAILURE;
    BaseType_t xExitFlag = pdFALSE;
    NetworkContext_t *pxNetworkContext;
    PkiObject_t pxRootCaChain[1];

    pxRootCaChain[0] = xPkiObjectFromLabel( TLS_GODADDY_CA_CERT_LABEL );

    if (pxRootCaChain[0].xForm == OBJ_FORM_NONE || pxRootCaChain[0].uxLen == 0) {
    	LogError("godaddy_ca_cert not set");
    	return NULL;
    }

	pxNetworkContext = mbedtls_transport_allocate();

	if( pxNetworkContext == NULL )
	{
		LogError( "Failed to allocate an mbedtls transport context." );
		xMQTTStatus = MQTTNoMemory;
		return NULL;
	}

    if( xMQTTStatus == MQTTSuccess )
    {
        xTlsStatus = mbedtls_transport_configure( pxNetworkContext,
                                                  NULL,					//pcAlpnProtocols,
                                                  NULL,
                                                  NULL,
												  pxRootCaChain,
                                                  1 );

        if( xTlsStatus != TLS_TRANSPORT_SUCCESS )
        {
            LogError( "Failed to configure mbedtls transport." );
            xMQTTStatus = MQTTBadParameter;

            mbedtls_transport_free(pxNetworkContext);
            return NULL;
        }
    }

   return pxNetworkContext;
}
