# FLIPPER ETHERNET — Current Hardware Reference

This document describes the current hardware interface and source-proven
controller state transitions. Software ownership is documented in
[`ARCHITECTURE.md`](ARCHITECTURE.md); unresolved work is tracked in
[`BACKLOG.md`](BACKLOG.md).

## 1. Ethernet Add-On

- Product: <https://electroniccats.com/store/flipper-zero-add-on-ethernet/>
- Hardware repository: <https://github.com/ElectronicCats/Flipper-Add-On-Ethernet>
- Controller: Microchip ENC28J60
- Ethernet: 10BASE-T
- Host interface: SPI, up to the controller's documented 20 MHz limit
- Controller SRAM: 8 KiB total

The current driver uses the following controller-buffer layout:

| Region | Address range | Size |
|---|---:|---:|
| RX ring | `0x0000`–`0x0BFF` | 3 KiB |
| TX region | `0x0C00`–`0x11FF` | 1.5 KiB |

Historical plans proposed a 6 KiB/2 KiB layout, but that is not the current
implementation.

## 2. Flipper GPIO and SPI assignment

Source: `EthernetAppDemo/libraries/chip/Spi_lib.h` and
`EthernetAppDemo/libraries/chip/rx_dispatch.c`.

| Function | Flipper pin | STM32WB55 GPIO | SDK resource | Owner |
|---|---:|---|---|---|
| SPI MOSI | 2 | PA7 | `gpio_ext_pa7` | SPI driver |
| SPI MISO | 3 | PA6 | `gpio_ext_pa6` | SPI driver |
| SPI CS | 4 | PA4 | `gpio_ext_pa4` | SPI driver |
| SPI SCK | 5 | PB3 | `gpio_ext_pb3` | SPI driver |
| ENC `/INT` | 10 | PA14 / SWCLK | `gpio_swclk` | RX Dispatch |

The application uses Flipper's external SPI bus handle. The App owns one ENC
wrapper, and that wrapper owns the SPI handle and a mutex that serializes
public controller operations.

## 3. `/INT` and receive wake-up

ENC `/INT` is active-low/open-drain. RX Dispatch configures PA14 as a
falling-edge interrupt input with an internal pull-up. The GPIO callback only
sets a flag on the RX Dispatch thread; it does not perform SPI or controller
register access.

The dispatcher also wakes after a 100 ms timeout and drains the receive queue.
This fallback keeps receive progress from depending solely on an edge being
observed.

PA14 is the SWCLK pin. SWD attachment is unavailable while the application
owns it as the ENC interrupt input. This is a hardware trade-off of the current
shield routing.

Historical performance and latency figures are not treated as current
guarantees without a reproducible measurement tied to the current build.

## 4. Controller startup

Application allocation performs the following hardware-level sequence:

1. allocate the ENC wrapper and its mutex;
2. initialize/acquire the external SPI resources;
3. reset and start the ENC28J60;
4. establish RX/TX ring state and the normal receive filter;
5. apply the configured MAC after settings are loaded;
6. initialize RX Dispatch and the PA14 callback;
7. register the permanent automatic ARP and ICMP handlers.

Starting the controller enables packet reception and controller packet
interrupts. A new application allocation repeats reset/start initialization;
feature-local filter changes are not relied upon to survive an app relaunch.

## 5. Receive filters

### Normal application baseline

The current normal receive filter enables:

- unicast reception (`UCEN`);
- CRC checking (`CRCEN`);
- the programmed pattern match (`PMEN`);
- broadcast reception (`BCEN`).

The pattern is configured for the application's current LLDP EtherType match.

### Passive Discovery

Passive Discovery temporarily adds multicast reception (`MCEN`) so LLDP/CDP/
EAPOL multicast traffic can reach the shared Passive worker. Normal cleanup
clears the multicast bit and returns to the reachable baseline filter.

### Packet Sniffer

Sniffer temporarily reduces filtering to CRC-accepted traffic for broad packet
capture. Normal cleanup restores the explicit application baseline
`UCEN | CRCEN | PMEN | BCEN`.

### DHCP

DHCP records the prior broadcast-filter state, enables broadcast reception for
negotiation as needed, and restores the prior state during normal cleanup.

Current source therefore does not support a claim that normal Passive or
Sniffer cleanup permanently poisons later receive filtering.

## 6. Four different notions of connectivity

The following states must not be conflated:

1. **Controller/Add-On start availability** — whether SPI/controller startup
   succeeded.
2. **Live PHY link** — the current Ethernet physical-link indication read from
   the ENC PHY.
3. **Usable network configuration** — whether the application has a coherent
   IP, subnet, gateway, and related state for the requested operation.
4. **Cached software state** — `App.enc28j60_connected`, which records a prior
   controller-start result.

`enc28j60_connected` is not a live cable/link probe. Current feature paths do
not consistently refresh or invalidate it, so it can become stale when the
Add-On or network changes while the application remains open. This is a
software-state limitation, not a claim about ENC PHY behavior.

## 7. Shutdown and reinitialization

The application removes the PA14 interrupt callback and stops/joins RX
Dispatch before freeing the ENC wrapper and SPI resources. This establishes
the repository-owned ordering that no dispatcher thread remains after the
controller wrapper is freed.

Two lower-level questions remain unresolved because the required HAL/hardware
contracts are not present in this repository:

- whether removal of a GPIO interrupt callback guarantees that any callback
  already in flight has quiesced;
- whether controller RX and interrupt-enable bits require an additional
  explicit disable before the wrapper/SPI handle is released.

These questions must not be documented as resolved in either direction until
the authoritative platform and controller contracts are established.

## 8. Short and runt frames

Several outer application predicates/wrappers inspect fixed Ethernet offsets.
Inner DHCP, CDP, EAPOL, and LLDP TLV parsing performs stronger bounds checks,
but repository source alone does not establish whether the configured ENC
receive path can deliver a short/runt frame to those outer wrappers.

Whether hardware CRC/filter behavior and the driver together guarantee a
minimum delivered frame length remains an external hardware/driver-contract
question. Do not assume either that runt frames are delivered or that they are
impossible.

## 9. Relevant ENC errata

The driver contains receive-ring and silicon workarounds inherited from its
EtherCard-derived implementation. The applicable Microchip errata remain an
engineering reference, but historical throughput, wraparound, or corruption
hypotheses are not current defects unless current source and reproducible
evidence establish them.

## 10. License boundary

`EthernetAppDemo/libraries/chip/enc28j60.c` and `.h` are EtherCard-derived and
licensed GPL-2.0-or-later. The SPI shim and the remainder of the application
use the repository's MIT licensing posture. See [`DECISIONS.md`](DECISIONS.md),
the top-level `LICENSE`, and `LICENSES/EtherCard.LICENSE`.
