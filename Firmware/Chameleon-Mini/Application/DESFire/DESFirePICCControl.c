/* 
 * DESFirePICCControl.c 
 * Maxie D. Schmidt (github.com/maxieds)
 */

#include "DESFirePICCControl.h"
#include "DESFirePICCHeaderLayout.h"
#include "DESFireApplicationDirectory.h"
#include "DESFireStatusCodes.h"
#include "DESFireISO14443Support.h"
#include "DESFireMemoryOperations.h"
#include "DESFireUtils.h"

const BYTE PICC_FORMATTED_MARKER[] = { 
     0xf0, 0x12, 0x34 
};
const BYTE DefaultDESFireATS[] = { 
     0x06, 0x75, 0x77, 0x81, 0x02, 0x80 
};
const BYTE DefaultJCOPDESFireATS[] = { 
     0x06, 0x75, 0xf7, 0xb1, 0x02, 0x80 
};

SIZET CardCapacityBlocks = 0;
DESFirePICCInfoType Picc = { 0 };
DESFireAppDirType AppDir = { 0 };
SelectedAppCacheType SelectedApp = { 0 };
SelectedFileCacheType SelectedFile = { 0 };
TransferStateType TransferState = { 0 };

DesfireStateType DesfireState = DESFIRE_IDLE;
uint8_t AuthenticatedWithKey = 0x00;

/* Transfer routines */

TransferStatus PiccToPcdTransfer(uint8_t* Buffer)
{
    TransferStatus Status;
    uint8_t XferBytes;

    /* Only read if required */
    if (TransferState.ReadData.BytesLeft) {
        /* Figure out how much to read */
        XferBytes = (uint8_t)TransferState.ReadData.BytesLeft;
        if (TransferState.ReadData.BytesLeft > DESFIRE_MAX_PAYLOAD_TDEA_BLOCKS * CRYPTO_DES_BLOCK_SIZE) {
            XferBytes = DESFIRE_MAX_PAYLOAD_TDEA_BLOCKS * CRYPTO_DES_BLOCK_SIZE;
        }
        /* Read input bytes */
        TransferState.ReadData.Source.Func(Buffer, XferBytes);
        TransferState.ReadData.BytesLeft -= XferBytes;
        /* Update checksum/MAC */
        if (TransferState.Checksums.UpdateFunc)
            TransferState.Checksums.UpdateFunc(Buffer, XferBytes);
        if (TransferState.ReadData.BytesLeft == 0) {
            /* Finalise TransferChecksum and append the checksum */
            if (TransferState.Checksums.FinalFunc)
                XferBytes += TransferState.Checksums.FinalFunc(&Buffer[XferBytes]);
        }
        /* Encrypt */
        Status.BytesProcessed = TransferState.ReadData.Encryption.Func(Buffer, XferBytes);
        Status.IsComplete = TransferState.ReadData.Encryption.AvailablePlaintext == 0;
    }
    else {
        /* Final encryption block */
        Status.BytesProcessed = TransferState.ReadData.Encryption.Func(Buffer, 0);
        Status.IsComplete = true;
    }

    return Status;
}

uint8_t PcdToPiccTransfer(uint8_t* Buffer, uint8_t Count)
{
     // TODO: Setup logging ... 
     return STATUS_OPERATION_OK;
}

/* Setup routines */

