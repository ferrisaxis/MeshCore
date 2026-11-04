# System Audit Report — Heltec V4 Companion (v1.18.0-0.2.0)

**Date:** 2026-11-04
**Build:** `heltec_v4_oled_companion_usb_ble_wifi`
**Git:** `HEAD` (uncommitted changes on top of `130af75a`)

---

## 1. Build & Firmware

| Parameter | Value |
|---|---|
| **Version** | v1.18.0-0.2.0 |
| **Base** | meshcore-dev/main (1.18.0) + 262 commits merged |
| **Fork layer** | TERMIN36 0.2.0 (cyrillic, power mgmt, radio profiles, UI) |
| **Flash mode** | dio / 40 MHz (QIO/80 causes boot-loop TG0WDT_SYS_RST) |
| **Flash usage** | 1 713 289 bytes (26.1% of 6.5 MB) |
| **RAM usage** | 198 336 bytes (9.5% of 2 MB) |
| **Merged image** | 1 779 248 bytes, sha256 `a36c8a14dbe7...3069fa` |
| **Bootloader** | 15 104 bytes |
| **Partitions** | default_16MB.csv (SPIFFS @ 0xC90000 3.456 MB, NVS @ 0x9000 20 KB) |

---

## 2. Hardware Support (Heltec V4)

| Component | Status | Notes |
|---|---|---|
| ESP32-S3 (16 MB flash, 2 MB PSRAM AP_3v3) | ✅ | Verified on device: MAC `f8:5b:1b:a1:0a:f0` |
| SX1262 LoRa (GC1109 PA + KCT8103L LNA) | ✅ | FEM control via `LoRaFEMControl` |
| OLED SSD1306 128×64 (I2C SDA=17, SCL=18, RST=21) | ✅ | `SSD1306Display` with Cyrillic |
| USB CDC (native, /dev/ttyACM0) | ✅ | 115200, CLI + MeshCore protocol |
| BLE (UART service, static/random PIN) | ✅ | NVS bonding, PIN on screen |
| WiFi STA (auto-reconnect, OTA) | ✅ | Settings in prefs, inhibits sleep |
| Buttons: USER (GPIO0), VEXT (GPIO36) | ✅ | Click / double / long / triple |
| Battery ADC (PIN_VBAT_READ=1, CTRL=37) | ✅ | `adc.multiplier` configurable (default 5.42) |

---

## 3. Radio & Mesh

| Feature | Status |
|---|---|
| **Radio Profiles** | Normal / Long / HiNoise — UI in RADIO page |
| **FEM LNA control** | CLI `set radio.fem.rxgain on/off`, persists in prefs |
| **TX power** | 10 dBm default, max 22 dBm |
| **Channel scan / freq scan** | SCAN / FSCAN pages |
| **Beacon (mayak)** | 5 modes: OFF, Advert Local, Advert Flood, Group, Chat |
| **Group channels** | Up to 40 (MAX_GROUP_CHANNELS) |
| **Offline queue** | 256 frames |

---

## 4. UI — Screen Map & Navigation

### Page Order (circular)
```
FIRST → RECENT → NEIGHBORS → RADIO → POWER → SCAN → FSCAN → BLUETOOTH → CONNECTIONS → GPS → BEACON → SENSORS → SETUP → SHUTDOWN → FIRST
```

### Controls (Heltec V4 — single button GPIO0 + analog)
| Action | Input | Context |
|---|---|---|
| **Next page** | Click / short press | Any page |
| **Prev page** | Double click | Any page |
| **Long press** | Hold ~1.2s | Page-specific (see below) |
| **Triple click** | 3× fast | Drop message / buzzer toggle |
| **Rotary** | N/A on V4 | — |

### Page Details

| Page | Short Press (Enter) | Long Press | Auto-refresh |
|---|---|---|---|
| **FIRST** | Reopen msg preview | — | 1s (clock) |
| **RECENT** | Send advert | — | 1s |
| **NEIGHBORS** | — | — | 1s |
| **RADIO** | Cycle radio profile | Toggle FEM LNA | 1s |
| **POWER** | Cycle power profile | — | 1s |
| **SCAN** | Start noise scan | — | 1s |
| **FSCAN** | Start freq scan | — | 1s |
| **BLUETOOTH** | — | Toggle BLE on/off | 1s |
| **CONNECTIONS** | Cycle select (BLE→WiFi→USB) | Toggle selected iface | 1s |
| **GPS** | — | Toggle GPS | 1s |
| **BEACON** | Cycle beacon mode | Cycle beacon interval | 1s |
| **SENSORS** | Toggle GPS | — | 1s/3s |
| **SETUP** | Edit toggle (clock/screen) | — | 1s |
| **SHUTDOWN** | Start hibernate | — | — |

