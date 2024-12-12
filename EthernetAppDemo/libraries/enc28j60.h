#ifndef _ENC28J60_LIBRARY_
#define _ENC28J60_LIBRARY_

#include <furi.h>
#include <furi/core/log.h>
#include <furi_hal.h>

#include "Spi_lib.h"

typedef struct {
    FuriHalSpiBusHandle* spi;
} enc28j60;

#endif
