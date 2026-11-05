# Макеты экранов MeshCore (128x64 OLED)

Сгенерировано из кода прошивки: `tools/ui_mockups.py`.

Шрифт — настоящий 5x7 из Adafruit GFX (glcdfont.c), координаты — как в `UITask.cpp`.

| Файл | Экран | Описание |
|---|---|---|
| `01-main-screen-connected.png` | main-screen-connected | Главный экран: телефон подключён, показан IP |
| `02-main-screen-pin.png` | main-screen-pin | Главный экран: подключения нет, показан PIN |
| `03-boot-splash.png` | boot-splash | Заставка при загрузке: лого, версия |
| `04-message-preview.png` | message-preview | Превью входящего сообщения (долгое нажатие на главном) |
| `05-recent-nodes.png` | recent-nodes | RECENT: недавно слышанные ноды и возраст |
| `06-recent-nodes-empty.png` | recent-nodes-empty | RECENT: список пуст, подсказка advert |
| `07-neighbors-snr.png` | neighbors-snr | NEIGHBORS: соседи и SNR |
| `08-neighbors-empty.png` | neighbors-empty | NEIGHBORS: пусто, подсказка poll |
| `09-radio-settings-lna.png` | radio-settings-lna | RADIO: частота/SF/BW/CR, мощность, LNA доступен |
| `10-radio-settings-no-lna.png` | radio-settings-no-lna | RADIO: плата без управляемого LNA |
| `11-power-profile.png` | power-profile | POWER: профиль питания и подсказка |
| `12-power-profile-blocked.png` | power-profile-blocked | POWER: профиль упирается в ресурс (BLE держит CPU) |
| `13-noise-scan.png` | noise-scan | SCAN: уровень шума, пик, шкала |
| `14-freq-scan-idle.png` | freq-scan-idle | FSCAN: ожидание запуска |
| `15-freq-scan-running.png` | freq-scan-running | FSCAN: идёт сканирование, прогресс |
| `16-freq-scan-result.png` | freq-scan-result | FSCAN: результат, топ-3 частоты |
| `17-interfaces-status.png` | interfaces-status | Экран интерфейсов: BLE/WiFi/USB ON-OFF + PIN |
| `18-interfaces-select-ble.png` | interfaces-select-ble | Подэкран выбора: маркер на BLE |
| `19-interfaces-select-wifi.png` | interfaces-select-wifi | Подэкран выбора: маркер на WiFi |
| `20-interfaces-select-usb.png` | interfaces-select-usb | Подэкран выбора: маркер на USB |
| `21-gps-status.png` | gps-status | GPS: модуль включён, есть фикс |
| `22-gps-unavailable.png` | gps-unavailable | GPS: модуль недоступен |
| `23-beacon.png` | beacon | BEACON: режим, интервал, цель |
| `24-setup.png` | setup | SETUP: Clock (на OLED одна строка) |
| `25-shutdown.png` | shutdown | SHUTDOWN: иконка питания |
| `26-shutdown-hibernating.png` | shutdown-hibernating | SHUTDOWN: идёт выключение |
| `27-alert-advert-sent.png` | alert-advert-sent | Всплывающее подтверждение: advert отправлен |
| `28-alert-power-mode.png` | alert-power-mode | Всплывающее подтверждение: смена профиля питания |
| `29-alert-radio-profile.png` | alert-radio-profile | Всплывающее подтверждение: смена радио-профиля |
| `30-alert-polling.png` | alert-polling | Всплывающее подтверждение: опрос соседей |

Каждый PNG — 128x64 (1x) и `<имя>@4x.png` (увеличение x4 без сглаживания).

Обзорный лист: `00-contact-sheet.png`.
