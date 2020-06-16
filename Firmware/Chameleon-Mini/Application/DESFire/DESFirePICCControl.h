/* 
 * DESFirePICCControl.h : 
 * Maxie D. Schmidt (github.com/maxieds)
 */ 

#ifndef __DESFIRE_PICC_CONTROL_H__
#define __DESFIRE_PICC_CONTROL_H__

#include "../../Configuration.h"

#include "DESFireFirmwareSettings.h" 
#include "DESFirePICCHeaderLayout.h"
#include "DESFireInstructions.h"
#include "DESFireApplicationDirectory.h"
#include "DESFireFile.h"
#include "DESFireCrypto.h"

/*
 * Internal state variables: 
 */

/* Cached data: flush to FRAM or relevant EEPROM addresses if changed */
extern DESFirePICCInfoType Picc;
extern DESFireAppDirType AppDir;

/* Cached app data */
extern SelectedAppCacheType SelectedApp;
extern SelectedFileCacheType SelectedFile;

typedef void (*TransferSourceFuncType)(BYTE *Buffer, BYTE Count);
typedef void (*TransferSinkFuncType)(BYTE *Buffer, BYTE Count);
typedef void (*TransferChecksumUpdateFuncType)(const BYTE *Buffer, BYTE Count);
typedef BYTE (*TransferChecksumFinalFuncType)(BYTE *Buffer);
typedef BYTE (*TransferEncryptFuncType)(BYTE *Buffer, BYTE Count);
typedef TransferStatus (*PiccToPcdTransferFilterFuncType)(BYTE *Buffer);
typedef BYTE (*PcdToPiccTransferFilterFuncType)(BYTE *Buffer, BYTE Count);

/* Stored transfer state for all transfers */
typedef union DESFIRE_FIRMWARE_PACKING {
    struct DESFIRE_FIRMWARE_ALIGNAT {
        BYTE NextIndex;
    } GetApplicationIds;
    struct DESFIRE_FIRMWARE_ALIGNAT {
        TransferChecksumUpdateFuncType UpdateFunc;
        TransferChecksumFinalFuncType FinalFunc;
        BYTE AvailablePlaintext;
        struct DESFIRE_FIRMWARE_ALIGNAT {
            BYTE               BlockBuffer[CRYPTO_MAX_BLOCK_SIZE];
            CryptoCBCFuncType  CryptoChecksumFunc;
            union {
                 SIZET CRCA;
                 UINT  CRC32;
                 UINT  CMAC[2];
            };
        } MACData;
    } Checksums;
    struct DESFIRE_FIRMWARE_ALIGNAT {
        SIZET BytesLeft;
        struct DESFIRE_FIRMWARE_ALIGNAT {
            TransferSourceFuncType Func;
            SIZET Pointer; /* in EEPROM */
        } Source;
        struct DESFIRE_FIRMWARE_ALIGNAT {
            BOOL FirstPaddingBitSet;
            TransferEncryptFuncType Func;
            BYTE AvailablePlaintext;
            BYTE BlockBuffer[CRYPTO_MAX_KEY_SIZE];
        } Encryption; 
    } ReadData;
    struct DESFIRE_FIRMWARE_ALIGNAT {
        SIZET BytesLeft;
        struct DESFIRE_FIRMWARE_ALIGNAT {
            TransferSinkFuncType Func;
            SIZET Pointer; /* in EEPROM */
        } Sink;
        struct DESFIRE_FIRMWARE_ALIGNAT {
            TransferEncryptFuncType Func;
            BYTE AvailablePlaintext;
            BYTE BlockBuffer[CRYPTO_MAX_BLOCK_SIZE];          
        } Encryption;
    } WriteData;
} TransferStateType;
extern TransferStateType TransferState;

/* Transfer routines */
void SyncronizePICCInfo(void);
TransferStatus PiccToPcdTransfer(uint8_t *Buffer);
uint8_t PcdToPiccTransfer(uint8_t *Buffer, uint8_t Count);

/* Setup routines */
uint8_t ReadDataFilterSetup(uint8_t CommSettings);
uint8_t WriteDataFilterSetup(uint8_t CommSettings);

/* PICC management */
void InitialisePiccBackendEV0(uint8_t StorageSize);
void InitialisePiccBackendEV1(uint8_t StorageSize);
void ResetPiccBackend(void);
bool IsEmulatingEV1(void);
void GetPiccHardwareVersionInfo(uint8_t* Buffer);
void GetPiccSoftwareVersionInfo(uint8_t* Buffer);
void GetPiccManufactureInfo(uint8_t* Buffer);
uint8_t GetPiccKeySettings(void);
void FormatPicc(void);
void CreatePiccApp(void);
void FactoryFormatPiccEV0(void);
void FactoryFormatPiccEV1(uint8_t StorageSize);
void GetPiccUid(ConfigurationUidType Uid);
void SetPiccUid(ConfigurationUidType Uid);

#endif
