/* 
 * DESFireCrypto.c
 * Maxie D. Schmidt (github.com/maxieds)
 */

#include <string.h>

#include "../../Log.h"

#include "DESFireCrypto.h"
#include "DESFireInstructions.h"
#include "DESFirePICCControl.h"
#include "DESFireISO14443Support.h"
#include "DESFireStatusCodes.h"
#include "DESFireLogging.h"

CryptoKeyBufferType SessionKey = { 0 };
CryptoIVBufferType SessionIV = { 0 };
BYTE SessionIVByteSize = { 0 };

DesfireAESCryptoContext AESCryptoContext = { 0 };
uint16_t AESCryptoKeySizeBytes = 0;
DesfireAESCryptoKey AESCryptoSessionKey = { 0 };
DesfireAESCryptoKey AESCryptoIVBuffer = { 0 };
DesfireAESCryptoCMACContext AESCryptoChecksumContext = { 0 };

uint8_t Authenticated = 0x00;
uint8_t AuthenticatedWithKey = 0x00;
uint8_t AuthenticatedWithPICCMasterKey = 0x00;
uint8_t CryptoAuthMethod = CRYPTO_TYPE_ANY;
uint8_t ActiveCommMode = DESFIRE_DEFAULT_COMMS_STANDARD;

void InvalidateAuthState(BYTE keepPICCAuthData) {
     if(!keepPICCAuthData) {
          AuthenticatedWithPICCMasterKey = DESFIRE_NOT_AUTHENTICATED;
     }
     Authenticated = DESFIRE_NOT_AUTHENTICATED;
     AuthenticatedWithKey = DESFIRE_NOT_AUTHENTICATED;
     CryptoAuthMethod = CRYPTO_TYPE_ANY;
     ActiveCommMode = DESFIRE_DEFAULT_COMMS_STANDARD;
}

BYTE GetDefaultCryptoMethodKeySize(uint8_t cryptoType) {
     switch(cryptoType) {
          case CRYPTO_TYPE_2K3DES:
               return CRYPTO_2KTDEA_KEY_SIZE;
          case CRYPTO_TYPE_3K3DES:
               return CRYPTO_3KTDEA_KEY_SIZE;
          case CRYPTO_TYPE_AES128:
               return 16;
          case CRYPTO_TYPE_AES192:
               return 24;
          case CRYPTO_TYPE_AES256:
               return 32;
          default:
               return 0;
     }
}

const char * GetCryptoMethodDesc(uint8_t cryptoType) {
     switch(cryptoType) {
          case CRYPTO_TYPE_2K3DES:
               return PSTR("2K3DES");
          case CRYPTO_TYPE_3K3DES:
               return PSTR("3K3DES");
          case CRYPTO_TYPE_AES128:
               return PSTR("AES128");
          case CRYPTO_TYPE_AES192:
               return PSTR("AES192");
          case CRYPTO_TYPE_AES256:
               return PSTR("AES256");
          default:
               return PSTR("");
     }
}

BYTE GetCryptoMethodCommSettings(uint8_t cryptoType) {
     switch(cryptoType) {
          case CRYPTO_TYPE_2K3DES:
               return DESFIRE_COMMS_PLAINTEXT_MAC;
          case CRYPTO_TYPE_3K3DES:
               return DESFIRE_COMMS_CIPHERTEXT_DES;
          case CRYPTO_TYPE_AES128:
               return DESFIRE_COMMS_CIPHERTEXT_AES128;
          case CRYPTO_TYPE_AES192:
               return DESFIRE_COMMS_CIPHERTEXT_AES192;
          case CRYPTO_TYPE_AES256:
               return DESFIRE_COMMS_CIPHERTEXT_AES256;
          default:
               return DESFIRE_COMMS_PLAINTEXT;
     }
}

const char * GetCommSettingsDesc(uint8_t cryptoType) {
     switch(cryptoType) {
          case CRYPTO_TYPE_2K3DES:
               return PSTR("PTEXT-MAC");
          case CRYPTO_TYPE_3K3DES:
               return PSTR("CTEXT-DES");
          case CRYPTO_TYPE_AES128:
               return PSTR("CTEXT-AES128-CMAC");
          case CRYPTO_TYPE_AES192:
               return PSTR("CTEXT-AES192-CMAC");
          case CRYPTO_TYPE_AES256:
               return PSTR("CTEXT-AES256-CMAC");
          default:
               return PSTR("PTEXT-DEFAULT");
     }
}


