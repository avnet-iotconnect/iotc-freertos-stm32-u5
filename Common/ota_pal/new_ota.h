/*
 * new_ota.h
 *
 *  Created on: Feb 6, 2024
 *      Author: mgilhespie
 */

#ifndef COMMON_OTA_PAL_NEW_OTA_H_
#define COMMON_OTA_PAL_NEW_OTA_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef uint32_t   OtaPalStatus_t;
typedef uint32_t   OtaPalMainStatus_t;


#define    OtaPalSuccess                 0x0U   /*!< @brief OTA platform interface success. */
#define    OtaPalUninitialized           0xe0U  /*!< @brief Result is not yet initialized from PAL. */
#define    OtaPalOutOfMemory             0xe1U  /*!< @brief Out of memory. */
#define    OtaPalNullFileContext         0xe2U  /*!< @brief The PAL is called with a NULL file context. */
#define    OtaPalSignatureCheckFailed    0xe3U  /*!< @brief The signature check failed for the specified file. */
#define    OtaPalRxFileCreateFailed      0xe4U  /*!< @brief The PAL failed to create the OTA receive file. */
#define    OtaPalRxFileTooLarge          0xe5U  /*!< @brief The OTA receive file is too big for the platform to support. */
#define    OtaPalBootInfoCreateFailed    0xe6U  /*!< @brief The PAL failed to create the OTA boot info file. */
#define    OtaPalBadSignerCert           0xe7U  /*!< @brief The signer certificate was not readable or zero length. */
#define    OtaPalBadImageState           0xe8U  /*!< @brief The specified OTA image state was out of range. */
#define    OtaPalAbortFailed             0xe9U  /*!< @brief Error trying to abort the OTA. */
#define    OtaPalRejectFailed            0xeaU  /*!< @brief Error trying to reject the OTA image. */
#define    OtaPalCommitFailed            0xebU  /*!< @brief The acceptance commit of the new OTA image failed. */
#define    OtaPalActivateFailed          0xecU  /*!< @brief The activation of the new OTA image failed. */
#define    OtaPalFileAbort               0xedU  /*!< @brief Error in low level file abort. */
#define    OtaPalFileClose               0xeeU  /*!< @brief Error in low level file close. */

typedef enum OtaImageState
{
    OtaImageStateUnknown = 0,  /*!< @brief The initial state of the OTA MCU Image. */
    OtaImageStateTesting = 1,  /*!< @brief The state of the OTA MCU Image post successful download and reboot. */
    OtaImageStateAccepted = 2, /*!< @brief The state of the OTA MCU Image post successful download and successful self_test. */
    OtaImageStateRejected = 3, /*!< @brief The state of the OTA MCU Image when the job has been rejected. */
    OtaImageStateAborted = 4,  /*!< @brief The state of the OTA MCU Image after a timeout publish to the stream request fails.
                                *   Also if the OTA MCU image is aborted in the middle of a stream. */
    OtaLastImageState = OtaImageStateAborted
} OtaImageState_t;

#if 0
typedef enum OtaPalImageState
{
    OtaPalImageStateUnknown = 0,   /*!< @brief The initial state of the OTA PAL Image. */
    OtaPalImageStatePendingCommit, /*!< @brief OTA PAL Image awaiting update. */
    OtaPalImageStateValid,         /*!< @brief OTA PAL Image is valid. */
    OtaPalImageStateInvalid        /*!< @brief OTA PAL Image is invalid. */
} OtaPalImageState_t;
#endif

#define kOTA_MaxSignatureSize           384 /* Max bytes supported for a file signature (3072 bit RSA is 384 bytes). */


typedef struct
{
    uint16_t size;                         /*!< @brief Size, in bytes, of the signature. */
    uint8_t data[ kOTA_MaxSignatureSize ]; /*!< @brief The binary signature data. */
} Sig_t;


/**
 * @brief Data type to represent a file.
 *
 * It is used to represent a file received via OTA. The file is declared as
 * the pointer of this type: otaconfigOTA_FILE_TYPE * pFile.
 *
 * <b>Possible values:</b> Any data type. <br>
 * <b>Default value:</b> FILE on Windows or Linux, uint8_t on other platforms.
 */
