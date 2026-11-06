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
settings screen lives one level down in `Меню`. Inside a sub-screen a double click
goes back, a long press performs that screen's action.

| Gesture | Timing | Root screens | Sub-screens |
|---|---|---|---|
| **Click** | — | next root | `Меню` / `Связь`: move the `>` marker; otherwise nothing |
| **Double click** | < 280 ms between clicks | previous root | back to the parent screen |
| **Long press** | 1000 ms | `Меню`: open the selected screen, `Связь`: open `Интерфейсы`, `Сообщения`/главная: message preview | the screen's own action (toggle, scan, calibrate); on the power-off dialog — apply the highlighted choice |
| **Triple click** | — | `Радио`: next radio profile, `Маяк`: next interval | same |
| **Quad click** | — | `Маяк`: stop the beacon | same |
| **Boot rescue** | long press within 8 s after power-on | CLI rescue mode | — |

Header on every screen: **page title** on the left, **`N/5`** on root screens or **`<`**
on a sub-screen at x=97, **charge percentage** (`86%`, `--%` when the ADC reading is
invalid) on the right, separator rule at y=12.

---

## Screen Map

Roots (click forward, double click back):

```
1 Главная (FDSKY) → 2 Сообщения → 3 Связь → 4 Сеть → 5 Меню → back to 1
```

Sub-screens — entered from a root, left with a double click:

```
1 Главная   --hold--> просмотр сообщений
2 Сообщения --hold--> просмотр сообщений
3 Связь     --hold--> Интерфейсы (BLE / WiFi / USB, порты 5000 и 5001)
5 Меню      --hold--> Радио · Питание · Батарея · Шум · Частоты · Недавние
                      GPS · Маяк · Время · Экран · Выключить
                Батарея --hold--> Калибровка АЦП
```

`Beacon` появляется в меню только при включённом GPS; `Батарея` — только на плате
Heltec V4. Датчики (`UI_SENSORS_PAGE`) в эту сборку не входят.

---

## Page Reference

### 1 · Главная (FIRST)
- `MSG n/m` — messages waiting for the phone / still stored on the device (size 2)
- middle line, one of:
  - **Wi-Fi address** (`192.168.1.42`) — Wi-Fi link is up
  - `< Connected >` — a client is attached over BLE or USB, but Wi-Fi is down
  - `Pin:XXXXXX` — nothing is connected; PIN for pairing
- bottom line — time and date (RU locale)
- **Long press** → message preview

### 2 · Сообщения (MESSAGES)
- `В памяти: n` — messages held on the device, `На телефон: m` — queued for the app
- `держать: читать` when there is anything to read, otherwise `Сообщений нет`
- **Long press** → message preview

### 3 · Связь (BLUETOOTH)
```
BLE   вкл   нет
WiFi  вкл   клиент
USB   вкл   ?
Pin:123456
```
- three columns: transport · adapter state · link state
- `клиент` — a peer actually holds the link, `нет` — adapter up but nobody attached
- `USB` link is always `?`: the serial transport reports "connected" unconditionally
- 4th row shows the PIN; while a client is attached it shows the Wi-Fi network name
- **Long press** → `Интерфейсы` sub-screen

### 4 · Сеть (NEIGHBORS)
- Direct neighbors with SNR, auto-scrolls when the list is longer than the screen
- Empty list shows `no neighbors`
- **Long press** → poll neighbors (`Polling...`)

### 5 · Меню (MENU)
- Scrolling list with a `>` marker; `v` on the right while more items follow
- **Click** → move the marker, **long press** → open the selected screen

### Недавние (RECENT) — подэкран меню
- Up to 5 recently heard nodes with age (`12s`, `4m`, `1h`)
- Empty list shows `no adverts` and the `advert: hold` hint
- **Long press** → send an advert

### Радио (RADIO) — подэкран меню
- `FQ` frequency, `SF` spreading factor, `BW` bandwidth, `CR` coding rate
- `TX: NNdBm` and `LNA: on/off` — the LNA part only on boards that can switch it
- current radio profile on the bottom line
- **Triple click** → next radio profile (`LongFast`, `LongSlow`, ...)
- **Long press** → toggle the FEM LNA (silent no-op if the board has none)