BYTE GetCryptoKeyTypeFromAuthenticateMethod(BYTE authCmdMethod) {
     switch(authCmdMethod) {
          case CMD_AUTHENTICATE_AES:
          case CMD_AUTHENTICATE_EV2_FIRST:
          case CMD_AUTHENTICATE_EV2_NONFIRST:
               return CRYPTO_TYPE_AES128;
          case CMD_AUTHENTICATE_ISO:
               return CRYPTO_TYPE_3K3DES;
          case CMD_AUTHENTICATE:
          case CMD_ISO7816_EXTERNAL_AUTHENTICATE:
          case CMD_ISO7816_INTERNAL_AUTHENTICATE:
          default:
               return CRYPTO_TYPE_2K3DES;
     }
}

void InitAESCryptoContext(DesfireAESCryptoContext *cryptoCtx) {
     uint8_t ctxBlock[32];
     uint8_t ctxBlockSize = 32;
     memcpy(ctxBlock, 0x00, ctxBlockSize);
     uint8_t ctxOffset = 0;
     while(ctxOffset < sizeof(DesfireAESCryptoContext)) {
          uint8_t sizeToCopy = MIN(sizeof(DesfireAESCryptoContext) - ctxOffset, ctxBlockSize);
          memcpy_P(cryptoCtx + ctxOffset, ctxBlock, sizeToCopy);
          ctxOffset += ctxBlockSize;
     }
}

void InitAESCryptoKeyData(DesfireAESCryptoKey *cryptoKeyData) {
     memset(cryptoKeyData, 0x00, sizeof(DesfireAESCryptoKey));
}

uint8_t * ExtractAESKeyBuffer(DesfireAESCryptoKey *cryptoKey, BYTE keySizeBytes) {
     if(cryptoKey == NULL) {
          return NULL;
     }
     switch(keySizeBytes) {
          case 16:
               return (uint8_t *) cryptoKey->aes128Key;
          case 24:
               return (uint8_t *) cryptoKey->aes192Key;
          case 32:
               return (uint8_t *) cryptoKey->aes256Key;
          default: 
               return NULL;
     }
}

uint16_t GetPaddedBufferSize(uint16_t bufSize) {
     uint16_t spareBytes = (bufSize % CRYPTO_AES_BLOCK_SIZE);
     if(spareBytes == 0) {
          return bufSize;
     }
     return bufSize + CRYPTO_AES_BLOCK_SIZE - spareBytes;
}

// assumes the crypto context struct is uninitialized: 
uint8_t DesfireAESCryptoInit(uint8_t *initKeyBuffer, uint16_t bufSize, 
                              DesfireAESCryptoContext *cryptoCtx) {
     if(initKeyBuffer == NULL || cryptoCtx == NULL) {
          return STATUS_PARAMETER_ERROR;
     }
     InitAESCryptoContext(cryptoCtx);
     if(bufSize == 0 || bufSize > 32) {
          return STATUS_PARAMETER_ERROR;
     }
     uint8_t keySize;
     uint8_t *paddedKeyBuf;
     DesfireAESCryptoKey cryptoKeyBuf = { 0 };
     if(bufSize > 24 && bufSize <= 32) {
          keySize = 32;
          paddedKeyBuf = (uint8_t *) cryptoKeyBuf.aes256Key;
          memcpy(paddedKeyBuf, initKeyBuffer, bufSize);
          aes256_init(paddedKeyBuf, cryptoCtx);
     }
     else if(bufSize > 16 && bufSize <= 24) {
          keySize = 24;
          paddedKeyBuf = (uint8_t *) cryptoKeyBuf.aes192Key;
          memcpy(paddedKeyBuf, initKeyBuffer, bufSize);
          aes192_init(paddedKeyBuf, cryptoCtx);
     }
     else {
          keySize = 16;
          paddedKeyBuf = (uint8_t *) cryptoKeyBuf.aes128Key;
          memcpy(paddedKeyBuf, initKeyBuffer, bufSize);
          aes128_init(paddedKeyBuf, cryptoCtx);
     }
     AESCryptoKeySizeBytes = keySize;
     return STATUS_OPERATION_OK;
}