---

## 5. Connections Page (NEW)

Shows all three interfaces with status:

```
BLE  ON
WiFi ON
    192.168.1.42
USB  ON

click:select  long:toggle  left/right:exit
```

- **Selection** cycles: BLE → WiFi → USB → BLE (highlighted line)
- **Long press** toggles the selected interface
- **Left/Right** (prev/next page) exits to menu

---

## 6. Power Management

| Profile | CPU Sleep | RF Boost | LNA | Use Case |
|---|---|---|---|---|
| Eco max | ✅ | ❌ | ❌ | Max battery |
| Eco high | ✅ | ✅ | ❌ | Good range |
| Eco good | ✅ | ✅ | ✅ | Best RX |
| Eco fair | ❌ | ❌ | ❌ | Always on |
| Eco min | ❌ | ✅ | ❌ | TX priority |
| Active | ❌ | ✅ | ✅ | Max performance |

- **Auto-shutdown** at 3.4V (`AUTO_SHUTDOWN_MILLIVOLTS`)
- **WiFi/BLE inhibit sleep** when active
- **BLE blocks CPU sleep** (no modem sleep in Arduino core)

---

## 7. Configuration & Persistence

| Storage | Path | Contents | Survives Flash |
|---|---|---|---|
| **NVS** | 0x9000 | BLE bonding, WiFi creds, radio prefs | ❌ (erase-flash clears) |
| **SPIFFS** | 0xC90000 | `prefs.json`, contacts, channels, blobs | ✅ (merged image ends ~0x1B2630) |

### Backup/Restore Script
```bash
./tools/backup_restore.sh save      # read NVS + SPIFFS from device
./tools/backup_restore.sh flash     # flash firmware WITHOUT erase
./tools/backup_restore.sh restore   # write NVS + SPIFFS back
```

---

## 8. CLI Commands (via USB/BLE/WiFi)

| Command | Description |
|---|---|
| `help` | List commands |
| `config` | Show all prefs |
| `radio.profile <normal\|long\|hinoise>` | Set radio profile |
| `power.profile <0-5>` | Set power profile |
| `adc.multiplier <float>` | Calibrate battery ADC |
| `pin <6-digit>` | Set BLE PIN |
| `wifi <ssid> <pwd>` | Set WiFi credentials |
| `ota <url>` | OTA update |
| `reboot` | Restart device |
| `beacon` | Show beacon status |

---

## 9. Known Issues / Limitations

| Issue | Severity | Workaround |
|---|---|---|
| Menu labels English only | Low | Cyrillic only for messages/date |
| MQTT/Web not in companion build | Medium | Available in `simple_repeater` example |
| USB `isConnected()` always true | Low | Cosmetic |
| Analog button (PIN_USER_BTN_ANA) untested | Low | Use GPIO0 button |

---

## 10. Git Status

```
M examples/companion_radio/AbstractUITask.h
M examples/companion_radio/ui-new/UITask.cpp
M src/helpers/MultiSerialInterface.h
?? backup/
```

**Uncommitted changes:**
- MultiSerialInterface: WiFi/USB enable/disable/isEnabled
- AbstractUITask: proxy methods for WiFi/USB
- UITask: CONNECTIONS page, handleLongPress toggle, CONNECTIONS navigation

---

## 11. Verification Checklist

- [x] Build passes (PlatformIO, esp32s3)
- [x] Flash mode dio/40m confirmed working on hardware
- [x] BLE PIN visible on BLUETOOTH page (fix applied)
- [x] Connections page shows all 3 interfaces
- [x] Long press toggles selected interface on Connections
- [x] Left/Right navigation works from Connections
- [x] Beacon page functional (cycle mode/interval)
- [x] Radio profiles switchable
- [x] Power profiles switchable
- [x] Version string v1.18.0-0.2.0 in binary
- [x] Backup script created and committed
- [x] Merged image created and verified