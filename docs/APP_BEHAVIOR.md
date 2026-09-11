# FLIPPER ETHERNET — Current Behavioral Specification

> **Purpose:** Define the current expected user-visible behavior of the
> FLIPPER ETHERNET FAP on physical Flipper Zero hardware, including navigation,
> controls, feature flows, persistence expectations, known behavioral
> deviations, and explicitly deferred improvement areas.
>
> **Scope:** This document describes application behavior from the user's
> perspective. It defines what the application is expected to do and records
> known deviations from that behavior.
>
> **Authority boundary:** This document is authoritative for intended
> user-visible behavior, but not for internal implementation details.
> Source code remains the ground truth for how the behavior is currently
> implemented. Any statement about internal architecture, ownership,
> concurrency, memory, hardware state, or protocol processing must be verified
> against the current source.

## 1. Application Purpose

**FLIPPER ETHERNET** is a Flipper Zero application (FAP) intended for network auditing. It provides both administrative/network-discovery functionality and capabilities intended for vulnerability assessment and pentesting.

The application runs on the Flipper Zero and works together with the **Flipper Add-On Ethernet**.

## 2. Hardware Context

The Flipper Add-On Ethernet is based on the **ENC28J60** Ethernet controller.

For detailed information about the hardware design, the SPI interface, and hardware compatibility, consult:

- `docs/HARDWARE.md`
- `.github/ISSUE_TEMPLATE/hardware_compat.md`

At a high level, the Add-On consists of the ENC28J60 together with the passive/supporting components required for the Ethernet physical interface, including resistors, capacitors, magnetics, and a crystal. These components support the connection between the controller, the RJ45 Ethernet interface, and the Flipper Zero SPI interface.

This document intentionally does not duplicate the hardware architecture described in the files above.

## 3. General Operational Baseline

When the application is used normally by itself, it is currently considered functionally stable.

The user can repeatedly execute the available features without normally encountering crashes or memory failures. This has also been observed across:

- first launch after installing/transferring the FAP to the Flipper Zero;
- repeated feature execution;
- leaving and reopening the application;
- rebooting the Flipper Zero and opening the application again.

### 3.1 Deferred qFlipper / CLI resource-coexistence concern

The above baseline applies when the application is operating by itself.

When qFlipper is active in the background, or when the CLI is open, reduced memory availability has been observed. The issue is more noticeable with qFlipper and less severe with CLI use.

This document records this only as a resource/memory-coexistence concern.
Detailed diagnosis is intentionally deferred to the memory/resource audit.

The exact failure patterns are intentionally not specified here. They must be investigated later during the comprehensive memory/resource audit without assuming in advance that any specific feature is defective.

## 4. Global UI and Navigation Conventions

The application appears on the Flipper Zero as **Ethernet App**, using an Electronic Cats cat logo.

The Flipper Zero center button is referred to throughout this document as **OK**.

Depending on the scene, OK can be used to:

- enter a menu or feature;
- confirm a value;
- start an operation;
- stop an operation;
- select an item.

The other physical buttons are referred to as:

- **BACK**
- **LEFT**
- **RIGHT**
- **UP**
- **DOWN**

## 5. Application Startup

The application is opened by pressing OK on its launcher entry.

The startup sequence is:

1. A brief loading scene appears with a rotating hourglass animation.
   - The same loading animation is reused by some operations later in the application.
2. A brief splash/start screen appears containing:
   - the Electronic Cats cat icon;
   - `ETHERNET APP`;
   - `Electronic Cats`.
3. The main application menu appears.

## 6. Main Menu

The main menu uses the static header:

`ETHERNET APP`

The main sections are:

- Administration
- Pentesting
- Settings
- About Us

---

# 7. Administration

Selecting **Administration** with OK opens the network-administration features.

These functions are intended to:

- identify and understand the current network;
- discover devices;
- communicate with devices;
- obtain information about devices.

The Administration menu uses the static header:

`ETHERNET ADMIN`

The header remains visually fixed at the top while the user navigates through the feature list.

## 7.1 Get IP

### Purpose

Automatically obtain an IP configuration through DHCP.

### Entry behavior

