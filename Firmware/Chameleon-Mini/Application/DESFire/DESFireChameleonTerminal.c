/* 
 * DESFireChameleonTerminal.c
 * Maxie D. Schmidt (github.com/maxieds)
 */

#include "../../Terminal/Terminal.h"
#include "../../Terminal/Commands.h"

#include "DESFireChameleonTerminal.h"
#include "DESFireFirmwareSettings.h"
#include "DESFirePICCControl.h"
#include "DESFireLogging.h"

CommandStatusIdType CommandDESFireGetHeaderProperty(char *OutParam) {
     snprintf_P(OutParam, TERMINAL_BUFFER_SIZE, 
                PSTR("%s <HardwareVersion-2|SoftwareVersion-2|BatchNumber-5|ProductionDate-2> <HexBytes-N>"), 
                DFCOMMAND_SET_HEADER);
     return COMMAND_INFO_OK_WITH_TEXT_ID;
}

CommandStatusIdType CommandDESFireSetHeaderProperty(char *OutParam, const char *InParams) {
     char hdrPropSpecStr[24];
     char propSpecBytesStr[16];
     BYTE propSpecBytes[16];
     SIZET dataByteCount = 0x00;
     BYTE StatusError = 0x00;
     if(!sscanf_P(InParams, PSTR("%24s %12s"), hdrPropSpecStr, propSpecBytesStr)) {
          CommandDESFireGetHeaderProperty(OutParam);
          return COMMAND_ERR_INVALID_PARAM_ID;
     }
     hdrPropSpecStr[23] = propSpecBytesStr[15] = '\0';
     dataByteCount = HexStringToBuffer(propSpecBytes, 16, propSpecBytesStr);
     if(!strcasecmp_P(hdrPropSpecStr, PSTR("HardwareVersion"))) {
          if(dataByteCount != 2) {
               StatusError = 0x01;
          }
          else {
               Picc.HwVersionMajor = propSpecBytes[0];
               Picc.HwVersionMinor = propSpecBytes[1];
          }
     }
     else if(!strcasecmp_P(hdrPropSpecStr, PSTR("SoftwareVersion"))) {
          if(dataByteCount != 2) {
               StatusError = 0x01;
          }
          else {
               Picc.SwVersionMajor = propSpecBytes[0];
               Picc.SwVersionMinor = propSpecBytes[1];
          }
     }
     else if(!strcasecmp_P(hdrPropSpecStr, PSTR("BatchNumber"))) {
          if(dataByteCount != 5) {
               StatusError = 0x01;
          }
          else {
               memcpy(Picc.BatchNumber, propSpecBytes, 5);
          }
     }
     else if(!strcasecmp_P(hdrPropSpecStr, PSTR("ProductionDate"))) {
         if(dataByteCount != 2) {
               StatusError = 0x01;
          }
          else {
               Picc.ProductionWeek = propSpecBytes[0];
               Picc.ProductionYear = propSpecBytes[1];
          }
     }
     else {
          StatusError = 0x01;
     }
     if(StatusError) {
          CommandDESFireGetHeaderProperty(OutParam);
          return COMMAND_ERR_INVALID_USAGE_ID;
     }
     return COMMAND_INFO_OK_ID;
}

CommandStatusIdType CommandDESFireLayoutPPrint(char *OutParam, const char *InParams) {
     char pprintListSpecStr[32];
     BYTE StatusError = 0x00;
     if(!sscanf_P(InParams, PSTR("%31s"), pprintListSpecStr)) {
          StatusError = 0x01;
     }
     else {
          pprintListSpecStr[31] = '\0';
          if(!strcasecmp_P(pprintListSpecStr, PSTR("FullImage"))) {
               PrettyPrintPICCImageData((BYTE *) OutParam, TERMINAL_BUFFER_SIZE, 0x00);
          }
          else if(!strcasecmp_P(pprintListSpecStr, PSTR("HeaderData"))) {
               PrettyPrintPICCHeaderData((BYTE *) OutParam, TERMINAL_BUFFER_SIZE, 0x01);
          }
          else if(!strcasecmp_P(pprintListSpecStr, PSTR("ListDirs"))) {
               PrettyPrintPICCAppDirsFull((BYTE *) OutParam, TERMINAL_BUFFER_SIZE, 0x01);
          }
          else {
               StatusError = 0x01;
          }
     }
     if(StatusError) {
          snprintf_P(OutParam, TERMINAL_BUFFER_SIZE, 
                     PSTR("%s <FullImage|HeaderData|ListDirs>"), 
                     DFCOMMAND_LAYOUT_PPRINT);
          return COMMAND_ERR_INVALID_USAGE_ID;
     }
     return COMMAND_INFO_OK_WITH_TEXT_ID;
}

