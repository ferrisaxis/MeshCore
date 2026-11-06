# Heltec V4 MeshCore Companion — User Guide

**Firmware:** v1.18.0-0.2.0  
**Build:** `heltec_v4_oled_companion_usb_ble_wifi`  
**Hardware:** Heltec V4 (ESP32-S3, 16MB flash, 2MB PSRAM, OLED SSD1306 128×64)

---

## Quick Start

1. **Power on** — hold USER button (GPIO0) if needed
2. **Pair via BLE** — open MeshCore app, find device `MC-XXXXXX`, enter PIN from screen
3. **Configure WiFi** (optional) — in app: Settings → WiFi, or CLI: `wifi <ssid> <pwd>`
4. **Calibrate battery** — read `get battery`, then use `calibrate battery <measured_mV>` with a multimeter reading, or the on-device 4.20 V option only for a fully charged 1-cell Li-ion/LiPo pack.

---

## Controls (single button, GPIO0)

Navigaion is two-level: **5 root screens** flip with click / double click, and every
settings screen lives one level down in `Menu`. Inside a sub-screen a double click
goes back, a long press performs that screen's action.

| Gesture | Timing | Root screens | Sub-screens |
|---|---|---|---|
| **Click** | — | next root | `Menu` / `Links`: move the `>` marker; otherwise nothing |
| **Double click** | < 280 ms between clicks | previous root | back to the parent screen |
| **Long press** | 1000 ms | `Menu`: open the selected screen, `Links`: open `Interfaces`, `Messages`/home: message preview | screens with several actions (Radio, Power, Beacon) open the **action list**; single-action screens execute right away |
| **Triple click** | — | `Radio`: next radio profile, `Beacon`: next interval | same |
| **Quad click** | — | `Beacon`: stop the beacon | same |
| **Boot rescue** | long press within 8 s after power-on | CLI rescue mode | — |

Header on every screen: **page title** on the left, **`N/5`** on root screens or **`<`**
on a sub-screen at x=97, **charge percentage** (`86%`, `--%` when the ADC reading is
invalid) on the right, separator rule at y=12.

### Languages

The interface ships in two languages and switches at runtime from
`Display` → `Language` (`RU` / `EN`, default `EN`, also available as the CLI command
`lang`). English strings sit inline in the code; Russian comes from a single table in
`src/helpers/ui/Localize.cpp` that `trStr()` looks up. Message text, node names and
channel names are data and are never translated, so Cyrillic still displays in full.

---

## Screen Map

Roots (click forward, double click back):

```
1 Home (FDSKY) → 2 Messages → 3 Links → 4 Network → 5 Menu → back to 1
```

Sub-screens — entered from a root, left with a double click:

```
1 Home      --hold--> message preview
2 Messages --hold--> message preview
3 Links     --hold--> Interfaces (BLE / WiFi / USB, ports 5000 and 5001)
5 Menu      --hold--> Radio · Power · Battery · Noise · Freqs · Recent
                      GPS · Beacon · Time · Display · Firmware · Power off
                Battery --hold--> ADC calibration
```

`Beacon` appears in the menu only while GPS is on; `Battery` only on the Heltec V4
board. The sensors page (`UI_SENSORS_PAGE=1`) is compiled into this build.

---

## Page Reference

### 1 · Home (FIRST)
- `MSG n/m` — messages waiting for the phone / still stored on the device (size 2)
- middle line, one of:
  - **Wi-Fi address** (`192.168.1.42`) — Wi-Fi link is up
  - `< Connected >` — a client is attached over BLE or USB, but Wi-Fi is down
  - `Pin:XXXXXX` — nothing is connected; PIN for pairing
- bottom line — time and date (weekday language comes from `Display` → `Language`)
- **Long press** → message preview

### 2 · Messages (MESSAGES)
- `In memory: n` — messages held on the device, `To phone: m` — queued for the app
- `hold: read` when there is anything to read, otherwise `no messages`
- **Long press** → message preview

### 3 · Links (BLUETOOTH)
```
BLE   on   no
WiFi  on   client
USB   on   ?
Pin:123456
```
- three columns: transport · adapter state · link state
- `client` — a peer actually holds the link, `no` — adapter up but nobody attached
- `USB` link is always `?`: the serial transport reports "connected" unconditionally
- 4th row shows the PIN; while a client is attached it shows the Wi-Fi network name
- **Long press** → `Interfaces` sub-screen