When entering Get IP, the displayed scene depends on the detected physical/network state.

### Add-On not connected

A custom screen appears with:

`DEVICE NOT CONNECTED`

### Known deviation — Add-On state is not always refreshed correctly

The custom Device Not Connected screen is reliably shown when the application itself was opened while the Add-On was already disconnected.

However, if the Add-On state changes while the application remains open, the feature does not always reevaluate the current physical state correctly.

For example, once the Add-On is connected while the application remains open, the feature may move to the network-related state even if there is no Ethernet network.

**Desired behavior:** Every time Get IP is entered, the application should evaluate the current Add-On connection state. “Add-On not connected” and “Ethernet network not detected” must remain distinct states.

### Add-On connected but no Ethernet network

When the Add-On is present but no Ethernet network is available, the scene displays:

`IP didn't got it`

Below it:

`IP by default`

Below that, the application displays the last IP address previously configured either:

- automatically by Get IP in an earlier session; or
- manually in Settings.

### Ethernet network available

When a network is available, Get IP obtains the network configuration through DHCP.

In the current intended workflow, Get IP is treated as the first step before executing the other network-dependent application functions.

BACK returns from Get IP to the `ETHERNET ADMIN` menu.

---

## 7.2 Scan Hosts

> The feature is referred to in the code as the **ARP Scanner**.

### Purpose

Discover devices by IPv4 address on the same network.

### Main Scan Hosts menu

The main configuration includes the following entries.

### Start IP `[X.X.X.X]`

This item defines the IPv4 address from which the host scan starts.

Entering the item opens an IPv4 digit editor.

The IPv4 address is configured as 12 decimal digits.

Controls:

- LEFT/RIGHT: move between digits.
- UP/DOWN: change the selected digit.
- OK: confirm the edited IP.

After the user returns to the previous menu, the value displayed inside the brackets is updated.

### Known deviation — BACK incorrectly commits Start IP

The current editor also commits the modified Start IP when BACK is pressed.

**Desired behavior:** BACK should cancel the edit and return without modifying the stored Start IP. Only OK should commit the new address.

### Range `[0]`

Entering Range opens a decimal keypad.

Controls:

- directional buttons navigate the keypad;
- OK enters the selected digit/action;
- the keypad includes a one-character delete action;
- the keypad includes a confirmation/Enter action.

After confirming, the displayed Range value is updated when the user returns to the previous menu.

If BACK is pressed instead of confirming the value through the keypad, the Range is not modified.

**This is considered the correct confirmation/cancellation behavior.**

### Start Scanning

Pressing OK starts the host scan.

The observable behavior depends on the current physical/network state.

#### Add-On not connected

The same current Add-On-state limitation described under Get IP applies and should be corrected later.

#### No Ethernet network

A custom screen appears with:

`Network Not Detected`

The screen also shows an RJ45 plug graphic marked with an X.

BACK exits this screen. The user can restore the network connection and try again.

#### Network available

The rotating-hourglass loading animation appears.

The scan can be interrupted by pressing BACK.

When BACK is pressed during scanning:

- the active process is interrupted;
- the user returns to the previous menu;
- starting the scan again requires starting a new scan session.

The duration of the loading/scanning scene depends on:

- the selected range;
- the hosts available on the network.

When the scan completes, the Scanned Hosts menu appears.

### Scanned Hosts menu

Static header:

`SCANNED HOSTS (X)`

where `X` is the number of discovered devices.

The detected IPv4 addresses are listed below the header.

Selecting a host with OK opens a detail view with the header:

`IP ADDRESS DETAILS`

The detail screen shows:

- `IP: X.X.X.X`
- the device MAC address;
- `DATE: xx|xx|xx-xx:xx`
- an OK action labeled `Select IP`

The displayed date/time corresponds to when that host was scanned.

Pressing OK on `Select IP` selects that IPv4 address for use by later target-based features.

Pressing BACK returns without selecting the IP.

A subsequent BACK returns toward the main Scan Hosts menu.

### Hosts `[xx|xx|xx-xx:xx]`

This entry provides direct access to the last `SCANNED HOSTS` results without starting a new scan.

