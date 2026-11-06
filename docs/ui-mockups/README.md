# Макеты экранов MeshCore (128x64 OLED)

Сгенерировано из кода прошивки: `tools/ui_mockups.py`.

- Латинский шрифт — 5x7 из Adafruit GFX (`glcdfont.c`)
- Кириллица — лицо CYR_OLED из `src/helpers/ui/CyrillicGlyphs.inc`
- Шапка: название экрана слева, счётчик `3/12` (или `<` на подэкранах) у x=101,
  иконка батареи справа, разделительная линия на y=12

| Файл | Экран | Описание |
|---|---|---|
| `01-root-home-wifi-ip.png` | root-home-wifi-ip | Корень 1/5: главная, Wi-Fi поднят, показан IP |
| `02-root-home-ble-client.png` | root-home-ble-client | Корень 1/5: подключён USB/BLE клиент, без Wi-Fi |
| `03-root-home-pin.png` | root-home-pin | Корень 1/5: подключения нет, PIN |
| `04-root-messages.png` | root-messages | Корень 2/5: счётчики сообщений |
| `05-root-connections.png` | root-connections | Корень 3/5: статусы BLE/WiFi/USB |
| `06-root-network.png` | root-network | Корень 4/5: сеть, список соседей |
| `07-root-network-empty.png` | root-network-empty | Корень 4/5: соседей нет |
| `08-root-menu-radio.png` | root-menu-radio | Корень 5/5: меню, выбран Радио |
| `09-root-menu-battery.png` | root-menu-battery | Корень 5/5: меню, прокрутка до Батареи |
| `10-root-menu-shutdown.png` | root-menu-shutdown | Корень 5/5: меню, прокрутка до Выключить |
| `11-sub-interfaces-toggle.png` | sub-interfaces-toggle | Подэкран Связи: выбор интерфейса (маркер BLE) |
| `12-sub-interfaces-toggle-wifi.png` | sub-interfaces-toggle-wifi | Подэкран Связи: маркер на WiFi |
| `13-sub-radio.png` | sub-radio | Подэкран Радио: параметры и LNA |
| `14-sub-radio-no-lna.png` | sub-radio-no-lna | Подэкран Радио: плата без управляемого LNA |
| `15-sub-power.png` | sub-power | Подэкран Питание: профиль и состояние сна |
| `16-sub-power-blocked.png` | sub-power-blocked | Подэкран Питание: сон держит BLE |
| `17-sub-battery.png` | sub-battery | Подэкран Батарея: напряжение и множитель АЦП |
| `18-sub-battery-calibration.png` | sub-battery-calibration | Подэкран калибровки: выбор операции |
| `19-sub-battery-confirm.png` | sub-battery-confirm | Подэкран калибровки: подтверждение |
| `20-sub-noise-scan.png` | sub-noise-scan | Подэкран Шум: уровень, пик, шкала |
| `21-sub-freq-scan-idle.png` | sub-freq-scan-idle | Подэкран Частоты: ожидание |
| `22-sub-freq-scan-running.png` | sub-freq-scan-running | Подэкран Частоты: идёт скан |
| `23-sub-freq-scan-result.png` | sub-freq-scan-result | Подэкран Частоты: результат |
| `24-sub-recent.png` | sub-recent | Подэкран Недавние: список и возраст |
| `25-sub-recent-empty.png` | sub-recent-empty | Подэкран Недавние: пусто |
| `26-sub-gps.png` | sub-gps | Подэкран GPS: включён, есть фикс |
| `27-sub-gps-unavailable.png` | sub-gps-unavailable | Подэкран GPS: модуль недоступен |
| `28-sub-beacon.png` | sub-beacon | Подэкран Маяк: режим, интервал, цель |
| `29-sub-screen-time.png` | sub-screen-time | Подэкран Экран: настройки дисплея |
| `30-sub-time.png` | sub-time | Подэкран Время: часы, пояс, источник |
| `31-sub-shutdown.png` | sub-shutdown | Подэкран Выключить: подтверждение, «Отмена» по умолчанию |
| `32-sub-shutdown-apply.png` | sub-shutdown-apply | Подэкран Выключить: выбрано «Применить» |
| `33-sub-shutdown-progress.png` | sub-shutdown-progress | Подэкран Выключить: процесс пошёл |
| `34-boot-splash.png` | boot-splash | Заставка при загрузке |
| `35-message-preview.png` | message-preview | Просмотр сообщения (без шапки) |
| `31-alert-advert-sent.png` | alert-advert-sent | Всплывающее: advert отправлен |
| `32-alert-power-mode.png` | alert-power-mode | Всплывающее: смена профиля |
| `33-alert-radio-profile.png` | alert-radio-profile | Всплывающее: смена радио-профиля |
| `34-alert-polling.png` | alert-polling | Всплывающее: опрос соседей |
| `35-alert-adc-saved.png` | alert-adc-saved | Всплывающее: АЦП сохранён |

Каждый PNG — 128x64 (1x) и `<имя>@4x.png` (увеличение x4 без сглаживания).

Обзорный лист: `00-contact-sheet.png`.
