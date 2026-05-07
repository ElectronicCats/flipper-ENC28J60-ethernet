# Roadmap — Flipper ENC28J60 Ethernet App

**Fecha:** 2026-05-05
**Estado:** Aprobado para Fase 0
**Autor:** Sabas + investigación multi-agente

---

## 1. Visión y posicionamiento

Convertir la app de un toolkit de pentest cableado funcional en la **plataforma de auditoría LAN/OT de referencia para Flipper Zero**, ocupando un nicho que hoy está vacío:

- **0 competidores** en pentesting cableado para Flipper. El único proyecto adyacente (`fz-eth-troubleshooter` sobre W5500) es solo diagnóstico.
- **Marauder y derivados WiFi no pueden tocar L1/L2 cableado** por física — segmentos ICS/SCADA, server-rooms, VLANs admin, y biomed/building-automation están completamente fuera de su alcance.
- **Sinergia interna Electronic Cats**: el shield Modbus-RS485 ya cubre serial industrial; un Modbus-TCP/S7/BACnet/DNP3 sobre ENC28J60 cierra el loop OT.
- **Producto dual red+blue team**: las features de capa 1-2 (recon, monitor, top-talkers, asset inventory) son tan útiles para un network admin como para un red-teamer, lo cual duplica el mercado direccionable.

Diferenciadores únicos que ningún competidor puede copiar:
1. Auditar segmentos sin WiFi (ICS, biomed, ATM, building-automation).
2. Sin huella 802.11 → invisible a Meraki/Aruba IDS de WiFi.
3. Drop-box mode con BLE telemetry (físicamente solo un Flipper puede combinar SPI + BLE + SD + battery en este factor).
4. PCAPs estrictamente Wireshark-clean (Marauder está plagado del bug "0-byte PCAP").

---

## 2. Modelo de gating elegido — Dos artefactos `.fap`

CI produce dos binarios independientes desde el mismo árbol:

| Artefacto | Compile-time flags | Capas habilitadas | Uso |
|-----------|--------------------|--------------------|-----|
| `ethernet-admin.fap` | `PENTEST_MODE=0`, `DEV_MODE=0` | A — Recon, passive, banner, scan, OT-passive, drop-box-recon | Network admin / blue team / educación. Distribuible públicamente sin gating runtime. |
| `ethernet-pentest.fap` | `PENTEST_MODE=1`, `DEV_MODE=0` | A + B (spoof, MITM, poisoning, rogue services) + C (DoS/flood, gated por archivo `AUTHORIZED.txt` en SD) | Pentesters con permiso por escrito. Release etiquetado claramente. |
| (build local) | `DEV_MODE=1` | + D (destructivo/persistente, dev-only) | Solo build desde source para desarrolladores. |

**Por qué dos artefactos en lugar de gate runtime único:**
- Separación legal limpia: `ethernet-admin.fap` no contiene **código** ofensivo, no solo está oculto. Una organización puede aprobar la distribución del admin sin revisar lo ofensivo.
- Claridad para usuarios casuales que descargan desde lab.flipper.net y community catalogs: el nombre del archivo deja claro qué están corriendo.
- Auditabilidad: un firewall corporativo o EDR puede whitelistear el hash de `ethernet-admin.fap` sin habilitar el pentest.
- Capa C dentro de pentest sigue requiriendo `/ext/apps_data/ethernet/AUTHORIZED.txt` — fricción adicional intencional para flood/DoS dentro del propio artefacto pentest.

**Convención de nivel de agresividad** (definida en cada feature del roadmap):
- **A1** — Passive sniff, decode, harvest. Cero frames TX.
- **A2** — Active recon. Probes/scans/enum. Detectable, no disruptivo.
- **A3** — Spoof/poison/MITM. Manipula estado del protocolo.
- **A4** — DoS/flood. Degrada disponibilidad.
- **A5** — Destructivo/persistente. Impacto duradero.

---

## 3. Fase 0 — Saneamiento (PRIORITARIA)

**Objetivo:** dejar la base lista para soportar el roadmap. Sin features nuevos. Solo refactors estructurales y bug fixes que hoy bloquean o degradan todo lo siguiente.

### F0.1 — Centralizar estado de scanners (RF-3)

