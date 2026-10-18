#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "../../../kernel/net/network.h"

#define TEST_FRAME_LIMIT 8U

static const uint8_t local_mac[6] = {0x52, 0x54, 0x00, 0x12, 0x34, 0x56};
static const uint8_t peer_mac[6] = {0x52, 0x54, 0x00, 0xaa, 0xbb, 0xcc};
static const uint32_t local_ip = UINT32_C(0x0a00020f);
static const uint32_t peer_ip = UINT32_C(0x0a000202);
static uint8_t input_frames[TEST_FRAME_LIMIT][ORION_NET_MAX_FRAME_BYTES];
static uint32_t input_lengths[TEST_FRAME_LIMIT];
static uint32_t input_read;
static uint32_t input_count;
static uint8_t output_frame[ORION_NET_MAX_FRAME_BYTES];
static uint32_t output_length;
static uint64_t fake_now;
static uint32_t received_datagrams;
static uint8_t received_payload[64];
static uint16_t received_payload_bytes;
static bool fake_interrupts_enabled = true;
static bool fail_transmit;

bool orion_x86_64_interrupts_enabled(void)
{
    return fake_interrupts_enabled;
}

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

static bool require(bool condition, const char *message)
{
    if (!condition) {
        (void)fprintf(stderr, "network test failed: %s\n", message);
    }
    return condition;
}

static orion_net_status fake_transmit(void *context, const uint8_t *frame,
                                      uint32_t frame_bytes)
{
    (void)context;
    if (fail_transmit) {
        return ORION_NET_IO_ERROR;
    }
    if (frame == NULL || frame_bytes > sizeof(output_frame)) {
        return ORION_NET_INVALID;
    }
    for (uint32_t index = 0; index < frame_bytes; ++index) {
        output_frame[index] = frame[index];
    }
    output_length = frame_bytes;
    return ORION_NET_OK;
}

static orion_net_status fake_poll(void *context,
                                  orion_net_frame_receiver receiver,
                                  void *receiver_context, uint32_t budget,
                                  uint32_t *frames_received)
{
    (void)context;
    if (receiver == NULL || frames_received == NULL || budget == 0) {
        return ORION_NET_INVALID;
    }
    *frames_received = 0;
    while (input_read < input_count && *frames_received < budget) {
        receiver(receiver_context, input_frames[input_read],
                 input_lengths[input_read]);
        ++input_read;
        ++*frames_received;
    }
    return ORION_NET_OK;
}

static bool fake_time(void *context, uint64_t *now_nanoseconds)
{
    (void)context;
    if (now_nanoseconds == NULL) {
        return false;
    }
    *now_nanoseconds = fake_now;
    fake_now += UINT64_C(100000);
    return true;
}

static uint32_t make_ethernet(uint8_t *frame, const uint8_t *destination,
                              const uint8_t *source, uint16_t ether_type,
                              const uint8_t *payload, uint32_t payload_bytes)
{
    for (uint32_t index = 0; index < 6U; ++index) {
        frame[index] = destination[index];
        frame[index + 6U] = source[index];
    }
    write_be16(frame + 12U, ether_type);
    for (uint32_t index = 0; index < payload_bytes; ++index) {
        frame[14U + index] = payload[index];
    }
    uint32_t frame_bytes = 14U + payload_bytes;
    while (frame_bytes < 60U) {
        frame[frame_bytes++] = 0;
    }
    return frame_bytes;
}

static void make_arp_request(uint8_t *frame, uint32_t *frame_bytes)
{
    uint8_t arp[28] = {0};
    write_be16(arp, 1);
    write_be16(arp + 2U, UINT16_C(0x0800));
    arp[4] = 6;
    arp[5] = 4;
    write_be16(arp + 6U, 1);
    for (uint32_t index = 0; index < 6U; ++index) {
        arp[8U + index] = peer_mac[index];
    }
    write_be32(arp + 14U, peer_ip);
    write_be32(arp + 24U, local_ip);
    static const uint8_t broadcast[6] = {
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff
    };
    *frame_bytes = make_ethernet(frame, broadcast, peer_mac,
                                 UINT16_C(0x0806), arp, sizeof(arp));
}

