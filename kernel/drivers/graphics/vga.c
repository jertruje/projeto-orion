#include "vga.h"

#include <stddef.h>
#include <stdint.h>

#include "../arch/x86_64/interrupt_state.h"

static volatile uint16_t* const vga_buffer = (volatile uint16_t*)0xB8000;

#define VGA_WIDTH 80
#define VGA_HEIGHT 25

bool orion_vga_graphics_init(void)
{
    // Clear the buffer on init
    orion_vga_clear(0x00);
    return true;
}

void orion_vga_set_pixel(uint32_t x, uint32_t y, uint8_t color)
{
    if (x >= VGA_WIDTH || y >= VGA_HEIGHT) {
        return;
    }
    volatile uint16_t* ptr = &vga_buffer[y * VGA_WIDTH + x];
    // Character: solid block (0xDB)
    // Attribute: foreground = color (lower 4 bits), background = 0 (black) 
    // Actually we want the pixel to be the color; we can set foreground to color and background to 0.
    // Or we can set background to color and foreground to white for visibility.
    // Let's set foreground to color and background to 0 (so only foreground color visible if we treat as pixel).
    // But the attribute byte: bits 0-3 foreground, 4-7 background.
    // We'll set background to 0, foreground to color.
    *ptr = (uint16_t)(0xDB00 | (color & 0x0F));
}

void orion_vga_fill_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint8_t color)
{
    for (uint32_t dy = 0; dy < height; ++dy) {
        for (uint32_t dx = 0; dx < width; ++dx) {
            orion_vga_set_pixel(x + dx, y + dy, color);
        }
    }
}

void orion_vga_clear(uint8_t color)
{
    for (uint32_t y = 0; y < VGA_HEIGHT; ++y) {
        for (uint32_t x = 0; x < VGA_WIDTH; ++x) {
            orion_vga_set_pixel(x, y, color);
        }
    }
}