The date/time shown in the menu corresponds to the latest host scan.

### Persistence

Scanned hosts remain available after:

- leaving the application;
- reopening the application;
- rebooting the Flipper Zero.

The data is stored under:

`apps_data/ethernet/last_scan.bin`

The application creates the `apps_data/ethernet` directory on the SD card as needed.

From the user-visible workflow, the stored host list changes when a new host scan is performed. A new scan replaces the previously stored last-scan results.

---

## 7.3 Passive Discovery

### Purpose

Discover devices passively by listening for network discovery/authentication traffic.

The currently implemented protocols/modes are:

- LLDP
- CDP
- EAPOL
- Discover All

`Discover All` listens for devices transmitting LLDP, CDP, or EAPOL frames.

### Main Passive Discovery scene

The scene uses the bold header:

`Passive Discovery`

Below it:

`Protocol`

Below that, the currently selected discovery mode is displayed.

RIGHT is labeled `Other` and cycles through:

- Discover All
- LLDP
- CDP
- EAPOL

### Starting discovery

Pressing OK starts discovery using the currently selected mode.

The scene updates:

- `Protocol` changes to `Listening...`
- the selected protocol/mode text changes to `Neighbors: X`

`X` is the number of neighbors detected during the current discovery session.

### Controls while listening

BACK:

- stops the discovery process;
- returns to the previous Passive Discovery scene.

OK:

- opens the list of neighbors detected during the current session.

### No neighbors detected

If no neighbors were detected, the result scene indicates that no neighbors were obtained.

BACK returns to the Passive Discovery main scene, from which another session can be started.

The results menu also includes:

`[Clear Results]`

Selecting this item with OK clears the relevant neighbor results.

### Current neighbor list

When neighbors exist, the application displays a menu with the header:

`SAVED NEIGHBORS`

The detected devices are listed underneath.

For `Discover All`, each entry contains:

- the identifier selected by the application from the available protocol fields, for example:
  - Device ID
  - System Name
  - Identity
- a protocol label in brackets identifying whether the entry came from LLDP, CDP, or EAPOL.

When a specific protocol is selected, the protocol label is not shown per entry because every listed neighbor belongs to the same protocol.

Pressing OK on a selected neighbor opens that neighbor's detailed information.

### Saved Neighbor History

From the Passive Discovery main scene, LEFT opens the saved-neighbor history accumulated across discovery sessions.

The saved list remains available until the user clears it.

The selected mode determines which history is opened:

- LLDP shows LLDP neighbors;
- CDP shows CDP neighbors;
- EAPOL shows EAPOL neighbors;
- Discover All shows the combined history.

Clearing a protocol-specific list affects the Discover All list by removing those protocol entries from the combined history.

Likewise, clearing entries through the combined history affects the underlying saved results.

BACK returns from saved history to the Passive Discovery main scene.

### Desired UI improvement — discovery mode label

During active discovery, the scene currently uses the generic `Listening...` label.

A future UI improvement should make the active protocol/mode more explicit while discovery is running.

### Desired UI improvement — saved-list header

All saved-neighbor views currently use the generic header:

`SAVED NEIGHBORS`

For protocol-specific views, the header should ideally identify which protocol's saved neighbors are being displayed.

### Neighbor detail navigation

Different protocols expose different detail fields.

Controls:

- LEFT/RIGHT: move between the protocol detail fields/pages.
- BACK: return to the previous neighbor list.

### LLDP fields

The current physical/user-level description identifies the following LLDP fields:

- SYSTEM NAME
- SOURCE MAC
- MANAGEMENT IPV4
- PORT MAC
- CHASSIS MAC
- TTL
- CAPABILITIES
- SYSTEM DESCRIPTION
- PORT VLAN ID
- NAMED VLAN ID
- VLAN NAME
- NETWORK POLICY VLAN
- POE/DEVICE
- POWER PAIR/CLASS
- MED POWER
- TYPE/SOURCE/PRIORITY

> The original notes listed `SYSTEM DESCRIPTION` twice. This draft treats that as a likely duplicate in the notes and lists it once. The exact implemented field set must later be verified against the current source and physical UI.

### CDP fields