### Питание (POWER) — подэкран меню
```
Power                       Eco max
CPU on                  RX on
LNA on
              mode: hold
```
- `Режим` + profile name, generated from the available options (`Eco max` … `Eco min`)
- `Сон: да/нет/н/д` — whether light sleep is permitted (the CPU still runs while the UI draws), `RX: вкл/выкл` — SX1262 boosted gain, separate from the FEM LNA
- `LNA: вкл/выкл` only on boards with a controllable FEM
- bottom line is a real reason, not a hint: `GPS держит CPU`, `BLE держит CPU` or `сон недоступен`
- **Long press** → apply the next power profile

### Батарея (BATTERY) — подэкран меню, Heltec V4
- `Voltage: 4.012 V` and `ADC: 5.420`
- **Long press** → open calibration:
  - `Full 4.20V` — only for a fully charged single-cell Li-ion/LiPo pack
  - `Reset ADC` — restore the board default
  - `Meter: use CLI` — tells you to send `calibrate battery <mV>`
  - inside: **click** moves/selects, **long press** confirms, **double click** cancels

### Шум (SCAN) — подэкран меню
- live RSSI (size 2), noise floor `NL`, peak `pk`, `PKT` flag on reception, bar scale
- **Long press** → reset the peak

### Частоты (FSCAN) — подэкран меню
- idle: `find quiet freq` and `scan: hold`
- running: `scan N/M`, current frequency, progress bar
- finished: `now <freq>` and the three quietest channels, the current one marked `*`
- **Long press** → start the scan

### Интерфейсы (CONNECTIONS) — подэкран «Связи»
```
>BLE   ON
 WiFi  OFF
 USB   ON
```
- **Click** → move the `>` marker (BLE → WiFi → USB)
- **Long press** → toggle the selected interface (Wi-Fi switches both TCP servers, 5000 and 5001)
- **Double click** → back to BLUETOOTH
- bottom row reports the two TCP servers separately: `5000:+ 5001:-` (`+` client attached, `-` none, `н/д` no such server); `порты: --` while Wi-Fi is off
- header shows `<` instead of the page counter

### GPS — подэкран меню
- `module on/off` (may show `off(hw)` when a hardware switch disagrees)
- `fix yes/no`, `sats N`; `Can't access GPS` when no module answers
- **Long press** → toggle GPS

### Маяк (BEACON) — подэкран меню
- mode, interval and target
- **Long press** → next mode (OFF → Local → Flood → Group → Chat)
- **Triple click** → next interval
- **Quad click** → stop the beacon

### Экран (SETUP) — подэкран меню
- `Clock on/off` (controls the clock on FIRST); e-ink build also has `ScreenMode`
- **Long press** → enter/leave edit mode
- in edit mode: **click** → next row, **triple click** → change value

### Выключить (SHUTDOWN) — подэкран меню
```
Выключить питание?
> Отмена
  Применить
клик: выбор  держ: ок
```
- a confirmation dialog, **`Отмена` is preselected** — a stray long press only leaves the dialog
- **Click** → switch between `Отмена` and `Применить`
- **Long press** → apply the selected choice (execution happens after the button is released)
- after hibernate there is no wake source: `powerOff()` routes to `enterDeepSleep(0)`, which
  disables every wakeup source, so only a physical reset / power cycle brings the board back

### Время (TIME) — подэкран меню
```
14:32  Чт, 05.11
пояс: +3 ч        язык: RU
источник: задано
держать: запрос
```
- live time and date in the configured timezone (`tz_offset_mins`) and language
- `источник: задано` once the clock was set by GPS, the phone, a trusted node or CLI;
  `источник: нет` plus `время не задано` while the RTC still holds its reset value
- **Long press** → ask the mesh for the time (`pullClock()`); the alert reports if there
  is no node to answer

### Просмотр сообщения
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