### 4 · Network (NEIGHBORS)
- Direct neighbors with SNR, auto-scrolls when the list is longer than the screen
- Empty list shows `no neighbors`
- **Long press** → poll neighbors (`Polling...`)

### 5 · Menu (MENU)
- Scrolling list with a `>` marker; `v` on the right while more items follow
- **Click** → move the marker, **long press** → open the selected screen

### Recent (RECENT) — menu sub-screen
- Up to 5 recently heard nodes with age (`12s`, `4m`, `1h`)
- Empty list shows `no adverts` and the `advert: hold` hint
- **Long press** → send an advert

### Radio (RADIO) — menu sub-screen
```
FQ: 868.731   SF: 7
BW: 62.50     CR: 7
TX: 10dBm  LNA: on
LongFast        RXB: on
```
- live frequency, bandwidth, spreading factor, coding rate and TX power
- `LNA` — external FEM amplifier, shown only when the board has a controllable one
- `RXB` — SX1262 boosted receive gain, a **separate** setting from the FEM LNA
- profile name in the bottom-left corner
- **Long press** → action list:
```
> Profile: next
  RX Boost: off
  LNA: off
```
  click moves the marker, a second hold runs the action, a double click closes the list

### Power (POWER) — menu sub-screen
```
Mode                   Eco max
Sleep: yes               RX: on
LNA: on
```
- `Mode` + generated profile name, `Sleep: yes/no/n/a` (light sleep permitted — the CPU still
  runs while the UI draws), `RX: on/off`, `LNA: on/off`
- bottom line only appears as a real reason: `GPS holds CPU`, `BLE holds CPU`, `sleep n/a`
- **Long press** → action list `Profile: next` / `Profile: back`

### Battery (BATTERY) — menu sub-screen, Heltec V4
- `Voltage: 4.012 V` and `ADC: 5.420` (the multiplier the calibration changes)
- **Long press** → calibration menu:
```
ADC calibration
> Full 4.20 V
  Multimeter
  Reset ADC
```
- **Full 4.20 V** — only for a fully charged single-cell Li-ion/LiPo pack
- **Multimeter** — enter the voltage you measured. The editor starts from the current
  device reading; **click** adds 10 mV (wraps 3000 → 4250), **long press** moves on
- **Reset ADC** — restore the board default multiplier
- every operation ends on a confirmation with **`Cancel` preselected**; long press applies
- the result is reported as `ADC saved` / `ADC not saved`; the multiplier is stored in
  prefs (`adc_mult`) and survives a reboot

### Noise (SCAN) — menu sub-screen
- live RSSI (size 2), noise floor `NL`, peak `pk`, `PKT` flag on reception, bar scale
- **Long press** → reset the peak

### Freqs (FSCAN) — menu sub-screen
- idle: `find quiet freq` and `scan: hold`
- running: `scan N/M`, current frequency, progress bar
- finished: `now <freq>` and the three quietest channels, the current one marked `*`
- **Long press** → start the scan

### Sensors (SENSORS) — menu sub-screen
```
vbat                   3.87 V
temp                      21.4 C
pressure                    1013 hPa
hold: refresh
```
- live telemetry read from the board's own sensors plus everything received from
  contacts (CayenneLPP): `vbat`, `current`, `temp`, `humidity`, `pressure`, `altitude`, `power`, `GPS`
- three rows at a time; a longer list scrolls by one row every 5 s and the footer says
  `list scrolls` instead of `hold: refresh`
- values refresh every 5 s while the screen is open; **long press** forces a read now
- `no sensors` when nothing but the board battery answers
- this page is enabled per environment (`-D UI_SENSORS_PAGE=1`); the Heltec V4 companion
  build has it compiled in

### Interfaces (CONNECTIONS) — sub-screen of Links
```
>BLE   ON
 WiFi  OFF
 USB   ON
```
- **Click** → move the `>` marker (BLE → WiFi → USB)
- **Long press** → toggle the selected interface (Wi-Fi switches both TCP servers, 5000 and 5001)
- **Double click** → back to BLUETOOTH
- bottom row reports the two TCP servers separately: `5000:+ 5001:-` (`+` client attached, `-` none, `n/a` no such server); `ports: --` while Wi-Fi is off
- header shows `<` instead of the page counter

### GPS — menu sub-screen
```
module                  on
fix: yes           sats 7
pos      55.9165 37.7601
alt                 156.20
```
- `module` — GPS powered on/off; a hardware switch that disagrees shows `off(hw)` / `off(sw)`
- `fix: yes/no` and the satellite count; `powered off` / `still powered` when the module
  is switched off; `no GPS access` when a build has no module driver
