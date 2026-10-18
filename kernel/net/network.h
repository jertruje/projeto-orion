#ifndef ORION_KERNEL_NET_NETWORK_H
#define ORION_KERNEL_NET_NETWORK_H

#include <stdbool.h>
#include <stdint.h>

#define ORION_NET_MAC_BYTES 6U
#define ORION_NET_MTU 1500U
#define ORION_NET_MAX_FRAME_BYTES (ORION_NET_MTU + 18U)
#define ORION_NET_MAX_UDP_PAYLOAD (ORION_NET_MTU - 28U)
#define ORION_NET_ARP_CACHE_LIMIT 16U
#define ORION_NET_UDP_ENDPOINT_LIMIT 8U

typedef enum {
    ORION_NET_OK,
    ORION_NET_INVALID,
    ORION_NET_UNAVAILABLE,
    ORION_NET_BUSY,
    ORION_NET_TOO_LARGE,
    ORION_NET_NOT_FOUND,
    ORION_NET_NO_SPACE,
    ORION_NET_IO_ERROR,
    ORION_NET_TIMEOUT,
    ORION_NET_CHECKSUM
} orion_net_status;

typedef struct {
    uint64_t tx_frames;
    uint64_t rx_frames;
    uint64_t rx_dropped;
    uint64_t tx_errors;
    uint64_t rx_errors;
    uint64_t arp_requests_sent;
    uint64_t arp_replies_sent;
    uint64_t ipv4_packets;
    uint64_t icmp_echo_requests_sent;
    uint64_t icmp_echo_replies_received;
    uint64_t icmp_echo_requests_received;
    uint64_t icmp_echo_replies_sent;
    uint64_t udp_datagrams_sent;
    uint64_t udp_datagrams_received;
    uint64_t malformed_packets;
} orion_net_stats;

typedef struct orion_net_interface orion_net_interface;
typedef void (*orion_net_frame_receiver)(void *context,
                                         const uint8_t *frame,
                                         uint32_t frame_bytes);
typedef orion_net_status (*orion_net_transmit_fn)(
    void *context, const uint8_t *frame, uint32_t frame_bytes);
typedef orion_net_status (*orion_net_poll_fn)(
    void *context, orion_net_frame_receiver receiver,
    void *receiver_context, uint32_t budget, uint32_t *frames_received);
typedef bool (*orion_net_time_fn)(void *context, uint64_t *now_nanoseconds);

struct orion_net_interface {
    const char *name;
    uint8_t mac[ORION_NET_MAC_BYTES];
    uint16_t mtu;
    bool available;
    void *context;
    orion_net_transmit_fn transmit;
    orion_net_poll_fn poll;
};

typedef struct {
    uint64_t round_trip_nanoseconds;
    uint32_t destination_ipv4;
    uint32_t requests_sent;
    uint32_t replies_received;
} orion_net_ping_result;

typedef void (*orion_net_udp_receiver)(
    void *context, uint32_t source_ipv4, uint16_t source_port,
    const uint8_t *payload, uint16_t payload_bytes);

uint16_t orion_net_checksum(const uint8_t *bytes, uint32_t length);
bool orion_net_checksum_valid(const uint8_t *bytes, uint32_t length);
orion_net_status orion_net_attach(orion_net_interface *interface);
orion_net_status orion_net_configure(uint32_t address_ipv4,
                                     uint32_t subnet_mask_ipv4,
                                     uint32_t gateway_ipv4,
                                     orion_net_time_fn time_source,
                                     void *time_context);
orion_net_status orion_net_udp_bind(uint16_t port,
                                    orion_net_udp_receiver receiver,
                                    void *context);
orion_net_status orion_net_udp_unbind(uint16_t port);
orion_net_status orion_net_udp_send(uint16_t source_port,
                                    uint32_t destination_ipv4,
                                    uint16_t destination_port,
                                    const uint8_t *payload,
                                    uint16_t payload_bytes);
orion_net_status orion_net_ping(uint32_t destination_ipv4,
                                uint16_t timeout_milliseconds,
                                orion_net_ping_result *result);
orion_net_status orion_net_poll(uint32_t budget,
                                uint32_t *frames_received);
bool orion_net_get_stats(orion_net_stats *stats);
bool orion_net_is_available(void);
bool orion_net_is_configured(void);

#endif