uint8_t ReadDataFilterSetup(uint8_t CommSettings)
{
    memset(&TransferState, 0, sizeof(TransferState));

    switch (CommSettings) {
    case DESFIRE_COMMS_PLAINTEXT:
        break;

    case DESFIRE_COMMS_PLAINTEXT_MAC:
        TransferState.Checksums.UpdateFunc = &TransferChecksumUpdateMACTDEA;
        TransferState.Checksums.FinalFunc = &TransferChecksumFinalMACTDEA;
        TransferState.Checksums.MAC.MACFunc = &CryptoEncrypt2KTDEA_CBCSend;
        memset(SessionIV.LegacyTransferIV, 0, sizeof(SessionIV.LegacyTransferIV));
        break;

    case DESFIRE_COMMS_CIPHERTEXT_DES:
        TransferState.Checksums.UpdateFunc = &TransferChecksumUpdateCRCA;
        TransferState.Checksums.FinalFunc = &TransferChecksumFinalCRCA;
        TransferState.Checksums.CRCA = ISO14443A_CRCA_INIT;
        TransferState.ReadData.Encryption.Func = &TransferEncryptTDEASend;
        memset(SessionIV.IsoTransferIV, 0, sizeof(SessionIV.IsoTransferIV));
        break;

    default:
        return STATUS_PARAMETER_ERROR;
    }
    return STATUS_OPERATION_OK;
}

uint8_t WriteDataFilterSetup(uint8_t CommSettings)
{
    memset(&TransferState, 0, sizeof(TransferState));

    switch (CommSettings) {
    case DESFIRE_COMMS_PLAINTEXT:
        break;

    case DESFIRE_COMMS_PLAINTEXT_MAC:
        TransferState.Checksums.UpdateFunc = &TransferChecksumUpdateMACTDEA;
        TransferState.Checksums.FinalFunc = &TransferChecksumFinalMACTDEA;
        TransferState.Checksums.MAC.MACFunc = &CryptoEncrypt2KTDEA_CBCReceive;
        memset(SessionIV.LegacyTransferIV, 0, sizeof(SessionIV.LegacyTransferIV));
        break;

    case DESFIRE_COMMS_CIPHERTEXT_DES:
        TransferState.Checksums.UpdateFunc = &TransferChecksumUpdateCRCA;
        TransferState.Checksums.FinalFunc = &TransferChecksumFinalCRCA;
        TransferState.Checksums.CRCA = ISO14443A_CRCA_INIT;
        TransferState.WriteData.Encryption.Func = &TransferEncryptTDEAReceive;
        memset(SessionIV.IsoTransferIV, 0, sizeof(SessionIV.IsoTransferIV));
        break;

    default:
        return STATUS_PARAMETER_ERROR;
    }
    return STATUS_OPERATION_OK;
}

/*
 * PICC management routines
 */

void InitialisePiccBackendEV0(uint8_t StorageSize) {
    /* Init backend junk */
    ReadBlockBytes(&Picc, DESFIRE_PICC_INFO_BLOCK_ID, sizeof(Picc));
    if (Picc.Uid[0] == 0xFF && Picc.Uid[1] == 0xFF && Picc.Uid[2] == 0xFF && Picc.Uid[3] == 0xFF) {
        DEBUG_PRINT("\r\nFactory resetting the device\r\n");
        FactoryFormatPiccEV0();
    }
    else {
        ReadBlockBytes(&AppDir, DESFIRE_APP_DIR_BLOCK_ID, sizeof(AppDir));
    }
}

void InitialisePiccBackendEV1(uint8_t StorageSize) {
    /* Init backend junk */
    ReadBlockBytes(&Picc, DESFIRE_PICC_INFO_BLOCK_ID, sizeof(Picc));
    if (Picc.Uid[0] == 0xFF && Picc.Uid[1] == 0xFF && Picc.Uid[2] == 0xFF && Picc.Uid[3] == 0xFF) {
        DEBUG_PRINT("\r\nFactory resetting the device\r\n");
        FactoryFormatPiccEV1(StorageSize);
    }
    else {
        ReadBlockBytes(&AppDir, DESFIRE_APP_DIR_BLOCK_ID, sizeof(AppDir));
    }
}

void ResetPiccBackend(void) {
    SelectPiccApp();
}

bool IsEmulatingEV1(void) {
    return Picc.HwVersionMajor >= DESFIRE_HW_MAJOR_EV1;
}

void GetPiccHardwareVersionInfo(uint8_t* Buffer) {
    Buffer[0] = Picc.HwVersionMajor;
    Buffer[1] = Picc.HwVersionMinor;
    Buffer[2] = Picc.StorageSize;
}

