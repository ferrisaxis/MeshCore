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

## Controls

| Button | Action |
|---|---|
| **Click** (short) | Next page / Confirm / Cycle selection |
| **Double click** | Previous page |
| **Long press** (1.2s) | Page-specific action (toggle, start scan, etc.) |
| **Triple click** | Drop message / Toggle buzzer |
| **Boot rescue** | Long press in first 8s after boot → CLI rescue mode |

---

## Screen Map (circular navigation)

```
FIRST → RECENT → NEIGHBORS → RADIO → POWER → SCAN → FSCAN
  ↑                                                                   ↓
SHUTDOWN ← SETUP ← SENSORS ← BEACON ← GPS ← CONNECTIONS ← BLUETOOTH
```

---

## Page Reference

### FIRST — Main Screen
- **MSG: X/Y** — unread / stored messages
- **IP: x.x.x.x** — WiFi IP (if connected)
- **< Connected >** — BLE client connected
- **Pin:XXXXXX** — BLE PIN (if no client connected)
- **Date/Time** — bottom line (RU locale)

### RECENT — Recent Adverts
- List of recently heard nodes
- **Click** → Send advert

### NEIGHBORS — Known Neighbors
- Nodes with established routes
- Signal strength, last heard

### RADIO — Radio Profile
- **Normal** — balanced (default)
- **Long** — max range, lower data rate
- **HiNoise** — noisy environments
- **Click** → Cycle profile
- **Long press** → Toggle FEM LNA (if supported)

### POWER — Power Profile
| Profile | CPU | RF Boost | LNA | Battery Life |
|---|---|---|---|---|
| Eco max | Sleep | Off | Off | Maximum |
| Eco high | Sleep | On | Off | Long |
| Eco good | Sleep | On | On | Balanced |
| Eco fair | Active | Off | Off | Standard |
| Eco min | Active | On | Off | TX priority |
| Active | Active | On | On | Maximum perf |

- **Click** → Cycle profile
- Auto-shutdown at 3.4V

### SCAN — Noise Scan
- Real-time RSSI noise floor graph
- **Click** → Start/stop scan

### FSCAN — Frequency Scan
- Sweeps LoRa channels, shows activity
- **Click** → Start scan

### BLUETOOTH — BLE Status
- Large icon: ON / OFF
- **Pin:XXXXXX** — pairing PIN
- **Long press** → Toggle BLE on/off

### CONNECTIONS — Interface Manager (NEW)
```
BLE  ON
WiFi ON
    192.168.1.42
USB  ON

click:select  long:toggle  left/right:exit
```
- **Click** → Cycle selection (highlighted line): BLE → WiFi → USB
- **Long press** → Toggle selected interface ON/OFF
- **Left/Right** (prev/next page) → Exit to menu
- WiFi shows IP when connected

### GPS — GPS Module
- **module on/off** — software state
- **fix yes/no** — satellite fix
- **sat N** — satellites count
- **pos** — lat/lon
- **alt** — altitude (m)
- **Long press** → Toggle GPS power

### BEACON — Mayak (Position Beacon)
*Requires GPS fix to operate*

| Mode | Interval | Description |
|---|---|---|
| **OFF** | — | Disabled |
| **Advert Local** | 10 min | Self-advert, 1 hop |
| **Advert Flood** | 30 min | Self-advert, network flood |
| **Group** | 5–180 min | To group channel |
| **Chat** | 5–180 min | Direct to contact (ACK) |

- **Click** → Cycle mode: OFF → Local → Flood → Group → Chat → OFF
- **Long press** → Cycle interval (Group/Chat): 5 → 10 → 15 → 30 → 60 → 120 min
- Auto-enables GPS when activated

### SENSORS — Sensor Data (Cayenne LPP)
- Temperature, humidity, pressure, battery, etc.
- **Click** → Toggle GPS
- Auto-scrolls if multiple sensors

### SETUP — Device Settings
- **Clock** — on/off (shows on FIRST page)
- **ScreenMode** — 1/2/3 (e-ink refresh mode)
- **Click** → Enter edit mode
- **Up/Down** (in edit) → Change value
- **Click** → Save & exit edit

### SHUTDOWN — Hibernate
- **Click** → Enter hibernation (deep sleep)
- Wake: LoRa packet (DIO1) or USER button
- Preserves RAM, restores mesh state

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