static uint32_t make_ipv4_frame(uint8_t *frame, uint8_t protocol,
                                const uint8_t *transport,
                                uint16_t transport_bytes, bool corrupt_ip)
{
    uint8_t packet[1500] = {0};
    packet[0] = 0x45;
    write_be16(packet + 2U, (uint16_t)(20U + transport_bytes));
    write_be16(packet + 6U, UINT16_C(0x4000));
    packet[8] = 64;
    packet[9] = protocol;
    write_be32(packet + 12U, peer_ip);
    write_be32(packet + 16U, local_ip);
    write_be16(packet + 10U, orion_net_checksum(packet, 20));
    if (corrupt_ip) {
        packet[10] ^= 0x80U;
    }
    for (uint32_t index = 0; index < transport_bytes; ++index) {
        packet[20U + index] = transport[index];
    }
    return make_ethernet(frame, local_mac, peer_mac, UINT16_C(0x0800),
                         packet, 20U + transport_bytes);
}

static void set_udp_checksum(uint8_t *datagram, uint16_t datagram_bytes)
{
    uint8_t pseudo_header[12U + 64U] = {0};
    write_be32(pseudo_header, peer_ip);
    write_be32(pseudo_header + 4U, local_ip);
    pseudo_header[9] = 17;
    write_be16(pseudo_header + 10U, datagram_bytes);
    write_be16(datagram + 6U, 0);
    for (uint32_t index = 0; index < datagram_bytes; ++index) {
        pseudo_header[12U + index] = datagram[index];
    }
    uint16_t checksum =
        orion_net_checksum(pseudo_header, 12U + datagram_bytes);
    if (checksum == 0) {
        checksum = UINT16_MAX;
    }
    write_be16(datagram + 6U, checksum);
}

static void echo_datagram(void *context, uint32_t source_ipv4,
                          uint16_t source_port, const uint8_t *payload,
                          uint16_t payload_bytes)
{
    (void)context;
    ++received_datagrams;
    received_payload_bytes = payload_bytes;
    for (uint32_t index = 0; index < payload_bytes; ++index) {
        received_payload[index] = payload[index];
    }
    (void)orion_net_udp_send(9000, source_ipv4, source_port, payload,
                             payload_bytes);
}

