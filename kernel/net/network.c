#include "network.h"

#include <stddef.h>

#include "../arch/x86_64/interrupt_state.h"

#define ETHERNET_HEADER_BYTES 14U
#define ETHERNET_MIN_FRAME_BYTES 60U
#define ARP_PACKET_BYTES 28U
#define IPV4_HEADER_BYTES 20U
#define ICMP_HEADER_BYTES 8U
#define UDP_HEADER_BYTES 8U
#define ARP_ENTRY_LIFETIME_NS UINT64_C(60000000000)
#define PING_IDENTIFIER UINT16_C(0x4f52)
#define IPV4_PROTOCOL_ICMP 1U
#define IPV4_PROTOCOL_UDP 17U
#define ETHERNET_TYPE_IPV4 UINT16_C(0x0800)
#define ETHERNET_TYPE_ARP UINT16_C(0x0806)

typedef struct {
    uint32_t address;
    uint8_t mac[ORION_NET_MAC_BYTES];
    uint64_t learned_at;
    bool valid;
} arp_entry;

typedef struct {
    uint16_t port;
    orion_net_udp_receiver receiver;
    void *context;
    bool active;
} udp_endpoint;

static orion_net_interface *network_interface;
static orion_net_stats network_stats;
static arp_entry arp_cache[ORION_NET_ARP_CACHE_LIMIT];
static udp_endpoint udp_endpoints[ORION_NET_UDP_ENDPOINT_LIMIT];
static uint8_t transmit_buffer[ORION_NET_MAX_FRAME_BYTES]
    __attribute__((aligned(8)));
static uint8_t icmp_reply_buffer[ORION_NET_MTU]
    __attribute__((aligned(8)));
static uint32_t local_address;
static uint32_t subnet_mask;
static uint32_t default_gateway;
static orion_net_time_fn time_reader;
static void *time_context;
static bool configured;
static bool polling;
static bool transmitting;
static bool ping_waiting;
static bool ping_completed;
static uint16_t ping_sequence;
static uint32_t ping_destination;
static uint64_t ping_started_at;
static uint64_t ping_round_trip;

static uint16_t read_be16(const uint8_t *bytes)
{
    return (uint16_t)((uint16_t)bytes[0] << 8) | bytes[1];
}

static uint32_t read_be32(const uint8_t *bytes)
{
    return ((uint32_t)bytes[0] << 24) | ((uint32_t)bytes[1] << 16) |
           ((uint32_t)bytes[2] << 8) | bytes[3];
}

static void write_be16(uint8_t *bytes, uint16_t value)
{
    bytes[0] = (uint8_t)(value >> 8);
    bytes[1] = (uint8_t)value;
}

static void write_be32(uint8_t *bytes, uint32_t value)
{
    bytes[0] = (uint8_t)(value >> 24);
    bytes[1] = (uint8_t)(value >> 16);
    bytes[2] = (uint8_t)(value >> 8);
    bytes[3] = (uint8_t)value;
}

static bool bytes_equal(const uint8_t *left, const uint8_t *right,
                        uint32_t length)
{
    for (uint32_t index = 0; index < length; ++index) {
        if (left[index] != right[index]) {
            return false;
        }
    }
    return true;
}

static bool mac_is_unicast(const uint8_t *mac)
{
    uint8_t combined = 0;
    for (uint32_t index = 0; index < ORION_NET_MAC_BYTES; ++index) {
        combined |= mac[index];
    }
    return combined != 0 && (mac[0] & 1U) == 0;
}

static bool ipv4_is_unicast(uint32_t address)
{
    const uint8_t first = (uint8_t)(address >> 24);
    return address != 0 && address != UINT32_MAX &&
           first != 0 && first < 224U;
}

static void increment(uint64_t *value)
{
    if (*value != UINT64_MAX) {
        ++*value;
    }
}

static bool now_nanoseconds(uint64_t *now)
{
    return time_reader != NULL && time_reader(time_context, now);
}