Mover los `target_ip[4]` triplicados de `OsDetector.c:4`, `ArpSpoofingSpecificIP.c:9`, `PortsScannerScene.c:10`, junto con `target_port`, `range_port`, `protocols_index`, `ip_ping`, `ip_start`, `range_ip` (file-static en `PingScene.c:14`, `ArpScannerScene.c:16-17`), a un nuevo struct `App.scan_params`. Eliminar la colisión del símbolo no-static.

**Criterio de aceptación:** `grep -rn "static .* target_ip\[4\]" EthernetAppDemo/` retorna 0 resultados; compila; smoke-test de port scanner, OS detector y ARP spoof a IP específica pasan.

### F0.2 — Settings persistencia

Implementar `/ext/apps_data/ethernet/settings.cfg` usando `flipper_format`. Persistir: MAC actual, IP actual / static-vs-DHCP, last-target-IP, last-port-range, hostname para DORA, prefijo de PCAP filename.

**Criterio:** cambiar MAC en Settings, salir, reentrar app, MAC se conserva.

### F0.3 — Generic scanner session (RF-1)

Nuevo `libraries/scanner/scanner_session.{c,h}` con tres primitivas:
- `scanner_resolve_target(session, ip) -> mac` (cached, evita llamar `arp_get_specific_mac` repetidamente — hoy duplicado en ArpSpoofingSpecificIP).
- `scanner_wait_for_packet(session, predicate_fn, timeout_ms) -> rx_buffer | NULL` (reemplaza el `while (now-last < N) { receive_packet; if matches break }` duplicado en `arp_module.c:212-222`, `tcp_module.c:263, 369, 488, 752`, `ping_module`, `udp_module`).
- `scanner_progress(session, submenu_index, text)` que envía custom event al ViewDispatcher en lugar de tocar el Submenu desde el worker thread.

**Criterio:** los 6 scanners actuales se reescriben sobre `scanner_session_t`; cada uno pierde 60-100 líneas; tests manuales pasan.

### F0.4 — RX dispatch decoupling (RF-2)

Reemplazar el patrón "scene suspends worker, takes radio exclusively, resumes" por un único `enc28j60_rx_dispatch_thread` con tabla de handlers `(filter_predicate, callback)` registrables. Scenes hacen `rx_register(predicate, cb)` en `on_enter` y `rx_unregister` en `on_exit`.

Beneficio inmediato: ARP auto-reply puede seguir corriendo durante un port-scan; sniff puede correr durante un MITM. Habilita Fase 2 entera.

**Criterio:** ningún `furi_thread_suspend(ethernet->thread)` o `furi_thread_resume` queda en `scenes/`.

### F0.5 — Bulk SPI + INT pin (RF-4)

Reescribir `read_buffer` / `write_buffer` en `libraries/chip/enc28j60.c` para hacer single-acquire bulk SPI (hoy un acquire por byte costoso en FreeRTOS). Conectar el INT pin del módulo ENC28J60 a un GPIO de Flipper como interrupción → reemplazar el polling de `EPKTCNT` (`enc28j60.c:547`).

Ampliar layout SRAM del chip: RX 6 KB / TX 2 KB (hoy 3 KB / 1.5 KB con 3.5 KB ociosos).

**Criterio:** sniff sostenido sube de ~3-4 Mbps a ≥6 Mbps medido con iperf TCP unidireccional desde laptop.

### F0.6 — Limpiar dev scaffolding (RF-5)

- Eliminar `ofp_tseq` en `os_detector_module.c:517-585` o reescribirla para honrar el `target_ip` argumento (hoy ignorado, hardcodea `192.168.0.103` y MAC `00:E0:4C:68:0E:C5`).
- Borrar los `for(j=0;j<prbOpts[i].len;j++);` empty loops en `os_detector_module.c:519, 574, 891, 938, 948`.
- Flip `enc28j60.c:7` `DEBUG_MESSAGE` → 0 y `enc28j60.c:24` `SHOW_PACKETS_RECEIVED` → 0 en release.
- Gate `tcp_module.c:12 DEBUG` y `udp_module.c:13 DEBUG` detrás de `DEV_MODE`.
- Eliminar `TestingScene` o conectar el `case TESTING_OPTION` en `MainMenuScene.c:49-55`. Decisión: **eliminar**, no es valioso.
- Eliminar `flipper_process_dora` (sin hostname, dead code) — solo `flipper_process_dora_with_host_name` se usa en producción.

