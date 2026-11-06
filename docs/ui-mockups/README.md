# Макеты экранов MeshCore (128x64 OLED)

Сгенерировано из кода прошивки: `tools/ui_mockups.py`.

- Латинский шрифт — 5x7 из Adafruit GFX (`glcdfont.c`)
- Кириллица — лицо CYR_OLED из `src/helpers/ui/CyrillicGlyphs.inc`
- Шапка: название экрана слева, счётчик `3/12` (или `<` на подэкранах) у x=101,
  иконка батареи справа, разделительная линия на y=12

| Файл | Экран | Описание |
|---|---|---|
| `01-main-screen-wifi-ip.png` | main-screen-wifi-ip | Главный: Wi-Fi поднят, показан IP |
| `02-main-screen-ble-client.png` | main-screen-ble-client | Главный: подключён USB/BLE клиент, без Wi-Fi |
| `03-main-screen-pin.png` | main-screen-pin | Главный: подключения нет, показан PIN |
| `04-boot-splash.png` | boot-splash | Заставка при загрузке |
| `05-message-preview.png` | message-preview | Превью входящего сообщения |
| `06-recent-nodes.png` | recent-nodes | Недавние: список и возраст |
| `07-recent-nodes-empty.png` | recent-nodes-empty | Недавние: пусто |
| `08-neighbors-snr.png` | neighbors-snr | Соседи: список и SNR |
| `09-neighbors-empty.png` | neighbors-empty | Соседи: пусто |
| `10-radio-settings-lna.png` | radio-settings-lna | Радио: параметры, LNA есть |
| `11-radio-settings-no-lna.png` | radio-settings-no-lna | Радио: без управляемого LNA |
| `12-power-profile.png` | power-profile | Питание: профиль и подсказка |
| `13-power-profile-blocked.png` | power-profile-blocked | Питание: профиль упирается в ресурс |
| `14-battery-adc.png` | battery-adc | Батарея: напряжение и множитель АЦП |
| `15-battery-calibration.png` | battery-calibration | Батарея: выбор калибровки |
| `16-battery-calibration-confirm.png` | battery-calibration-confirm | Батарея: подтверждение |
| `17-noise-scan.png` | noise-scan | Шум: уровень, пик, шкала |
| `18-freq-scan-idle.png` | freq-scan-idle | Частоты: ожидание |
| `19-freq-scan-running.png` | freq-scan-running | Частоты: идёт скан |
| `20-freq-scan-result.png` | freq-scan-result | Частоты: результат |
| `21-interfaces-status.png` | interfaces-status | Bluetooth: статус BLE/WiFi/USB и PIN |
| `22-interfaces-select-ble.png` | interfaces-select-ble | Связь: маркер на BLE |
| `23-interfaces-select-wifi.png` | interfaces-select-wifi | Связь: маркер на WiFi |
| `24-interfaces-select-usb.png` | interfaces-select-usb | Связь: маркер на USB |
| `25-gps-status.png` | gps-status | GPS: включён, есть фикс |
| `26-gps-unavailable.png` | gps-unavailable | GPS: модуль недоступен |
| `27-beacon.png` | beacon | Маяк: режим, интервал, цель |
| `28-setup.png` | setup | Настройки: Clock |
| `29-shutdown.png` | shutdown | Выключение: иконка питания |
| `30-shutdown-hibernating.png` | shutdown-hibernating | Выключение: идёт процесс |
| `31-alert-advert-sent.png` | alert-advert-sent | Всплывающее: advert отправлен |
| `32-alert-power-mode.png` | alert-power-mode | Всплывающее: смена профиля |
| `33-alert-radio-profile.png` | alert-radio-profile | Всплывающее: смена радио-профиля |
| `34-alert-polling.png` | alert-polling | Всплывающее: опрос соседей |
| `35-alert-adc-saved.png` | alert-adc-saved | Всплывающее: АЦП сохранён |

Каждый PNG — 128x64 (1x) и `<имя>@4x.png` (увеличение x4 без сглаживания).

Обзорный лист: `00-contact-sheet.png`.
