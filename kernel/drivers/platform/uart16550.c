#include "uart16550.h"

#include <stddef.h>

#include "../../console/serial.h"

static bool platform_name_matches(const char *left, const char *right)
{
    uint32_t index = 0;
    while (left[index] != '\0' && right[index] != '\0') {
        if (left[index] != right[index]) {
            return false;
        }
        ++index;
    }
    return left[index] == right[index];
}

static bool uart_matches(const orion_device *device)
{
    return device != NULL && device->bus == ORION_DEVICE_PLATFORM &&
           platform_name_matches(device->platform_name,
                                 "uart16550-com1");
}

static bool uart_start(const orion_device *device, void **driver_data)
{
    if (!uart_matches(device) || driver_data == NULL) {
        return false;
    }
    orion_serial_init();
    static const char ready_message[] =
        "[ORION][DRIVER][INFO] 16550-compatible COM1 driver online\n";
    if (!orion_serial_write(ready_message, sizeof(ready_message) - 1U)) {
        return false;
    }
    *driver_data = NULL;
    return true;
}

static void uart_cleanup(const orion_device *device, void *driver_data)
{
    (void)device;
    (void)driver_data;
}

static const orion_driver uart_driver = {
    .name = "uart16550-com1",
    .match = uart_matches,
    .start = uart_start,
    .cleanup = uart_cleanup
};

const orion_driver *orion_uart16550_driver(void)
{
    return &uart_driver;
}