int main(void)
{
    static const uint8_t checksum_vector[] = {
        0x00, 0x01, 0xf2, 0x03, 0xf4, 0xf5, 0xf6, 0xf7
    };
    orion_net_interface interface = {
        .name = "host-fake0",
        .mac = {0x52, 0x54, 0x00, 0x12, 0x34, 0x56},
        .mtu = ORION_NET_MTU,
        .available = true,
        .context = NULL,
        .transmit = fake_transmit,
        .poll = fake_poll
    };
    if (!require(orion_net_checksum(checksum_vector,
                                    sizeof(checksum_vector)) ==
                     UINT16_C(0x220d),
                 "known Internet checksum vector") ||
        !require(orion_net_checksum_valid(checksum_vector,
                                          sizeof(checksum_vector)) == false,
                 "checksum vector is not itself complemented") ||
        !require(orion_net_attach(&interface) == ORION_NET_OK,
                 "attach test interface") ||
        !require(orion_net_configure(local_ip, UINT32_C(0xffffff00),
                                     peer_ip, fake_time, NULL) == ORION_NET_OK,
                 "configure static IPv4") ||
        !require(orion_net_configure(local_ip, UINT32_C(0xff00ff00),
                                     peer_ip, fake_time, NULL) ==
                     ORION_NET_INVALID,
                 "reject non-contiguous subnet mask") ||
        !require(orion_net_udp_bind(40171, echo_datagram, NULL) ==
                     ORION_NET_OK,
                 "bind UDP endpoint") ||
        !require(orion_net_udp_bind(40171, echo_datagram, NULL) ==
                     ORION_NET_BUSY,
                 "reject duplicate UDP port")) {
        return 1;
    }
    for (uint32_t index = 0; index < ORION_NET_UDP_ENDPOINT_LIMIT - 1U;
         ++index) {
        if (!require(orion_net_udp_bind((uint16_t)(41000U + index),
                                        echo_datagram, NULL) == ORION_NET_OK,
                     "fill bounded UDP endpoint table")) {
            return 1;
        }
    }
    if (!require(orion_net_udp_bind(42000, echo_datagram, NULL) ==
                     ORION_NET_NO_SPACE,
                 "reject UDP endpoint beyond static capacity")) {
        return 1;
    }
    for (uint32_t index = 0; index < ORION_NET_UDP_ENDPOINT_LIMIT - 1U;
         ++index) {
        if (!require(orion_net_udp_unbind((uint16_t)(41000U + index)) ==
                         ORION_NET_OK,
                     "release bounded test endpoints")) {
            return 1;
        }
    }

    uint32_t frame_bytes = 0;
    make_arp_request(input_frames[0], &frame_bytes);
    input_lengths[0] = frame_bytes;
    input_count = 1;
    uint32_t polled = 0;
    if (!require(orion_net_poll(4, &polled) == ORION_NET_OK && polled == 1,
                 "poll and process ARP request") ||
        !require(output_length >= 60U &&
                     read_be16(output_frame + 12U) == UINT16_C(0x0806) &&
                     read_be16(output_frame + 20U) == 2U &&
                     read_be32(output_frame + 28U) == local_ip,
                 "transmit ARP reply for local IPv4 address")) {
        return 1;
    }

    make_arp_request(input_frames[0], &input_lengths[0]);
    input_frames[0][6] = 1;
    input_read = 0;
    input_count = 1;
    output_length = 0;
    if (!require(orion_net_poll(4, &polled) == ORION_NET_OK &&
                     output_length == 0,
                 "drop Ethernet frame with multicast source MAC")) {
        return 1;
    }

    uint8_t udp[32] = {0};
    static const uint8_t message[] = "phase17-udp-echo";
    write_be16(udp, 5555);
    write_be16(udp + 2U, 40171);
    write_be16(udp + 4U, (uint16_t)(8U + sizeof(message) - 1U));
    for (uint32_t index = 0; index < sizeof(message) - 1U; ++index) {
        udp[8U + index] = message[index];
    }
    set_udp_checksum(udp, (uint16_t)(8U + sizeof(message) - 1U));
    input_read = 0;
    input_count = 1;
    input_lengths[0] =
        make_ipv4_frame(input_frames[0], 17, udp,
                        (uint16_t)(8U + sizeof(message) - 1U), false);
    if (!require(orion_net_poll(4, &polled) == ORION_NET_OK && polled == 1,
                 "process incoming UDP datagram") ||
        !require(received_datagrams == 1 &&
                     received_payload_bytes == sizeof(message) - 1U &&
                     received_payload[0] == message[0],
                 "deliver datagram payload to bound endpoint") ||
        !require(read_be16(output_frame + 12U) == UINT16_C(0x0800) &&
                     output_frame[23] == 17U &&
                     read_be16(output_frame + 34U) == 9000U &&
                     read_be16(output_frame + 36U) == 5555U &&
                     read_be16(output_frame + 38U) ==
                         8U + sizeof(message) - 1U,
                 "transmit UDP echo through IPv4 and Ethernet")) {
        return 1;
    }

    udp[6] ^= 0x80U;
    input_read = 0;
    input_count = 1;
    input_lengths[0] =
        make_ipv4_frame(input_frames[0], 17, udp,
                        (uint16_t)(8U + sizeof(message) - 1U), false);
    if (!require(orion_net_poll(4, &polled) == ORION_NET_OK &&
                     received_datagrams == 1,
                 "reject UDP datagram with invalid pseudo-header checksum")) {
        return 1;
    }
    udp[6] ^= 0x80U;
    write_be16(udp + 4U, (uint16_t)(7U + sizeof(message) - 1U));
    input_read = 0;
    input_count = 1;
    input_lengths[0] =
        make_ipv4_frame(input_frames[0], 17, udp,
                        (uint16_t)(8U + sizeof(message) - 1U), false);
    if (!require(orion_net_poll(4, &polled) == ORION_NET_OK &&
                     received_datagrams == 1,
                 "reject UDP datagram with inconsistent length")) {
        return 1;
    }
    write_be16(udp + 4U, (uint16_t)(8U + sizeof(message) - 1U));

    uint8_t icmp[16] = {0};
    icmp[0] = 8;
    write_be16(icmp + 4U, 7);
    write_be16(icmp + 6U, 9);
    icmp[8] = 'p';
    icmp[9] = 'i';
    icmp[10] = 'n';
    icmp[11] = 'g';
    write_be16(icmp + 2U, orion_net_checksum(icmp, sizeof(icmp)));
    input_read = 0;
    input_count = 1;
    input_lengths[0] =
        make_ipv4_frame(input_frames[0], 1, icmp, sizeof(icmp), false);
    if (!require(orion_net_poll(4, &polled) == ORION_NET_OK,
                 "process ICMP echo request") ||
        !require(output_frame[34] == 0 &&
                     output_frame[35] == 0 &&
                     orion_net_checksum_valid(output_frame + 34U,
                                              sizeof(icmp)),
                 "transmit valid ICMP echo reply")) {
        return 1;
    }
    icmp[2] ^= 0x20U;
    input_read = 0;
    input_count = 1;
    input_lengths[0] =
        make_ipv4_frame(input_frames[0], 1, icmp, sizeof(icmp), false);
    output_length = 0;
    if (!require(orion_net_poll(4, &polled) == ORION_NET_OK,
                 "poll malformed ICMP checksum") ||
        !require(output_length == 0,
                 "do not answer invalid ICMP echo request")) {
        return 1;
    }

    input_read = 0;
    input_count = 1;
    input_lengths[0] =
        make_ipv4_frame(input_frames[0], 17, udp,
                        (uint16_t)(8U + sizeof(message) - 1U), true);
    if (!require(orion_net_poll(4, &polled) == ORION_NET_OK,
                 "poll malformed IPv4 packet without kernel failure")) {
        return 1;
    }
    input_read = 0;
    input_count = 1;
    input_lengths[0] = 13;
    if (!require(orion_net_poll(4, &polled) == ORION_NET_OK,
                 "drop truncated Ethernet frame without kernel failure")) {
        return 1;
    }
    orion_net_stats stats;
    if (!require(orion_net_get_stats(&stats) &&
                     stats.malformed_packets != 0 &&
                     stats.rx_dropped != 0 &&
                     stats.icmp_echo_requests_received == 1 &&
                     stats.icmp_echo_replies_sent == 1 &&
                     stats.udp_datagrams_sent == 1 &&
                     stats.udp_datagrams_received == 1,
                 "count malformed packets and ICMP request/reply direction")) {
        return 1;
    }

    fake_now += UINT64_C(70000000000);
    output_length = 0;
    if (!require(orion_net_udp_send(9000, peer_ip, 5555,
                                    message, sizeof(message) - 1U) ==
                     ORION_NET_NOT_FOUND &&
                     output_length >= 60U &&
                     read_be16(output_frame + 12U) == UINT16_C(0x0806),
                 "expire ARP entry and issue bounded resolution request")) {
        return 1;
    }

    orion_net_ping_result ping_result;
    if (!require(orion_net_ping(peer_ip, 1, &ping_result) ==
                     ORION_NET_TIMEOUT &&
                     ping_result.destination_ipv4 == peer_ip &&
                     ping_result.requests_sent == 0 &&
                     ping_result.replies_received == 0,
                 "report finite ARP-resolution timeout with initialized result")) {
        return 1;
    }
    input_read = 0;
    input_count = 1;
    make_arp_request(input_frames[0], &input_lengths[0]);
    if (!require(orion_net_poll(4, &polled) == ORION_NET_OK,
                 "refresh ARP entry before ping timeout test")) {
        return 1;
    }
    fake_interrupts_enabled = false;
    if (!require(orion_net_ping(peer_ip, 1, &ping_result) ==
                     ORION_NET_INVALID,
                 "reject blocking ping while interrupts are disabled")) {
        return 1;
    }
    fake_interrupts_enabled = true;
    if (!require(orion_net_ping(peer_ip, 1, &ping_result) ==
                     ORION_NET_TIMEOUT &&
                     ping_result.requests_sent == 1 &&
                     ping_result.replies_received == 0,
                 "ICMP ping ends on finite timeout")) {
        return 1;
    }
    fail_transmit = true;
    if (!require(orion_net_udp_send(9000, peer_ip, 5555, message,
                                    sizeof(message) - 1U) ==
                     ORION_NET_IO_ERROR,
                 "propagate NIC transmission failure")) {
        return 1;
    }
    fail_transmit = false;
    if (!require(orion_net_udp_unbind(40171) == ORION_NET_OK &&
                     orion_net_udp_unbind(40171) == ORION_NET_NOT_FOUND,
                 "release UDP endpoint")) {
        return 1;
    }
    (void)puts("Ethernet, ARP, IPv4, ICMP, and UDP tests: PASS");
    return 0;
}
