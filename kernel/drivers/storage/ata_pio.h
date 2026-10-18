#ifndef ORION_KERNEL_DRIVERS_STORAGE_ATA_PIO_H
#define ORION_KERNEL_DRIVERS_STORAGE_ATA_PIO_H

#include <stdbool.h>

#include "../core/driver.h"
#include "../../storage/block.h"

bool orion_ata_pio_prepare(void);
orion_block_device *orion_ata_pio_block_device(void);
const orion_driver *orion_ata_pio_driver(void);

#endif
