#ifndef ORION_SERIAL_H
#define ORION_SERIAL_H

#include <stdbool.h>
#include <stdint.h>

void orion_serial_init(void);
bool orion_serial_write(const char *text, uint32_t limit);
bool orion_serial_putc(char value);

#endif
