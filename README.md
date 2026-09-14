![GitHub release](https://img.shields.io/github/v/release/ElectronicCats/flipper-ENC28J60-ethernet?color=%23008000)
![GitHub Actions](https://img.shields.io/github/actions/workflow/status/ElectronicCats/flipper-ENC28J60-ethernet/build.yml)

# FLIPPER ETHERNET

FLIPPER ETHERNET is a Flipper Zero external application for wired Ethernet
administration, discovery, traffic capture, and authorized security testing
with the Microchip ENC28J60 controller.

<p align=center>
 <a href="https://github.com/ElectronicCats/flipper-addons/wiki">
  <img width=200 src="https://github.com/ElectronicCats/flipper-MCP2515-CANBUS/assets/107638696/75e82f16-ae59-4d86-a465-6c6e6b761e80" />
 </a>
</p>

Requires the [**Electronic Cats Flipper Add-On: Ethernet**](https://electroniccats.com/store/flipper-zero-add-on-ethernet/).

The current manifest builds one FAP containing the Administration, Pentesting,
Settings, and About menus. Separate Admin and Pentesting artifacts are not part
of the current build.

## Features

- DHCP and manual IPv4 configuration
- ARP host scanning and saved scan results
- Passive neighbor discovery and history
- Ping, TCP port scanning, and OS detection
- ARP spoof testing
- Packet capture to PCAP and on-device packet viewing

Use active or disruptive functionality only on networks where you have
authorization.

# Key Features:

- Compatible with Flipper Zero via GPIO port
- Based on the Microchip ENC28J60 Ethernet chip
- Enables wired LAN scanning and analysis
- Supports ARP scan, ping, ARP spoofing, PCAP generation, and more
- Plug & Play, no Flipper hardware modifications required

## Documentation

- [Application behavior](docs/APP_BEHAVIOR.md): Intended user-visible behavior
- [Architecture](docs/ARCHITECTURE.md): Current implementation structure and invariants
- [Hardware](docs/HARDWARE.md): ENC28J60 integration and electrical/network contracts
- [Decisions](docs/DECISIONS.md): Accepted project decisions and their status
- [Backlog](docs/BACKLOG.md): Current defects, conditional questions, and resource limits

Current source is the ground truth for implementation details. Dated plans and
specifications under `docs/superpowers/` and release notes are historical
records, not descriptions of the current implementation.

## How to contribute <img src="https://electroniccats.com/wp-content/uploads/2018/01/fav.png" alt="Electronic Cats Logo" height="35"/><img src="https://raw.githubusercontent.com/gist/ManulMax/2d20af60d709805c55fd784ca7cba4b9/raw/bcfeac7604f674ace63623106eb8bb8471d844a6/github.gif" alt="GitHub Logo" height="30"/>

Contributions are welcome!

Please read the document [**Contribution Manual**](https://github.com/ElectronicCats/electroniccats-cla/blob/main/electroniccats-contribution-manual.md)  which will show you how to contribute your changes to the project.

✨ Thanks to all our [**contributors**](https://github.com/ElectronicCats/flipper-MCP2515-CANBUS/graphs/contributors)! ✨

See [**_Electronic Cats CLA_**](https://github.com/ElectronicCats/electroniccats-cla/blob/main/electroniccats-cla.md) for more information.

See the  [**community code of conduct**](https://github.com/ElectronicCats/electroniccats-cla/blob/main/electroniccats-community-code-of-conduct.md) for a vision of the community we want to build and what we expect from it.

## Maintainer

<p align="center">
 <a href="https://github.com/sponsors/ElectronicCats">
  <img src="https://electroniccats.com/wp-content/uploads/2020/07/Badge_GHS.png" alt="Sponsor button" height="104" />
 </a>
</p>

Electronic Cats invests time and resources in providing this open-source design, please support Electronic Cats and open-source hardware by purchasing products from Electronic Cats!

## License

This project is distributed under the **MIT License** (see [`LICENSE`](LICENSE))
except for the ENC28J60 driver files, which are derivative works of
[EtherCard](https://github.com/njh/EtherCard) and carry
**GPL-2.0-or-later**:

- `EthernetAppDemo/libraries/chip/enc28j60.c`
- `EthernetAppDemo/libraries/chip/enc28j60.h`

Those files include SPDX headers and refer to
[`LICENSES/EtherCard.LICENSE`](LICENSES/EtherCard.LICENSE). The remaining source
tree is original Electronic Cats code under MIT.