CommandStatusIdType CommandDESFireFirmwareInfo(char *OutParam) {
     snprintf_P(OutParam, TERMINAL_BUFFER_SIZE, 
                PSTR("Chameleon-Mini DESFire enabled firmware built on %s "
                     "based on %s from "
                     "https://github.com/maxieds/ChameleonMiniFirmwareDESFireStack.git.\n"
                     "Revision: %s\nLicense: GPLv3\n" 
                     "This is free software; see the source for copying conditions. "
                     "There is NO warranty; not even for MERCHANTABILITY nor "
                     "FITNESS FOR A PARTICULAR PURPOSE."), 
                DESFIRE_FIRMWARE_BUILD_TIMESTAMP, 
                DESFIRE_FIRMWARE_GIT_COMMIT_ID, 
                DESFIRE_FIRMWARE_REVISION);
     return COMMAND_INFO_OK_WITH_TEXT_ID;
}

CommandStatusIdType CommandDESFireGetLoggingMode(char *OutParam) {
    switch(LocalLoggingMode) {
         case OFF:
              snprintf_P(OutParam, TERMINAL_BUFFER_SIZE, PSTR("%s"), PSTR("OFF"));
              break;
         case NORMAL:
              snprintf_P(OutParam, TERMINAL_BUFFER_SIZE, PSTR("%s"), PSTR("NORMAL"));
              break;
         case VERBOSE:
              snprintf_P(OutParam, TERMINAL_BUFFER_SIZE, PSTR("%s"), PSTR("VERBOSE"));
              break;
         case DEBUGGING:
              snprintf_P(OutParam, TERMINAL_BUFFER_SIZE, PSTR("%s"), PSTR("DEBUGGING"));
              break;
         default:
              break;
    }
    return COMMAND_INFO_OK_WITH_TEXT_ID;
}

CommandStatusIdType CommandDESFireSetLoggingMode(char *OutParam, const char *InParams) {
     char valueStr[16];
     if(!sscanf_P(InParams, PSTR("%15s"), valueStr)) {
          return COMMAND_ERR_INVALID_PARAM_ID;
     }
     valueStr[15] = '\0';
     if(!strcasecmp_P(valueStr, PSTR("1")) || !strcasecmp_P(valueStr, PSTR("TRUE")) || 
        !strcasecmp_P(valueStr, PSTR("ON"))) {
          LocalLoggingMode = NORMAL;
          return COMMAND_INFO_OK_ID;
     }
     else if(!strcasecmp_P(valueStr, PSTR("0")) || !strcasecmp_P(valueStr, PSTR("FALSE")) || 
             !strcasecmp_P(valueStr, PSTR("OFF"))) {
          LocalLoggingMode = OFF;
          return COMMAND_INFO_OK_ID;
     }
     else if(!strcasecmp_P(valueStr, PSTR("VERBOSE"))) {
          LocalLoggingMode = VERBOSE;
          return COMMAND_INFO_OK_ID;
     }
     else if(!strcasecmp_P(valueStr, PSTR("DEBUGGING"))) {
          LocalLoggingMode = DEBUGGING;
          return COMMAND_INFO_OK_ID;
     }
     else {
          snprintf_P(OutParam, TERMINAL_BUFFER_SIZE, PSTR("%s <ON|OFF|VERBOSE|DEBUGGING>"), 
                     DFCOMMAND_LOGGING_MODE);
          return COMMAND_ERR_INVALID_USAGE_ID;
     }
}

CommandStatusIdType CommandDESFireGetTestingMode(char *OutParam) {
     if(LocalTestingMode) {
          return COMMAND_INFO_TRUE_ID;
     }
     return COMMAND_INFO_FALSE_ID;
}

CommandStatusIdType CommandDESFireSetTestingMode(char *OutParam, const char *InParams) {
     char valueStr[16];
     if(!sscanf_P(InParams, PSTR("%15s"), valueStr)) {
          return COMMAND_ERR_INVALID_PARAM_ID;
     }
     valueStr[15] = '\0';
     if(!strcasecmp_P(valueStr, PSTR("1")) || !strcasecmp_P(valueStr, PSTR("TRUE")) || 
        !strcasecmp_P(valueStr, PSTR("ON"))) {
          LocalTestingMode = 0x01;
          return COMMAND_INFO_TRUE_ID;
     }
     else if(!strcasecmp_P(valueStr, PSTR("0")) || !strcasecmp_P(valueStr, PSTR("FALSE")) || 
             !strcasecmp_P(valueStr, PSTR("OFF"))) {
          LocalTestingMode = 0x00;
          return COMMAND_INFO_FALSE_ID;
     }
     return COMMAND_ERR_INVALID_USAGE_ID;
}