uint8_t DesfireAESEncryptBuffer(DesfireAESCryptoContext *cryptoCtx, uint8_t *plainSrcBuf, 
                                uint8_t *encDestBuf, uint16_t bufSize) {
     if(cryptoCtx == NULL || plainSrcBuf == NULL || encDestBuf == NULL) {
          return STATUS_PARAMETER_ERROR;
     }
     else if((bufSize % CRYPTO_AES_BLOCK_SIZE) != 0) {
          return STATUS_PARAMETER_ERROR;
     }
     uint8_t encBlockData[CRYPTO_AES_BLOCK_SIZE];
     for(int blockOffset = 0; blockOffset < bufSize / CRYPTO_AES_BLOCK_SIZE; blockOffset += CRYPTO_AES_BLOCK_SIZE) {
          memcpy(encBlockData, plainSrcBuf + blockOffset, CRYPTO_AES_BLOCK_SIZE);
          switch(AESCryptoKeySizeBytes) {
               case 16:
                    aes128_enc(encBlockData, cryptoCtx);
                    break;
               case 24:
                    aes192_enc(encBlockData, cryptoCtx);
                    break;
               case 32:
                    aes256_enc(encBlockData, cryptoCtx);
                    break;
               default:
                    return STATUS_NO_SUCH_KEY;
          }
          memcpy(encDestBuf + blockOffset, encBlockData, CRYPTO_AES_BLOCK_SIZE);
     }
     return STATUS_OPERATION_OK;
}

uint8_t DesfireAESDecryptBuffer(DesfireAESCryptoContext *cryptoCtx, 
                                uint8_t *encSrcBuf, uint8_t *plainDestBuf, uint16_t bufSize) {
     if(cryptoCtx == NULL || encSrcBuf == NULL || plainDestBuf == NULL) {
          return STATUS_PARAMETER_ERROR;
     }
     else if((bufSize % CRYPTO_AES_BLOCK_SIZE) != 0) {
          return STATUS_PARAMETER_ERROR;
     }
     uint8_t decBlockData[CRYPTO_AES_BLOCK_SIZE];
     for(int blockOffset = 0; blockOffset < bufSize / CRYPTO_AES_BLOCK_SIZE; blockOffset += CRYPTO_AES_BLOCK_SIZE) {
          memcpy(decBlockData, encSrcBuf + blockOffset, CRYPTO_AES_BLOCK_SIZE);
          switch(AESCryptoKeySizeBytes) {
               case 16:
                    aes128_dec(decBlockData, cryptoCtx);
                    break;
               case 24:
                    aes192_dec(decBlockData, cryptoCtx);
                    break;
               case 32:
                    aes256_dec(decBlockData, cryptoCtx);
                    break;
               default:
                    return STATUS_NO_SUCH_KEY;
          }
          memcpy(plainDestBuf + blockOffset, decBlockData, CRYPTO_AES_BLOCK_SIZE);
     }
     return STATUS_OPERATION_OK;
}

uint8_t TransferEncryptAESCryptoSend(uint8_t *Buffer, uint8_t Count) {
    uint8_t AvailablePlaintext = TransferState.ReadData.Encryption.AvailablePlaintext;
    uint8_t TempBuffer[(DESFIRE_MAX_PAYLOAD_AES_BLOCKS + 1) * CRYPTO_DES_BLOCK_SIZE];
    uint16_t tempBufSize = (DESFIRE_MAX_PAYLOAD_AES_BLOCKS + 1) * CRYPTO_DES_BLOCK_SIZE;
    uint16_t bufFillSize = MIN(tempBufSize, AvailablePlaintext), bufFillSize2;
    uint8_t *tempBufOffset;
    if(AvailablePlaintext) {
        /* Fill the partial block */
        memcpy(&TempBuffer[0], &TransferState.BlockBuffer[0], bufFillSize);
    }
    /* Copy fresh plaintext to the temp buffer */
    if(Count > bufFillSize && tempBufSize - bufFillSize > 0) {
         tempBufOffset = &TempBuffer[bufFillSize];
         bufFillSize2 = bufFillSize;
         bufFillSize = MIN(Count, tempBufSize - bufFillSize);
         memcpy(tempBufOffset, Buffer, bufFillSize);
         Count += bufFillSize2 + Count - bufFillSize;
    }
    uint8_t BlockCount = Count / CRYPTO_AES_BLOCK_SIZE;
    /* Stash extra plaintext for later */
    AvailablePlaintext = Count - BlockCount * CRYPTO_AES_BLOCK_SIZE;
    if (AvailablePlaintext) {
        memcpy(&TransferState.BlockBuffer[0],
               &Buffer[BlockCount * CRYPTO_AES_BLOCK_SIZE], AvailablePlaintext);
    }
    TransferState.ReadData.Encryption.AvailablePlaintext = AvailablePlaintext;
    /* Encrypt complete blocks in the buffer */
    CryptoEncryptAES_CBCSend(BlockCount, &TempBuffer[0], &Buffer[0], 
                             ExtractAESKeyBuffer(&AESCryptoIVBuffer, &AESCryptoContext), 
                             AESCryptoKeySizeBytes);
    /* Return byte count to transfer */
    return BlockCount * CRYPTO_AES_BLOCK_SIZE; 
}