uint16_t orion_net_checksum(const uint8_t *bytes, uint32_t length)
{
    if (bytes == NULL && length != 0) {
        return 0;
    }
    uint32_t sum = 0;
    uint32_t index = 0;
    while (length - index >= 2U) {
        sum += ((uint32_t)bytes[index] << 8) | bytes[index + 1U];
        index += 2U;
    }
    if (index < length) {
        sum += (uint32_t)bytes[index] << 8;
    }
    while ((sum >> 16) != 0) {
        sum = (sum & UINT32_C(0xffff)) + (sum >> 16);
    }
    return (uint16_t)~sum;
}

bool orion_net_checksum_valid(const uint8_t *bytes, uint32_t length)
{
    return bytes != NULL && orion_net_checksum(bytes, length) == 0;
}

static orion_net_status transmit_ethernet(const uint8_t *destination_mac,
                                          uint16_t ether_type,
                                          const uint8_t *payload,
                                          uint32_t payload_bytes)
{
    if (!orion_net_is_available()) {
        return ORION_NET_UNAVAILABLE;
    }
    if (destination_mac == NULL ||
        (payload == NULL && payload_bytes != 0) ||
        payload_bytes > network_interface->mtu ||
        payload_bytes + ETHERNET_HEADER_BYTES >
            sizeof(transmit_buffer)) {
        return ORION_NET_TOO_LARGE;
    }
    if (transmitting) {
        return ORION_NET_BUSY;
    }
    transmitting = true;
    for (uint32_t index = 0; index < ORION_NET_MAC_BYTES; ++index) {
        transmit_buffer[index] = destination_mac[index];
        transmit_buffer[index + ORION_NET_MAC_BYTES] =
            network_interface->mac[index];
    }
    write_be16(transmit_buffer + 12U, ether_type);
    for (uint32_t index = 0; index < payload_bytes; ++index) {
        transmit_buffer[ETHERNET_HEADER_BYTES + index] = payload[index];
    }
    uint32_t frame_bytes = ETHERNET_HEADER_BYTES + payload_bytes;
    while (frame_bytes < ETHERNET_MIN_FRAME_BYTES) {
        transmit_buffer[frame_bytes++] = 0;
    }
    const orion_net_status result =
        network_interface->transmit(network_interface->context,
                                    transmit_buffer, frame_bytes);
    transmitting = false;
    if (result == ORION_NET_OK) {
        increment(&network_stats.tx_frames);
    } else {
        increment(&network_stats.tx_errors);
    }
    return result;
}

static bool arp_cache_lookup(uint32_t address, uint8_t mac_out[6])
{
    uint64_t now = 0;
    const bool has_time = now_nanoseconds(&now);
    for (uint32_t index = 0; index < ORION_NET_ARP_CACHE_LIMIT; ++index) {
        arp_entry *entry = &arp_cache[index];
        if (!entry->valid) {
            continue;
        }
        if (has_time && now >= entry->learned_at &&
            now - entry->learned_at >= ARP_ENTRY_LIFETIME_NS) {
            entry->valid = false;
            continue;
        }
        if (entry->address == address) {
            for (uint32_t byte = 0; byte < ORION_NET_MAC_BYTES; ++byte) {
                mac_out[byte] = entry->mac[byte];
            }
            return true;
        }
    }
    return false;
}

static void arp_cache_update(uint32_t address, const uint8_t mac[6])
{
    if (!ipv4_is_unicast(address) || !mac_is_unicast(mac)) {
        return;
    }
    uint64_t now = 0;
    if (!now_nanoseconds(&now)) {
        return;
    }
    uint32_t selected = ORION_NET_ARP_CACHE_LIMIT;
    for (uint32_t index = 0; index < ORION_NET_ARP_CACHE_LIMIT; ++index) {
        if (arp_cache[index].valid && arp_cache[index].address == address) {
            selected = index;
            break;
        }
        if (!arp_cache[index].valid && selected == ORION_NET_ARP_CACHE_LIMIT) {
            selected = index;
        }
    }
    if (selected == ORION_NET_ARP_CACHE_LIMIT) {
        selected = 0;
        for (uint32_t index = 1; index < ORION_NET_ARP_CACHE_LIMIT; ++index) {
            if (arp_cache[index].learned_at <
                arp_cache[selected].learned_at) {
                selected = index;
            }
        }
    }
    arp_entry *entry = &arp_cache[selected];
    entry->address = address;
    for (uint32_t byte = 0; byte < ORION_NET_MAC_BYTES; ++byte) {
        entry->mac[byte] = mac[byte];
    }
    entry->learned_at = now;
    entry->valid = true;
}