- DEVICE ID
- SOURCE MAC
- PORT ID
- TTL
- CAPABILITIES
- PLATFORM
- SOFTWARE VERSION

### EAPOL fields

- IDENTITY
- SOURCE MAC
- PACKET TYPE
- EAPOL VERSION
- EAP CODE
- EAP TYPE

### Known presentation issue — long fields

Fields such as:

- CAPABILITIES
- SYSTEM DESCRIPTION
- SOFTWARE VERSION

can occupy several partially filled pages because of how the current dynamic layout assigns display space.

This can make the UI visually inefficient and navigation unnecessarily tedious.

**Desired future improvement:** Improve field/page layout so the Flipper Zero screen space is used more efficiently without losing information.

### Known deviation — Add-On/network error presentation

When discovery starts without the Add-On connected, the application currently shows the listening scene with text such as:

`Device not connected`

instead of reusing the existing custom Device Not Connected screen.

Likewise, when no Ethernet network is available, the same general scene is reused with:

`Network not detected`

instead of the existing custom Network Not Detected screen.

**Desired behavior:** Use the established custom connection-state screens consistently.

### Persistence

Passive Discovery neighbors are stored under:

`apps_data/ethernet/passive_discovery.bin`

They remain available after:

- leaving the application;
- reopening the application;
- rebooting the Flipper Zero.

The saved data changes when:

- new neighbors are added; or
- saved results are cleared from the application.

---

## 7.4 Ping Host

> The original notes contained the truncated heading `ing Host`; the described behavior clearly corresponds to **Ping Host**.

### Purpose

Test whether the Flipper Zero can communicate, through the Ethernet Add-On, with a target device that was previously discovered or manually configured.

The feature sends packets and waits for responses from the target.

### Hosts shortcut

The feature exposes the same saved-host menu described under Scan Hosts.

This allows the user to quickly select one of the previously scanned IPv4 devices.

The same saved-host menu is reused by other features that require an IPv4 target.

This shared menu refers to the Scan Hosts results only; it does not use Passive Discovery neighbors.

### Target IP

`Target IP` uses the same basic IPv4 editor behavior as `Start IP`, but edits the target address.

The selected Target IP is shared with other target-based functions.

It can be set either:

- manually; or
- by selecting an IPv4 address from the saved Hosts list.

### Known deviation — BACK incorrectly commits Target IP

The current Target IP editor also preserves modifications when BACK is pressed.

**Desired behavior:** BACK should cancel the edit. Only OK should commit the modified Target IP.

### Start Ping

Pressing OK starts the ping process.

The active scene contains the bold header:

`Ping to`

Below it, the selected Target IP is displayed.

Below that:

`Responses X of X`

The first value represents responses received.

The second value represents packets sent by the Flipper Zero/Add-On.

Expected visible behavior:

- when communication is successful, both values should generally increase together;
- with packet loss, the response count may remain behind the sent count;
- when there is no response, the first value can remain at `0` while the second continues increasing.

BACK exits the ping operation.

### Known deviation — Add-On/network state

The same stale connection-state behavior described for other network-dependent features can occur here.

The dedicated Add-On-disconnected state is most reliably shown when the application was initially opened without the Add-On already connected.

The behavior should later be corrected so current physical/network state is evaluated when the operation is started.

---

## 7.5 Scan Ports

### Purpose

Scan TCP ports on a target device.

Additional protocols may be considered in the future, but the currently exposed scanning behavior is TCP-oriented.

### Main scene

Static bold header:

`SCAN PORTS`

The menu includes:

### Hosts `[xx|xx|xx-xx:xx]`

Provides access to the most recent Scan Hosts results.

### Target IP `[X.X.X.X]`

Displays the target IP shared with other target-based functions.

### Known deviation — BACK on Target IP

The same Target IP editor problem applies here: BACK can commit changes even though only explicit confirmation through OK should save the edited address.

### Start Port

Opens a decimal keypad similar to the Range editor from Scan Hosts.

The entered value defines the first port to scan.

### Range

Uses the same kind of decimal keypad.

The Range defines how many ports beyond Start Port are scanned.

Example:

- Start Port: `22`
- Range: `1000`