**Criterio:** `grep -rn "printf\|//" EthernetAppDemo/` muestra reducción ≥30% en líneas comentadas/printf-debug.

### F0.7 — Bug fixes consolidados (RF-6)

| Bug | Ubicación | Fix |
|-----|-----------|-----|
| PCAP timestamps mal (meses×30, sin bisiestos) | `capture_module.c:42-48` | Usar `furi_hal_rtc_*` epoch + `time_t` correcto. |
| `is_duplicated_ip` retorna 0xFF cuando count=0 | `arp_module.c:323` | Retornar `false` explícito. |
| `tcp_send_xmas_probe` siempre retorna `false` | `tcp_module.c:817` | Retornar `true` tras send exitoso. |
| `subnet_mask` escrito en buffer MAC de 6 bytes | `dhcp_protocol.c:232,392` | Pasar buffer correcto de 4 bytes. |
| Falta `break;` entre cases en handshake | `tcp_module.c:349, 437, 467, 583` | Añadir o documentar fall-through con `__attribute__((fallthrough))`. |
| `pcap_scan` overflow silencioso de `packet_positions[2000]` | `ReadPcapsScene.c:5` + `capture_module.c:195` | Allocar dinámico en on_enter; check de bounds en `pcap_scan`. |
| Worker auto-responde ARP/ICMP con IP default antes de DORA | `app_worker.c` | Gate auto-reply detrás de `is_static_ip || dhcp_completed`. |
| Logo delay 1s bloquea GUI thread | `MainMenuScene.c:39` | Mover el splash a un FuriTimer con callback no-bloqueante. |

**Criterio:** todos los bugs cerrados con commit individual referenciando el archivo:línea.

### F0.8 — Doble build CI

`.github/workflows/build.yml` produce dos artefactos: `ethernet-admin.fap` (PENTEST_MODE=0) y `ethernet-pentest.fap` (PENTEST_MODE=1). Tag de release adjunta ambos.

`app_user.h` define `PENTEST_MODE` y nuevo macro `IF_PENTEST(...)` análogo al `#if DEV_MODE` actual. Cada nuevo feature de Capa B/C usa este macro.

**Criterio:** Action verde produce dos `.fap` con tamaños diferentes; el admin no contiene strings de las features pentest (verificable con `strings`).

### F0.9 — Activar Discussions y poblar templates

Habilitar GitHub Discussions en el repo, crear 3 categorías (Feature requests, Show & tell, Compat reports), y abrir 3 issue templates: bug report, feature request, hardware-compat.

**Checkpoint humano:** Sabas confirma que Discussions está visible y los templates aparecen al abrir un nuevo issue.

### Salida de Fase 0

- Codebase 30-40% más liviano (refactors + dead code purge).
- 2 artefactos `.fap` produciéndose en cada push.
- Settings persisten.
- Throughput de captura ≥6 Mbps.
- Backlog público abierto en GitHub Discussions.

**Estimado:** 3-5 semanas (depende de quién hace el trabajo y disponibilidad para testing en hardware).

---

## 4. Fase 1 — Recon completo (Capa A)

Todas las features aquí entran al `ethernet-admin.fap`. Aggr 1-2. Sin gating runtime.

