#include "ne2k_pci.h"

#include <stddef.h>

#include "../../arch/x86_64/interrupt_state.h"
#include "../bus/pci.h"

#define NE2K_VENDOR_ID UINT16_C(0x10ec)
#define NE2K_DEVICE_ID UINT16_C(0x8029)
#define PCI_COMMAND_OFFSET UINT8_C(0x04)
#define PCI_COMMAND_IO UINT32_C(0x0001)
#define NE2K_REGISTER_COUNT 32U
#define NE2K_CR 0x00U
#define NE2K_PSTART 0x01U
#define NE2K_PSTOP 0x02U
#define NE2K_BNRY 0x03U
#define NE2K_TPSR 0x04U
#define NE2K_TBCR0 0x05U
#define NE2K_TBCR1 0x06U
#define NE2K_ISR 0x07U
#define NE2K_RSAR0 0x08U
#define NE2K_RSAR1 0x09U
#define NE2K_RBCR0 0x0aU
#define NE2K_RBCR1 0x0bU
#define NE2K_RCR 0x0cU
#define NE2K_TCR 0x0dU
#define NE2K_DCR 0x0eU
#define NE2K_IMR 0x0fU
#define NE2K_DATA 0x10U
#define NE2K_RESET 0x1fU
#define NE2K_CR_STOP 0x01U
#define NE2K_CR_START 0x02U
#define NE2K_CR_TXP 0x04U
#define NE2K_CR_RD0 0x08U
#define NE2K_CR_RD1 0x10U
#define NE2K_CR_NODMA 0x20U
#define NE2K_CR_PAGE1 0x40U
#define NE2K_ISR_RDC 0x40U
#define NE2K_ISR_RST 0x80U
#define NE2K_ISR_TXE 0x08U
#define NE2K_ISR_PTX 0x02U
#define NE2K_RCR_MON 0x20U
#define NE2K_RCR_AB 0x04U
#define NE2K_TCR_LB0 0x02U
#define NE2K_DCR_WORD_TRANSFER 0x49U
#define NE2K_RX_START_PAGE 0x46U
#define NE2K_RX_STOP_PAGE 0x80U
#define NE2K_TX_START_PAGE 0x40U
#define NE2K_POLL_LIMIT 1000000U
#define NE2K_PROM_BYTES 32U
#define NE2K_FRAME_MAX_BYTES ORION_NET_MAX_FRAME_BYTES
#define NE2K_FRAME_MIN_BYTES 60U
#define NE2K_MAX_RX_PACKET_BYTES (NE2K_FRAME_MAX_BYTES + NE2K_RX_FCS_BYTES)
#define NE2K_RX_HEADER_BYTES 4U
#define NE2K_RX_FCS_BYTES 4U

static uint16_t io_base;
static uint8_t receive_buffer[NE2K_FRAME_MAX_BYTES]
    __attribute__((aligned(8)));
static uint8_t prom_buffer[NE2K_PROM_BYTES]
    __attribute__((aligned(8)));
static bool initialized;
static bool command_busy;
static orion_net_interface interface;
static uint64_t tx_errors;
static uint64_t rx_errors;
static uint64_t rx_dropped;

static void increment(uint64_t *value)
{
    if (*value != UINT64_MAX) {
        ++*value;
    }
}

static void out8(uint16_t port, uint8_t value)
{
    __asm__ volatile("outb %0, %1"
                     :
                     : "a"(value), "Nd"(port)
                     : "memory");
}

static uint8_t in8(uint16_t port)
{
    uint8_t value;
    __asm__ volatile("inb %1, %0"
                     : "=a"(value)
                     : "Nd"(port)
                     : "memory");
    return value;
}

static uint16_t in16(uint16_t port)
{
    uint16_t value;
    __asm__ volatile("inw %1, %0"
                     : "=a"(value)
                     : "Nd"(port)
                     : "memory");
    return value;
}

static void out16(uint16_t port, uint16_t value)
{
    __asm__ volatile("outw %0, %1"
                     :
                     : "a"(value), "Nd"(port)
                     : "memory");
}