The scan covers ports beginning at 22 and extending through the configured range.

### Protocol

A field exists for selecting which port-scanning protocol is used.

At present, it is effectively disabled/static because only the currently supported protocol behavior is exposed.

### Start Scanning

Pressing OK starts the port scan.

The active scene uses the bold static header:

`PORTS OPEN`

Below it, a value is continuously updated to show the port numbers being processed.

When an open port is found, it is added to the displayed results while scanning continues.

Once the configured range is complete, the UI shows:

`FINISH`

BACK then returns to the previous menu.

The scan can also be interrupted before normal completion.

### Current limitation — scan duration

The feature is functional, but large scans are slow.

A scan of approximately 60,000 ports can take close to one hour.

### Desired future improvements

After the current architecture is fully audited, the feature should be evaluated for possible optimization/refactoring.

Potential future improvements include:

- reducing scan duration where technically possible;
- reporting additional relevant port states rather than only open ports, for example:
  - FILTERED
  - CLOSED
  - other meaningful states

Whether this is feasible should be decided only after the implementation and architecture are understood.

### Known deviation — Add-On/network state

The same inconsistent Add-On/network error-state behavior described for Scan Hosts and Ping Host applies here.

---

## 7.6 OS Detector

### Purpose

Estimate the target device operating system using a heuristic based on network responses to probes sent by the Flipper Zero/Add-On.

The described current behavior involves SYN/ACK-related communication, target port behavior, and heuristic classification.

The exact protocol implementation must later be verified against source.

### Main options

### Hosts `[xx|xx|xx-xx:xx]`

Provides access to the most recent Scan Hosts results.

### Target IP `[X.X.X.X]`

Displays the shared Target IP used by target-based functions.

### Known deviation — BACK on Target IP

The same Target IP editor issue applies: BACK currently preserves edits that should only be committed through OK.

### Start Detection

Starts an OS detection session.

The rotating-hourglass loading scene appears.

BACK can cancel the operation and return to the previous screen.

### Detection results

After completion, a result screen appears.

UP/DOWN are used to navigate through the information.

The displayed information includes:

#### `OS DETECTION RESULTS`

The heading currently behaves as a normal scrollable row rather than remaining fixed, so it can disappear from the screen while navigating.

#### Target IP

`Target IP: X.X.X.X`

#### Detection status

Depending on the result, the status can include:

- `OS not detected`
- `OS DETECTED`
- `OS GUESSED`

The current interpretation described by the application behavior is:

- `OS DETECTED`: approximately 90–100% confidence;
- `OS GUESSED`: lower confidence.

Currently classified OS families include:

- Windows
- Linux
- iOS/macOS
- not detected

#### Experimental warning

The screen contains an informational warning similar to:

`*Experimental Feature* (Heuristic may be wrong)`

#### Initial Source Port

Displays a source port that changes for each new detection session.

The current behavioral description treats it as a randomly assigned initial source port used by the Add-On/application for communication.

The exact source-port generation/ownership must later be verified against source.

#### Scanned Ports

The feature reports a predefined set of ports used as part of the heuristic.

The currently described ports are:

- 22
- 80
- 135
- 139
- 443
- 445
- 3389
- 5000
- 7000
- 49152
- 62078

Each port is listed with one of the following states:

- OPEN
- CLOSED
- FILTERED
- UNKNOWN

### Known deviation — Add-On/network state

The same Add-On/network-state presentation issue described for previous network-dependent features can occur here.

---

# 8. Pentesting

The **Pentesting** section contains features that actively interact with or intervene in the Ethernet network, including spoofing and sniffing.

The menu uses the static header:

`ETHERNET PENTESTING`

## 8.1 ARP Actions

ARP Actions groups ARP-related operations.

At present, the active user-visible capability described here is ARP spoofing.

The menu includes:

- Hosts `[xx|xx|xx-xx:xx]`
- ARP Spoofing All

### ARP Spoofing Specific IP — currently disabled

`ARP Spoofing Specific IP` existed previously but is currently disabled because it had problems and has fallen behind several later refactors.

**Deferred work:** Revisit and refactor this feature so it matches the current application architecture before re-enabling it.