| ID | Feature | Aggr | Esfuerzo | Notas |
|----|---------|------|----------|-------|
| F1.1 | LLDP passive harvest | A1 | S | Decode TLVs: chassis ID, port ID, system name, mgmt addr, VLAN, PoE. Filtro hardware vía PMEN (pattern match a EtherType 0x88CC). |
| F1.2 | CDP passive harvest | A1 | S | Cisco multicast 01:00:0C:CC:CC:CC. Mismo pattern-match. |
| F1.3 | 802.1X EAPOL recon | A1 | S | Detect EAPOL-Start, identidad EAP. Pattern-match a EtherType 0x888E. |
| F1.4 | Native-VLAN / mgmt-VLAN sniffer | A1 | S | Decode 802.1Q tag, lista todas las VLAN IDs vistas. |
| F1.5 | mDNS/Bonjour passive harvest | A1 | S | UDP/5353. Decode `_services._dns-sd._udp.local` → printers, AirPlay, AirDrop, Chromecasts. |
| F1.6 | DHCP option-55 fingerprint | A1 | S | Sobre el DHCP module existente. Mapear option-55 → OS conocido. |
| F1.7 | DHCP client tracker continuo | A1 | S | Long-running log de DISCOVERs vistos. |
| F1.8 | SSDP / UPnP discovery (M-SEARCH) | A2 | S | Enumera routers, smart TVs, NAS, IoT. |
| F1.9 | Traceroute (ICMP TTL + UDP variants) | A2 | S | Reusa `protocol_tools/icmp.c`. |
| F1.10 | Reverse DNS sweep | A2 | S | PTR query a cada IP del ARP scan. |
| F1.11 | Wake-on-LAN sender | A2 | S | UDP magic packet broadcast/directed. Demo killer. |
| F1.12 | WoL passive capture | A1 | S | Hardware-unlocked vía MPEN del ENC28J60. |
| F1.13 | Banner grab TCP (HTTP/SSH/SMTP/FTP) | A2 | S | Connect, read greeting, log. |
| F1.14 | Payload-aware UDP service scan | A2 | M | DNS, SNMP-get, NTP version, NetBIOS name, mDNS query, SSDP. Reemplaza udp_port_scan genérico. |
| F1.15 | SYN scan (half-open) | A2 | S | Más rápido y stealthier que connect scan actual. |
| F1.16 | FIN/NULL/Xmas scans (fix Xmas roto) | A2 | S | OS-specific differentiator. |
| F1.17 | SNMP community brute (public/private/wordlist) | A2 | S | UDP/161, dictionary local. |
| F1.18 | SNMP v1/v2c walk (ifTable, MAC table, IP-MIB) | A2 | M | Construye topology completo del switch. |
| F1.19 | Top-talkers / heat-map en pantalla | A1 | M | Bar chart custom canvas en Widget. |
| F1.20 | Per-host packet/byte counters time-series | A1 | S | Estructura de datos sobre el rx_dispatch (F0.4). |
| F1.21 | Asset inventory CSV/JSON export | A1 | S | Dump a SD de IP/MAC/hostname/OS/services. |
| F1.22 | PCAP live-write con BPF-lite filter | A1 | M | Pre-capture filter screen (broadcast / IP / port). Fix #1 más visible. |
| F1.23 | Link integrity diagnostics | A1 | S | Up/down, 10/100, FD/HD, errata 802.3 (PHY regs). |
| F1.24 | Stealth mode (TX disable) | A1 | S | PHCON2.TXDIS, hardware-unlocked. Elimina la huella de gratuitous ARPs durante sniff. |
| F1.25 | Passive OS fingerprint (p0f-style) | A1 | M | Complementa el OS detector activo existente. |
| F1.26 | On-device PCAP analyzer | A1 | M | Filtros, extract creds FTP/HTTP, TLS metadata. Mejora del viewer existente. |
| F1.27 | PCAP-to-report auto-summary | A1 | M | Hosts, services, creds, DNS queries. |

**Criterio Fase 1:** `ethernet-admin.fap` distribuible con todas estas features funcionando. Demo end-to-end "plug 60s, get inventory + topology + PCAP + report".

---

## 5. Fase 2 — MITM kill chain (Capa B)

Solo en `ethernet-pentest.fap`. Aggr 3. Cada feature con warning screen al primer uso por sesión.

