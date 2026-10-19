#ifndef ORION_DRIVERS_GRAPHICS_VGA_H
#define ORION_DRIVERS_GRAPHICS_VGA_H

#include <stdbool.h>
#include <stdint.h>

bool orion_vga_graphics_init(void);
void orion_vga_set_pixel(uint32_t x, uint32_t y, uint8_t color);
void orion_vga_fill_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint8_t color);
void orion_vga_clear(uint8_t color);

#endif