static orion_net_status send_arp_request(uint32_t target_address)
{
    uint8_t packet[ARP_PACKET_BYTES] = {0};
    write_be16(packet, 1);
    write_be16(packet + 2, ETHERNET_TYPE_IPV4);
    packet[4] = ORION_NET_MAC_BYTES;
    packet[5] = 4;
    write_be16(packet + 6, 1);
    for (uint32_t index = 0; index < ORION_NET_MAC_BYTES; ++index) {
        packet[8U + index] = network_interface->mac[index];
        packet[18U + index] = 0;
    }
    write_be32(packet + 14U, local_address);
    write_be32(packet + 24U, target_address);
    const uint8_t broadcast[ORION_NET_MAC_BYTES] = {
        UINT8_MAX, UINT8_MAX, UINT8_MAX,
        UINT8_MAX, UINT8_MAX, UINT8_MAX
    };
    const orion_net_status result =
        transmit_ethernet(broadcast, ETHERNET_TYPE_ARP, packet,
                          sizeof(packet));
    if (result == ORION_NET_OK) {
        increment(&network_stats.arp_requests_sent);
    }
    return result;
}

static orion_net_status resolve_neighbor(uint32_t destination,
                                         uint8_t mac_out[6])
{
    const uint32_t next_hop =
        (destination & subnet_mask) == (local_address & subnet_mask)
            ? destination
            : default_gateway;
    if (!ipv4_is_unicast(next_hop)) {
        return ORION_NET_NOT_FOUND;
    }
    if (arp_cache_lookup(next_hop, mac_out)) {
        return ORION_NET_OK;
    }
    return send_arp_request(next_hop) == ORION_NET_OK
               ? ORION_NET_NOT_FOUND
               : ORION_NET_IO_ERROR;
}

static orion_net_status send_ipv4(uint32_t destination,
                                  uint8_t protocol,
                                  const uint8_t *payload,
                                  uint16_t payload_bytes)
{
    if (!configured || !ipv4_is_unicast(destination) ||
        payload == NULL || payload_bytes > network_interface->mtu -
                                               IPV4_HEADER_BYTES) {
        return ORION_NET_INVALID;
    }
    uint8_t destination_mac[ORION_NET_MAC_BYTES];
    const orion_net_status neighbor =
        resolve_neighbor(destination, destination_mac);
    if (neighbor != ORION_NET_OK) {
        return neighbor;
    }
    uint8_t packet[ORION_NET_MTU];
    const uint16_t total_bytes = IPV4_HEADER_BYTES + payload_bytes;
    packet[0] = 0x45;
    packet[1] = 0;
    write_be16(packet + 2U, total_bytes);
    write_be16(packet + 4U, (uint16_t)(network_stats.tx_frames + 1U));
    write_be16(packet + 6U, UINT16_C(0x4000));
    packet[8] = 64;
    packet[9] = protocol;
    write_be16(packet + 10U, 0);
    write_be32(packet + 12U, local_address);
    write_be32(packet + 16U, destination);
    write_be16(packet + 10U,
               orion_net_checksum(packet, IPV4_HEADER_BYTES));
    for (uint32_t index = 0; index < payload_bytes; ++index) {
        packet[IPV4_HEADER_BYTES + index] = payload[index];
    }
    return transmit_ethernet(destination_mac, ETHERNET_TYPE_IPV4, packet,
                             total_bytes);
}

