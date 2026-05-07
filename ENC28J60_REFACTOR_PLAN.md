# ENC28J60 Ethernet App — Plan Maestro

> Refactor de saneamiento + roadmap de auditoría LAN/OT para Flipper Zero sobre el shield ENC28J60. Dos artefactos `.fap` (admin / pentest) producidos desde el mismo árbol, arquitectura en capas, RX dispatch desacoplado, persistencia de settings, y track paralelo OT/ICS. Documento companion del roadmap completo en `docs/superpowers/specs/2026-05-05-roadmap-design.md`.

> **Nota de versioning:** firmware versionado independiente del shield hardware. Tags del repo en `vMAJOR.MINOR-fN` (fase). Releases públicos en `vMAJOR.MINOR.PATCH`.

---

## 1. Decisiones congeladas

| # | Decisión | Valor |
|---|----------|-------|
| 1 | Alcance del trabajo | Refactor incremental F0 + roadmap de features F1-F3 + track OT |
| 2 | Lenguaje/stack principal | C, Flipper Application Framework (FAP), uFBT/FBT build |
| 3 | Arquitectura | Capas: chip-driver / protocol-craft / module / scene / app, con RX dispatcher único |
| 4 | Estrategia de dependencias | Sin gestor; bibliotecas internas en `EthernetAppDemo/libraries/`. Flipper SDK pineado en `application.fam` |
| 5 | Target platform/runtime | Flipper Zero (STM32WB55RG @ 64 MHz, ~200 KB heap libre, FuriOS sobre FreeRTOS) |
| 6 | Hardware | Electronic Cats Flipper Add-On Ethernet con Microchip ENC28J60, conectado vía SPI por GPIO |
| 7 | Distribución | Dos artefactos: `ethernet-admin.fap` (recon, A1-A2) y `ethernet-pentest.fap` (A1-A4 con gating runtime). Dev (A5) compile-time only |
| 8 | Features in-scope | Recon completo, MITM kill chain, DoS gated, OT/ICS recon+R/W, drop-box mode con BLE telemetry |
| 9 | Features out-of-scope (sin 2da NIC) | 802.1X transparent bridge bypass, transparent inline MITM con forwarding, cable wiremap |
| 10 | Backward compatibility | Romper sin culpa entre F0 y F1 (estructura interna). Mantener PCAP path `/ext/apps_data/ethernet/files/` y MAC default visible al usuario |
| 11 | Idioma de comentarios | Inglés en código. Español permitido solo en `docs/` user-facing |
| 12 | Patrón de UI | Submenu como primario (no VariableItemList). Continúa la dirección de los commits 4857322 y eae1f70 |
| 13 | Persistencia | `flipper_format` sobre `/ext/apps_data/ethernet/settings.cfg`. No JSON, no SQLite |
| 14 | Gating de capa C (DoS) | Archivo `/ext/apps_data/ethernet/AUTHORIZED.txt` debe existir en SD para ver items A4 en menú |
| 15 | Gating de capa D (destructivo) | Compile-time `#define DEV_MODE 1`. Nunca en `.fap` distribuido |
| 16 | OT/ICS | In-scope. Recon en admin, R/W en pentest, write con doble confirmación |
| 17 | Drop-box mode | In-scope. Profile script declarativo en SD, BLE GATT findings service, SubGHz arm/disarm |
| 18 | Fase 0 | Prerequisito absoluto. Sin features nuevos hasta que F0 esté verde |

### Contexto crítico verificado