## 8.1.1 ARP Spoofing All

### Entry

Upon entering, a brief scene displays:

`Your IP is:`

followed on the next line by:

`X.X.X.X`

Then a custom image appears showing a sleeping cat.

A label indicates that the center button starts the attack:

`Attack`

### Running state

Pressing OK starts ARP spoofing.

The UI shows an animation consisting of approximately four or five images of the cat hitting a router.

While this animation is active, the spoofing process is running.

The center button is labeled:

`STOP`

Pressing OK stops the spoofing process.

### Known deviation — BACK currently stops the active process

BACK can currently also stop the spoofing process.

**Desired behavior:** The active attack should be stopped through the explicit center-button STOP action. BACK should not implicitly act as the stop control while spoofing is active.

### After stopping

The application returns to the sleeping-cat scene.

BACK from the sleeping-cat scene returns to the ARP Actions menu.

---

## 8.2 Packet Sniffer

### Purpose

Capture Ethernet traffic and store the captured packets in PCAP files.

### Entry behavior

Entering Packet Sniffer starts sniffing automatically.

The active scene displays:

`Packets Received`

Below it, a number updates nearly in real time as packets are received.

The screen also exposes an OK action for:

`Open`

### Leaving/opening captures

While sniffing, the user can:

- press BACK to leave/cancel the active sniffing session; or
- press OK/Open to access the captured PCAP data directly.

Captured PCAP files are stored under:

`apps_data/ethernet/files`

Captured files can later be accessed either:

- directly through Open during/after the sniffing workflow; or
- from `View Packets` in the Pentesting menu.

### PCAP file naming

An example filename is:

`pcap_09_09_2026_1`

The naming reflects the capture date and a capture/session number.

### PCAP packet viewer

A single PCAP file can contain multiple captured packets.

An example packet display is:

```text
<== Packet 1 of 10 ==>
--- Packet Information ---
Ether Type: UDP

Ethernet Header:
 Source MAC: XX:XX:XX:XX:XX:XX
 Destination MAC: XX:XX:XX:XX:XX:XX

IP Header:
 Source IP: X.X.X.X
 Destination IP: X.X.X.X
 Protocol: UDP

Transport Layer Information
(UDP):
 Source Port: X
 Destination Port: X

Payload Size: X Bytes
------------------
```

The original notes showed some MAC-address line wrapping caused by the small display; the semantic fields above preserve the intended information.

### Packet viewer controls

- UP/DOWN: scroll through the currently displayed packet information.
- LEFT/RIGHT: move between packets in the selected PCAP file.

## 8.3 View Packets

`View Packets` exposes the files stored under:

`apps_data/ethernet/files`

The user can browse capture files associated with previous sniffing sessions.

Selecting a file with OK opens the same packet-viewing interface used when captures are opened directly from the Sniffer workflow.

### Known deviation — Add-On/network state is stale in Sniffer

If the application is initially opened without the Add-On connected, attempting to start Sniffer shows the Add-On-not-connected state.

However, connecting the Add-On/network while the application remains open does not necessarily refresh Sniffer's state.

In the described current behavior, running another feature with the Add-On/network correctly connected can indirectly cause the state to become usable, after which Sniffing can be started.

### Implementation assumption requiring verification

The current user-level hypothesis is that network-dependent features do not consistently refresh Add-On/network state immediately before starting.

This is **not** treated as a confirmed implementation fact in Phase 0.

It must later be verified against source.

---

# 9. Settings

Settings allows manual configuration of the IP and MAC address associated with the Flipper Zero/application.

The menu uses the static header:

`SETTINGS`

## 9.1 IP `[X.X.X.X]`

Displays the IP currently assigned/configured for the Flipper.

The value may originate from:

- a previous Get IP/DHCP operation; or
- manual configuration.

Selecting IP opens a submenu.

### `SET IP ADRESS`

> This preserves the UI text spelling as described in the original notes. The actual implementation string should later be verified.

The submenu includes:

### Get IP

Provides access to the same Get IP functionality exposed under Administration.

This is similar to how the shared Hosts results can be accessed from multiple features.

### Set IP manually