| ID | Feature | Aggr | Esfuerzo | Notas |
|----|---------|------|----------|-------|
| F2.1 | Rogue DHCP server | A3 | M | Completa el DHCP module existente. Custom gateway/DNS/WPAD options. Single biggest L3 MITM primitive. |
| F2.2 | LLMNR poisoner | A3 | M | UDP/5355. Responder-on-Flipper. Captura NTLMv2. |
| F2.3 | NBT-NS poisoner | A3 | M | UDP/137. Mismo patrón que LLMNR. |
| F2.4 | mDNS spoof / poison | A3 | S | UDP/5353. Responder a `.local`. |
| F2.5 | WPAD spoofing | A3 | M | Responder a `wpad.<domain>` + serve PAC. |
| F2.6 | DNS spoof on-wire | A3 | M | Race a forged reply. Útil cuando ya somos gateway. |
| F2.7 | IPv6 RA spoofing / SLAAC takeover | A3 | M | mitm6-style. Killer feature en redes dual-stack. |
| F2.8 | NDP spoofing | A3 | M | v6 ARP-spoof equivalent. |
| F2.9 | DHCPv6 rogue server | A3 | M | Pair con F2.7. |
| F2.10 | ICMP redirect / ICMPv6 redirect | A3 | S | Install Flipper as next-hop. |
| F2.11 | DTP trunk negotiation + 802.1Q hop | A3 | M | Voice VLAN escape one-button. |
| F2.12 | LLDP impersonation (VoIP phone) | A3 | M | LLDP-MED voice VLAN request. |
| F2.13 | CDP impersonation (Cisco IP phone) | A3 | M | Mismo objetivo voice-VLAN. |
| F2.14 | 802.1Q double-tag injection | A3 | S | Probe one-way en VLAN target. |
| F2.15 | VLAN ID brute (1-4094) | A2 | M | DHCP/ARP probe per tag. |
| F2.16 | TCP RST injection | A3 | M | Kick SSH/RDP sessions activos. |
| F2.17 | MAC spoof / clone (NAC bypass) | A3 | S | Setear MAC aprendida via LLDP/EAPOL recon. |
| F2.18 | Port stealing | A3 | M | Lower-noise alternativa a ARP poison. |
| F2.19 | HSRP / VRRP / GLBP hijack | A3 | M | Becoming the gateway directamente. |
| F2.20 | Captive portal forced | A3 | M | Post-rogue-DHCP, serve fake login. |
| F2.21 | TFTP rogue server | A3 | M | Push poisoned configs a switches/phones. |
| F2.22 | SNMP trap injection | A3 | S | Confunde NMS / cubre otras actividades. |

**Criterio Fase 2:** kill-chain completa "plug → recon → become gateway → capture creds" demonstrable en lab.

---

## 6. Fase 3 — Stress (Capa C)

Solo en `ethernet-pentest.fap`. Visible solo si existe `/ext/apps_data/ethernet/AUTHORIZED.txt`. Aggr 4.

| ID | Feature | Aggr | Esfuerzo | Notas |
|----|---------|------|----------|-------|
| F3.1 | MAC flood (CAM overflow) | A4 | S | Macof. Per-packet MAADR rotation, hardware-unlocked. |
| F3.2 | Gratuitous ARP storm | A4 | S | Extensión del ARP spoof existente. |
| F3.3 | DHCP starvation | A4 | S | Pair con F2.1. |
| F3.4 | DHCPv6 starvation | A4 | S | Pair con F2.9. |
| F3.5 | RA flood / RS flood (SLAAC overflow) | A4 | S | Tabla flip clásico v6. |
| F3.6 | SYN flood | A4 | S | Authorized stress test. |
| F3.7 | BPDU flood / RPVST manipulation | A4 | M | STP storms. |
| F3.8 | STP root-bridge hijack | A3-A4 | M | Superior BPDU. Aggr depende de impacto. |
| F3.9 | CDP/LLDP flood | A4 | S | Crash old IOS. |
| F3.10 | 802.3x PAUSE-frame DoS | A4 | S | Hardware-unlocked, simple, brutal. |
| F3.11 | Bad-CRC injection mode | A4 | S | Hardware-unlocked vía PKTCTRL.PCRCEN per-packet. |
| F3.12 | ARP cache flush DoS targeted | A4 | S | Blackhole un host. |

**Criterio Fase 3:** Cada attack pide confirmación "Yes / Cancel" antes de disparar; sin `AUTHORIZED.txt` ninguno de los items de menú aparece.

---

## 7. Track paralelo — OT/ICS (diferenciador clave)

Independiente de la numeración Fase 1/2/3, este track tiene su propia cadencia y empuja el posicionamiento "Electronic Cats: Modbus serial + ENC28J60 industrial TCP".

Recon (entra en `ethernet-admin.fap`):
- **OT.1 — Modbus-TCP probe** (port 502): function 17 "Report Slave ID" → vendor info.
- **OT.2 — Siemens S7 probe** (port 102): COTP + S7-comm SSL setup → CPU type/firmware.
- **OT.3 — BACnet/IP probe** (port 47808 UDP): Who-Is / I-Am → device list, vendor.
- **OT.4 — DNP3 probe** (port 20000): Read Internal Indications → vendor/version.
- **OT.5 — EtherNet/IP probe** (port 44818): List Identity → CIP device discovery.
- **OT.6 — MQTT probe** (port 1883): CONNECT broker, list topics si SUBSCRIBE permitido.
- **OT.7 — CoAP probe** (port 5683 UDP): GET /.well-known/core → resource list.