static void handle_arp(const uint8_t *packet, uint32_t packet_bytes,
                       const uint8_t ethernet_source[6])
{
    if (packet_bytes < ARP_PACKET_BYTES || read_be16(packet) != 1U ||
        read_be16(packet + 2U) != ETHERNET_TYPE_IPV4 ||
        packet[4] != ORION_NET_MAC_BYTES || packet[5] != 4U ||
        (read_be16(packet + 6U) != 1U &&
         read_be16(packet + 6U) != 2U) ||
        !bytes_equal(packet + 8U, ethernet_source, ORION_NET_MAC_BYTES) ||
        !mac_is_unicast(packet + 8U)) {
        increment(&network_stats.malformed_packets);
        return;
    }
    const uint32_t sender_address = read_be32(packet + 14U);
    const uint32_t target_address = read_be32(packet + 24U);
    const uint16_t operation = read_be16(packet + 6U);
    if (operation == 1U) {
        if (target_address != local_address) {
            return;
        }
        arp_cache_update(sender_address, packet + 8U);
    } else {
        if (target_address != local_address ||
            !bytes_equal(packet + 18U, network_interface->mac,
                         ORION_NET_MAC_BYTES)) {
            increment(&network_stats.rx_dropped);
            return;
        }
        arp_cache_update(sender_address, packet + 8U);
    }
    if (operation != 1U) {
        return;
    }

    uint8_t reply[ARP_PACKET_BYTES] = {0};
    write_be16(reply, 1);
    write_be16(reply + 2U, ETHERNET_TYPE_IPV4);
    reply[4] = ORION_NET_MAC_BYTES;
    reply[5] = 4;
    write_be16(reply + 6U, 2);
    for (uint32_t index = 0; index < ORION_NET_MAC_BYTES; ++index) {
        reply[8U + index] = network_interface->mac[index];
        reply[18U + index] = packet[8U + index];
    }
    write_be32(reply + 14U, local_address);
    write_be32(reply + 24U, sender_address);
    if (transmit_ethernet(packet + 8U, ETHERNET_TYPE_ARP, reply,
                          sizeof(reply)) == ORION_NET_OK) {
        increment(&network_stats.arp_replies_sent);
    }
}

static bool verify_udp_checksum(uint32_t source, uint32_t destination,
                                const uint8_t *datagram,
                                uint16_t datagram_bytes)
{
    if (read_be16(datagram + 6U) == 0) {
        return true;
    }
    uint32_t sum = 0;
    sum += (source >> 16) & UINT32_C(0xffff);
    sum += source & UINT32_C(0xffff);
    sum += (destination >> 16) & UINT32_C(0xffff);
    sum += destination & UINT32_C(0xffff);
    sum += IPV4_PROTOCOL_UDP;
    sum += datagram_bytes;
    for (uint32_t index = 0; index + 1U < datagram_bytes; index += 2U) {
        sum += ((uint32_t)datagram[index] << 8) | datagram[index + 1U];
    }
    if ((datagram_bytes & 1U) != 0) {
        sum += (uint32_t)datagram[datagram_bytes - 1U] << 8;
    }
    while ((sum >> 16) != 0) {
        sum = (sum & UINT32_C(0xffff)) + (sum >> 16);
    }
    return (uint16_t)sum == UINT16_MAX;
}

static void handle_udp(uint32_t source, uint32_t destination,
                       const uint8_t *datagram, uint16_t datagram_bytes)
{
    if (datagram_bytes < UDP_HEADER_BYTES ||
        read_be16(datagram + 4U) != datagram_bytes ||
        read_be16(datagram) == 0 || read_be16(datagram + 2U) == 0 ||
        !verify_udp_checksum(source, destination, datagram, datagram_bytes)) {
        increment(&network_stats.malformed_packets);
        return;
    }
    const uint16_t destination_port = read_be16(datagram + 2U);
    for (uint32_t index = 0; index < ORION_NET_UDP_ENDPOINT_LIMIT; ++index) {
        udp_endpoint *endpoint = &udp_endpoints[index];
        if (endpoint->active && endpoint->port == destination_port) {
            increment(&network_stats.udp_datagrams_received);
            endpoint->receiver(endpoint->context, source,
                               read_be16(datagram), datagram + UDP_HEADER_BYTES,
                               (uint16_t)(datagram_bytes - UDP_HEADER_BYTES));
            return;
        }
    }
    increment(&network_stats.rx_dropped);
}

