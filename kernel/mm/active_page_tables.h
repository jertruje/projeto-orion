#ifndef ORION_ACTIVE_PAGE_TABLES_H
#define ORION_ACTIVE_PAGE_TABLES_H

#include <stdbool.h>

#include <orion/boot_info.h>

bool orion_x86_64_active_page_tables_smoke_test(
    const orion_boot_info *boot_info, bool nx_enabled, bool *performed);

#endif
