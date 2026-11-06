# MeshCore screen mockups (128x64 OLED)

Generated from the firmware code: `tools/ui_mockups.py`.

- Latin font: 5x7 from Adafruit GFX (`glcdfont.c`)
- Cyrillic glyphs come from `src/helpers/ui/CyrillicGlyphs.inc` (messages only)
- Header: screen title on the left, counter `3/12` (or `<` on sub-screens) at x=101,
  battery indicator on the right, rule at y=12

| File | Screen | Description |
|---|---|---|
| `01-root-home-wifi-ip.png` | root-home-wifi-ip | Root 1/5: home, Wi-Fi up, IP shown |
| `02-root-home-ble-client.png` | root-home-ble-client | Root 1/5: USB/BLE client attached, no Wi-Fi |
| `03-root-home-pin.png` | root-home-pin | Root 1/5: no connection, PIN |
| `04-root-messages.png` | root-messages | Root 2/5: message counters |
| `05-root-connections.png` | root-connections | Root 3/5: BLE/WiFi/USB status |
| `06-root-network.png` | root-network | Root 4/5: network, neighbor list |
| `07-root-network-empty.png` | root-network-empty | Root 4/5: no neighbors |
| `08-root-menu-radio.png` | root-menu-radio | Root 5/5: menu, Radio selected |
| `09-root-menu-battery.png` | root-menu-battery | Root 5/5: menu scrolled to Battery |
| `10-root-menu-shutdown.png` | root-menu-shutdown | Root 5/5: menu scrolled to Power off |
| `11-sub-interfaces-toggle.png` | sub-interfaces-toggle | Links: interface selection (BLE) |
| `12-sub-interfaces-toggle-wifi.png` | sub-interfaces-toggle-wifi | Links: marker on WiFi |
| `13-sub-radio.png` | sub-radio | Radio: parameters and LNA |
| `14-sub-radio-no-lna.png` | sub-radio-no-lna | Radio: board without a controllable LNA |
| `14b-sub-radio-actions.png` | sub-radio-actions | Radio actions: profile, RX Boost, LNA |
| `15-sub-power.png` | sub-power | Power: profile and sleep state |
| `16-sub-power-blocked.png` | sub-power-blocked | Power: sleep blocked by BLE |
| `16b-sub-power-actions.png` | sub-power-actions | Power actions: profile forward/back |
| `17-sub-battery.png` | sub-battery | Battery: voltage and ADC multiplier |
| `18-sub-battery-calibration.png` | sub-battery-calibration | ADC calibration: operation list |
| `18b-sub-battery-meter.png` | sub-battery-meter | ADC calibration: Multimeter highlighted |
| `19-sub-battery-editor.png` | sub-battery-editor | Multimeter calibration: reference editor |
| `19b-sub-battery-confirm.png` | sub-battery-confirm | Calibration: confirmation, Cancel preselected |
| `20-sub-noise-scan.png` | sub-noise-scan | Noise: level, peak, bar |
| `21-sub-freq-scan-idle.png` | sub-freq-scan-idle | Freq scan: idle |
| `22-sub-freq-scan-running.png` | sub-freq-scan-running | Freq scan: running |
| `23-sub-freq-scan-result.png` | sub-freq-scan-result | Freq scan: result |
| `24-sub-recent.png` | sub-recent | Recent: list with age |
| `25-sub-recent-empty.png` | sub-recent-empty | Recent: empty |
| `26-sub-gps.png` | sub-gps | GPS: on, fix acquired |
| `27-sub-gps-unavailable.png` | sub-gps-unavailable | GPS: module not responding |
| `28-sub-beacon.png` | sub-beacon | Beacon: mode, interval, target |
| `25a-sub-sensors.png` | sub-sensors | Sensors: voltage, temperature, pressure |
| `25b-sub-sensors-empty.png` | sub-sensors-empty | Sensors: no data |
| `29c-sub-clock-nodes.png` | sub-clock-nodes | Time nodes: two trusted nodes |
| `29d-sub-clock-nodes-empty.png` | sub-clock-nodes-empty | Time nodes: none set |
| `29e-sub-node-picker.png` | sub-node-picker | Contact picker for a time node |
| `29f-sub-time-actions.png` | sub-time-actions | Time actions: request and nodes |
| `28c-sub-beacon-no-fix.png` | sub-beacon-no-fix | Beacon: no GPS fix |
| `28b-sub-beacon-actions.png` | sub-beacon-actions | Beacon actions: mode, interval, stop |
| `29-sub-screen-time.png` | sub-screen-time | Display: Clock and Timeout |
| `29b-sub-about.png` | sub-about | Firmware: version, build, node, uptime |
| `30-sub-time.png` | sub-time | Time: clock, timezone, source |
| `31-sub-shutdown.png` | sub-shutdown | Power off: confirmation, Cancel preselected |
| `32-sub-shutdown-apply.png` | sub-shutdown-apply | Power off: Apply selected |
| `33-sub-shutdown-progress.png` | sub-shutdown-progress | Power off: in progress |
| `34-boot-splash.png` | boot-splash | Boot splash |
| `35-message-preview.png` | message-preview | Message preview (no header) |
| `31-alert-advert-sent.png` | alert-advert-sent | Alert: advert sent |
| `32-alert-power-mode.png` | alert-power-mode | Alert: profile changed |
| `33-alert-radio-profile.png` | alert-radio-profile | Alert: radio profile changed |
| `34-alert-polling.png` | alert-polling | Alert: neighbor poll |
| `35-alert-adc-saved.png` | alert-adc-saved | Alert: ADC saved |

Each PNG is 128x64 (1x) plus `<name>@4x.png` (4x, no smoothing).

Overview sheet: `00-contact-sheet.png`.