static void handle_icmp(uint32_t source, const uint8_t *packet,
                        uint16_t packet_bytes)
{
    if (packet_bytes < ICMP_HEADER_BYTES ||
        !orion_net_checksum_valid(packet, packet_bytes)) {
        increment(&network_stats.malformed_packets);
        return;
    }
    if (packet[0] == 0U && packet[1] == 0U && ping_waiting &&
        source == ping_destination &&
        read_be16(packet + 4U) == PING_IDENTIFIER &&
        read_be16(packet + 6U) == ping_sequence) {
        uint64_t now = 0;
        if (now_nanoseconds(&now) && now >= ping_started_at) {
            ping_round_trip = now - ping_started_at;
            ping_completed = true;
            increment(&network_stats.icmp_echo_replies_received);
        }
        return;
    }
    if (packet[0] != 8U || packet[1] != 0U) {
        return;
    }
    increment(&network_stats.icmp_echo_requests_received);
    if (packet_bytes > sizeof(icmp_reply_buffer)) {
        increment(&network_stats.rx_dropped);
        return;
    }
    for (uint32_t index = 0; index < packet_bytes; ++index) {
        icmp_reply_buffer[index] = packet[index];
    }
    icmp_reply_buffer[0] = 0;
    write_be16(icmp_reply_buffer + 2U, 0);
    write_be16(icmp_reply_buffer + 2U,
               orion_net_checksum(icmp_reply_buffer, packet_bytes));
    if (send_ipv4(source, IPV4_PROTOCOL_ICMP, icmp_reply_buffer,
                  packet_bytes) == ORION_NET_OK) {
        increment(&network_stats.icmp_echo_replies_sent);
    }
}

static void handle_ipv4(const uint8_t *packet, uint32_t packet_bytes)
{
    if (packet_bytes < IPV4_HEADER_BYTES || (packet[0] >> 4) != 4U) {
        increment(&network_stats.malformed_packets);
        return;
    }
    const uint32_t header_bytes = (packet[0] & 0x0fU) * 4U;
    const uint16_t total_bytes = read_be16(packet + 2U);
    if (header_bytes < IPV4_HEADER_BYTES || header_bytes > packet_bytes ||
        total_bytes < header_bytes || total_bytes > packet_bytes ||
        total_bytes > network_interface->mtu ||
        !orion_net_checksum_valid(packet, header_bytes) ||
        (read_be16(packet + 6U) & UINT16_C(0xbfff)) != 0) {
        increment(&network_stats.malformed_packets);
        return;
    }
    const uint32_t source = read_be32(packet + 12U);
    const uint32_t destination = read_be32(packet + 16U);
    if (destination != local_address || !ipv4_is_unicast(source)) {
        increment(&network_stats.rx_dropped);
        return;
    }
    increment(&network_stats.ipv4_packets);
    const uint8_t protocol = packet[9];
    const uint8_t *transport = packet + header_bytes;
    const uint16_t transport_bytes = (uint16_t)(total_bytes - header_bytes);
    if (protocol == IPV4_PROTOCOL_ICMP) {
        handle_icmp(source, transport, transport_bytes);
    } else if (protocol == IPV4_PROTOCOL_UDP) {
        handle_udp(source, destination, transport, transport_bytes);
    } else {
        increment(&network_stats.rx_dropped);
    }
}

