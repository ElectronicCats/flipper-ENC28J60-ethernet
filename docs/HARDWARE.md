# Hardware notes — Electronic Cats Flipper Add-On Ethernet

## Shield

- Product: https://electroniccats.com/store/flipper-zero-add-on-ethernet/
- Repo (HW): https://github.com/ElectronicCats/Flipper-Add-On-Ethernet
- IC: Microchip ENC28J60 (10BASE-T, half/full duplex, SPI ≤20 MHz, 8 KB
  on-chip SRAM).
- Errata applicable: DS80349C — see `ENC28J60_REFACTOR_PLAN.md` §1
  "Contexto crítico verificado" for the items already mitigated in the
  driver and the items relevant to upcoming sub-phases (errata #1 no
  auto-neg; #14 ERXRDPT must be odd; #18 RX wraparound; etc.).

## Flipper GPIO claimed by this app today

Source: `EthernetAppDemo/libraries/chip/Spi_lib.h:10-13`.

| Function | Flipper pin | STM32WB55 GPIO | SDK macro       |
|----------|-------------|----------------|-----------------|
| SPI MOSI | 2           | PA7            | `gpio_ext_pa7`  |
| SPI MISO | 3           | PA6            | `gpio_ext_pa6`  |
| SPI CS   | 4           | PA4            | `gpio_ext_pa4`  |
| SPI SCK  | 5           | PB3            | `gpio_ext_pb3`  |

These match Flipper's standard external SPI bus assignment
(`FuriHalSpiBusHandleExternal`).

## ENC28J60 /INT pin status (D2)

- **Routed to Flipper pin:** 10 (PA14 / SWCLK alternate function).
- **F0.5 plan:** use `furi_hal_gpio_add_int_callback` on the SWCLK pin
  to drive interrupt-based RX. Replace the `EPKTCNT` polling loop in
  `enc28j60.c:547` with a semaphore/flag set by the ISR; the new
  `rx_dispatch` thread (F0.4) waits on it.

### Suggested initialization sequence

```c
// in F0.5: rx_dispatch.c initialization
const GpioPin* int_pin = &gpio_swclk;  // verify exact macro in
                                       // furi_hal_resources.{c,h} —
                                       // alternate name may apply.
furi_hal_gpio_init(int_pin,
                   GpioModeInterruptFall,
                   GpioPullUp,            // /INT is open-drain active-low
                   GpioSpeedLow);
furi_hal_gpio_add_int_callback(int_pin, rx_isr_callback, dispatcher_ctx);
```

The pull-up is required because the ENC28J60 `/INT` output is
active-low and open-drain.

## Implications for F0.5

If the interrupt path lands cleanly:
- Sniff sustained throughput target ≥6 Mbps (combined with bulk SPI
  rewrite).
- Worker thread CPU drops; UI thread freed.
- RX latency improves materially under bursty traffic.

Trade-off:
- **SWD debug attach is unavailable** while the app runs. The SWCLK
  line is repurposed as a GPIO input. Flipper firmware can still attach
  SWD when the app is not running. For in-app debugging, use
  `FURI_LOG_*` over UART (Flipper RX/TX pins on the external connector
  are pins 13/14 — `PB6`/`PB7`).

## Implications for future hardware

A v2 shield could route `/INT` to a pin that is not also SWCLK (e.g.
PB2 / pin 6 or PC3 / pin 7) so SWD remains usable during app runtime.
This is recorded as a recommendation for the shield team but is **not**
required for the F0.5 path — the current routing works.

## Pin map summary (post-F0.5)

| Function           | Flipper pin | STM32 GPIO | Owner      |
|--------------------|-------------|------------|------------|
| SPI MOSI           | 2           | PA7        | SPI driver |
| SPI MISO           | 3           | PA6        | SPI driver |
| SPI CS             | 4           | PA4        | SPI driver |
| SPI SCK            | 5           | PB3        | SPI driver |
| ENC28J60 /INT      | 10          | PA14 (SWCLK) | rx_dispatch (F0.5) |
| 3V3 / GND / etc.   | (unchanged) | —          | shield     |

All other Flipper external pins remain free for future expansion (e.g.
a second NIC for 802.1X bridge bypass — backlog only, not in roadmap
F0/F1/F2/F3).
