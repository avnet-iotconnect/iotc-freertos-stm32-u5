/*
 * FreeRTOS V202107.00
 * Copyright (C) 2021 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
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
 * http://aws.amazon.com/freertos
 * http://www.FreeRTOS.org
 */

/**
 * @file  ota_pal.h
 * @brief Function declarations for the functions in ota_pal.c
 */

#ifndef OTA_PAL_H_
#define OTA_PAL_H_



#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/*
 * Types used by OTA APIs
 */
typedef uint32_t   OtaPalStatus_t;
typedef uint32_t   OtaPalMainStatus_t;

/*
 * @brief 	Constants for values of OtaPalStatus_t and OtaPalMainStatus_t
 */
typedef enum OtaPalStatusValues
{
	OtaPalSuccess                = 0,   /*!< @brief OTA platform interface success. */
	OtaPalUninitialized          = 1,   /*!< @brief Result is not yet initialized from PAL. */
	OtaPalOutOfMemory            = 2,   /*!< @brief Out of memory. */
	OtaPalNullFileContext        = 3,   /*!< @brief The PAL is called with a NULL file context. */
	OtaPalSignatureCheckFailed   = 4,   /*!< @brief The signature check failed for the specified file. */
	OtaPalRxFileCreateFailed     = 5,   /*!< @brief The PAL failed to create the OTA receive file. */
	OtaPalRxFileTooLarge         = 6,   /*!< @brief The OTA receive file is too big for the platform to support. */
	OtaPalBootInfoCreateFailed   = 7,   /*!< @brief The PAL failed to create the OTA boot info file. */
	OtaPalBadSignerCert          = 8,   /*!< @brief The signer certificate was not readable or zero length. */
	OtaPalBadImageState          = 9,   /*!< @brief The specified OTA image state was out of range. */
	OtaPalAbortFailed            = 10,  /*!< @brief Error trying to abort the OTA. */
	OtaPalRejectFailed           = 11,  /*!< @brief Error trying to reject the OTA image. */
	OtaPalCommitFailed           = 12,  /*!< @brief The acceptance commit of the new OTA image failed. */
	OtaPalActivateFailed         = 13,  /*!< @brief The activation of the new OTA image failed. */
	OtaPalFileAbort              = 14,  /*!< @brief Error in low level file abort. */
	OtaPalFileClose              = 15,  /*!< @brief Error in low level file close. */
};

/*
 * @brief	Constant of the signature size
 */
#define kOTA_MaxSignatureSize           384 /* Max bytes supported for a file signature (3072 bit RSA is 384 bytes). */

/*
 * @brief	Constants representing the state machine of the OTA update code
 */
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


/*
 * @brief	Data type for a signature in the binary data
 */
typedef struct
{
    uint16_t size;                         /*!< @brief Size, in bytes, of the signature. */
    uint8_t data[ kOTA_MaxSignatureSize ]; /*!< @brief The binary signature data. */
} Sig_t;

/*
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

/*
 * @brief	Structure for maintaining the downloading and update of OTA image
 */
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


/*
 * @brief	OTA helper macros
 */
#define OTA_PAL_ERR_MASK    0xffffffUL                                                                                                           /*!< The PAL layer uses the signed low 24 bits of the OTA error code. */
#define OTA_PAL_SUB_BITS    24U                                                                                                                  /*!< The OTA Agent error code is the highest 8 bits of the word. */
#define OTA_PAL_MAIN_ERR( err )             ( ( OtaPalMainStatus_t ) ( uint32_t ) ( ( uint32_t ) ( err ) >> ( uint32_t ) OTA_PAL_SUB_BITS ) )    /*!< Helper to get the OTA PAL main error code. */
#define OTA_PAL_SUB_ERR( err )              ( ( ( uint32_t ) ( err ) ) & ( ( uint32_t ) OTA_PAL_ERR_MASK ) )                                     /*!< Helper to get the OTA PAL sub error code. */
#define OTA_PAL_COMBINE_ERR( main, sub )    ( ( ( uint32_t ) ( main ) << ( uint32_t ) OTA_PAL_SUB_BITS ) | ( uint32_t ) OTA_PAL_SUB_ERR( sub ) ) /*!< Helper to combine the OTA PAL main and sub error code. */


/**
 * @brief Create a new receive file.
 *
 * @note Opens the file indicated in the OTA file context in the MCU file system.
 *
 * @note The previous image may be present in the designated image download partition or file, so the
 * partition or file must be completely erased or overwritten in this routine.
 *
 * @note The input OtaFileContext_t pFileContext is checked for NULL by the OTA agent before this
 * function is called.
 * The device file path is a required field in the OTA job document, so pFileContext->pFilePath is
 * checked for NULL by the OTA agent before this function is called.
 *
 * @param[in] pFileContext OTA file context information.
 *
 * @return The OtaPalStatus_t error code is a combination of the main OTA PAL interface error and
 *         the MCU specific sub error code. See ota_platform_interface.h for the OtaPalMainStatus_t
 *         error codes and your specific PAL implementation for the sub error code.
 *
 * Major error codes returned are:
 *
 *   OtaPalSuccess: File creation was successful.
 *   OtaPalRxFileTooLarge: The OTA receive file is too big for the platform to support.
 *   OtaPalBootInfoCreateFailed: The bootloader information file creation failed.
 *   OtaPalRxFileCreateFailed: Returned for other errors creating the file in the device's
 *                             non-volatile memory. If this error is returned, then the sub error
 *                             should be set to the appropriate platform specific value.
 */