uint8_t TransferEncryptAESCryptoReceive(uint8_t *Buffer, uint8_t Count) {
     LogEntry(LOG_INFO_DESFIRE_INCOMING_DATA_ENC, Buffer, Count);
     return STATUS_OPERATION_OK;
}

void CryptoEncryptAES_CBCSend(uint16_t Count, const void *PlainText, void *CipherText, 
                              void *IV, DesfireAESCryptoContext *AESCryptoContextData) {
}

void CryptoDecryptAES_CBCSend(uint16_t Count, const void *PlainText, void *CipherText, 
                              void *IV, DesfireAESCryptoContext *AESCryptoContextData) {
}

void CryptoEncryptAES_CBCReceive(uint16_t Count, const void *PlainText, void *CipherText, 
                                 void *IV, DesfireAESCryptoContext *AESCryptoContextData) {
}

void CryptoDecryptAES_CBCReceive(uint16_t Count, const void *PlainText, void *CipherText, 
                                 void *IV, DesfireAESCryptoContext *AESCryptoContextData) {
}

BYTE InitAESCryptoCMACContext(DesfireAESCryptoCMACContext *cmacCtx, DesfireAESCryptoContext *cryptoCtx) {
     if(cmacCtx == NULL || cryptoCtx == NULL) {
          return STATUS_PARAMETER_ERROR;
     }
     switch(AESCryptoKeySizeBytes) {
          case 16:
               cmacCtx->desc = &aes128_desc;
               cmacCtx->cctx.keysize = 16;
               cmacCtx->cctx.ctx = cryptoCtx;
               break;
          case 24:
               cmacCtx->desc = &aes192_desc;
               cmacCtx->cctx.keysize = 24;
               cmacCtx->cctx.ctx = cryptoCtx;
               break;
          case 32:
               cmacCtx->desc = &aes256_desc;
               cmacCtx->cctx.keysize = 32;
               cmacCtx->cctx.ctx = cryptoCtx;
               break;
          default:
               return STATUS_PARAMETER_ERROR;
     }
     return bcal_cmac_init(cmacCtx->desc, NULL, AESCryptoKeySizeBytes, cmacCtx);
}

void CalculateAESCryptoCMAC(BYTE *cmacDestBytes, const BYTE *srcBuf, SIZET bufSize, 
                            DesfireAESCryptoCMACContext *cmacCtx) {
     if(cmacDestBytes == NULL || srcBuf == NULL || cmacCtx == NULL) {
          return;
     }
     bcal_cmac(cmacDestBytes, CMAC_LENGTH, srcBuf, bufSize, cmacCtx);
}

/* Checksum routines */

void TransferChecksumUpdateCRCA(const uint8_t* Buffer, uint8_t Count) {
    TransferState.Checksums.MACData.CRCA = 
                  ISO14443AUpdateCRCA(Buffer, Count, TransferState.Checksums.MACData.CRCA);
}

uint8_t TransferChecksumFinalCRCA(uint8_t* Buffer) {
    /* Copy the checksum to destination */
    memcpy(Buffer, &TransferState.Checksums.MACData.CRCA, 2);
    /* Return the checksum size */
    return 2;
}

