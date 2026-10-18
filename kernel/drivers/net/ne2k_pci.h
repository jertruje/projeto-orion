#ifndef ORION_KERNEL_DRIVERS_NET_NE2K_PCI_H
#define ORION_KERNEL_DRIVERS_NET_NE2K_PCI_H

#include "../core/driver.h"
#include "../../net/network.h"

const orion_driver *orion_ne2k_pci_driver(void);
orion_net_interface *orion_ne2k_pci_interface(void);

#endif