static void register_write(uint8_t offset, uint8_t value)
{
    out8((uint16_t)(io_base + offset), value);
}

static uint8_t register_read(uint8_t offset)
{
    return in8((uint16_t)(io_base + offset));
}

static bool command_acquire(void)
{
    const bool was_enabled = orion_x86_64_interrupts_save_disable();
    if (command_busy) {
        orion_x86_64_interrupts_restore(was_enabled);
        return false;
    }
    command_busy = true;
    orion_x86_64_interrupts_restore(was_enabled);
    return true;
}

static void command_release(void)
{
    const bool was_enabled = orion_x86_64_interrupts_save_disable();
    command_busy = false;
    orion_x86_64_interrupts_restore(was_enabled);
}

static bool wait_remote_dma(void)
{
    for (uint32_t attempt = 0; attempt < NE2K_POLL_LIMIT; ++attempt) {
        if ((register_read(NE2K_ISR) & NE2K_ISR_RDC) != 0) {
            register_write(NE2K_ISR, NE2K_ISR_RDC);
            return true;
        }
    }
    return false;
}

static bool remote_read(uint16_t address, uint8_t *destination,
                        uint16_t length)
{
    if (destination == NULL || length == 0) {
        return false;
    }
    register_write(NE2K_CR,
                   NE2K_CR_START | NE2K_CR_NODMA | NE2K_CR_PAGE1);
    register_write(NE2K_CR,
                   NE2K_CR_START | NE2K_CR_NODMA);
    register_write(NE2K_RSAR0, (uint8_t)address);
    register_write(NE2K_RSAR1, (uint8_t)(address >> 8));
    register_write(NE2K_RBCR0, (uint8_t)length);
    register_write(NE2K_RBCR1, (uint8_t)(length >> 8));
    register_write(NE2K_ISR, NE2K_ISR_RDC);
    register_write(NE2K_CR, NE2K_CR_START | NE2K_CR_RD0);
    uint32_t index = 0;
    while (index + 1U < length) {
        const uint16_t word = in16((uint16_t)(io_base + NE2K_DATA));
        destination[index++] = (uint8_t)word;
        destination[index++] = (uint8_t)(word >> 8);
    }
    if (index < length) {
        destination[index] =
            (uint8_t)in16((uint16_t)(io_base + NE2K_DATA));
    }
    return wait_remote_dma();
}

static bool remote_write(uint16_t address, const uint8_t *source,
                         uint16_t length)
{
    if (source == NULL || length == 0) {
        return false;
    }
    register_write(NE2K_CR,
                   NE2K_CR_START | NE2K_CR_NODMA | NE2K_CR_PAGE1);
    register_write(NE2K_CR,
                   NE2K_CR_START | NE2K_CR_NODMA);
    register_write(NE2K_RSAR0, (uint8_t)address);
    register_write(NE2K_RSAR1, (uint8_t)(address >> 8));
    register_write(NE2K_RBCR0, (uint8_t)length);
    register_write(NE2K_RBCR1, (uint8_t)(length >> 8));
    register_write(NE2K_ISR, NE2K_ISR_RDC);
    register_write(NE2K_CR, NE2K_CR_START | NE2K_CR_RD1);
    uint32_t index = 0;
    while (index + 1U < length) {
        const uint16_t word =
            (uint16_t)source[index] |
            (uint16_t)((uint16_t)source[index + 1U] << 8);
        out16((uint16_t)(io_base + NE2K_DATA), word);
        index += 2U;
    }
    if (index < length) {
        out16((uint16_t)(io_base + NE2K_DATA), source[index]);
    }
    return wait_remote_dma();
}

static uint8_t next_ring_page(uint8_t page, uint32_t bytes)
{
    const uint32_t pages =
        (bytes + UINT32_C(255)) / UINT32_C(256);
    uint32_t next = (uint32_t)page + pages;
    while (next >= NE2K_RX_STOP_PAGE) {
        next = NE2K_RX_START_PAGE + (next - NE2K_RX_STOP_PAGE);
    }
    return (uint8_t)next;
}