void TransferChecksumUpdateMACTDEA(const uint8_t* Buffer, uint8_t Count) {
    uint8_t AvailablePlaintext = TransferState.Checksums.AvailablePlaintext;
    uint8_t TempBuffer[CRYPTO_DES_BLOCK_SIZE];

    if (AvailablePlaintext) {
        uint8_t TempBytes;
        /* Fill the partial block */
        TempBytes = CRYPTO_DES_BLOCK_SIZE - AvailablePlaintext;
        if (TempBytes > Count)
            TempBytes = Count;
        memcpy(&TransferState.BlockBuffer[AvailablePlaintext], &Buffer[0], TempBytes);
        Count -= TempBytes;
        Buffer += TempBytes;
        /* MAC the partial block */
        TransferState.Checksums.MACData.CryptoChecksumFunc.TDEAFunc(1, &TransferState.BlockBuffer[0], 
                                                                    &TempBuffer[0], SessionIV, SessionKey);
    }
    /* MAC complete blocks in the buffer */
    while (Count >= CRYPTO_DES_BLOCK_SIZE) {
        /* NOTE: This is block-by-block, hence slow. 
         *       See if it's better to just allocate a temp buffer large enough (64 bytes). */
        TransferState.Checksums.MACData.CryptoChecksumFunc.TDEAFunc(1, &Buffer[0], &TempBuffer[0], 
                                                                    SessionIV, SessionKey);
        Count -= CRYPTO_DES_BLOCK_SIZE;
        Buffer += CRYPTO_DES_BLOCK_SIZE;
    }
    /* Copy the new partial block */
    if (Count) {
        memcpy(&TransferState.BlockBuffer[0], &Buffer[0], Count);
    }
    TransferState.Checksums.AvailablePlaintext = Count;
}

uint8_t TransferChecksumFinalMACTDEA(uint8_t* Buffer) {
    uint8_t AvailablePlaintext = TransferState.Checksums.AvailablePlaintext;
    uint8_t TempBuffer[CRYPTO_DES_BLOCK_SIZE];

    if (AvailablePlaintext) {
        /* Apply padding */
        CryptoPaddingTDEA(&TransferState.BlockBuffer[0], AvailablePlaintext, false);
        /* MAC the partial block */
        TransferState.Checksums.MACData.CryptoChecksumFunc.TDEAFunc(1, &TransferState.BlockBuffer[0], 
                                                                    &TempBuffer[0], SessionIV, SessionKey);
        TransferState.Checksums.AvailablePlaintext = 0;
    }
    /* Copy the checksum to destination */
    memcpy(Buffer, SessionIV, 4);
    /* Return the checksum size */
    return 4;
}

/* Encryption routines */

uint8_t TransferEncryptTDEASend(uint8_t* Buffer, uint8_t Count) {
    uint8_t AvailablePlaintext = TransferState.ReadData.Encryption.AvailablePlaintext;
    uint8_t TempBuffer[(DESFIRE_MAX_PAYLOAD_TDEA_BLOCKS + 1) * CRYPTO_DES_BLOCK_SIZE];
    uint8_t BlockCount;

    if (AvailablePlaintext) {
        /* Fill the partial block */
        memcpy(&TempBuffer[0], &TransferState.BlockBuffer[0], AvailablePlaintext);
    }
    /* Copy fresh plaintext to the temp buffer */
    memcpy(&TempBuffer[AvailablePlaintext], Buffer, Count);
    Count += AvailablePlaintext;
    BlockCount = Count / CRYPTO_DES_BLOCK_SIZE;
    /* Stash extra plaintext for later */
    AvailablePlaintext = Count - BlockCount * CRYPTO_DES_BLOCK_SIZE;
    if (AvailablePlaintext) {
        memcpy(&TransferState.BlockBuffer[0], 
               &Buffer[BlockCount * CRYPTO_DES_BLOCK_SIZE], AvailablePlaintext);
    }
    TransferState.ReadData.Encryption.AvailablePlaintext = AvailablePlaintext;
    /* Encrypt complete blocks in the buffer */
    CryptoEncrypt2KTDEA_CBCSend(BlockCount, &TempBuffer[0], &Buffer[0], 
                                SessionIV, SessionKey);
    /* Return byte count to transfer */
    return BlockCount * CRYPTO_DES_BLOCK_SIZE;
}

uint8_t TransferEncryptTDEAReceive(uint8_t* Buffer, uint8_t Count) {
     LogEntry(LOG_INFO_DESFIRE_INCOMING_DATA_ENC, Buffer, Count);
     return 0;
}

void CryptoPaddingTDEA(uint8_t* Buffer, uint8_t BytesInBuffer, bool FirstPaddingBitSet)
{
    uint8_t PaddingByte = FirstPaddingBitSet << 7;
    uint8_t i;

    for (i = BytesInBuffer; i < CRYPTO_DES_BLOCK_SIZE; ++i) {
        Buffer[i] = PaddingByte;
        PaddingByte = 0x00;
    }
}