Opens the IPv4 editor described earlier.

In this context, the user is configuring the Flipper/application IP directly.

The editor header changes to something equivalent to:

`Set IP to Flipper`

The center-button action is labeled:

`Set`

Pressing OK/Set commits the address.

### Known deviation — BACK incorrectly commits manually configured IP

BACK currently also preserves the modified IP.

**Desired behavior:** BACK should cancel and return to the previous scene without storing the modified address. Only OK/Set should commit it.

---

## 9.2 MAC `[XX:XX:XX:XX:XX:XX]`

Displays the currently configured MAC address.

Selecting MAC opens:

`SET MAC ADDRESS`

The available actions are:

### Set Random MAC

Pressing OK generates a random MAC address.

The operation is effectively immediate.

The application returns to the previous screen with the updated value displayed in:

`MAC [XX:XX:XX:XX:XX:XX]`

### Set MAC manually

Opens a hexadecimal keypad.

The selector begins at the first MAC-address character.

When a hexadecimal character is entered:

- it replaces the currently selected character;
- the selection advances to the next character.

The keypad exposes a SAVE/confirmation action.

### Known deviation — BACK incorrectly commits manually edited MAC

BACK currently preserves the modified MAC as though SAVE had been selected.

**Desired behavior:** BACK should cancel the edit and return without storing the modified MAC. Only explicit SAVE/OK confirmation should commit the new value.

### Hardware/network dependency

Manual IP assignment and random/manual MAC assignment are expected to work without requiring the Add-On to be connected.

The Add-On/network-state issue applies to Get IP because that operation depends on the Ethernet interface/network.

---

# 10. About Us

About Us is a brief informational section presenting the application and Electronic Cats.

It includes items such as:

- Electronic Cats/application information;
- official repository QR codes;
- related informational pages.

The scenes progress linearly.

Controls:

- LEFT/RIGHT: move between the About Us scenes/pages.

---

# 11. Cross-Feature Behavioral Patterns

The following patterns are derived from the behavioral description and should be reviewed during the source audit.

They are not claims about current implementation.

## 11.1 Shared saved-host selection

The host list produced by Scan Hosts is reused by target-based features such as:

- Ping Host
- Scan Ports
- OS Detector
- other features that require a target IPv4 address

Passive Discovery uses its own neighbor/history model and should not be conflated with the Scan Hosts saved-host list.

## 11.2 Shared Target IP

Target-based features reuse the same configured Target IP.

The Target IP can be changed:

- manually; or
- by selecting an IP from saved Scan Hosts results.

## 11.3 Confirmation versus cancellation

For editable values, the desired interaction model is:

- explicit OK/Set/Save/Enter action → commit;
- BACK → cancel and return without modifying the stored value.

The current application does not follow this consistently.

Known affected editors include:

- Scan Hosts Start IP
- shared Target IP
- manually configured Settings IP
- manually configured MAC

The Range keypad behavior is the reference example where BACK already cancels correctly.

## 11.4 Connection-state evaluation

Network-dependent features should distinguish at least:

1. Add-On not connected.
2. Add-On connected but Ethernet network not detected.
3. Add-On and Ethernet network available.

The physical state should be evaluated at the relevant feature/operation entry rather than relying on stale state from application startup or another feature.

This is a desired behavioral rule. The implementation must later be audited to determine how connection state is actually tracked.

## 11.5 Long-running operation cancellation

Several long-running operations are intentionally cancelable with BACK, including:

- Scan Hosts
- Ping Host exit
- Scan Ports
- OS Detector
- Passive Discovery

ARP Spoofing All is an explicit exception in the desired UI: while spoofing is actively running, the user intends the explicit STOP/OK control to stop the operation rather than BACK.

The exact lifecycle and resource-cleanup semantics of cancellation must later be verified in source.

---

# 12. Consolidated Known Behavioral Deviations / Improvement Candidates

The following items were explicitly identified in the current physical/user-level description.

They are **not** intended behavior and must not be normalized as part of the specification.

1. **Stale Add-On/network detection**
   - Several features do not consistently reflect physical connection changes made while the application remains open.
   - Current behavior differs depending on whether the FAP was originally launched with or without the Add-On connected.

