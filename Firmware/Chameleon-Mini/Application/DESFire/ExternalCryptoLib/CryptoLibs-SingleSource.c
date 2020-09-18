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
 * CryptoLibs-SingleSource.c 
 * Maxie D. Schmidt (github.com/maxieds)
 */

#ifndef __CRYPTOLIBS_SINGLE_SOURCE_C__
#define __CRYPTOLIBS_SINGLE_SOURCE_C__

#include "AVRCryptoLib/gf256mul/gf256mul.h"

#include "AVRCryptoLib/memxor/memxor.h"
#include "AVRCryptoLib/memxor/memxor_c.c"

#include "AVRCryptoLib/des/des.h"
#include "AVRCryptoLib/des/des.c"

#include "ArduinoCryptoLib/aes/aes-common.h"
#include "ArduinoCryptoLib/aes/aes128.h"
#include "ArduinoCryptoLib/aes/ProgMemUtil.h"

#include "ArduinoCryptoLib/aes/aes-common.c"
#include "ArduinoCryptoLib/aes/aes128.c"

#include "ArduinoCryptoLib/aes/aes-cmac.h"
#include "ArduinoCryptoLib/aes/aes-cmac.c"

#endif