static void receive_frame(void *context, const uint8_t *frame,
                          uint32_t frame_bytes)
{
    (void)context;
    if (frame == NULL || frame_bytes < ETHERNET_HEADER_BYTES ||
        frame_bytes > network_interface->mtu + ETHERNET_HEADER_BYTES) {
        increment(&network_stats.malformed_packets);
        increment(&network_stats.rx_dropped);
        return;
    }
    if (!mac_is_unicast(frame + ORION_NET_MAC_BYTES)) {
        increment(&network_stats.malformed_packets);
        increment(&network_stats.rx_dropped);
        return;
    }
    const uint8_t broadcast[ORION_NET_MAC_BYTES] = {
        UINT8_MAX, UINT8_MAX, UINT8_MAX,
        UINT8_MAX, UINT8_MAX, UINT8_MAX
    };
    if (!bytes_equal(frame, network_interface->mac,
                     ORION_NET_MAC_BYTES) &&
        !bytes_equal(frame, broadcast, ORION_NET_MAC_BYTES)) {
        increment(&network_stats.rx_dropped);
        return;
    }
    increment(&network_stats.rx_frames);
    const uint16_t ether_type = read_be16(frame + 12U);
    if (ether_type == ETHERNET_TYPE_ARP) {
        handle_arp(frame + ETHERNET_HEADER_BYTES,
                   frame_bytes - ETHERNET_HEADER_BYTES, frame + 6U);
    } else if (ether_type == ETHERNET_TYPE_IPV4 && configured) {
        handle_ipv4(frame + ETHERNET_HEADER_BYTES,
                    frame_bytes - ETHERNET_HEADER_BYTES);
    } else {
        increment(&network_stats.rx_dropped);
    }
}

orion_net_status orion_net_attach(orion_net_interface *interface)
{
    if (interface == NULL || interface->name == NULL ||
        interface->transmit == NULL || interface->poll == NULL ||
        !interface->available || !mac_is_unicast(interface->mac) ||
        interface->mtu < 576U || interface->mtu > ORION_NET_MTU) {
        return ORION_NET_INVALID;
    }
    if (network_interface != NULL) {
        return ORION_NET_BUSY;
    }
    network_interface = interface;
    return ORION_NET_OK;
}

orion_net_status orion_net_configure(uint32_t address_ipv4,
                                     uint32_t subnet_mask_ipv4,
                                     uint32_t gateway_ipv4,
                                     orion_net_time_fn time_source,
                                     void *time_source_context)
{
    if (!orion_net_is_available() || !ipv4_is_unicast(address_ipv4) ||
        !ipv4_is_unicast(gateway_ipv4) || time_source == NULL) {
        return ORION_NET_INVALID;
    }
    const uint32_t inverse = ~subnet_mask_ipv4;
    if (subnet_mask_ipv4 == 0 || (inverse & (inverse + 1U)) != 0 ||
        gateway_ipv4 == address_ipv4 ||
        (address_ipv4 & subnet_mask_ipv4) !=
            (gateway_ipv4 & subnet_mask_ipv4)) {
        return ORION_NET_INVALID;
    }
    local_address = address_ipv4;
    subnet_mask = subnet_mask_ipv4;
    default_gateway = gateway_ipv4;
    time_reader = time_source;
    time_context = time_source_context;
    for (uint32_t index = 0; index < ORION_NET_ARP_CACHE_LIMIT; ++index) {
        arp_cache[index].valid = false;
    }
    configured = true;
    return ORION_NET_OK;
}

orion_net_status orion_net_udp_bind(uint16_t port,
                                    orion_net_udp_receiver receiver,
                                    void *context)
{
    if (!configured || port == 0 || receiver == NULL) {
        return ORION_NET_INVALID;
    }
    uint32_t free_slot = ORION_NET_UDP_ENDPOINT_LIMIT;
    for (uint32_t index = 0; index < ORION_NET_UDP_ENDPOINT_LIMIT; ++index) {
        if (udp_endpoints[index].active &&
            udp_endpoints[index].port == port) {
            return ORION_NET_BUSY;
        }
        if (!udp_endpoints[index].active &&
            free_slot == ORION_NET_UDP_ENDPOINT_LIMIT) {
            free_slot = index;
        }
    }
    if (free_slot == ORION_NET_UDP_ENDPOINT_LIMIT) {
        return ORION_NET_NO_SPACE;
    }
    udp_endpoints[free_slot] = (udp_endpoint){
        .port = port,
        .receiver = receiver,
        .context = context,
        .active = true
    };
    return ORION_NET_OK;
}