- **ENC28J60** soporta solo 10BASE-T half/full duplex, sin 100M, sin auto-negotiation funcional (errata #1 — duplex debe forzarse en ambos extremos). 8 KB SRAM interna. Filtros HW: HTEN, PMEN, MPEN, BCEN, MCEN, UCEN, CRCEN. Pin INT disponible no cableado a GPIO de Flipper en el código actual (polling de EPKTCNT en `enc28j60.c:547`).
- **Throughput hoy**: ~3-4 Mbps sniff sostenido / ~1500-3000 pps TX small frames. Bottleneck primario: SPI per-byte acquire/release en `read_buffer`/`write_buffer` (lines 268-269, 280-285 de `enc28j60.c`). Bulk SPI duplica throughput sin tocar el chip.
- **Layout SRAM actual**: RX 0x0000-0x0BFF (3 KB), TX 0x0C00-0x11FF (1.5 KB). 3.5 KB ociosos. Puede pasar a RX 6 KB / TX 2 KB.
- **Heap del FAP**: stack 30 KB (`application.fam:7`), worker thread 10 KB (`app_user.c:105`), `enc28j60_t` embebe `tx_buffer[1518]+rx_buffer[1518]` directamente (3 KB). Heap libre runtime medible pero estimado ~120-150 KB durante operación normal.
- **Bugs verificados** (con file:line — todos en el reporte de auditoría): PCAP timestamps mal calculados (`capture_module.c:42-48`), `is_duplicated_ip` underflow a 0xFF (`arp_module.c:323`), `tcp_send_xmas_probe` siempre retorna false (`tcp_module.c:817`), `subnet_mask` escrito en buffer MAC de 6 bytes (`dhcp_protocol.c:232,392`), 3 declaraciones colisionantes de `target_ip[4]`, `TestingScene` registrada pero inalcanzable (`MainMenuScene.c:49-55`), `ofp_tseq` con IPs hardcodeadas en producción (`os_detector_module.c:523-527`), `SHOW_PACKETS_RECEIVED 1` hex-dumpea cada frame por UART en release (`enc28j60.c:24`).
- **Repo state**: 8 stars, 0 issues abiertos, 0 forks, 0 PRs abiertos, Discussions deshabilitadas. 6 closed PRs (1 inicial + 4 fixes/features de external contributors HeikkiRadu y Pepe19946 + 1 OS detect merge).
- **Sibling apps Electronic Cats** (mismo patrón estructural): `flipper-MCP2515-CANBUS` (161 stars), `flipper-rs485modbus` (26), `flipper-SX1262-LoRa` (88). Convenciones de release: `.fap` adjunto al GitHub Release, pre-commit + clang-format.

---

## 2. Fuentes base y cómo se combinan

| Fuente | Licencia | Aporta | Ubicación en el repo |
|--------|----------|--------|---------------------|
| Microchip ENC28J60 datasheet DS39662 + errata DS80349C | Microchip ref | Spec del chip y errata workarounds | (referencia, no vendored) |
| EtherCard / EtherShield (Arduino) | GPLv2 / MIT por archivo | Patrones del driver (ya absorbidos en `enc28j60.c`) | `EthernetAppDemo/libraries/chip/` |
| Flipper Application SDK | MIT | Furi APIs, GUI, threading, storage | Pineado en `application.fam` |
| `flipper-MCP2515-CANBUS` (sibling) | MIT (Electronic Cats) | Patrón de scenes/modules/libraries layout | (referencia, no copiado) |

### Cómo se combinan

El driver del chip (`libraries/chip/`) absorbe los patrones de EtherCard/EtherShield pero ya está reescrito y debugged. La arquitectura de scenes mimica la del MCP2515-CANBUS sibling app — esto es estándar Electronic Cats, no se cambia. Flipper SDK provee GUI/storage/threading/SPI HAL; nunca se reimplementa lo que SDK ya da. El roadmap añade nuevos `modules/` y `scenes/` sobre el patrón existente, más una nueva `libraries/scanner/scanner_session.{c,h}` para el primitive de F0.3.

### Licencias

Todo MIT-compatible. ENC28J60 datasheet es referencia, no se redistribuye. EtherCard/EtherShield: las porciones absorbidas históricamente fueron ya adaptadas por el equipo; verificar headers de `enc28j60.c` y `enc28j60.h` durante F0 — si tienen GPLv2 declarado, decidir si se reescribe la sección o se mantiene attribution. **Pendiente verificar en F0.**

---

## 3. Arquitectura objetivo

```
flipper-ENC28J60-ethernet/
├── EthernetAppDemo/
│   ├── application.fam               # manifest (PENTEST_MODE flag aquí)
│   ├── app_user.{c,h}                # entry point, thread setup, settings load
│   ├── app_worker.c                  # se elimina; reemplaza rx_dispatch
│   │
│   ├── scenes/                       # capa alta — UI por feature
│   │   └── *.c                       # cada feature una scene
│   │
│   ├── scenes_config/                # registry de scenes (X-macro)
│   │
│   ├── modules/                      # capa media — lógica de feature
│   │   ├── arp/                      # subdir por feature complejo
│   │   ├── dhcp/
│   │   ├── os_detector/
│   │   ├── responder/                # NEW (F2)
│   │   ├── rogue_dhcp/               # NEW (F2)
│   │   ├── ot/                       # NEW (track OT)
│   │   │   ├── modbus_tcp.{c,h}
│   │   │   ├── s7.{c,h}
│   │   │   ├── bacnet.{c,h}
│   │   │   └── ...
│   │   └── ...
│   │
│   ├── libraries/
│   │   ├── chip/                     # capa baja — driver SPI + ENC28J60
│   │   │   ├── enc28j60.{c,h}
│   │   │   ├── Spi_lib.{c,h}
│   │   │   └── rx_dispatch.{c,h}     # NEW (F0.4) — single RX loop
│   │   ├── protocol_tools/           # capa media — packet craft (existing)
│   │   │   ├── ethernet_protocol.{c,h}
│   │   │   ├── arp.{c,h}
│   │   │   ├── ipv4.{c,h}
│   │   │   ├── tcp.{c,h}
│   │   │   ├── udp.{c,h}
│   │   │   ├── icmp.{c,h}
│   │   │   ├── dhcp.{c,h}
│   │   │   ├── ipv6.{c,h}            # NEW (F2.7-F2.10)
│   │   │   └── lldp.{c,h}            # NEW (F1.1)
│   │   ├── scanner/                  # NEW (F0.3) — generic scanner primitive
│   │   │   └── scanner_session.{c,h}
│   │   ├── settings/                 # NEW (F0.2) — flipper_format wrapper
│   │   │   └── settings.{c,h}
│   │   └── generals/                 # ethernet_general (existing)
│   │
│   ├── draw_functions/               # custom canvas (top-talkers, etc)
│   │
│   ├── assets/                       # icons
│   │
│   └── dist/                         # release artifacts
│
├── docs/
│   ├── ARCHITECTURE.md
│   ├── SCANNER_SESSION.md
│   ├── RX_DISPATCH.md
│   ├── PROTOCOLS.md
│   ├── OT_PROTOCOLS.md
│   ├── DROPBOX.md
│   ├── SECURITY.md                   # gating, AUTHORIZED.txt, EULA
│   ├── LAB_SETUP.md
│   ├── PCAP_FORMAT.md
│   ├── ROADMAP.md                    # user-facing
│   └── superpowers/
│       └── specs/
│           └── 2026-05-05-roadmap-design.md
│
├── tools/
│   └── bootstrap.sh                  # wrap de uFBT setup
│
├── .github/
│   └── workflows/
│       └── build.yml                 # produce 2 .fap (admin, pentest)
│
├── .clang-format
├── .pre-commit-config.yaml
├── README.md
├── CHANGELOG.md
├── LICENSE
└── ENC28J60_REFACTOR_PLAN.md         # ESTE archivo
```

### Principios de capas

1. **chip-driver (`libraries/chip/`)** abstrae SPI + ENC28J60 + RX dispatch. NO conoce IP, ARP, DHCP, escenas, GUI. Expone: `init`, `set_mac`, `enable_promiscuous`, `send_packet(buf, len)`, `rx_register(predicate, callback)`, `rx_unregister(handle)`, `link_status`, `phy_diagnostics`.
2. **protocol-craft (`libraries/protocol_tools/`)** construye y parsea paquetes. NO conoce escenas, GUI, scanner sessions. Pure functions sobre buffers.
3. **scanner (`libraries/scanner/`)** orquesta una sesión de scan: resolve target MAC, emit probes, wait for response, report progress. Usa chip-driver y protocol-craft. NO conoce GUI directamente — emite custom events al ViewDispatcher.
4. **modules (`modules/`)** son la lógica de feature: ARP scan, OS detect, responder, rogue DHCP, Modbus probe, etc. Usan scanner y protocol-craft. NO renderizan UI.
5. **scenes (`scenes/`)** son capa de presentación. Llaman modules, renderizan resultado, manejan input del usuario.
6. **app (`app_user.{c,h}`, `application.fam`)** entry point, lifecycle, settings load.

> **Regla de oro:** si un archivo en `libraries/chip/` importa algo de `modules/` o `scenes/`, es un bug de arquitectura. Si un archivo en `modules/` toca el `Submenu` directamente desde un worker thread, es un bug de capa.

---

## 4. RX dispatch — componente crítico

Este es el componente que si está mal planeado hace fracasar la Fase 2 entera. Detalle:

```c
// libraries/chip/rx_dispatch.h
typedef bool (*rx_filter_fn)(const uint8_t* frame, uint16_t len, void* ctx);
typedef void (*rx_handler_fn)(const uint8_t* frame, uint16_t len, void* ctx);

typedef struct rx_handle rx_handle_t;

rx_handle_t* rx_register(rx_filter_fn filter, rx_handler_fn handler, void* ctx);
void rx_unregister(rx_handle_t* h);

// Internamente: un solo FuriThread "rx_dispatch" lee del chip y llama a todos
// los filters registrados; los que retornen true reciben el handler.
// Multiple handlers pueden matchear el mismo frame (sniff + ARP reply, p.ej.)
```

**Garantías:**
- Un solo thread lee del chip. Elimina contention.
- Handlers corren en el thread del dispatcher — deben ser cortos. Si necesitan trabajo pesado, hacen `furi_message_queue_put` a su propia cola.
- `rx_register`/`unregister` son thread-safe (mutex interno).
- Frames descartados (sin filter match) se contabilizan en métricas de drop.

**Migración (parte de F0.4):**
- `app_worker.c` se reemplaza: la auto-respuesta ARP/ICMP se vuelve dos handlers registrados al boot.
- Cada scanner scene migra: en `on_enter` registra su handler con su predicate; en `on_exit` lo desregistra. Borra los `furi_thread_suspend`/`resume` de scenes.

**Ownership:** un solo desarrollador escribe `rx_dispatch.{c,h}` en F0.4. PR de migración separado por feature (ARP, ICMP, sniffer, DHCP, OS detector, port scan, ARP spoof) — total 7 PRs pequeños tras el merge del dispatcher.

**Riesgos específicos:**
- Race entre register/unregister y dispatch loop. Mitigación: RCU-style lock-free linked list o RW-mutex con generation counter.
- Latencia de handler bloquea el resto. Mitigación: timeout interno + warning log.
- Memoria del frame: zero-copy desde el RX FIFO del chip o copy-once a buffer compartido. **Decisión de F0.4**: copy-once a un staging buffer del dispatcher; los handlers reciben un `const uint8_t*` que solo es válido durante la llamada. Quien necesite persistirlo hace su propio copy.

---

## 5. Vendoring / dependencias

No hay vendoring de terceros. Todo es:
- Flipper SDK (pineado en `application.fam` con `targetSdkVersion=`).
- Código propio en `libraries/`.

**Pendiente F0:** verificar headers de `enc28j60.{c,h}` para confirmar attribution / licencia de fragmentos absorbidos de EtherShield/EtherCard. Si hay GPLv2, decidir reescritura o explicit attribution en `LICENSE`.

---

## 6. Plan por fases

> **Regla maestra:** cada fase termina con commit anotado y tag `vX.Y-fN`. Claude Code para y espera validación humana antes de la siguiente. No salta fases. Si F(N) no está verde, no toca F(N+1).

---

### F0 — Saneamiento (PRIORITARIA)

**Objetivo:** dejar la base lista para soportar el roadmap. Sin features nuevos. Solo refactors estructurales y bug fixes.

**Sub-fases (pueden ejecutarse en orden o paralelo donde se indica):**

#### F0.0 — Bootstrap y verificaciones de realidad (PRIMERO)
- Crear branch `refactor/phase-0`.
- Confirmar uFBT instalado y compila el proyecto actual sin cambios.
- Verificar licencia upstream de `libraries/chip/enc28j60.c` (decisión #11 del template).
- Crear `docs/ARCHITECTURE.md` v0 (snapshot del estado actual).
- Habilitar Discussions en GitHub + 3 categorías + 3 issue templates (F0.9 del roadmap).

**Criterio:** repo en `refactor/phase-0`, build verde sin cambios funcionales, `docs/ARCHITECTURE.md` v0 commiteado.

#### F0.1 — Centralizar scan_params en App
**Entregables:**
- Nuevo struct `App.scan_params` con: `target_ip[4]`, `target_port`, `range_port_start`, `range_port_end`, `protocols_index`, `ip_ping[4]`, `ip_start[4]`, `range_ip`, `hostname[64]`.
- Eliminar las 3 declaraciones de `target_ip[4]` (`OsDetector.c:4`, `ArpSpoofingSpecificIP.c:9`, `PortsScannerScene.c:10`) y todos los demás file-static target globals.
- Actualizar todos los call-sites.

**Criterio:** `grep -rn "static .*target_ip\[4\]\|^uint8_t target_ip\[4\]" EthernetAppDemo/` retorna 0. Smoke-test manual: port scanner, OS detector, ARP spoof a IP específica funcionan.

#### F0.2 — Settings persistencia
**Entregables:**
- `libraries/settings/settings.{c,h}` wrap de `flipper_format`.
- `settings_load(app)` en boot, `settings_save(app)` en cada cambio.
- Persistir: MAC actual, static-vs-DHCP, last `scan_params.target_ip`, last port range, hostname DORA, prefijo PCAP.

**Criterio:** cambiar MAC en SettingsScene, cerrar app, reabrir → MAC persistente.

#### F0.3 — Generic scanner_session
**Entregables:**
- `libraries/scanner/scanner_session.{c,h}` con `scanner_resolve_target`, `scanner_wait_for_packet`, `scanner_progress`.
- Migrar 6 scanners actuales: arp_scan_network, tcp_syn_scan, udp_port_scan, os_scan, ping_thread, arpspoofing_thread.

**Criterio:** los 6 scanners reescritos sobre `scanner_session_t`. Cada uno pierde 60-100 líneas. Tests manuales pasan. `wait` con timeout no spin-locks.

#### F0.4 — RX dispatch decoupling (sección §4)
**Entregables:**
- `libraries/chip/rx_dispatch.{c,h}` (single thread, register/unregister, copy-once).
- Borrar `app_worker.c`. Auto-ARP-reply y auto-ICMP-reply son ahora 2 handlers registrados en boot.
- Migrar cada scanner scene a `rx_register`/`rx_unregister` en `on_enter`/`on_exit`.
- Eliminar todos los `furi_thread_suspend(ethernet->thread)` / `resume` de `scenes/`.

**Criterio:** `grep -rn "furi_thread_suspend\|furi_thread_resume" EthernetAppDemo/scenes/` retorna 0. Sniff funciona durante un port-scan simultáneo.

#### F0.5 — Bulk SPI + INT pin + SRAM layout
**Entregables:**
- Reescribir `read_buffer` / `write_buffer` en `enc28j60.c` con single-acquire bulk SPI.
- Conectar pin INT del módulo ENC28J60 a un GPIO de Flipper (decisión de pin: F0.0). Reemplazar polling de `EPKTCNT` por `furi_hal_gpio_add_int_callback`.
- Cambiar layout SRAM: `RXSTART_INIT=0x0000`, `RXSTOP_INIT=0x17FF` (6 KB RX), `TXSTART_INIT=0x1800`, `TXSTOP_INIT=0x1FFF` (2 KB TX).

**Criterio:** sniff sostenido medido con iperf TCP unidireccional ≥6 Mbps. RX FIFO drops bajo carga sostenida documentados (puede no llegar a 0; se acepta).

#### F0.6 — Limpiar dev scaffolding
**Entregables:**
- Eliminar `ofp_tseq` en `os_detector_module.c:517-585` (decision: eliminar, no rescatar — el caller `os_scan` ignora el resultado en producción según la auditoría).
- Borrar empty loops `for(j=0;j<prbOpts[i].len;j++);` en líneas 519, 574, 891, 938, 948.
- `enc28j60.c:7 DEBUG_MESSAGE` → 0; `enc28j60.c:24 SHOW_PACKETS_RECEIVED` → 0.
- `tcp_module.c:12 DEBUG` y `udp_module.c:13 DEBUG` gated por `DEV_MODE`.
- Eliminar `TestingScene` completa (registro en `scenes_config/app_scene_config.h:34` + archivos `.c/.h` + case en `MainMenuScene.c:49-55`).
- Eliminar `flipper_process_dora` (sin hostname, dead code).
- Normalizar comentarios a inglés en archivos tocados.

**Criterio:** `grep -rn "ofp_tseq\|TestingScene\|flipper_process_dora\b" EthernetAppDemo/` retorna 0. `printf` count se reduce ≥30%.

#### F0.7 — Bug fixes consolidados
**Entregables:** un commit por bug, referenciando file:line.

| Bug | Ubicación | Fix |
|-----|-----------|-----|
| PCAP timestamps | `capture_module.c:42-48` | Usar `furi_hal_rtc_get_timestamp` + struct `time_t`. |
| `is_duplicated_ip` | `arp_module.c:323` | Retornar 0 explícito si count==0. |
| `tcp_send_xmas_probe` | `tcp_module.c:817` | Retornar `true` si send OK. |
| `subnet_mask` buffer | `dhcp_protocol.c:232,392` | Buffer 4 bytes correcto. |
| Falta `break` | `tcp_module.c:349, 437, 467, 583` | `__attribute__((fallthrough))` o break. |
| `pcap_scan` overflow | `ReadPcapsScene.c:5`, `capture_module.c:195` | Allocar dinámico, bounds check. |
| ARP/ICMP auto-reply prematuro | (post-F0.4) | Gate por `is_static_ip || dhcp_completed`. |
| Logo delay | `MainMenuScene.c:39` | FuriTimer no-bloqueante. |

**Criterio:** todos los bugs cerrados con commit individual. Tests manuales del feature afectado.

#### F0.8 — Doble build CI
**Entregables:**
- `application.fam` define `PENTEST_MODE` flag en `cdefines`.
- `app_user.h` añade macro `IF_PENTEST(...)` (análogo a `#if DEV_MODE`).
- `.github/workflows/build.yml` corre dos builds: uno con `PENTEST_MODE=0` → `ethernet-admin.fap`, otro con `PENTEST_MODE=1` → `ethernet-pentest.fap`. Ambos artifacts en cada release.

**Criterio:** Action verde, dos `.fap` con tamaños distintos. `strings ethernet-admin.fap | grep -i "rogue\|spoof"` debe retornar 0 (los strings de pentest no deben colarse al admin).

#### F0.9 — Discussions + templates
Ya cubierto en F0.0. Confirmar visibilidad pública.

**Salida F0:**
- Codebase 30-40% más liviano.
- 2 artefactos `.fap` produciéndose en CI.
- Settings persisten.
- Throughput ≥6 Mbps.
- Discussions abiertas.

**Criterio de aceptación binario:** todos los criterios anteriores cumplidos. Tag `v2.0-f0`.

**Checkpoint humano F0:**
1. Sabas verifica: build verde, dos `.fap` adjuntos al release de prueba.
2. Sabas corre los 6 scanners migrados sobre red de lab y confirma que funcionan.
3. Sabas mide throughput con iperf.
4. Sabas confirma que MAC persiste tras reboot del Flipper.

---

### F1 — Recon completo (Capa A)

**Objetivo:** `ethernet-admin.fap` en estado distribuible con todas las features de recon.

**Entregables:** F1.1..F1.27 del roadmap (`docs/superpowers/specs/2026-05-05-roadmap-design.md` §4). Cada feature es un PR independiente sobre `main` post-F0.

**Orden recomendado** (depende de bloqueadores):
1. F1.1, F1.2, F1.3 — LLDP/CDP/EAPOL passive harvest (rápido, ROI alto, valida la pipeline post-F0).
2. F1.5, F1.6, F1.7 — mDNS, DHCP fingerprint, DHCP tracker.
3. F1.9, F1.10, F1.11, F1.12 — traceroute, rDNS sweep, WoL send/capture.
4. F1.13, F1.14, F1.15, F1.16 — banner grab + UDP service scan + SYN/FIN/NULL/Xmas.
5. F1.17, F1.18 — SNMP brute + walk.
6. F1.19, F1.20, F1.21 — top-talkers, counters, asset export.
7. F1.22, F1.26, F1.27 — PCAP live-write + on-device analyzer + auto-summary.
8. F1.4, F1.8, F1.23, F1.24 — VLAN sniffer, SSDP, link diag, stealth mode (cleanup).
9. F1.25 — passive OS fingerprint (más complejo, último).

**Criterio:** `ethernet-admin.fap` con todos los features. Demo "plug 60s → inventory + topology + PCAP + report" pasa en lab.

**Checkpoint humano F1:** Sabas corre el demo end-to-end y aprueba. Tag `v2.0`.

---

### F2 — MITM kill chain (Capa B)

**Objetivo:** `ethernet-pentest.fap` con kill chain completa.

**Entregables:** F2.1..F2.22 del roadmap §5. Cada feature gated por `IF_PENTEST` + warning screen runtime.

**Orden recomendado:**
1. F2.1 — Rogue DHCP (force multiplier, completa el DHCP module).
2. F2.2, F2.3, F2.4, F2.5 — Responder mode (LLMNR, NBT-NS, mDNS, WPAD).
3. F2.6 — DNS spoof.
4. F2.7, F2.8, F2.9 — IPv6 RA + NDP + DHCPv6.
5. F2.11, F2.12, F2.13, F2.14 — VLAN hopping (DTP, LLDP/CDP impersonation, double-tag).
6. F2.10 — ICMP redirect.
7. F2.16 — TCP RST injection.
8. F2.17, F2.18, F2.19 — MAC spoof, port stealing, HSRP/VRRP.
9. F2.20, F2.21, F2.22 — Captive portal, TFTP rogue, SNMP trap.

**Criterio:** kill chain "plug → recon → become gateway → capture creds" demonstrable end-to-end.

**Checkpoint humano F2:** Sabas reproduce el kill chain en lab con un Windows test target. Tag `v3.0`.

---

### F3 — Stress (Capa C)

**Objetivo:** features Aggr 4 detrás de `AUTHORIZED.txt`.

**Entregables:** F3.1..F3.12 del roadmap §6. Cada feature requiere check runtime de `/ext/apps_data/ethernet/AUTHORIZED.txt` para aparecer en menú.

**Criterio:** sin AUTHORIZED.txt, ningún item Aggr 4 aparece. Cada attack pide confirmación "Yes/Cancel" antes de disparar.

**Checkpoint humano F3:** Sabas verifica gating con/sin AUTHORIZED.txt.

---

### F4 — Track OT/ICS

Paralelo a F1/F2/F3, con su propia cadencia.

**Entregables:**
- Recon (en admin): OT.1..OT.7 (Modbus-TCP, S7, BACnet, DNP3, EtherNet/IP, MQTT, CoAP probes).
- R/W (en pentest, gated): OT.8, OT.9 (Modbus-TCP read, S7 read).
- Write (gated por AUTHORIZED.txt): OT.10, OT.11 (write con doble confirmación).

**Criterio:** sección "Industrial / OT" en el menú principal con submenú por protocolo. Detección de un PLC Siemens en lab (si hay disponible).

**Checkpoint humano F4:** Sabas (o user con acceso a lab industrial) prueba contra HW real.

---

### F5 — Drop-box mode + BLE telemetry

**Entregables:** D.1..D.6 del roadmap §8.

**Criterio:** Flipper con perfil `recon-only.script` + `AUTHORIZED.txt` ausente puede sentarse 24h y reportar inventario via BLE sin tocar pantalla.

---

### FN — Hardening, docs, release final

**Entregables:**
- `docs/` completa según §13 del roadmap.
- Benchmarks de throughput, heap libre, latencia.
- Security review de gating runtime (F0.8 + F3 + F4 OT-write).
- CHANGELOG completo.
- Release `v4.0` con ambos `.fap`, `AUTHORIZED.txt.example`, README actualizado.

---

## 7. Prompt inicial para Claude Code

> Pegar como **primer mensaje** después de crear `refactor/phase-0`.

```
Vamos a refactorizar la app Flipper ENC28J60 Ethernet en una plataforma
de auditoría LAN/OT con dos artefactos .fap (admin/pentest) y track
paralelo OT/ICS.

Todo el plan, decisiones y arquitectura están en
ENC28J60_REFACTOR_PLAN.md en la raíz del repo, y el roadmap completo en
docs/superpowers/specs/2026-05-05-roadmap-design.md.
Léelos ENTEROS antes de escribir una línea de código.

Resumen de decisiones irrevocables (no las re-litigues):
  - Refactor incremental F0 + roadmap F1-F3 + track OT.
  - C, Flipper FAP, uFBT/FBT.
  - Arquitectura en capas: chip-driver / protocol-craft / scanner /
    module / scene / app, con un único RX dispatcher.
  - Sin gestor de deps; bibliotecas internas en libraries/.
  - Dos artefactos: ethernet-admin.fap (PENTEST_MODE=0) y
    ethernet-pentest.fap (PENTEST_MODE=1). Capa C en pentest gated por
    AUTHORIZED.txt en SD. Capa D solo compile-time DEV_MODE.
  - OT/ICS in-scope. Recon en admin, R/W en pentest.
  - Drop-box mode + BLE telemetry in-scope.
  - 802.1X bridge bypass / transparent inline MITM / cable wiremap
    out-of-scope (necesitan 2da NIC).
  - Comentarios en inglés. Submenu como UI primaria.
  - Persistencia: flipper_format en /ext/apps_data/ethernet/settings.cfg.
  - PCAP path no cambia: /ext/apps_data/ethernet/files/.
  - Fase 0 es prerequisito ABSOLUTO. Sin F1 hasta F0 verde.

Contexto verificado del entorno:
  - ENC28J60: 10BASE-T only, sin auto-neg funcional, 8 KB SRAM,
    filtros HW HTEN/PMEN/MPEN/BCEN/MCEN/UCEN/CRCEN, INT pin no cableado.
  - Throughput hoy: ~3-4 Mbps sniff / ~1500-3000 pps TX.
    Bottleneck: SPI per-byte acquire en read_buffer/write_buffer.
  - Layout SRAM hoy: RX 3 KB / TX 1.5 KB. 3.5 KB ociosos.
  - Heap del FAP: stack 30 KB, worker 10 KB, enc28j60_t 3 KB embebido.
  - Bugs verificados con file:line listados en §1 contexto crítico
    del plan maestro.
  - Repo: 8 stars, 0 issues, 0 forks, Discussions deshabilitadas.
  - Sibling Electronic Cats apps siguen el mismo layout
    (MCP2515-CANBUS, rs485modbus, SX1262-LoRa).

Metodología:
  1. Trabajamos sub-fase por sub-fase F0.0 → F0.9 → F1 → F2 → ...
     siguiendo el orden del plan.
  2. Cada sub-fase termina con commit + tag anotado (vX.Y-fN.M).
  3. Al final de cada sub-fase paras y reportas:
     (a) entregables hechos,
     (b) criterio de aceptación verificable,
     (c) checkpoints que debo correr yo (build + smoke test en HW),
     (d) dudas abiertas.
     Esperas mi confirmación antes de F(N+1) o F0.(M+1).
  4. No saltas fases. Si F(N) no está verde, no tocas F(N+1).
  5. Tests manuales pasan siempre antes de commitear.
  6. Cualquier feature de Capa B/C/D requiere flag IF_PENTEST/DEV_MODE
     correspondiente. Capa B/C nunca compila en admin.fap.

Reglas críticas de trabajo:
  - NUNCA supongas datos que puedes verificar leyendo el código,
    el datasheet, los docs Flipper o corriendo un comando.
    Si no sabes algo, lee o pregunta. No adivines.
  - Si encuentras una contradicción entre el plan y la realidad
    (ej: una feature ya parcialmente hecha que no detectamos en la
    auditoría), para y reporta. No "resuelvas" la contradicción
    unilateralmente.
  - Si una decisión de §1 resulta inviable (ej: heap insuficiente
    para responder + rogue dhcp simultáneos), para y reporta. No
    busques workarounds silenciosos.
  - Comentarios y commit messages en inglés. Doc user-facing en
    español/inglés según ya esté el archivo.
  - Antes de borrar código que parece muerto, verifica con grep que
    no hay un caller oculto.

Empieza con F0.0. Primero:
  1. Léeme el plan resumido estructurado en máximo 30 líneas.
  2. Confírmame que entendiste las 18 decisiones de §1.
  3. Tareas de investigación específicas de F0.0:
     a. Verificar la licencia upstream de los archivos
        EthernetAppDemo/libraries/chip/enc28j60.{c,h} (decisión #11
        del plan maestro pendiente).
     b. Identificar qué pin GPIO de Flipper estaría disponible para
        cablear el INT del ENC28J60 (revisar pinout actual del shield
        Electronic Cats Ethernet en docs/HARDWARE.md o repo del shield).
     c. Verificar la versión exacta de targetSdkVersion en
        application.fam y si requiere bump.
  4. Propón el plan DETALLADO de F0.0 sin escribir código:
     - Lista de archivos a crear/modificar con descripción de 1 línea.
     - Comandos exactos para habilitar Discussions
       (gh api repos/.../discussions o UI manual).
     - Plantillas issue propuestas (bug, feature, hardware-compat).
     - Riesgos identificados con plan de mitigación.
  5. Yo apruebo o ajusto, y recién ahí ejecutas F0.0.

Preguntas abiertas que quiero que respondas en tu propuesta de F0.0:
  a. ¿La licencia de enc28j60.{c,h} permite el uso MIT actual del
     repo, o requiere reescritura/attribution explícita?
  b. ¿Qué pin GPIO usaríamos para INT? ¿Está libre en el shield?
  c. ¿Cuál es el targetSdkVersion mínimo que necesitamos pinear para
     soportar furi_format + furi_hal_gpio_add_int_callback?
  d. ¿Hay algún PR pending unmerged que debería re-basearse antes de
     empezar F0.1?
```

---

## 8. Superpowers: skill del proyecto

Crear `.claude/skills/enc28j60-fase-actual/SKILL.md` y actualizarlo al inicio de cada sub-fase. Plantilla:

```markdown
---
name: enc28j60-fase-actual
description: Contexto activo del refactor ENC28J60. Consultar antes de cualquier commit, creación de archivo, o cambio de dirección.
---

# Fase actual: F0.<N> — <nombre de la sub-fase>

## Qué está hecho
- v2.0-f0.0: bootstrap + Discussions + license verify
- v2.0-f0.1: scan_params centralizado
- ...

## En qué estamos
<2-3 líneas del estado>. Issues abiertos:
- <issue 1>
- <issue 2>

## Qué NO tocar
- Áreas fuera de la sub-fase actual.
- Scenes/modules sin cambios planificados en F0.

## Salida de F0.<N>
- [ ] criterio 1
- [ ] criterio 2

## Reglas extra de esta fase
- <reglas temporales>
```

---

## 9. Riesgos y decisiones pendientes

### R1 — Heap budget en Fase 2 (alto)
Responder + rogue DHCP + DHCPv6 simultáneos pueden chocar contra el techo de heap (~150 KB libres durante operación normal). Plan A: medir heap libre como métrica continua exhibida en Settings, gate features simultáneas con un counter. Plan B (si plan A insuficiente): mover el `analysis_module` 1500-byte stack array al heap (allocate on demand), y/o reducir worker thread stack a 6 KB.

### R2 — Errata #18 RX wraparound bajo carga sostenida (medio)
El driver mitiga (RXSTART=0x0000, ERXRDPT odd), pero stress tests largos en redes ruidosas pueden re-exhibir corrupción. Plan A: añadir test de stress automatizado (plan crece a iperf + tcpreplay durante 1h con check de PCAP integrity). Plan B: si recurrente, fallback a layout SRAM original (RX 3 KB) y aceptar throughput menor.

### R3 — Perfil legal del `pentest.fap` distribuible (alto)
Distribuir un binario con LLMNR poisoner / rogue DHCP puede exponer a Electronic Cats legalmente. Plan A: README del release con texto legal explícito, separación clara admin vs pentest, registro en lab.flipper.net solo del admin.fap. Plan B: gate adicional en GitHub Release que requiere aceptación de EULA antes del download (custom action). Plan C (último recurso): mover `pentest.fap` a un repo separado privado/gated.

### R4 — Test hardware setup limitado (medio)
Features L2-L3 requieren switches reales con CDP/LLDP/STP/802.1X/voice VLAN. Sabas no necesariamente tiene un Cisco managed disponible. Plan A: documentar lab en `docs/LAB_SETUP.md`, recomendar EVE-NG / GNS3 con IOSv para test virtuales. Plan B: scapy emulando peer en CI para items pure-software (LLDP/CDP probes mock).

### R5 — Drift con upstream firmware Flipper SDK (bajo)
Flipper rompe APIs ocasionalmente entre minor versions. Plan A: pinear `targetSdkVersion` en `application.fam`, CI con matriz de SDK versions. Plan B: dependabot opcional cuando GitHub lo soporte para Flipper SDK.

### R6 — Ambigüedad en el patrón de scenes durante migración F0.4 (medio)
La migración de cada scene a `rx_register/unregister` puede romper navegación. Plan A: migrar una scene a la vez, smoke-test después de cada migración, no batchear. Plan B: si una scene resiste la migración, documentar en `docs/RX_DISPATCH.md` y considerarla "legacy" sin promover features nuevos sobre ella hasta refactor dedicado.

### R7 — Pin INT del ENC28J60 ocupado en el shield Electronic Cats (medio)
F0.5 asume que hay un GPIO libre en el conector del shield para el INT. Si no lo está físicamente, fallback a polling (sin la mejora de eficiencia). Plan A: verificar pinout del shield en F0.0 antes de commit. Plan B: si no hay pin libre, F0.5 entrega solo el bulk SPI mejor (sin INT), y el throughput esperado baja a ~5 Mbps.

### Decisiones pendientes (a cerrar en F0.0)
- D1 — Licencia upstream de `enc28j60.{c,h}`.
- D2 — Pin GPIO específico para INT.
- D3 — `targetSdkVersion` exacto a pinear.
- D4 — ¿Borrar ARP-spoof-to-IP del DEV_MODE actual y promoverlo a `pentest.fap`? (Está implementado, solo gated.)

---

## 10. Relación con el sistema actual

Este es un proyecto greenfield-ish: existe código en `main`, pero el F0 lo refactoriza estructuralmente sin romper la "experiencia de usuario actual". La compatibility surface visible es:
- PCAP path: `/ext/apps_data/ethernet/files/` — **se mantiene**.
- MAC default `0xba 0x3f 0x91 0xc2 0x7e 0x5d` — **se mantiene** como fallback (settings.cfg lo override si el usuario cambió).
- Menú principal layout — **se mantiene** salvo eliminación de `TestingScene` (oculta hoy).
- Scenes existentes — **se mantienen** todas, refactorizadas internamente.

**Migración de PCAPs viejos:** los PCAPs guardados con timestamps incorrectos (bug de `capture_module.c:42-48`) seguirán siendo legibles por Wireshark pero con timestamps erráticos. Se documenta en `docs/PCAP_FORMAT.md`. No se hace migración automática.

**Migración de settings:** previo a F0.2 no había settings persistentes; primer arranque post-F0.2 inicializa con defaults. Sin migración necesaria.

---

## 11. Documentación mínima esperada

A producir a lo largo de las fases:

- `docs/ARCHITECTURE.md` — F0.0 (snapshot inicial), iterado en F0.4.
- `docs/SCANNER_SESSION.md` — F0.3.
- `docs/RX_DISPATCH.md` — F0.4.
- `docs/PROTOCOLS.md` — F1.
- `docs/OT_PROTOCOLS.md` — F4.
- `docs/DROPBOX.md` — F5.
- `docs/SECURITY.md` — F0.8 (gating, AUTHORIZED.txt format) + F3 (DoS warnings) + F4 (OT-write doble confirmación).
- `docs/LAB_SETUP.md` — F1 (cuando se necesite Cisco/managed switch).
- `docs/PCAP_FORMAT.md` — F0.7 + F1.22.
- `docs/HARDWARE.md` — F0.0 (pinout del shield, INT pin decision).
- `docs/ROADMAP.md` — versión user-facing simplificada del roadmap, cada major release.
- `CHANGELOG.md` — desde `v2.0-f0.0`.
- `README.md` — actualizado en F1 release con descripción admin/pentest.

---

## 12. Decisiones que Claude Code resolverá en F0.0

- D1 — Licencia exacta de `libraries/chip/enc28j60.{c,h}` (leer headers, comparar con EtherShield/EtherCard upstream).
- D2 — Pin GPIO de Flipper para INT del ENC28J60 (verificar pinout shield Electronic Cats).
- D3 — `targetSdkVersion` exacto a pinear en `application.fam`.
- D4 — Convención exacta de tags: `vX.Y-fN.M` con `M` opcional para sub-fases.
- D5 — Convención de naming de issue templates (`.github/ISSUE_TEMPLATE/`).
- D6 — Path exacto de `AUTHORIZED.txt` y formato (vacío vs. requirement de engagement ID dentro).
- D7 — Estructura del profile script para drop-box (F5) — se decide cerca de F5, no en F0.

Cada decisión se commitea como entrada en `docs/DECISIONS.md` con fecha, opciones consideradas, decisión, rationale.
