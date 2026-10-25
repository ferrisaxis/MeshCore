#!/usr/bin/env bash
# Heltec V4 (MeshCore) — сохранение/восстановление настроек, контактов, каналов, радио.
#
#   ./backup_restore.sh save    — снять бэкап NVS + SPIFFS с живой платы
#   ./backup_restore.sh restore — залить бэкап обратно (после прошивки)
#   ./backup_restore.sh flash   — прошить firmware-merged.bin, НЕ трогая настройки
#
# Порог: SPIFFS начинается на 0xC90000 (3.456 МБ), NVS — 0x9000 (20 КБ).
# merged-образ заканчивается ~0x1B21E0, поэтому обычная прошивка НЕ затирает настройки.
# Настройки стирает только erase-flash.

set -euo pipefail

PORT="${PORT:-/dev/ttyACM0}"
BAUD="${BAUD:-460800}"
DIR="${DIR:-$(dirname "$0")/../backup}"
BUILD="${BUILD:-$(dirname "$0")/../.pio/build/heltec_v4_oled_companion_usb_ble_wifi}"

NVS_ADDR=0x9000;    NVS_SIZE=0x5000
SPIFFS_ADDR=0xC90000; SPIFFS_SIZE=0x360000

mkdir -p "$DIR"

case "${1:-}" in
  save)
    echo ">> NVS    $NVS_ADDR / $NVS_SIZE"
    esptool --chip esp32s3 -p "$PORT" -b "$BAUD" read-flash $NVS_ADDR $NVS_SIZE "$DIR/nvs.bin"
    echo ">> SPIFFS $SPIFFS_ADDR / $SPIFFS_SIZE"
    esptool --chip esp32s3 -p "$PORT" -b "$BAUD" read-flash $SPIFFS_ADDR $SPIFFS_SIZE "$DIR/spiffs.bin"
    ls -l "$DIR"
    ;;

  restore)
    [ -f "$DIR/nvs.bin" ]    || { echo "нет $DIR/nvs.bin";    exit 1; }
    [ -f "$DIR/spiffs.bin" ] || { echo "нет $DIR/spiffs.bin"; exit 1; }
    echo ">> NVS"
    esptool --chip esp32s3 -p "$PORT" -b "$BAUD" write-flash $NVS_ADDR    "$DIR/nvs.bin"
    echo ">> SPIFFS"
    esptool --chip esp32s3 -p "$PORT" -b "$BAUD" write-flash $SPIFFS_ADDR "$DIR/spiffs.bin"
    echo ">> reset"
    esptool --chip esp32s3 -p "$PORT" -b "$BAUD" --after hard-reset run
    ;;

  flash)
    # Прошивка без стирания: настройки в SPIFFS (0xC90000) не затрагиваются.
    esptool --chip esp32s3 -p "$PORT" -b "$BAUD" \
      --before default-reset --after hard-reset \
      write-flash --flash-mode dio --flash-freq 40m --flash-size 16MB \
      0x0 "$BUILD/firmware-merged.bin"
    ;;

  *)
    echo "usage: $0 {save|restore|flash}"
    exit 1
    ;;
esac