Read interactions (entra en `ethernet-pentest.fap`, capa B):
- **OT.8 — Modbus-TCP read holding/input registers** sin escritura.
- **OT.9 — S7-comm read DB / Merker bytes** read-only.

Write/control (capa C, requiere AUTHORIZED.txt):
- **OT.10 — Modbus-TCP write coil/register** con confirmación per-action.
- **OT.11 — S7-comm write** con doble confirmación.

**Criterio OT:** una nueva sección en el menú principal "Industrial / OT" que ramifica en los protocolos. Mismo patrón Submenu que el resto.

---

## 8. Drop-box mode + BLE telemetry (capability transversal)

No es un feature único sino una modalidad que envuelve features de las fases anteriores.

- **D.1 — Modo dropbox**: boot directo a perfil configurable (sin pasar por menús), screen apagada después de N segundos, log a SD.
- **D.2 — Profile script**: archivo `.script` en SD que define la secuencia (ej: ARP scan 30s → port scan top-100 → PCAP 5min → SNMP brute → exfil). Estilo declarativo simple.
- **D.3 — BLE GATT findings service**: streaming de hallazgos a un Flipper companion app o a un teléfono. Operador retrieves resultados sin acercarse físicamente.
- **D.4 — Sub-GHz remote arm/disarm**: handshake corto via SubGHz para activar la fase ruidosa solo cuando el operador da OK.
- **D.5 — BadUSB exfil pivot**: al reconectar el Flipper a USB, type-out automatic de findings clave (creds, hosts críticos) a la pantalla del PC víctima — para escenarios donde el operador recoge el dispositivo después del engagement.
- **D.6 — Scheduled attack windows**: cron-like, ejecuta items ruidosos solo en ventana horaria definida.

**Criterio drop-box:** un Flipper con perfil `recon-only.script` + `AUTHORIZED.txt` ausente puede sentarse 24h en una LAN, generar inventario completo, y ser leído via BLE sin tocar la pantalla.

---

## 9. Backlog futuro (post-roadmap, no priorizado)

Bloqueado por hardware (necesita 2da NIC):
- 802.1X transparent bridge bypass (Marvin Smith / silentbridge).
- Inline transparent MITM con forwarding.
- Cable wiremap (T568A/B / cross / fault test).

Bloqueado por RAM/CPU:
- HTTP MITM / SSL strip completo con proxy en RAM.
- TLS handshake fingerprint (JA3/JA3S) calculado on-device.
- TCP sequence prediction / hijack de sesiones establecidas (mostly demo value).

Investigación pendiente:
- DMA-checksum offload del ENC28J60 (errata-bloqueado en half-duplex; viable solo si forced full-duplex).
- LED-pattern covert channel via PHLCON.

---

## 10. Métricas de éxito y release cadence

**Métricas product:**
- Estrellas GitHub: 8 → 200+ en 12 meses.
- Forks > 5 (señal de comunidad activa).
- Discussions con ≥20 hilos en categorías Feature requests + Show & tell.
- Inclusión en lab.flipper.net / awesome-flipperzero networking section.

**Métricas técnicas:**
- Throughput captura: 3-4 Mbps → ≥6 Mbps tras Fase 0.
- Heap libre durante capture: documentado, ≥80 KB libres.
- Tiempo build CI: dos artefactos < 5 min.
- Cobertura módulos sin TODOs: 100% al final de F0.

**Cadencia de release:**
- v1.x: Fase 0 (multiple minor releases, una por commit anotado).
- v2.0: Fase 1 completa — `ethernet-admin.fap` listo para distribución pública.
- v2.x: features Fase 1 incrementales + OT recon.
- v3.0: Fase 2 completa — `ethernet-pentest.fap` distribuible.
- v3.x: OT read/write.
- v4.0: Fase 3 + Drop-box mode completo.

**Reglas de release:**
- Cada `.fap` se adjunta al GitHub Release.
- Cambio mayor entre admin.fap y pentest.fap: ambos en cada release, mismo tag.
- CHANGELOG.md por versión, separando entries por artefacto.

---

## 11. Decisiones de diseño congeladas (no re-litigar)