static void set_boundary(uint8_t next_page)
{
    uint8_t boundary = (uint8_t)(next_page - 1U);
    if (next_page == NE2K_RX_START_PAGE) {
        boundary = NE2K_RX_STOP_PAGE - 1U;
    }
    register_write(NE2K_CR,
                   NE2K_CR_START | NE2K_CR_NODMA | NE2K_CR_PAGE1);
    register_write(NE2K_CR,
                   NE2K_CR_START | NE2K_CR_NODMA);
    register_write(NE2K_BNRY, boundary);
}

static bool initialize_hardware(uint16_t base, uint8_t mac[6])
{
    io_base = base;
    const uint8_t reset_value = in8((uint16_t)(io_base + NE2K_RESET));
    out8((uint16_t)(io_base + NE2K_RESET), reset_value);
    bool reset_seen = false;
    for (uint32_t attempt = 0; attempt < NE2K_POLL_LIMIT; ++attempt) {
        if ((register_read(NE2K_ISR) & NE2K_ISR_RST) != 0) {
            reset_seen = true;
            break;
        }
    }
    if (!reset_seen) {
        return false;
    }
    register_write(NE2K_CR, NE2K_CR_STOP | NE2K_CR_NODMA);
    register_write(NE2K_IMR, 0);
    register_write(NE2K_ISR, UINT8_MAX);
    register_write(NE2K_DCR, NE2K_DCR_WORD_TRANSFER);
    register_write(NE2K_RBCR0, 0);
    register_write(NE2K_RBCR1, 0);
    register_write(NE2K_RCR, NE2K_RCR_MON);
    register_write(NE2K_TCR, NE2K_TCR_LB0);
    register_write(NE2K_PSTART, NE2K_RX_START_PAGE);
    register_write(NE2K_PSTOP, NE2K_RX_STOP_PAGE);
    register_write(NE2K_BNRY, NE2K_RX_START_PAGE);
    register_write(NE2K_TPSR, NE2K_TX_START_PAGE);
    if (!remote_read(0, prom_buffer, NE2K_PROM_BYTES)) {
        return false;
    }
    for (uint32_t index = 0; index < 6U; ++index) {
        mac[index] = prom_buffer[index * 2U];
        if (prom_buffer[index * 2U] != prom_buffer[index * 2U + 1U]) {
            return false;
        }
    }
    if ((mac[0] & 1U) != 0) {
        return false;
    }
    register_write(NE2K_CR,
                   NE2K_CR_START | NE2K_CR_NODMA | NE2K_CR_PAGE1);
    for (uint32_t index = 0; index < 6U; ++index) {
        register_write((uint8_t)(1U + index), mac[index]);
    }
    register_write(7, (uint8_t)(NE2K_RX_START_PAGE + 1U));
    for (uint32_t index = 8; index < 16U; ++index) {
        register_write((uint8_t)index, 0);
    }
    register_write(NE2K_CR,
                   NE2K_CR_START | NE2K_CR_NODMA);
    register_write(NE2K_BNRY, NE2K_RX_START_PAGE);
    register_write(NE2K_ISR, UINT8_MAX);
    register_write(NE2K_RCR, NE2K_RCR_AB);
    register_write(NE2K_TCR, 0);
    register_write(NE2K_IMR, 0);
    register_write(NE2K_CR,
                   NE2K_CR_START | NE2K_CR_NODMA);
    return true;
}