orion_net_status orion_net_udp_unbind(uint16_t port)
{
    for (uint32_t index = 0; index < ORION_NET_UDP_ENDPOINT_LIMIT; ++index) {
        if (udp_endpoints[index].active &&
            udp_endpoints[index].port == port) {
            udp_endpoints[index] = (udp_endpoint){0};
            return ORION_NET_OK;
        }
    }
    return ORION_NET_NOT_FOUND;
}

orion_net_status orion_net_udp_send(uint16_t source_port,
                                    uint32_t destination_ipv4,
                                    uint16_t destination_port,
                                    const uint8_t *payload,
                                    uint16_t payload_bytes)
{
    if (!configured || source_port == 0 || destination_port == 0 ||
        (payload == NULL && payload_bytes != 0)) {
        return ORION_NET_INVALID;
    }
    if (payload_bytes > ORION_NET_MAX_UDP_PAYLOAD) {
        return ORION_NET_TOO_LARGE;
    }
    uint8_t datagram[ORION_NET_MTU - IPV4_HEADER_BYTES];
    const uint16_t datagram_bytes = UDP_HEADER_BYTES + payload_bytes;
    write_be16(datagram, source_port);
    write_be16(datagram + 2U, destination_port);
    write_be16(datagram + 4U, datagram_bytes);
    write_be16(datagram + 6U, 0);
    for (uint32_t index = 0; index < payload_bytes; ++index) {
        datagram[UDP_HEADER_BYTES + index] = payload[index];
    }
    uint32_t sum = 0;
    sum += (local_address >> 16) & UINT32_C(0xffff);
    sum += local_address & UINT32_C(0xffff);
    sum += (destination_ipv4 >> 16) & UINT32_C(0xffff);
    sum += destination_ipv4 & UINT32_C(0xffff);
    sum += IPV4_PROTOCOL_UDP;
    sum += datagram_bytes;
    for (uint32_t index = 0; index + 1U < datagram_bytes; index += 2U) {
        sum += ((uint32_t)datagram[index] << 8) | datagram[index + 1U];
    }
    if ((datagram_bytes & 1U) != 0) {
        sum += (uint32_t)datagram[datagram_bytes - 1U] << 8;
    }
    while ((sum >> 16) != 0) {
        sum = (sum & UINT32_C(0xffff)) + (sum >> 16);
    }
    uint16_t checksum = (uint16_t)~sum;
    if (checksum == 0) {
        checksum = UINT16_MAX;
    }
    write_be16(datagram + 6U, checksum);
    const orion_net_status result =
        send_ipv4(destination_ipv4, IPV4_PROTOCOL_UDP, datagram,
                  datagram_bytes);
    if (result == ORION_NET_OK) {
        increment(&network_stats.udp_datagrams_sent);
    }
    return result;
}