void CryptoEncrypt2KTDEA(void *Plaintext, void *Ciphertext, const uint8_t *Keys) {
    uint8_t tempBlock[CRYPTO_DES_BLOCK_SIZE]; 
    des_enc(tempBlock, Plaintext, Keys);
    des_enc(Ciphertext, tempBlock, Keys + CRYPTO_DES_BLOCK_SIZE);
}

void CryptoEncryptBuffer2KTDEA(void *Plaintext, void *Ciphertext, uint16_t numBytes, const uint8_t *Keys) {
    uint16_t numBlocks = CRYPTO_BYTES_TO_BLOCKS(numBytes, CRYPTO_DES_BLOCK_SIZE);
    uint16_t blockIndex = 0;
    uint8_t *ptBuf = (uint8_t *) Plaintext, *ctBuf = (uint8_t *) Ciphertext;
    bool lastBlockPadding = false;
    if(numBlocks * CRYPTO_DES_BLOCK_SIZE > numBytes) {
         lastBlockPadding = true;
    }
    while(blockIndex < numBlocks) {
        if(blockIndex + 1 == numBlocks && lastBlockPadding) {
            uint8_t bytesInBuffer = numBytes - (numBlocks - 1) * CRYPTO_DES_BLOCK_SIZE;
            CryptoPaddingTDEA(ptBuf + blockIndex * CRYPTO_DES_BLOCK_SIZE, bytesInBuffer, false);
        }
        CryptoEncrypt2KTDEA(ptBuf + blockIndex * CRYPTO_DES_BLOCK_SIZE, 
                            ctBuf + blockIndex * CRYPTO_DES_BLOCK_SIZE, Keys);
        blockIndex++;
    }
}

void CryptoDecrypt2KTDEA(void *Plaintext, void *Ciphertext, const uint8_t *Keys) {
    uint8_t tempBlock[CRYPTO_DES_BLOCK_SIZE]; 
    des_dec(tempBlock, Ciphertext, Keys + CRYPTO_DES_BLOCK_SIZE);
    des_dec(Plaintext, tempBlock, Keys);
}

void CryptoDecryptBuffer2KTDEA(void *Plaintext, void *Ciphertext, uint16_t numBytes, const uint8_t *Keys) {
    uint16_t numBlocks = CRYPTO_BYTES_TO_BLOCKS(numBytes, CRYPTO_DES_BLOCK_SIZE);
    uint16_t blockIndex = 0;
    uint8_t *ptBuf = (uint8_t *) Plaintext, *ctBuf = (uint8_t *) Ciphertext;
    while(blockIndex < numBlocks) {
        CryptoDecrypt2KTDEA(ptBuf + blockIndex * CRYPTO_DES_BLOCK_SIZE, 
                            ctBuf + blockIndex * CRYPTO_DES_BLOCK_SIZE, Keys);
        blockIndex++;
    }
}

void CryptoEncrypt3KTDEA(void *Plaintext, void *Ciphertext, const uint8_t *Keys) {
    tdes_enc(Ciphertext, Plaintext, Keys);
}

void CryptoEncryptBuffer3KTDEA(void *Plaintext, void *Ciphertext, uint16_t numBytes, const uint8_t *Keys) {
    uint16_t numBlocks = CRYPTO_BYTES_TO_BLOCKS(numBytes, CRYPTO_3KTDEA_BLOCK_SIZE);
    uint16_t blockIndex = 0;
    uint8_t *ptBuf = (uint8_t *) Plaintext, *ctBuf = (uint8_t *) Ciphertext;
    bool lastBlockPadding = false;
    if(numBlocks * CRYPTO_3KTDEA_BLOCK_SIZE > numBytes) {
         lastBlockPadding = true;
    }
    while(blockIndex < numBlocks) {
        if(blockIndex + 1 == numBlocks && lastBlockPadding) {
            uint8_t bytesInBuffer = numBytes - (numBlocks - 1) * CRYPTO_3KTDEA_BLOCK_SIZE;
            CryptoPaddingTDEA(ptBuf + blockIndex * CRYPTO_3KTDEA_BLOCK_SIZE, bytesInBuffer, false);
        }
        CryptoEncrypt3KTDEA(ptBuf + blockIndex * CRYPTO_3KTDEA_BLOCK_SIZE, 
                            ctBuf + blockIndex * CRYPTO_3KTDEA_BLOCK_SIZE, Keys);
        blockIndex++;
    }
}

void CryptoDecrypt3KTDEA(void *Plaintext, void *Ciphertext, const uint8_t *Keys) {
    tdes_dec(Plaintext, Ciphertext, Keys);
}

