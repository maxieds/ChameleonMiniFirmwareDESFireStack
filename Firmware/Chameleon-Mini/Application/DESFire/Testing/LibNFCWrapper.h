/* LibNFCWrapper.h */

#ifndef __LIBNFC_WRAPPER_H__
#define __LIBNFC_WRAPPER_H__

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <err.h>

#include <nfc/nfc.h>

static inline nfc_device * GetNFCDeviceDriver(nfc_context **context) {
    nfc_init(context);
    if(context == NULL) {
        ERR("Unable to init libnfc (malloc)");
        return NULL;
    }
    nfc_device *pnd = nfc_open(*context, NULL);
    if(pnd == NULL) {
        ERR("Error opening NFC reader");
        nfc_exit(context);
        return NULL;
    }
    if(nfc_initiator_init(pnd) < 0) {
        nfc_perror(pnd, "nfc_initiator_init");
        nfc_close(pnd);
        nfc_exit(context);
        return NULL;
    }
    // Configure some convenient common settings:
    //nfc_pnd_set_property_bool(pnd, NP_ACTIVATE_FIELD, false);
    //nfc_pnd_set_property_bool(pnd, NP_HANDLE_CRC, true);
    //nfc_pnd_set_property_bool(pnd, NP_HANDLE_PARITY, true);
    //nfc_pnd_set_property_bool(pnd, NP_AUTO_ISO14443_4, true);
    //nfc_pnd_set_property_bool(pnd, NP_ACTIVATE_FIELD, true);
    return pnd;
}

static inline void FreeNFCDeviceDriver(nfc_context **context, nfc_device **device) {
    nfc_close(*device);
    nfc_exit(*context);
}

#endif