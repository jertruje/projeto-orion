# ORION OS network stack — Phase 17

## Status and support boundary

Phase 17 has a working, bounded kernel network path for one tested QEMU
configuration. The supported device is QEMU's NE2000-compatible PCI adapter
(RTL8029, PCI `10ec:8029`), tested with QEMU 11.1.2, OVMF, q35 and one vCPU.
The host-side integration test proves a real UDP request/reply through QEMU
user networking. This is not broad device compatibility or Internet
compatibility.

Implemented protocols are Ethernet II, ARP for IPv4, IPv4 without fragment
reassembly, ICMPv4 Echo Request/Reply and UDP. TCP, DHCP, IPv6, VLAN, DNS,
firewall policy, TLS and user sockets are not implemented. TCP is deliberately
pending rather than exposed as an unreliable placeholder. There is no user
mode, syscall ABI, shell or network service process; the interface is a
kernel-internal API.

## Layers and contracts

`kernel/drivers/net/ne2k_pci.c` performs PCI matching, I/O BAR setup, MAC PROM
readout, remote-DMA transfers to the NIC's on-card memory, transmit and receive
ring polling. It uses programmed I/O and does not DMA into system RAM or enable
a device IRQ. `kernel/net/network.c` implements the device-independent
interface, Ethernet framing, ARP, IPv4, ICMP and UDP.

The interface has a fixed 1500-byte MTU and a maximum frame buffer capacity
of 1518 bytes (14-byte Ethernet header, MTU and 4-byte FCS margin). The stack
accepts Ethernet frames up to 1514 bytes without FCS; the NIC driver removes
received FCS bytes.
The network layer uses static buffers, a 16-entry ARP cache with a 60-second
expiry, and at most eight bound UDP endpoints. There is no unbounded packet
queue or packet allocation. A UDP payload is at most 1472 bytes.

Transmit buffers are borrowed by the synchronous driver call and may be reused
only after it returns. A received frame is borrowed by the protocol callback
only for the duration of that callback. The stack copies data it needs to
retain. There is no asynchronous transmit completion or retained caller
buffer. Driver receive processing is polled from the kernel idle loop, not
from a device interrupt. The current stack is a single-CPU, kernel-context
implementation; it does not promise SMP safety or a general concurrent
callers contract.

The interface reports availability, MAC address, MTU and counters. Physical
link state is not reported by this driver. `orion_net_get_stats()` provides
frame/error/drop counts and direction-specific ARP, ICMP and UDP counters.
There is no shell, so the QEMU integration image emits bounded diagnostics for
the detected MAC/MTU, its test IPv4 address, one ICMP ping result and one
post-echo counter snapshot.

## Addressing and protocol behavior

IPv4 values in the kernel API are host-order integers. The first test image
uses static address `10.0.2.15/24`, gateway `10.0.2.2`; this is test-only
configuration and is not enabled during ordinary boot. The implementation
has one default gateway and no general route table.

Ethernet accepts only frames addressed to the local MAC or broadcast, validates
frame bounds and requires a unicast source MAC, and forwards only ARP and IPv4.
VLAN tags are not handled.

ARP fields and hardware/protocol address lengths are checked before use. The
cache is fixed-size and expires entries after 60 seconds. It does not
authenticate ARP; a peer able to send valid local-link ARP packets can spoof
addresses. IPv4 parsing checks version, header length, total length, MTU,
header checksum, destination and unsupported fragmentation. Packets are not
reassembled; fragmented packets are dropped. Received IPv4 packets do not
populate the ARP cache, avoiding cache updates from packets with forged
Ethernet source addresses.

ICMP accepts Echo Request and matching Echo Reply only. A ping permits one
outstanding request, requires interrupts enabled so the PIT clock can advance,
and accepts timeouts from 1 to 5000 ms. Its timeout covers ARP resolution and
the echo wait; measured RTT starts at transmit time and therefore excludes
ARP resolution. The returned result includes destination, requests sent,
replies received and RTT. The QEMU diagnostic prints these values and packet
loss. An absent peer returns a finite timeout; it does not wait indefinitely.

UDP validates ports, declared length and nonzero checksums. A zero UDP checksum
is accepted as permitted for IPv4; otherwise the IPv4 pseudo-header checksum
is verified. Datagrams for unbound ports are dropped without an ICMP error.
`orion_net_udp_send()` with an unresolved next hop sends an ARP request and
returns `ORION_NET_NOT_FOUND`; callers must retry after resolution. There is
no queued send, retransmission or connection semantics.

Packet contents are parsed with explicit endian helpers, not cast to native
structures. Invalid or unsupported traffic increments bounded counters and
does not generate per-packet log output. Checksums are validated before
transport callbacks are invoked.

## QEMU setup and test commands

Run from the repository root on the configured Windows/MSYS2 toolchain:

```text
mingw32-make test-unit
mingw32-make build inspect
mingw32-make test-boot
mingw32-make test-preemption-qemu
mingw32-make test-network-qemu
```

`test-network-qemu` starts QEMU with an isolated user-mode network and explicit
UDP forwarding:

```text
-netdev user,id=net0,net=10.0.2.0/24,host=10.0.2.2,dhcpstart=10.0.2.15,hostfwd=udp:127.0.0.1:40170-10.0.2.15:40171
-device ne2k_pci,netdev=net0,mac=52:54:00:12:34:56
```

The PowerShell harness boots the test kernel through OVMF, waits for the
interface/configuration marker, sends the 22-byte payload
`ORION_PHASE17_UDP_ECHO` from the Windows host to `127.0.0.1:40170`, and
requires an identical response. It also requires serial evidence for driver
initialization, UDP receive/transmit, finite ping diagnostics and frame/error
counters. QEMU is stopped by the harness after the exchange; this does not
measure long-duration stability. Ping success or timeout is recorded as
observed and is not assumed by the UDP integration assertion.

In QEMU runs, ping to `10.0.2.2` succeeded (1 request, 1 reply, 0 lost). The
reported RTT samples were 0 ns and 10,000,151 ns while the PIT clocksource
quantum was 10,000,150 ns; this shows the coarse tick resolution and is not a
latency guarantee (0 means below one observed clock tick, not zero physical
duration). The UDP run reported 3 transmitted and 3 received frames, with 0
drops and 0 driver errors. QEMU also emitted its legacy-NIC-wrapper warning;
the UDP exchange and ping still passed.

The normal boot does not require a network adapter. If none is detected, the
kernel logs a warning and continues. The test kernel requires the NE2000 and
fails if it is unavailable.

## Validation evidence and limits

Host unit tests use a fake link endpoint for checksum vectors, ARP reply
construction, IPv4 validation, ICMP echo, UDP receive/send, invalid UDP
checksums and lengths, truncated frames, bounded endpoint capacity, ARP
expiration, finite ARP/ICMP ping timeouts and transmit errors. These tests do
not replace hardware integration.

QEMU integration validates the real NE2000 driver's initialization and a
byte-identical host-to-guest-to-host UDP exchange. The selected PIO driver
avoids the unimplemented system-memory DMA mapping and device-interrupt
infrastructure. It does not validate a physical NE2000/RTL8029 card or another
NIC model. Receive ring error recovery is bounded but has no injected hardware
fault test.

The phase remains **partial**. TCP, user sockets, DHCP, IPv6, link-state
detection, multiple interfaces, general routes, SMP synchronization, DMA,
device IRQ handling, persistent network configuration and a network shell are
pending. The network stack is not a firewall and provides no authentication,
confidentiality or protection against compromised firmware/hardware, CPU
side-channels or malicious peers.