#ifndef otaconfigOTA_FILE_TYPE
    #if defined( WIN32 ) || defined( __linux__ )
        #define otaconfigOTA_FILE_TYPE    FILE
    #else
        #define otaconfigOTA_FILE_TYPE    uint8_t
    #endif
#endif


typedef struct OtaFileContext
{
    uint8_t * pFilePath;            /*!< @brief Update file pathname. */
    uint16_t filePathMaxSize;       /*!< @brief Maximum size of the update file path */
    otaconfigOTA_FILE_TYPE * pFile; /*!< @brief File type after file is open for write. */
    uint32_t fileSize;              /*!< @brief The size of the file in bytes. */
    uint32_t blocksRemaining;       /*!< @brief How many blocks remain to be received (a code optimization). */
    uint32_t fileAttributes;        /*!< @brief Flags specific to the file being received (e.g. secure, bundle, archive). */
    uint32_t serverFileID;          /*!< @brief The file is referenced by this numeric ID in the OTA job. */
    uint8_t * pJobName;             /*!< @brief The job name associated with this file from the job service. */
    uint16_t jobNameMaxSize;        /*!< @brief Maximum size of the job name. */
    uint8_t * pStreamName;          /*!< @brief The stream associated with this file from the OTA service. */
    uint16_t streamNameMaxSize;     /*!< @brief Maximum size of the stream name. */
    uint8_t * pRxBlockBitmap;       /*!< @brief Bitmap of blocks received (for deduplicating and missing block request). */
    uint16_t blockBitmapMaxSize;    /*!< @brief Maximum size of the block bitmap. */
    uint8_t * pCertFilepath;        /*!< @brief Pathname of the certificate file used to validate the receive file. */
    uint16_t certFilePathMaxSize;   /*!< @brief Maximum certificate path size. */
    uint8_t * pUpdateUrlPath;       /*!< @brief Url for the file. */
    uint16_t updateUrlMaxSize;      /*!< @brief Maximum size of the url. */
    uint8_t * pAuthScheme;          /*!< @brief Authorization scheme. */
    uint16_t authSchemeMaxSize;     /*!< @brief Maximum size of the auth scheme. */
    uint32_t updaterVersion;        /*!< @brief Used by OTA self-test detection, the version of Firmware that did the update. */
    bool isInSelfTest;              /*!< @brief True if the job is in self test mode. */
    uint8_t * pProtocols;           /*!< @brief Authorization scheme. */
    uint16_t protocolMaxSize;       /*!< @brief Maximum size of the  supported protocols string. */
    uint8_t * pDecodeMem;           /*!< @brief Decode memory. */
    uint32_t decodeMemMaxSize;      /*!< @brief Maximum size of the decode memory. */
    uint32_t fileType;              /*!< @brief The file type id set when creating the OTA job. */
    Sig_t * pSignature;             /*!< @brief Pointer to the file's signature structure. */
} OtaFileContext_t;



/* @[define_ota_err_code_helpers] */
#define OTA_PAL_ERR_MASK    0xffffffUL                                                                                                           /*!< The PAL layer uses the signed low 24 bits of the OTA error code. */
#define OTA_PAL_SUB_BITS    24U                                                                                                                  /*!< The OTA Agent error code is the highest 8 bits of the word. */
#define OTA_PAL_MAIN_ERR( err )             ( ( OtaPalMainStatus_t ) ( uint32_t ) ( ( uint32_t ) ( err ) >> ( uint32_t ) OTA_PAL_SUB_BITS ) )    /*!< Helper to get the OTA PAL main error code. */
#define OTA_PAL_SUB_ERR( err )              ( ( ( uint32_t ) ( err ) ) & ( ( uint32_t ) OTA_PAL_ERR_MASK ) )                                     /*!< Helper to get the OTA PAL sub error code. */
#define OTA_PAL_COMBINE_ERR( main, sub )    ( ( ( uint32_t ) ( main ) << ( uint32_t ) OTA_PAL_SUB_BITS ) | ( uint32_t ) OTA_PAL_SUB_ERR( sub ) ) /*!< Helper to combine the OTA PAL main and sub error code. */
/* @[define_ota_err_code_helpers] */


#endif /* COMMON_OTA_PAL_NEW_OTA_H_ */