static orion_net_status transmit_frame(void *context, const uint8_t *frame,
                                       uint32_t frame_bytes)
{
    (void)context;
    if (!initialized || frame == NULL ||
        frame_bytes < NE2K_FRAME_MIN_BYTES ||
        frame_bytes > NE2K_FRAME_MAX_BYTES) {
        return ORION_NET_INVALID;
    }
    if (!command_acquire()) {
        return ORION_NET_BUSY;
    }
    register_write(NE2K_ISR, NE2K_ISR_PTX | NE2K_ISR_TXE);
    const bool copied = remote_write(
        (uint16_t)((uint16_t)NE2K_TX_START_PAGE << 8), frame,
        (uint16_t)frame_bytes);
    if (!copied) {
        increment(&tx_errors);
        command_release();
        return ORION_NET_IO_ERROR;
    }
    register_write(NE2K_TBCR0, (uint8_t)frame_bytes);
    register_write(NE2K_TBCR1, (uint8_t)(frame_bytes >> 8));
    register_write(NE2K_CR,
                   NE2K_CR_START | NE2K_CR_TXP | NE2K_CR_NODMA);
    for (uint32_t attempt = 0; attempt < NE2K_POLL_LIMIT; ++attempt) {
        const uint8_t status = register_read(NE2K_ISR);
        if ((status & NE2K_ISR_TXE) != 0) {
            register_write(NE2K_ISR, NE2K_ISR_TXE);
            increment(&tx_errors);
            command_release();
            return ORION_NET_IO_ERROR;
        }
        if ((status & NE2K_ISR_PTX) != 0) {
            register_write(NE2K_ISR, NE2K_ISR_PTX);
            command_release();
            return ORION_NET_OK;
        }
    }
    increment(&tx_errors);
    command_release();
    return ORION_NET_TIMEOUT;
}

static bool receive_ring_copy(uint16_t address, uint8_t *destination,
                              uint16_t length)
{
    const uint16_t ring_end =
        (uint16_t)((uint16_t)NE2K_RX_STOP_PAGE << 8);
    const uint16_t ring_start =
        (uint16_t)((uint16_t)NE2K_RX_START_PAGE << 8);
    const uint32_t until_end = (uint32_t)ring_end - address;
    const uint16_t first_part =
        (uint16_t)(length < until_end ? length : until_end);
    if (!remote_read(address, destination, first_part)) {
        return false;
    }
    const uint16_t remainder = (uint16_t)(length - first_part);
    return remainder == 0 ||
           remote_read(ring_start, destination + first_part, remainder);
}

static orion_net_status poll_frames(void *context,
                                    orion_net_frame_receiver receiver,
                                    void *receiver_context, uint32_t budget,
                                    uint32_t *frames_received)
{
    (void)context;
    if (!initialized || receiver == NULL || budget == 0 ||
        frames_received == NULL) {
        return ORION_NET_INVALID;
    }
    if (!command_acquire()) {
        return ORION_NET_BUSY;
    }
    *frames_received = 0;
    orion_net_status result = ORION_NET_OK;
    for (uint32_t processed = 0; processed < budget; ++processed) {
        register_write(NE2K_CR,
                       NE2K_CR_START | NE2K_CR_NODMA | NE2K_CR_PAGE1);
        const uint8_t current_page = register_read(7);
        register_write(NE2K_CR,
                       NE2K_CR_START | NE2K_CR_NODMA);
        const uint8_t boundary = register_read(NE2K_BNRY);
        if (boundary < NE2K_RX_START_PAGE ||
            boundary >= NE2K_RX_STOP_PAGE ||
            current_page < NE2K_RX_START_PAGE ||
            current_page >= NE2K_RX_STOP_PAGE) {
            increment(&rx_dropped);
            set_boundary(NE2K_RX_STOP_PAGE - 1U);
            result = ORION_NET_IO_ERROR;
            break;
        }
        uint8_t packet_page = (uint8_t)(boundary + 1U);
        if (packet_page >= NE2K_RX_STOP_PAGE) {
            packet_page = NE2K_RX_START_PAGE;
        }
        if (packet_page == current_page) {
            break;
        }
        uint8_t header[NE2K_RX_HEADER_BYTES];
        if (!remote_read((uint16_t)((uint16_t)packet_page << 8),
                         header, sizeof(header))) {
            result = ORION_NET_IO_ERROR;
            increment(&rx_errors);
            break;
        }
        const uint8_t status = header[0];
        const uint8_t next_page = header[1];
        const uint16_t packet_bytes =
            (uint16_t)header[2] | (uint16_t)((uint16_t)header[3] << 8);
        const uint32_t frame_bytes =
            packet_bytes >= NE2K_RX_FCS_BYTES
                ? packet_bytes - NE2K_RX_FCS_BYTES
                : 0;
        const uint8_t expected_next_page = next_ring_page(
            packet_page, NE2K_RX_HEADER_BYTES + packet_bytes);
        if (next_page < NE2K_RX_START_PAGE ||
            next_page >= NE2K_RX_STOP_PAGE ||
            packet_bytes < NE2K_FRAME_MIN_BYTES + NE2K_RX_FCS_BYTES ||
            packet_bytes > NE2K_MAX_RX_PACKET_BYTES ||
            frame_bytes > sizeof(receive_buffer) ||
            next_page != expected_next_page ||
            (status & 0x1fU) != 0x01U) {
            increment(&rx_dropped);
            const uint32_t bounded_packet_bytes =
                packet_bytes <= NE2K_MAX_RX_PACKET_BYTES
                    ? packet_bytes
                    : NE2K_MAX_RX_PACKET_BYTES;
            const uint8_t recovered = next_ring_page(
                packet_page, NE2K_RX_HEADER_BYTES + bounded_packet_bytes);
            set_boundary(recovered);
            continue;
        }
        const uint16_t frame_address =
            (uint16_t)(((uint16_t)packet_page << 8) +
                       NE2K_RX_HEADER_BYTES);
        if (!receive_ring_copy(frame_address, receive_buffer,
                               (uint16_t)frame_bytes)) {
            result = ORION_NET_IO_ERROR;
            increment(&rx_errors);
            break;
        }
        set_boundary(next_page);
        command_release();
        receiver(receiver_context, receive_buffer, frame_bytes);
        ++*frames_received;
        if (!command_acquire()) {
            result = ORION_NET_BUSY;
            break;
        }
    }
    command_release();
    return result;
}