- coordinates and altitude once there is a fix
- **Long press** → toggle GPS on/off
- GPS pins for this board: RX 38, TX 39, reset 42, enable 34 (`-D ENV_INCLUDE_GPS=1`)

### Beacon (BEACON) — menu sub-screen
```
group                       30m
FDSKY-Base
55.9165  37.7601
sent 4m ago
```
- mode and interval, target name, position and the age of the last beacon
- `no fix` (position unknown) / `no gps` (no module) and `not sent` until the first send
- **Long press** → action list: `Mode: next`, `Interval: next`, `Stop beacon`
- interval applies to group/chat modes only (5/10/15/30/60/120 min)
- needs a valid fix; the beacon is sent by the mesh task, so the screen only reports state

### Display (SETUP) — menu sub-screen
```
Clock            on
Language              RU
Timeout         60 s
hold: edit
```
- `Clock` — clock on the main screen; `Timeout` — display auto-off: `off` / 15 / 30 / 60 / 120 s
  (stored as `scroff` in prefs)
- `Language` — `RU` / `EN`: switches the **whole interface** at runtime (every label, value
  and hint, plus the weekday/date on the clock). English strings live in the code, Russian
  comes from one table in `src/helpers/ui/Localize.cpp`; message text, node and channel
  names are never translated. Default is `EN`, the same setting as the CLI command `lang`.
- e-ink builds add `ScreenMode` (refresh mode) as an extra row → 4 rows, no hint line
- **Long press** → enter/leave edit mode; in edit mode **click** moves between rows and
  **triple click** changes the value

### Firmware (ABOUT) — menu sub-screen
```
v1.18.0-0.2.0
build: Nov  6 2026
node FDSKY
uptime 1h 12m
```
- firmware version (`MESHCORE_BASE_VERSION`-`FORK_VERSION`), build date, node name, uptime

### Power off (SHUTDOWN) — menu sub-screen
```
Power off?
> Cancel
  Apply
click: pick  hold: ok
```
- a confirmation dialog, **`Cancel` is preselected** — a stray long press only leaves the dialog
- **Click** → switch between `Cancel` and `Apply`
- **Long press** → apply the selected choice (execution happens after the button is released)
- after hibernate there is no wake source: `powerOff()` routes to `enterDeepSleep(0)`, which
  disables every wakeup source, so only a physical reset / power cycle brings the board back

### Time (TIME) — menu sub-screen
```
14:32  Thu, 05.11
tz: +3 h          lang: EN
source: set
hold: request
```
- live time and date in the configured timezone (`tz_offset_mins`) and language
- `source: set` once the clock was set by GPS, the phone, a trusted node or CLI;
  `source: none` plus `time not set` while the RTC still holds its reset value
- **Long press** → action list:
```
> Request time
  Time nodes
```
  `Request time` asks the trusted nodes for the time (`pullClock()`); the alert reports
  `no one to ask` when no node is configured or reachable. `Time nodes` opens the list
  of trusted nodes.

### Nodes (CLOCK_NODES) — time sub-screen
```
FDSKY-Base
Tower-2
hold: menu
```
- the nodes the clock is pulled from, taken from the contact book (up to 16 slots, the
  screen shows the first three plus `+N more`)
- `no nodes set` when the list is empty — the clock is then set only by GPS, the phone
  or the CLI
- **Long press** → action list: `Add node` · `Request time` · `Clear all`
- `Clear all` needs one more hold and reports `nodes cleared`

### Pick node (NODE_PICKER) — node sub-screen
```
> FDSKY-Base
  Tower-2
  Home-rep
hold: add          1/4
```
- the contact book, scrolled by **click** (forward) and **double click** (back)
- **Long press** → add the highlighted contact as a trusted time node; the alert is
  `node added`, `already a node` or `list is full`
- the same list is managed by the CLI command `clock add|remove|list`

### Message preview
- sender in the top-left corner, age in the top-right corner, rule at y=11, wrapped text below
- **Click** → next message / close, **double click** → next page of a long text,
  **triple click** → drop the message from the buffer

---

## CLI Commands (USB / BLE / WiFi Telnet)