void CryptoDecryptBuffer3KTDEA(void *Plaintext, void *Ciphertext, uint16_t numBytes, const uint8_t *Keys) {
    uint16_t numBlocks = CRYPTO_BYTES_TO_BLOCKS(numBytes, CRYPTO_3KTDEA_BLOCK_SIZE);
    uint16_t blockIndex = 0;
    uint8_t *ptBuf = (uint8_t *) Plaintext, *ctBuf = (uint8_t *) Ciphertext;
    while(blockIndex < numBlocks) {
        CryptoDecrypt3KTDEA(ptBuf + blockIndex * CRYPTO_3KTDEA_BLOCK_SIZE, 
                            ctBuf + blockIndex * CRYPTO_3KTDEA_BLOCK_SIZE, Keys);
        blockIndex++;
    }
}

// This routine performs the CBC "send" mode chaining: C = E(P ^ IV); IV = C
void CryptoTDEA_CBCSend(uint16_t Count, void* Plaintext, void* Ciphertext, 
                        void *IV, const uint8_t* Keys, CryptoTDEA_CBCSpec CryptoSpec) {
    uint16_t numBlocks = CRYPTO_BYTES_TO_BLOCKS(Count, CryptoSpec.blockSize);
    uint16_t blockIndex = 0;
    uint8_t *ptBuf = (uint8_t *) Plaintext, *ctBuf = (uint8_t *) Ciphertext;
    uint8_t tempBlock[CryptoSpec.blockSize], ivBlock[CryptoSpec.blockSize];
    bool lastBlockPadding = false;
    if(numBlocks * CryptoSpec.blockSize > Count) {
         lastBlockPadding = true;
    }
    while(blockIndex < numBlocks) {
        if(blockIndex + 1 == numBlocks && lastBlockPadding) {
            uint8_t bytesInBuffer = Count - (numBlocks - 1) * CryptoSpec.blockSize;
            CryptoPaddingTDEA(ptBuf + blockIndex * CryptoSpec.blockSize, bytesInBuffer, false);
        }
        memcpy(tempBlock, ptBuf + blockIndex * CryptoSpec.blockSize, CryptoSpec.blockSize);
        memcpy(ivBlock, IV + blockIndex * CryptoSpec.blockSize, CryptoSpec.blockSize);
        memxor(ivBlock, tempBlock, CryptoSpec.blockSize);
        CryptoSpec.cryptFunc(ivBlock, tempBlock, Keys);
        memcpy(IV + blockIndex * CryptoSpec.blockSize, tempBlock, CryptoSpec.blockSize);
        memcpy(Ciphertext + blockIndex * CryptoSpec.blockSize, tempBlock, CryptoSpec.blockSize);
        blockIndex++;
    }
}

// This routine performs the CBC "receive" mode chaining: C = E(P) ^ IV; IV = C
void CryptoTDEA_CBCRecv(uint16_t Count, void* Plaintext, void* Ciphertext,
                        void *IV, const uint8_t* Keys, CryptoTDEA_CBCSpec CryptoSpec) {
    uint16_t numBlocks = CRYPTO_BYTES_TO_BLOCKS(Count, CryptoSpec.blockSize);
    uint16_t blockIndex = 0;
    uint8_t *ptBuf = (uint8_t *) Plaintext, *ctBuf = (uint8_t *) Ciphertext;
    uint8_t tempBlock[CryptoSpec.blockSize], ivBlock[CryptoSpec.blockSize];
    bool lastBlockPadding = false;
    if(numBlocks * CryptoSpec.blockSize > Count) {
         lastBlockPadding = true;
    }
    while(blockIndex < numBlocks) {
        if(blockIndex + 1 == numBlocks && lastBlockPadding) {
            uint8_t bytesInBuffer = Count - (numBlocks - 1) * CryptoSpec.blockSize;
            CryptoPaddingTDEA(ptBuf + blockIndex * CryptoSpec.blockSize, bytesInBuffer, false);
        }
        memcpy(ivBlock, ptBuf + blockIndex * CryptoSpec.blockSize, CryptoSpec.blockSize);
        CryptoSpec.cryptFunc(ivBlock, tempBlock, CryptoSpec.blockSize);
        memcpy(ivBlock, IV + blockIndex * CryptoSpec.blockSize, CryptoSpec.blockSize);
        memxor(ivBlock, tempBlock, CryptoSpec.blockSize);
        memcpy(IV + blockIndex * CryptoSpec.blockSize, ivBlock, CryptoSpec.blockSize);
        memcpy(Ciphertext + blockIndex * CryptoSpec.blockSize, ivBlock, CryptoSpec.blockSize);
        blockIndex++;
    }
}

