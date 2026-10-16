#include "serial.h"

#define COM1 0x3f8
#define UART_LINE_STATUS 5
#define UART_TRANSMIT_EMPTY 0x20
#define UART_TIMEOUT 1000000U

static void outb(uint16_t port, uint8_t value)
{
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static uint8_t inb(uint16_t port)
{
    uint8_t value;
    __asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

void orion_serial_init(void)
{
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x80);
    outb(COM1 + 0, 0x01);
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);
    outb(COM1 + 2, 0xc7);
    outb(COM1 + 4, 0x0b);
}

bool orion_serial_putc(char value)
{
    uint32_t timeout = UART_TIMEOUT;
    while ((inb(COM1 + UART_LINE_STATUS) & UART_TRANSMIT_EMPTY) == 0 &&
           timeout != 0) {
        --timeout;
    }
    if (timeout == 0) {
        return false;
    }
    outb(COM1, (uint8_t)value);
    return true;
}

bool orion_serial_write(const char *text, uint32_t limit)
{
    if (text == 0) {
        return false;
    }

    for (uint32_t index = 0; index < limit && text[index] != '\0'; ++index) {
        if (text[index] == '\n' && !orion_serial_putc('\r')) {
            return false;
        }
        if (!orion_serial_putc(text[index])) {
            return false;
        }
    }
    return true;
}