| Command | Description |
|---|---|
| `help` | List all commands |
| `config` | Show all preferences |
| `version` | Firmware version |
| `radio.profile <normal\|long\|hinoise>` | Set radio profile |
| `power.profile <0-5>` | Set power profile |
| `adc.multiplier <float>` | Set battery ADC multiplier |
| `get adc.multiplier` | Read current multiplier |
| `get battery` | Read battery voltage and current ADC multiplier |
| `calibrate battery <mV>` | Calibrate against a multimeter reading (3000–4250 mV) |
| `reset adc.multiplier` | Restore the Heltec V4 board default |
| `pin <6-digit>` | Set BLE PIN (0 = random each boot) |
| `get pin` | Read current PIN |
| `wifi <ssid> <pwd>` | Set WiFi credentials |
| `wifi on\|off` | Enable/disable WiFi |
| `ota <url>` | OTA update from URL |
| `reboot` | Restart device |
| `beacon` | Show beacon status |
| `neighbors` | List known neighbors |
| `contacts` | List contacts |
| `channels` | List group channels |
| `radio.fem.rxgain on\|off` | Toggle FEM LNA |
| `get radio.fem.rxgain` | Read LNA state |

---

## Battery Calibration

1. Measure the battery with a multimeter while the board is running in the same charging state as the reading.
2. Check `get battery` (board default multiplier: 5.42).
3. Send `calibrate battery <actual_mV>`. The firmware averages eight readings, checks stability, computes `new_mult = current_mult * actual_mV / reported_mV`, applies and saves it. Alternatively, set a checked multiplier with `set adc.multiplier <value>` (the old `adc.multiplier <value>` form also works).
4. Verify `get battery` again and repeat after reboot. `reset adc.multiplier` restores the board default without resetting other settings.

Example: multimeter 4100mV, device reports 3950mV
```
calibrate battery 4100
```

---

## Backup / Restore (NVS + SPIFFS)

Requires `esptool.py` and device on `/dev/ttyACM0`

```bash
# Save settings from device
./tools/backup_restore.sh save

# Flash new firmware WITHOUT losing settings
./tools/backup_restore.sh flash

# Restore settings to device
./tools/backup_restore.sh restore

# Full cycle: save → erase-flash → flash → restore
./tools/backup_restore.sh save
esptool --chip esp32s3 -p /dev/ttyACM0 erase-flash
./tools/backup_restore.sh flash
./tools/backup_restore.sh restore
```

---

## Flashing Firmware

**Web Flasher:** https://meshcore-dev.github.io/MeshCoreFlasher/  
Select: `firmware-merged.bin` → Flash at `0x0`

**CLI (dio/40m required for Heltec V4):**
```bash
esptool --chip esp32s3 -p /dev/ttyACM0 -b 460800 erase-flash
esptool --chip esp32s3 -p /dev/ttyACM0 -b 460800 \
  --before default-reset --after hard-reset \
  write-flash --flash-mode dio --flash-freq 40m --flash-size 16MB \
  0x0 firmware-merged.bin
```

**Separate files (if web flasher fails):**
```bash
esptool --chip esp32s3 -p /dev/ttyACM0 -b 460800 \
  write-flash --flash-mode dio --flash-freq 40m --flash-size 16MB \
  0x0 bootloader.bin 0x8000 partitions.bin 0x10000 firmware.bin
```

---

## Troubleshooting

| Symptom | Cause | Fix |
|---|---|---|
| Boot loop `TG0WDT_SYS_RST` | Wrong flash mode | Use **dio/40m**, not qio/80m |
| BLE PIN not visible | Old firmware | Update to v1.18.0-0.2.0+ |
| WiFi not connecting | Wrong creds / no AP | Check `config`, re-enter `wifi` |
| GPS no fix | Indoor / no sky view | Move outdoors, wait 2-5 min |
| Beacon not sending | GPS off / no fix | Enable GPS, wait for fix |
| Settings lost after flash | Used `erase-flash` | Use backup script or flash without erase |

---

## Files of Interest

| File | Purpose |
|---|---|
| `SYSTEM_AUDIT.md` | Full technical audit |
| `tools/backup_restore.sh` | Backup/restore script |
| `examples/companion_radio/main.cpp` | App entry point |
| `variants/heltec_v4/platformio.ini` | Build config |
| `src/helpers/FirmwareVersion.h` | Version defines |

---

## Links

- **MeshCore repo:** https://github.com/meshcore-dev/MeshCore
- **Our fork:** https://github.com/ferrisaxis/MeshCore
- **Web flasher:** https://meshcore-dev.github.io/MeshCoreFlasher/
- **TERMIN36 fork (cyrillic/power/UI):** https://github.com/TERMIN36/MeshCore