static bool driver_match(const orion_device *device)
{
    return device != NULL && device->bus == ORION_DEVICE_PCI &&
           device->vendor_id == NE2K_VENDOR_ID &&
           device->device_id == NE2K_DEVICE_ID;
}

static bool driver_start(const orion_device *device, void **driver_data)
{
    if (!driver_match(device) || driver_data == NULL ||
        device->bar_count == 0 || (device->bars[0] & 1U) == 0) {
        return false;
    }
    const uint32_t base = device->bars[0] & ~UINT32_C(3);
    if (base < 0x100U || base > UINT16_MAX - (NE2K_REGISTER_COUNT - 1U)) {
        return false;
    }
    uint32_t command = 0;
    if (!orion_pci_read_config_legacy(NULL, device->bus_number,
                                     device->device_number,
                                     device->function_number,
                                     PCI_COMMAND_OFFSET, &command) ||
        !orion_pci_write_config_legacy(
            NULL, device->bus_number, device->device_number,
            device->function_number, PCI_COMMAND_OFFSET,
            (command & UINT32_C(0x0000ffff)) | PCI_COMMAND_IO)) {
        return false;
    }
    uint8_t mac[6];
    if (!initialize_hardware((uint16_t)base, mac)) {
        return false;
    }
    interface = (orion_net_interface){
        .name = "ne2k-pci0",
        .mac = {mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]},
        .mtu = ORION_NET_MTU,
        .available = true,
        .context = NULL,
        .transmit = transmit_frame,
        .poll = poll_frames
    };
    initialized = true;
    if (orion_net_attach(&interface) != ORION_NET_OK) {
        initialized = false;
        interface.available = false;
        return false;
    }
    *driver_data = &interface;
    return true;
}

static void driver_cleanup(const orion_device *device, void *driver_data)
{
    (void)device;
    (void)driver_data;
    register_write(NE2K_IMR, 0);
    register_write(NE2K_RCR, NE2K_RCR_MON);
    register_write(NE2K_CR, NE2K_CR_STOP | NE2K_CR_NODMA);
    interface.available = false;
    initialized = false;
}

static const orion_driver ne2k_driver = {
    .name = "ne2k-pci-rtl8029",
    .match = driver_match,
    .start = driver_start,
    .cleanup = driver_cleanup
};

const orion_driver *orion_ne2k_pci_driver(void)
{
    return &ne2k_driver;
}

orion_net_interface *orion_ne2k_pci_interface(void)
{
    return initialized ? &interface : NULL;
}
