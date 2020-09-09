/*
The DESFire stack portion of this firmware source 
is free software written by Maxie Dion Schmidt (@maxieds): 
You can redistribute it and/or modify
it under the terms of this license.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 

The complete source distribution of  
this firmware is available at the following link:
https://github.com/maxieds/ChameleonMiniFirmwareDESFireStack.

Based in part on the original DESFire code created by  
@dev-zzo (GitHub handle) [Dmitry Janushkevich] available at  
https://github.com/dev-zzo/ChameleonMini/tree/desfire.

This notice must be retained at the top of all source files where indicated. 

This source code is only licensed for 
redistribution under for non-commercial users. 
All commerical use or inclusion of this 
software requires express written consent of the author (MDS). 
This restriction pertains to any binary distributions which 
are derivative works of this software.

The author is free to revoke or modify this license for future 
versions of the code at free will.
*/

/* 
 * DESFireMemoryOperations.c 
 * Maxie D. Schmidt (github.com/maxieds)
 */

#include "../../Memory.h"
#include "DESFireMemoryOperations.h"
#include "DESFirePICCControl.h"
#include "DESFireFile.h"
#include "DESFireLogging.h"

volatile char __InternalStringBuffer[STRING_BUFFER_SIZE] = { 0 };
char __InternalStringBuffer2[DATA_BUFFER_SIZE_SMALL] = { 0 };

void ReadBlockBytes(void* Buffer, SIZET StartBlock, SIZET Count) {
    if(StartBlock >= MEMORY_SIZE_PER_SETTING) {
        const char *loggingErrorMsg = PSTR("ReadBlockBytes: Block Address to large -- %d");
        DEBUG_PRINT_P(loggingErrorMsg, StartBlock);
        return;
    }
    MemoryReadBlock(Buffer, StartBlock * DESFIRE_EEPROM_BLOCK_SIZE, Count);
}

void WriteBlockBytes(const void* Buffer, SIZET StartBlock, SIZET Count) {
    if(StartBlock >= MEMORY_SIZE_PER_SETTING) {
        const char *loggingErrorMsg = PSTR("WriteBlockBytes: Block Address to large -- %d");
        DEBUG_PRINT_P(loggingErrorMsg, StartBlock);
        return;
    }
    MemoryWriteBlock(Buffer, StartBlock * DESFIRE_EEPROM_BLOCK_SIZE, Count);
}

void CopyBlockBytes(SIZET DestBlock, SIZET SrcBlock, SIZET Count) {
    uint8_t Buffer[DESFIRE_EEPROM_BLOCK_SIZE];
    uint16_t SrcOffset = SrcBlock; 
    uint16_t DestOffset = DestBlock; 
    while(Count > 0) {
        SIZET bytesToWrite = MIN(Count, DESFIRE_EEPROM_BLOCK_SIZE);
        ReadBlockBytes(Buffer, SrcOffset, bytesToWrite);
        WriteBlockBytes(Buffer, DestOffset, bytesToWrite);
        SrcOffset += 1; 
        DestOffset += 1; 
        Count -= DESFIRE_EEPROM_BLOCK_SIZE;
    }
}

void SetBlockBytes(SIZET DestBlock, BYTE InitByteValue, SIZET ByteCount) {
     BYTE initValueArray[DESFIRE_EEPROM_BLOCK_SIZE];
     memset(initValueArray, InitByteValue, DESFIRE_EEPROM_BLOCK_SIZE);
     while(ByteCount > 0) {
          SIZET bytesToWrite = MIN(ByteCount, DESFIRE_EEPROM_BLOCK_SIZE);
          WriteBlockBytes(initValueArray, DestBlock, bytesToWrite);
          DestBlock += 1; //DESFIRE_EEPROM_BLOCK_SIZE;
          ByteCount -= DESFIRE_EEPROM_BLOCK_SIZE;
     }
}

uint8_t AllocateBlocks(uint8_t BlockCount) {
    uint8_t Block;
    /* Check if we have space */
    Block = Picc.FirstFreeBlock;
    if (Block + BlockCount < Block) {
        return 0;
    }
    Picc.FirstFreeBlock = Block + BlockCount;
    SynchronizePICCInfo();
    SetBlockBytes(Block, 0x00, BlockCount * DESFIRE_EEPROM_BLOCK_SIZE);
    return Block;
}

uint8_t GetCardCapacityBlocks(void) {
    uint8_t MaxFreeBlocks;

    switch (Picc.StorageSize) {
    case DESFIRE_STORAGE_SIZE_2K:
        MaxFreeBlocks = 0x40 - DESFIRE_FIRST_FREE_BLOCK_ID;
        break;
    case DESFIRE_STORAGE_SIZE_4K:
        MaxFreeBlocks = 0x80 - DESFIRE_FIRST_FREE_BLOCK_ID;
        break;
    case DESFIRE_STORAGE_SIZE_8K:
        MaxFreeBlocks = (BYTE) (0x100 - DESFIRE_FIRST_FREE_BLOCK_ID);
        break;
    default:
        return 0;
    }
    return MaxFreeBlocks - Picc.FirstFreeBlock;
}

void ReadDataEEPROMSource(uint8_t* Buffer, uint8_t Count) {
    MemoryReadBlock(Buffer, TransferState.ReadData.Source.Pointer, Count);
    TransferState.ReadData.Source.Pointer += DESFIRE_BYTES_TO_BLOCKS(Count);
}

void WriteDataEEPROMSink(uint8_t* Buffer, uint8_t Count) {
    MemoryWriteBlock(Buffer, TransferState.WriteData.Sink.Pointer, Count);
    TransferState.WriteData.Sink.Pointer += DESFIRE_BYTES_TO_BLOCKS(Count);
}