void GetPiccSoftwareVersionInfo(uint8_t* Buffer) {
    Buffer[0] = Picc.SwVersionMajor;
    Buffer[1] = Picc.SwVersionMinor;
    Buffer[2] = Picc.StorageSize;
}

void GetPiccManufactureInfo(uint8_t* Buffer) {
    /* UID/Serial number; does not depend on card mode */
    memcpy(&Buffer[0], &Picc.Uid[0], DESFIRE_UID_SIZE);
    Buffer[7] = Picc.BatchNumber[0];
    Buffer[8] = Picc.BatchNumber[1];
    Buffer[9] = Picc.BatchNumber[2];
    Buffer[10] = Picc.BatchNumber[3];
    Buffer[11] = Picc.BatchNumber[4];
    Buffer[12] = Picc.ProductionWeek;
    Buffer[13] = Picc.ProductionYear;
}

uint8_t GetPiccKeySettings(void) {
    return GetAppKeySettings(DESFIRE_PICC_APP_SLOT);
}

void FormatPicc(void) {
    /* Wipe application directory */
    memset(&AppDir, 0, sizeof(AppDir));
    /* Set the first free slot to 1 -- slot 0 is the PICC app */
    AppDir.FirstFreeSlot = 1;
    /* Reset the free block pointer */
    Picc.FirstFreeBlock = DESFIRE_FIRST_FREE_BLOCK_ID;

    SynchronizePICCInfo();
    SynchronizeAppDir();
}

void CreatePiccApp(void) {
    Desfire2KTDEAKeyType Key;

    SetAppKeySettings(DESFIRE_PICC_APP_SLOT, 0x0F);
    SetAppKeyCount(DESFIRE_PICC_APP_SLOT, 1);
    SetAppKeyStorageBlockId(DESFIRE_PICC_APP_SLOT, AllocateBlocks(1));
    SelectPiccApp();
    memset(Key, 0, sizeof(Key));
    WriteSelectedAppKey(0, Key);
}

void FactoryFormatPiccEV0(void) {
    /* Wipe PICC data */
    memset(&Picc, 0xFF, sizeof(Picc));
    memset(&Picc.Uid[0], 0x00, DESFIRE_UID_SIZE);
    /* Initialize params to look like EV0 */
    Picc.StorageSize = DESFIRE_STORAGE_SIZE_4K;
    Picc.HwVersionMajor = DESFIRE_HW_MAJOR_EV0;
    Picc.HwVersionMinor = DESFIRE_HW_MINOR_EV0;
    Picc.SwVersionMajor = DESFIRE_SW_MAJOR_EV0;
    Picc.SwVersionMinor = DESFIRE_SW_MINOR_EV0;
    /* Initialize the root app data */
    CreatePiccApp();
    /* Continue with user data initialization */
    FormatPicc();
}

void FactoryFormatPiccEV1(uint8_t StorageSize) {
    /* Wipe PICC data */
    memset(&Picc, 0xFF, sizeof(Picc));
    memset(&Picc.Uid[0], 0x00, DESFIRE_UID_SIZE);
    /* Initialize params to look like EV1 */
    Picc.StorageSize = StorageSize;
    Picc.HwVersionMajor = DESFIRE_HW_MAJOR_EV1;
    Picc.HwVersionMinor = DESFIRE_HW_MINOR_EV1;
    Picc.SwVersionMajor = DESFIRE_SW_MAJOR_EV1;
    Picc.SwVersionMinor = DESFIRE_SW_MINOR_EV1;
    /* Initialize the root app data */
    CreatePiccApp();
    /* Continue with user data initialization */
    FormatPicc();
}

void GetPiccUid(ConfigurationUidType Uid) {
    memcpy(Uid, Picc.Uid, DESFIRE_UID_SIZE);
}

void SetPiccUid(ConfigurationUidType Uid) {
    memcpy(Picc.Uid, Uid, DESFIRE_UID_SIZE);
    SynchronizePICCInfo();
}