OtaPalStatus_t otaPal_CreateFileForRx( OtaFileContext_t * const pFileContext );

/**
 * @brief Authenticate and close the underlying receive file in the specified OTA context.
 *
 * @note The input OtaFileContext_t pFileContext is checked for NULL by the OTA agent before this
 * function is called. This function is called only at the end of block ingestion.
 * otaPAL_CreateFileForRx() must succeed before this function is reached, so
 * pFileContext->fileHandle(or pFileContext->pFile) is never NULL.
 * The file signature key is required job document field in the OTA Agent, so pFileContext->pSignature will
 * never be NULL.
 *
 * If the signature verification fails, file close should still be attempted.
 *
 * @param[in] pFileContext OTA file context information.
 *
 * @return The OtaPalStatus_t error code is a combination of the main OTA PAL interface error and
 *         the MCU specific sub error code. See ota_platform_interface.h for the OtaPalMainStatus_t
 *         error codes and your specific PAL implementation for the sub error code.
 *
 * Major error codes returned are:
 *
 *   OtaPalSuccess on success.
 *   OtaPalSignatureCheckFailed: The signature check failed for the specified file.
 *   OtaPalBadSignerCert: The signer certificate was not readable or zero length.
 *   OtaPalFileClose: Error in low level file close.
 */
OtaPalStatus_t otaPal_CloseFile( OtaFileContext_t * const pFileContext );

/**
 * @brief Write a block of data to the specified file at the given offset.
 *
 * @note The input OtaFileContext_t pFileContext is checked for NULL by the OTA agent before this
 * function is called.
 * The file pointer/handle pFileContext->pFile, is checked for NULL by the OTA agent before this
 * function is called.
 * pData is checked for NULL by the OTA agent before this function is called.
 * blockSize is validated for range by the OTA agent before this function is called.
 * offset is validated by the OTA agent before this function is called.
 *
 * @param[in] pFileContext OTA file context information.
 * @param[in] ulOffset Byte offset to write to from the beginning of the file.
 * @param[in] pData Pointer to the byte array of data to write.
 * @param[in] ulBlockSize The number of bytes to write.
 *
 * @return The number of bytes written successfully, or a negative error code from the platform
 * abstraction layer.
 */
int16_t otaPal_WriteBlock( OtaFileContext_t * const pFileContext,
                           uint32_t ulOffset,
                           uint8_t * const pData,
                           uint32_t ulBlockSize );

/**
 * @brief Activate the newest MCU image received via OTA.
 *
 * This function shall take necessary actions to activate the newest MCU
 * firmware received via OTA. It is typically just a reset of the device.
 *
 * @note This function SHOULD NOT return. If it does, the platform does not support
 * an automatic reset or an error occurred.
 *
 * @param[in] pFileContext OTA file context information.
 *
 * @return The OtaPalStatus_t error code is a combination of the main OTA PAL interface error and
 *         the MCU specific sub error code. See ota_platform_interface.h for the OtaPalMainStatus_t
 *         error codes and your specific PAL implementation for the sub error code.
 *
 * Major error codes returned are:
 *
 *   OtaPalSuccess on success.
 *   OtaPalActivateFailed: The activation of the new OTA image failed.
 */
OtaPalStatus_t otaPal_ActivateNewImage( OtaFileContext_t * const pFileContext );

/**
 * @brief Attempt to set the state of the OTA update image.
 *
 * Take required actions on the platform to Accept/Reject the OTA update image (or bundle).
 * Refer to the PAL implementation to determine what happens on your platform.
 *
 * @param[in] pFileContext File context of type OtaFileContext_t.
 * @param[in] eState The desired state of the OTA update image.
 *
 * @return The OtaPalStatus_t error code is a combination of the main OTA PAL interface error and
 *         the MCU specific sub error code. See ota_platform_interface.h for the OtaPalMainStatus_t
 *         error codes and your specific PAL implementation for the sub error code.
 *
 * Major error codes returned are:
 *
 *   OtaPalSuccess on success.
 *   OtaPalBadImageState: if you specify an invalid OtaImageState_t. No sub error code.
 *   OtaPalAbortFailed: failed to roll back the update image as requested by OtaImageStateAborted.
 *   OtaPalRejectFailed: failed to roll back the update image as requested by OtaImageStateRejected.
 *   OtaPalCommitFailed: failed to make the update image permanent as requested by OtaImageStateAccepted.
 */
OtaPalStatus_t otaPal_SetPlatformImageState( OtaFileContext_t * const pFileContext,
                                             OtaImageState_t eState );

/**
 * @brief Reset the device.
 *
 * This function shall reset the MCU and cause a reboot of the system.
 *
 * @note This function SHOULD NOT return. If it does, the platform does not support
 * an automatic reset or an error occurred.
 *
 * @param[in] pFileContext OTA file context information.
 *
 * @return The OtaPalStatus_t error code is a combination of the main OTA PAL interface error and
 *         the MCU specific sub error code. See ota_platform_interface.h for the OtaPalMainStatus_t
 *         error codes and your specific PAL implementation for the sub error code.
 */
OtaPalStatus_t otaPal_ResetDevice( OtaFileContext_t * const pFileContext );


/*
 * @brief	Accept a new firmware image as valid and working
 */
void otaPal_AcceptImage(void);

/*
 * @brief	Reject a new firmware iamge and roll back to previous image
 */
void otaPal_RejectImage(void);



#endif /* ifndef OTA_PAL_H_ */