1. **Dos artefactos `.fap`** producidos por CI desde el mismo árbol con `PENTEST_MODE` flag.
2. **Capa D (destructivo)** queda compile-time `DEV_MODE`, nunca en releases públicos.
3. **Capa C (DoS/flood)** dentro de `ethernet-pentest.fap` requiere `AUTHORIZED.txt` runtime.
4. **OT/ICS está in-scope** como track paralelo; recon va al admin, read/write al pentest.
5. **Fase 0 es prerequisito ABSOLUTO** de cualquier Fase 1+. No empezamos features nuevos antes.
6. **Patrón de scenes**: continuamos con Submenu (no VariableItemList), siguiendo refactors recientes.
7. **Idioma de comentarios**: normalizar a inglés. Spanish solo en docs/ user-facing.
8. **Persistencia**: `flipper_format` sobre `/ext/apps_data/ethernet/settings.cfg`. No `.json`, no SQLite.
9. **PCAP path**: `/ext/apps_data/ethernet/files/` (existente). No cambiar.
10. **Naming convention**: `ethernet-admin.fap` y `ethernet-pentest.fap`, no abreviaciones.

---

## 12. Riesgos identificados

### R1 — Heap budget en Fase 2 (medio)
Responder + rogue DHCP + DHCPv6 corriendo simultáneamente puede acercarse al techo de heap (~200 KB). Mitigación: medir heap libre como métrica continua, gate features simultáneas con un "active modules" counter, deshabilitar paths con allocations grandes (analysis_module 1500-byte stack) cuando hay módulos pesados activos.

### R2 — Errata #18 RX wraparound bajo carga sostenida (medio)
ENC28J60 tiene varios issues conocidos bajo sniff line-rate. Mitigación: el layout RX 6 KB / TX 2 KB y los workarounds de errata ya están aplicados en el driver; añadir test de stress automatizado en CI.

### R3 — Perfil legal del `pentest.fap` distribuible (alto)
Distribuir un binario que contiene LLMNR poisoner / rogue DHCP puede exponer a Electronic Cats. Mitigación: README del release con texto legal explícito, separación clara admin vs pentest, registro en lab.flipper.net solo del `admin.fap`. Considerar requerir aceptación de un EULA antes del download del pentest desde GitHub Release (action customizado).

### R4 — Test hardware setup limitado (medio)
Algunos features requieren switches reales con CDP/LLDP/STP/802.1X. Mitigación: documentar lab requirements en `docs/LAB_SETUP.md`, proveer Dockerfile con scapy emulando al peer en CI para los items pure-software.

### R5 — Drift con upstream firmware Flipper (bajo)
Flipper SDK rompe APIs ocasionalmente. Mitigación: pinear `targetSdkVersion` en `application.fam`, CI con matriz de SDK versions, dependabot para watch.

### R6 — Fragmentación de la audiencia entre 2 artefactos (bajo)
Un usuario puede confundirse sobre cuál bajar. Mitigación: README con tabla clara, lab.flipper.net solo lista admin, releases con descripción explícita.

---

## 13. Documentación mínima a producir

A lo largo de las fases:
- `docs/ARCHITECTURE.md` — F0 (capas, scenes, dispatch).
- `docs/SCANNER_SESSION.md` — F0.3 (cómo escribir un nuevo scanner).
- `docs/RX_DISPATCH.md` — F0.4 (handler registry).
- `docs/PROTOCOLS.md` — F1 (cada protocolo soportado, pattern-match config).
- `docs/OT_PROTOCOLS.md` — track OT (Modbus-TCP / S7 / BACnet / DNP3 / EtherNet/IP).
- `docs/DROPBOX.md` — D track (script format, BLE GATT service spec).
- `docs/SECURITY.md` — capas A/B/C/D, gating, AUTHORIZED.txt format, EULA.
- `docs/LAB_SETUP.md` — switches/networks recomendados para test.
- `docs/PCAP_FORMAT.md` — F0.7 + F1.22 (timestamps, BPF-lite filter spec).
- `CHANGELOG.md` — desde v1.x.
- `docs/ROADMAP.md` — versión user-facing de este documento, simplificada.

---

**Fin del roadmap.** Siguiente paso: `ENC28J60_REFACTOR_PLAN.md` en la raíz del repo, formato `PLAN_MAESTRO_TEMPLATE.md`, con Fase 0 detallada como F0.1..F0.9, las Fases 1/2/3 abreviadas, y el prompt inicial para Claude Code.