2. **Incorrect BACK commit behavior in editors**
   - Start IP can be committed with BACK.
   - Target IP can be committed with BACK.
   - Manually configured Flipper IP can be committed with BACK.
   - Manually configured MAC can be committed with BACK.
   - Desired behavior is explicit confirmation only.

3. **Passive Discovery error-screen inconsistency**
   - Uses the listening scene with simple error text instead of the existing custom Add-On/network error screens.

4. **Passive Discovery active-state labeling**
   - `Listening...` does not explicitly identify the currently active protocol/mode.

5. **Passive Discovery saved-list labeling**
   - The generic `SAVED NEIGHBORS` header does not identify the protocol in protocol-specific views.

6. **Passive Discovery long-field pagination**
   - Long fields can result in inefficient partially filled pages and tedious LEFT/RIGHT navigation.

7. **OS Detector results header**
   - `OS DETECTION RESULTS` scrolls like a normal row and can disappear from view rather than acting as a fixed header.

8. **Scan Ports performance**
   - Very large scans can take close to one hour.
   - Architectural feasibility of optimization should be assessed later.

9. **Scan Ports result scope**
   - Current UI emphasizes open ports.
   - Future consideration: expose additional relevant states such as CLOSED and FILTERED if the architecture/probing model supports them.

10. **ARP Spoofing All BACK behavior**
    - BACK can currently stop the active spoofing process.
    - Desired interaction is explicit STOP through the center button while the attack is active.

11. **ARP Spoofing Specific IP**
    - Currently disabled.
    - Needs future architectural refactoring before re-enablement.

---

# 13. Deferred Investigation Areas

The following topics are intentionally deferred from this behavioral specification.

They belong to later source/architecture audit phases.

## 13.1 Memory and external service coexistence

Audit:

- FAP resident memory;
- main stack;
- worker stacks;
- heap usage;
- large contiguous allocations;
- feature high-water marks;
- coexistence with qFlipper/RPC;
- coexistence with CLI-related activity;
- actual available headroom.

No specific feature should be presumed to be the root cause before this audit.

## 13.2 Connection-state architecture

Determine from source:

- how Add-On presence is detected;
- how Ethernet link/network state is detected;
- where those states are stored;
- whether they are cached;
- which features refresh them;
- whether state can become stale across scene transitions;
- what the correct shared abstraction should be, if any.

## 13.3 Editor commit/cancel semantics

Determine why several IPv4/MAC editors currently commit on BACK and whether the behavior comes from:

- scene exit handling;
- shared widget behavior;
- callback flow;
- model mutation before confirmation;
- another shared implementation pattern.

The later fix should address the shared root cause where possible rather than patching each screen independently.

## 13.4 Protocol and parser correctness

Phase 0 documents the fields and behaviors visible to the user.

Later source audit must independently verify:

- LLDP parsing;
- CDP parsing;
- EAPOL parsing;
- protocol-specific field validity;
- frame length/bounds validation;
- data ownership and persistence;
- exact semantics of OS detection probes;
- packet/PCAP parsing and presentation.

## 13.5 Persistence formats

Later audit must verify the implementation details and robustness of:

- `apps_data/ethernet/last_scan.bin`
- `apps_data/ethernet/passive_discovery.bin`
- `apps_data/ethernet/files/*`

including:

- format/version;
- bounds checks;
- short reads;
- corruption handling;
- compatibility across application versions;
- update/replace semantics.

---

# 14. Interpretation Rules for Source and Architecture Audits

Later audit phases should use this document as the canonical **user-level behavioral baseline**.

For every feature, the auditor should distinguish:

1. **Expected behavior** — what the user intends the FAP to do.
2. **Current observed deviation** — physical behavior already known to be undesirable.
3. **Current source implementation** — what the code actually does.
4. **Correctness/architecture finding** — whether the implementation correctly supports the intended behavior and lifecycle.

Existing source code, documentation, plans, skills, or historical debugging conclusions must not silently redefine the expected behavior documented here.

Likewise, this document must not be treated as proof of internal implementation details. Internal behavior must be source-verified in the later audit phases.