void CryptoEncrypt2KTDEA_CBCSend(uint16_t Count, void* Plaintext, void* Ciphertext,
                                 void *IV, const uint8_t* Keys) {
     CryptoTDEA_CBCSpec CryptoSpec = {
         .cryptFunc   = &CryptoEncrypt2KTDEA,
         .blockSize   = CRYPTO_DES_BLOCK_SIZE
     };
     CryptoTDEA_CBCSend(Count, Plaintext, Ciphertext, IV, Keys, CryptoSpec);
}

void CryptoDecrypt2KTDEA_CBCSend(uint16_t Count, void* Plaintext, void* Ciphertext,
                                 void *IV, const uint8_t* Keys) {
     CryptoTDEA_CBCSpec CryptoSpec = {
         .cryptFunc   = &CryptoDecrypt2KTDEA,
         .blockSize   = CRYPTO_DES_BLOCK_SIZE
     };
     CryptoTDEA_CBCSend(Count, Plaintext, Ciphertext, IV, Keys, CryptoSpec);
}

void CryptoEncrypt2KTDEA_CBCReceive(uint16_t Count, void* Plaintext, void* Ciphertext,
                                    void *IV, const uint8_t* Keys) {
     CryptoTDEA_CBCSpec CryptoSpec = {
         .cryptFunc   = &CryptoEncrypt2KTDEA,
         .blockSize   = CRYPTO_DES_BLOCK_SIZE
     };
     CryptoTDEA_CBCRecv(Count, Plaintext, Ciphertext, IV, Keys, CryptoSpec);
}

void CryptoDecrypt2KTDEA_CBCReceive(uint16_t Count, void* Plaintext, void* Ciphertext,
                                    void *IV, const uint8_t* Keys) {
     CryptoTDEA_CBCSpec CryptoSpec = {
         .cryptFunc   = &CryptoDecrypt2KTDEA,
         .blockSize   = CRYPTO_DES_BLOCK_SIZE
     };
     CryptoTDEA_CBCRecv(Count, Plaintext, Ciphertext, IV, Keys, CryptoSpec);
}

void CryptoEncrypt3KTDEA_CBCSend(uint16_t Count, void* Plaintext, void* Ciphertext,
                                 void *IV, const uint8_t* Keys) {
     CryptoTDEA_CBCSpec CryptoSpec = {
         .cryptFunc   = &CryptoEncrypt3KTDEA,
         .blockSize   = CRYPTO_3KTDEA_BLOCK_SIZE
     };
     CryptoTDEA_CBCSend(Count, Plaintext, Ciphertext, IV, Keys, CryptoSpec);
}

void CryptoDecrypt3KTDEA_CBCSend(uint16_t Count, void* Plaintext, void* Ciphertext,
                                 void *IV, const uint8_t* Keys) {
     CryptoTDEA_CBCSpec CryptoSpec = {
         .cryptFunc   = &CryptoDecrypt3KTDEA,
         .blockSize   = CRYPTO_3KTDEA_BLOCK_SIZE
     };
     CryptoTDEA_CBCSend(Count, Plaintext, Ciphertext, IV, Keys, CryptoSpec);
}

void CryptoEncrypt3KTDEA_CBCReceive(uint16_t Count, void* Plaintext, void* Ciphertext,
                                    void *IV, const uint8_t* Keys) {
     CryptoTDEA_CBCSpec CryptoSpec = {
         .cryptFunc   = &CryptoEncrypt3KTDEA,
         .blockSize   = CRYPTO_3KTDEA_BLOCK_SIZE
     };
     CryptoTDEA_CBCRecv(Count, Plaintext, Ciphertext, IV, Keys, CryptoSpec);
}

void CryptoDecrypt3KTDEA_CBCReceive(uint16_t Count, void* Plaintext, void* Ciphertext,
                                    void *IV, const uint8_t* Keys) {
     CryptoTDEA_CBCSpec CryptoSpec = {
         .cryptFunc   = &CryptoDecrypt3KTDEA,
         .blockSize   = CRYPTO_3KTDEA_BLOCK_SIZE
     };
     CryptoTDEA_CBCRecv(Count, Plaintext, Ciphertext, IV, Keys, CryptoSpec);
}