orion_net_status orion_net_ping(uint32_t destination_ipv4,
                                uint16_t timeout_milliseconds,
                                orion_net_ping_result *result)
{
    if (!configured || result == NULL || timeout_milliseconds == 0 ||
        timeout_milliseconds > 5000U ||
        !ipv4_is_unicast(destination_ipv4) ||
        !orion_x86_64_interrupts_enabled()) {
        return ORION_NET_INVALID;
    }
    *result = (orion_net_ping_result){
        .destination_ipv4 = destination_ipv4
    };
    uint64_t start = 0;
    if (!now_nanoseconds(&start)) {
        return ORION_NET_UNAVAILABLE;
    }
    const uint64_t timeout_ns =
        (uint64_t)timeout_milliseconds * UINT64_C(1000000);
    if (start > UINT64_MAX - timeout_ns) {
        return ORION_NET_INVALID;
    }
    const uint64_t deadline = start + timeout_ns;
    const uint32_t neighbor =
        (destination_ipv4 & subnet_mask) == (local_address & subnet_mask)
            ? destination_ipv4
            : default_gateway;
    uint8_t destination_mac[ORION_NET_MAC_BYTES];
    if (!arp_cache_lookup(neighbor, destination_mac)) {
        if (send_arp_request(neighbor) != ORION_NET_OK) {
            return ORION_NET_IO_ERROR;
        }
        while (!arp_cache_lookup(neighbor, destination_mac)) {
            uint64_t now = 0;
            uint32_t received = 0;
            if (!now_nanoseconds(&now)) {
                return ORION_NET_UNAVAILABLE;
            }
            if (now >= deadline) {
                return ORION_NET_TIMEOUT;
            }
            const orion_net_status poll_status = orion_net_poll(8, &received);
            if (poll_status != ORION_NET_OK &&
                poll_status != ORION_NET_BUSY) {
                return poll_status;
            }
        }
    }
    uint8_t request[ICMP_HEADER_BYTES + 8U] = {0};
    request[0] = 8;
    write_be16(request + 4U, PING_IDENTIFIER);
    ++ping_sequence;
    if (ping_sequence == 0) {
        ++ping_sequence;
    }
    write_be16(request + 6U, ping_sequence);
    uint64_t sent_at = 0;
    if (!now_nanoseconds(&sent_at)) {
        return ORION_NET_UNAVAILABLE;
    }
    if (sent_at >= deadline) {
        return ORION_NET_TIMEOUT;
    }
    write_be16(request + ICMP_HEADER_BYTES, (uint16_t)(sent_at >> 48));
    write_be16(request + ICMP_HEADER_BYTES + 2U,
               (uint16_t)(sent_at >> 32));
    write_be16(request + ICMP_HEADER_BYTES + 4U,
               (uint16_t)(sent_at >> 16));
    write_be16(request + ICMP_HEADER_BYTES + 6U, (uint16_t)sent_at);
    write_be16(request + 2U, orion_net_checksum(request, sizeof(request)));
    ping_waiting = true;
    ping_completed = false;
    ping_destination = destination_ipv4;
    ping_started_at = sent_at;
    const orion_net_status send_status =
        send_ipv4(destination_ipv4, IPV4_PROTOCOL_ICMP, request,
                  sizeof(request));
    if (send_status != ORION_NET_OK) {
        ping_waiting = false;
        return send_status;
    }
    result->requests_sent = 1;
    increment(&network_stats.icmp_echo_requests_sent);
    while (!ping_completed) {
        uint64_t now = 0;
        uint32_t received = 0;
        if (!now_nanoseconds(&now)) {
            ping_waiting = false;
            return ORION_NET_UNAVAILABLE;
        }
        if (now >= deadline) {
            ping_waiting = false;
            *result = (orion_net_ping_result){
                .destination_ipv4 = destination_ipv4,
                .requests_sent = 1,
                .replies_received = 0
            };
            return ORION_NET_TIMEOUT;
        }
        const orion_net_status poll_status = orion_net_poll(8, &received);
        if (poll_status != ORION_NET_OK && poll_status != ORION_NET_BUSY) {
            ping_waiting = false;
            return poll_status;
        }
    }
    ping_waiting = false;
    *result = (orion_net_ping_result){
        .round_trip_nanoseconds = ping_round_trip,
        .destination_ipv4 = destination_ipv4,
        .requests_sent = 1,
        .replies_received = 1
    };
    return ORION_NET_OK;
}

orion_net_status orion_net_poll(uint32_t budget,
                                uint32_t *frames_received)
{
    if (!orion_net_is_available()) {
        return ORION_NET_UNAVAILABLE;
    }
    if (budget == 0 || frames_received == NULL) {
        return ORION_NET_INVALID;
    }
    if (polling) {
        return ORION_NET_BUSY;
    }
    polling = true;
    *frames_received = 0;
    const orion_net_status result =
        network_interface->poll(network_interface->context, receive_frame,
                                NULL, budget, frames_received);
    polling = false;
    if (result != ORION_NET_OK) {
        increment(&network_stats.rx_errors);
    }
    return result;
}

bool orion_net_get_stats(orion_net_stats *stats)
{
    if (stats == NULL) {
        return false;
    }
    *stats = network_stats;
    return true;
}

bool orion_net_is_available(void)
{
    return network_interface != NULL && network_interface->available &&
           network_interface->transmit != NULL &&
           network_interface->poll != NULL;
}

bool orion_net_is_configured(void)
{
    return configured;
}
