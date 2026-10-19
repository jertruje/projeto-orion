#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "vga.h"

// Mock VGA buffer for testing
// In a real test, we would need to map this to actual memory or use a mock
// For this test, we'll just verify that the functions don't crash and produce
// the expected behavior in terms of bounds checking

static bool test_basic_operations(void)
{
    // Initialize VGA graphics
    assert(orion_vga_graphics_init() == true);
    
    // Test setting a pixel within bounds
    orion_vga_set_pixel(10, 10, 0x0F);  // White color
    // If we got here without crashing, the bounds check passed
    
    // Test setting a pixel at the edges
    orion_vga_set_pixel(0, 0, 0x01);    // Top-left corner
    orion_vga_set_pixel(79, 0, 0x02);   // Top-right corner (VGA_WIDTH-1, 0)
    orion_vga_set_pixel(0, 24, 0x03);   // Bottom-left corner (0, VGA_HEIGHT-1)
    orion_vga_set_pixel(79, 24, 0x04);  // Bottom-right corner
    
    // Test setting pixels outside bounds (should be ignored)
    orion_vga_set_pixel(80, 0, 0x05);   // x == VGA_WIDTH (out of bounds)
    orion_vga_set_pixel(0, 25, 0x06);   // y == VGA_HEIGHT (out of bounds)
    orion_vga_set_pixel(-1, 0, 0x07);   // Negative x (out of bounds)
    orion_vga_set_pixel(0, -1, 0x08);   // Negative y (out of bounds)
    
    // Test filling a rectangle within bounds
    orion_vga_fill_rect(10, 10, 20, 15, 0x0A);
    
    // Test filling a rectangle at the edges
    orion_vga_fill_rect(0, 0, 5, 5, 0x0B);
    orion_vga_fill_rect(75, 0, 5, 5, 0x0C);   // Near right edge
    orion_vga_fill_rect(0, 20, 5, 5, 0x0D);   // Near bottom edge
    orion_vga_fill_rect(75, 20, 5, 5, 0x0E);  // Near bottom-right corner
    
    // Test filling a rectangle outside bounds (should be handled gracefully)
    orion_vga_fill_rect(80, 0, 10, 10, 0x0F); // x == VGA_WIDTH (out of bounds)
    orion_vga_fill_rect(0, 25, 10, 10, 0x0F); // y == VGA_HEIGHT (out of bounds)
    orion_vga_fill_rect(-10, 0, 10, 10, 0x0F); // Negative x (out of bounds)
    orion_vga_fill_rect(0, -10, 10, 10, 0x0F); // Negative y (out of bounds)
    orion_vga_fill_rect(70, 20, 20, 10, 0x0F); // Partially out of bounds (x+width > VGA_WIDTH)
    orion_vga_fill_rect(10, 15, 10, 20, 0x0F); // Partially out of bounds (y+height > VGA_HEIGHT)
    
    // Test clearing the screen with different colors
    orion_vga_clear(0x00);  // Black
    orion_vga_clear(0x0F);  // White
    orion_vga_clear(0x01);  // Blue
    orion_vga_clear(0x02);  // Green
    orion_vga_clear(0x04);  // Red
    
    return true;
}

static void test_pixel_addressing(void)
{
    // Test that we understand the VGA memory layout
    // In text mode, each character cell is 2 bytes:
    //   Byte 0: ASCII character
    //   Byte 1: Attribute (background in bits 4-7, foreground in bits 0-3)
    //
    // Our implementation puts the 'pixel' in the attribute byte and uses
    // a fixed character (0xDB = solid block) for the ASCII byte
    
    // This test is more of a sanity check - we're not actually going to
    // read the VGA memory in this test environment, but we can at least
    // verify that our coordinate calculations are correct
    
    // For a 80x25 text mode buffer at 0xB8000:
    //   Offset = (y * 80 + x) * 2
    //   Character byte = offset
    //   Attribute byte = offset + 1
    
    // Since we can't actually access 0xB8000 in this test environment,
    // we'll just verify that our functions don't crash when called
    // with various coordinates
    
    orion_vga_clear(0x00);
    
    // Draw a diagonal line
    for (int i = 0; i < 20 && i < 80 && i < 25; i++) {
        orion_vga_set_pixel(i, i, 0x0F);
    }
    
    // Draw a horizontal line
    for (int i = 0; i < 40 && i < 80; i++) {
        orion_vga_set_pixel(i, 10, 0x0A);
    }
    
    // Draw a vertical line
    for (int i = 0; i < 12 && i < 25; i++) {
        orion_vga_set_pixel(20, i, 0x0C);
    }
    
    // Fill a rectangle
    orion_vga_fill_rect(30, 30, 10, 10, 0x0E);
}

int main(void)
{
    test_basic_operations();
    test_pixel_addressing();
    return 0;
}