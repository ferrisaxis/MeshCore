#!/usr/bin/env python3
"""
Pixel-accurate 128x64 mockups of every MeshCore companion screen and state.

Everything is taken from the firmware:
  * Latin font: 5x7 Adafruit GFX (glcdfont.c from .pio libdeps)
  * Cyrillic font: CYR_OLED face from src/helpers/ui/CyrillicGlyphs.inc
    (row-major, MSB-first, height 10, baseline 7, drawn 1px above the Latin pen)
  * header: page title left, battery icon right, page counter (or '<' on
    sub-screens) right-aligned at x=101, separator line at y=12
  * content area y = 13..63
  * alert popup: y = H/3, pad = H/32

Usage:  python3 tools/ui_mockups.py
Output: docs/ui-mockups/<NN>-<name>.png, ...@4x.png, 00-contact-sheet.png, README.md
"""
import math
import os
import re
import glob

from PIL import Image, ImageDraw, ImageFont

W, H = 128, 64
SCALE = 4
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "docs", "ui-mockups")

# ----------------------------------------------------------------- fonts
def load_glcd_font():
    path = glob.glob(os.path.join(ROOT, ".pio/libdeps/*/Adafruit GFX Library/glcdfont.c"))[0]
    src = open(path, encoding="utf-8", errors="ignore").read()
    body = src[src.index("{"):]
    nums = re.findall(r"0x([0-9a-fA-F]{2})", body)
    data = bytes(int(n, 16) for n in nums)
    return [data[i * 5:(i + 1) * 5] for i in range(256)]

def load_cyr_font():
    src = open(os.path.join(ROOT, "src/helpers/ui/CyrillicGlyphs.inc"),
               encoding="utf-8", errors="ignore").read()

    def block(name, typ):
        m = re.search(rf"{name}\[(\d+)\]\s*PROGMEM\s*=\s*\{{(.*?)\}};", src, re.S)
        nums = re.findall(r"0x([0-9a-fA-F]+)", m.group(2))
        return [int(n, 16) for n in nums]

    return {
        "h": int(re.search(r"CYR_OLED_H\s*=\s*(\d+)", src).group(1)),
        "base": int(re.search(r"CYR_OLED_BASE\s*=\s*(\d+)", src).group(1)),
        "adv": block("CYR_OLED_ADV", "u8"),
        "off": block("CYR_OLED_OFF", "u16"),
        "bits": block("CYR_OLED_BITS", "u8"),
    }

def load_logo():
    src = open(os.path.join(ROOT, "examples/companion_radio/ui-new/icons.h"),
               encoding="utf-8", errors="ignore").read()
    start = src.index("meshcore_logo")
    body = src[src.index("{", start):]
    body = body[:body.index("}")]
    return bytes(int(n, 16) for n in re.findall(r"0x([0-9a-fA-F]{2})", body))

GLYPHS = load_glcd_font()
CYR = load_cyr_font()
LOGO = load_logo()

def cyr_index(cp):
    if 0x0410 <= cp <= 0x042F:
        return cp - 0x0410
    if cp == 0x0401:
        return 32
    if 0x0430 <= cp <= 0x044F:
        return 33 + (cp - 0x0430)
    if cp == 0x0451:
        return 65
    return -1

def cyr_bit(idx, x, y):
    w = CYR["adv"][idx]
    if x >= w or y >= CYR["h"]:
        return False
    rowb = (w + 7) >> 3
    b = CYR["bits"][CYR["off"][idx] + y * rowb + (x >> 3)]
    return bool(b & (0x80 >> (x & 7)))

# ----------------------------------------------------------------- primitives
def canvas():
    img = Image.new("L", (W, H), 0)
    return img, img.load()

def px_set(px, x, y):
    if 0 <= x < W and 0 <= y < H:
        px[x, y] = 255

def draw_char(px, x, y, ch, size=1):
    code = ord(ch)
    if code > 255:
        code = ord("?")
    g = GLYPHS[code]
    for col in range(5):
        bits = g[col]
        for row in range(8):
            if bits & (1 << row):
                for dy in range(size):
                    for dx in range(size):
                        px_set(px, x + col * size + dx, y + row * size + dy)

def char_advance(ch, size=1):
    cp = ord(ch)
    if cp < 0x80:
        return 6 * size
    idx = cyr_index(cp)
    if idx < 0:
        return 6 * size
    return CYR["adv"][idx] * size

def draw_cyr(px, x, y, ch, size=1):
    idx = cyr_index(ord(ch))
    if idx < 0:
        return
    w = CYR["adv"][idx]
    for col in range(w):
        for row in range(CYR["h"]):
            if cyr_bit(idx, col, row):
                for dy in range(size):
                    for dx in range(size):
                        px_set(px, x + col * size + dx, y + row * size + dy)

def text_w(s, size=1):
    return sum(char_advance(c, size) for c in s)

def draw_text(px, x, y, s, size=1):
    """Mixed ASCII + Cyrillic. Cyrillic sits 1px above the Latin pen (baseline 7 vs 6)."""
    cx = x
    for ch in s:
        if ord(ch) < 0x80:
            draw_char(px, cx, y, ch, size)
        else:
            draw_cyr(px, cx, y - 1 * size, ch, size)
        cx += char_advance(ch, size)

def draw_centered(px, y, s, size=1):
    draw_text(px, (W - text_w(s, size)) // 2, y, s, size)

def draw_right(px, y, s, size=1, margin=1):
    draw_text(px, W - text_w(s, size) - margin, y, s, size)

def fill_rect(px, x, y, w, h, val=255):
    for i in range(x, x + w):
        for j in range(y, y + h):
            if 0 <= i < W and 0 <= j < H:
                px[i, j] = val

def rect(px, x, y, w, h):
    for i in range(x, x + w):
        px_set(px, i, y)
        px_set(px, i, y + h - 1)
    for j in range(y, y + h):
        px_set(px, x, j)
        px_set(px, x + w - 1, j)

def draw_xbm(px, x, y, data, w, h):
    bpr = (w + 7) // 8
    for row in range(h):
        for col in range(w):
            if data[row * bpr + col // 8] & (0x80 >> (col % 8)):
                px_set(px, x + col, y + row)

def ellipsize(s, max_px, size=1):
    if text_w(s, size) <= max_px:
        return s
    out = ""
    for ch in s:
        if text_w(out + ch + "..", size) > max_px:
            break
        out += ch
    return out + ".."

def wrap(s, chars):
    words, lines, cur = s.split(), [], ""
    for w in words:
        if not cur:
            cur = w
        elif len(cur) + 1 + len(w) <= chars:
            cur += " " + w
        else:
            lines.append(cur)
            cur = w
    if cur:
        lines.append(cur)
    return lines

# ----------------------------------------------------------------- chrome
def battery(px, pct=78):
    """P1: charge is a text percentage in the header, no icon."""
    draw_right(px, 1, f"{pct}%", 1, margin=2)

def header(px, title, page=None, total=None, sub=False, pct=78):
    """Codex layout: title left, counter (or '<') at x=101, battery right, rule at y=12."""
    draw_text(px, 0, 2, ellipsize(title, 64), 1)
    if sub:
        draw_right(px, 2, "<", 1, margin=W - 97 - 1)
    elif page is not None and total:
        draw_right(px, 2, f"{page}/{total}", 1, margin=W - 97 - 1)
    battery(px, pct)
    fill_rect(px, 0, 12, W, 1)

TOTAL = 5    # root screens: Главная, Сообщения, Связь, Сеть, Меню

def screen(title, page=None, sub=False, pct=78):
    img, px = canvas()
    header(px, title, page, TOTAL, sub, pct)
    return img, px

def alert(img, text):
    px = img.load()
    y, p = H // 3, H // 32
    fill_rect(px, p, y, W - p * 2, y, val=0)
    rect(px, p, y, W - p * 2, y)
    draw_centered(px, y + p * 3, text, 1)
    return img

# ----------------------------------------------------------------- screens
def s_main_connected():
    img, px = screen("FDSKY", 1)
    draw_centered(px, 20, "MSG 3/5", 2)
    draw_centered(px, 43, "192.168.1.42", 1)
    draw_centered(px, 53, "14:32   05.11", 1)
    return img

def s_main_pin():
    img, px = screen("FDSKY", 1)
    draw_centered(px, 20, "MSG 3/5", 2)
    draw_centered(px, 43, "Pin:123456", 1)   # small font when the date line is shown
    draw_centered(px, 53, "14:32   05.11", 1)
    return img

def s_main_usb():
    img, px = screen("FDSKY", 1)
    draw_centered(px, 20, "MSG 3/5", 2)
    draw_centered(px, 43, "< Connected >", 1)
    draw_centered(px, 53, "14:32   05.11", 1)
    return img

def s_splash():
    img, px = canvas()
    draw_xbm(px, 0, 3, LOGO, 128, 13)
    draw_text(px, (W - text_w("https://meshcore.io")) // 2, 22, "https://meshcore.io", 1)
    draw_centered(px, 35, "v1.18.0-0.2.0", 1)
    draw_centered(px, 48, "by Termin36", 1)
    return img

def s_msg_preview():
    img, px = canvas()
    draw_text(px, 0, 0, "MO Mytischi Mira 37", 1)
    draw_right(px, 0, "2m", 1)
    fill_rect(px, 0, 11, W, 1)
    body = "Privet! Ja na meste, vidim tebja po mesh. Kogda vyhodish?"
    for i, line in enumerate(wrap(body, 21)[:5]):
        draw_text(px, 0, 14 + i * 10, line, 1)
    return img


def s_messages():
    img, px = screen("Сообщения", 2)
    draw_text(px, 0, 20, "В памяти: 5", 1)
    draw_text(px, 0, 32, "На телефон: 3", 1)
    draw_text(px, 0, 44, "держать: читать", 1)
    return img

def s_menu(sel=0):
    items = ["Радио", "Питание", "Батарея", "Шум", "Частоты", "Недавние",
             "GPS: выкл", "Маяк", "Время", "Экран", "О прошивке", "Выключить"]
    img, px = screen("Меню", 5)
    start = 0
    if sel >= 4:
        start = sel - 3
    for r in range(4):
        idx = start + r
        if idx >= len(items):
            break
        y = 18 + r * 12
        draw_text(px, 0, y, ">" if idx == sel else " ", 1)
        draw_text(px, 10, y, items[idx], 1)
    if start + 4 < len(items):
        draw_right(px, 18 + 3 * 12, "v", 1)
    return img

def s_recent(full=True):
    img, px = screen("Недавние", sub=True)
    if full:
        for i, (nm, t) in enumerate([("MO Mytischi-4", "12s"), ("CAO Taganka", "4m"),
                                     ("Zelenograd 23", "1h"), ("MO Klyazma", "3h")]):
            y = 20 + i * 11
            draw_text(px, 0, y, ellipsize(nm, W - text_w(t) - 4), 1)
            draw_right(px, y, t, 1)
    else:
        draw_centered(px, 28, "no adverts", 1)
        draw_centered(px, 20 + 4 * 11, "В списке: 0", 1)
    return img

def s_neighbors(full=True):
    img, px = screen("Сеть", 4)
    if full:
        for i, (nm, snr) in enumerate([("MO Mytischi Mira 37", "+9"), ("FDSKY-2", "+7"),
                                       ("CAO Moscow-City", "+5"), ("TROYAN-Repeater", "+3")]):
            y = 20 + i * 11
            draw_text(px, 0, y, ellipsize(nm, W - text_w(snr) - 4), 1)
            draw_right(px, y, snr, 1)
    else:
        draw_centered(px, 28, "no neighbors", 1)
        draw_centered(px, 53, "poll: hold", 1)
    return img

def s_radio(lna=True):
    img, px = screen("Радио", sub=True)
    draw_text(px, 0, 20, "FQ: 868.731   SF: 7", 1)
    draw_text(px, 0, 31, "BW: 62.50     CR: 7", 1)
    draw_text(px, 0, 42, "TX: 10dBm  LNA: on" if lna else "TX: 10dBm", 1)
    draw_text(px, 0, 53, "LongFast", 1)
    draw_right(px, 53, "RXB: вкл", 1)
    return img

def s_power(blocked=False):
    img, px = screen("Питание", sub=True)
    draw_text(px, 0, 18, "Режим", 1)
    draw_right(px, 18, "Eco max", 1)
    draw_text(px, 0, 30, "Сон: да", 1)
    draw_right(px, 30, "RX: вкл", 1)
    draw_text(px, 0, 42, "LNA: вкл", 1)
    if blocked:
        draw_centered(px, 54, "BLE держит CPU", 1)
    return img

def s_battery():
    img, px = screen("Батарея", sub=True)
    draw_text(px, 0, 21, "Battery / ADC", 1)
    draw_text(px, 0, 32, "Voltage: 4.012 V", 1)
    draw_text(px, 0, 43, "ADC: 5.420", 1)
    draw_centered(px, 53, "calibrate: long press", 1)
    return img

def s_battery_cal(sel=0):
    img, px = screen("Батарея", sub=True)
    draw_text(px, 0, 18, "Калибровка АЦП", 1)
    items = ["Полный 4.20 В", "Мультиметр", "Сброс АЦП"]
    for i, name in enumerate(items):
        y = 30 + i * 11
        draw_text(px, 4, y, ">" if i == sel else " ", 1)
        draw_text(px, 14, y, name, 1)
    return img

def s_battery_edit():
    img, px = screen("Батарея", sub=True)
    draw_text(px, 0, 18, "Мультиметр", 1)
    draw_text(px, 0, 32, "4.150 В", 1)
    draw_text(px, 0, 46, "клик: +10 мВ", 1)
    draw_text(px, 0, 56, "держать: далее", 1)
    return img

def s_battery_confirm():
    img, px = screen("Батарея", sub=True)
    draw_text(px, 0, 18, "Подтвердить", 1)
    draw_text(px, 0, 30, "эталон 4.150 В", 1)
    draw_text(px, 4, 42, "> Отмена", 1)
    draw_text(px, 4, 54, "Применить", 1)
    return img

def s_scan():
    img, px = screen("Шум", sub=True)
    draw_centered(px, 18, "-97", 2)
    draw_right(px, 18, "PKT", 1)
    draw_text(px, 0, 36, "NL -105", 1)
    draw_right(px, 36, "pk -89", 1)
    bx, by, bw, bh = 2, 48, W - 4, 8
    rect(px, bx, by, bw, bh)
    fill_rect(px, bx + 1, by + 1, max(((-97 + 120) * bw) // 70 - 2, 1), bh - 2)
    return img

def s_fscan(state="done"):
    img, px = screen("Частоты", sub=True)
    if state == "idle":
        draw_centered(px, 28, "find quiet freq", 1)
        draw_centered(px, 53, "scan: hold", 1)
        return img
    if state == "run":
        draw_centered(px, 22, "scan 12/48", 1)
        draw_centered(px, 36, "868.731", 1)
        bw = W - 4
        rect(px, 2, 50, bw, 8)
        fill_rect(px, 3, 51, max((12 * bw) // 48 - 2, 1), 6)
        return img
    draw_text(px, 0, 18, "now 868.731", 1)
    for i, row in enumerate(["868.731  -97 *", "869.525 -104", "867.900 -106"]):
        draw_text(px, 0, 29 + i * 11, row, 1)
    return img

def s_interfaces_status(pin=True):
    img, px = screen("Связь", 3)
    rows = [("BLE", "вкл", "нет"), ("WiFi", "выкл", ""), ("USB", "вкл", "?")]
    y = 18
    for name, st, link in rows:
        draw_text(px, 0, y, name, 1)
        draw_text(px, 36, y, st, 1)
        if link:
            draw_text(px, 72, y, link, 1)
        y += 12
    draw_text(px, 0, y, "Pin:123456" if pin else "ntoas-home", 1)
    return img

def s_interfaces_select(sel):
    img, px = screen("Интерфейсы", sub=True)
    y = 18
    for i, (name, st) in enumerate([("BLE", "вкл"), ("WiFi", "выкл"), ("USB", "вкл")]):
        draw_text(px, 0, y, ">" if i == sel else " ", 1)
        draw_text(px, 10, y, name, 1)
        draw_text(px, 46, y, st, 1)
        y += 12
    draw_text(px, 0, y, "порты: --", 1)
    return img

def s_gps(available=True):
    img, px = screen("GPS", sub=True)
    draw_text(px, 0, 20, "module", 1)
    draw_right(px, 20, "on" if available else "off", 1)
    if available:
        draw_text(px, 0, 32, "fix yes", 1)
        draw_text(px, 0, 44, "sats 7", 1)
    else:
        draw_text(px, 0, 32, "Can't access GPS", 1)
    return img

def s_beacon():
    img, px = screen("Маяк", sub=True)
    draw_text(px, 0, 20, "mode", 1)
    draw_right(px, 20, "2", 1)
    draw_text(px, 0, 32, "interval", 1)
    draw_right(px, 32, "30m", 1)
    draw_text(px, 0, 44, "target", 1)
    draw_right(px, 44, "55.9165", 1)
    return img

def s_setup():
    img, px = screen("Экран", sub=True)
    draw_text(px, 10, 20, "Clock", 1)
    draw_right(px, 20, "вкл", 1)
    draw_text(px, 10, 32, "Таймаут", 1)
    draw_right(px, 32, "60 с", 1)
    draw_centered(px, 56, "держать: правка", 1)
    return img

def s_actions(title, page, items, sel=0):
    img, px = screen(title, sub=True)
    for i, name in enumerate(items[:4]):
        y = 18 + i * 12
        draw_text(px, 0, y, ">" if i == sel else " ", 1)
        draw_text(px, 10, y, name, 1)
    draw_text(px, 0, 54, "держать: выполнить", 1)
    return img

def s_about():
    img, px = screen("О прошивке", sub=True)
    draw_centered(px, 18, "v1.18.0-0.2.0", 1)
    draw_text(px, 0, 30, "сборка: Nov  6 2026", 1)
    draw_text(px, 0, 42, "узел FDSKY", 1)
    draw_text(px, 0, 54, "аптайм 1ч 12м", 1)
    return img

def s_shutdown(state="ready"):
    img, px = screen("Выключить", sub=True)
    if state == "hibernating":
        draw_centered(px, 34, "выключаю...", 1)
        return img
    draw_text(px, 0, 20, "Выключить питание?", 1)
    draw_text(px, 4, 34, "> Отмена", 1)      # safe choice, preselected
    draw_text(px, 4, 46, "Применить", 1)
    draw_text(px, 0, 56, "клик: выбор  держ: ок", 1)
    return img

def s_shutdown_apply():
    img, px = screen("Выключить", sub=True)
    draw_text(px, 0, 20, "Выключить питание?", 1)
    draw_text(px, 4, 34, "Отмена", 1)
    draw_text(px, 4, 46, "> Применить", 1)
    draw_text(px, 0, 56, "клик: выбор  держ: ок", 1)
    return img

def s_time():
    img, px = screen("Время", sub=True)
    draw_text(px, 0, 18, "14:32  Чт, 05.11", 1)
    draw_text(px, 0, 30, "пояс: +3 ч", 1)
    draw_right(px, 30, "язык: RU", 1)
    draw_text(px, 0, 42, "источник: задано", 1)
    draw_text(px, 0, 54, "держать: запрос", 1)
    return img

# ----------------------------------------------------------------- build
SCREENS = [
    ("01-root-home-wifi-ip",          "Корень 1/5: главная, Wi-Fi поднят, показан IP",         s_main_connected()),
    ("02-root-home-ble-client",       "Корень 1/5: подключён USB/BLE клиент, без Wi-Fi",      s_main_usb()),
    ("03-root-home-pin",              "Корень 1/5: подключения нет, PIN",                      s_main_pin()),
    ("04-root-messages",              "Корень 2/5: счётчики сообщений",                        s_messages()),
    ("05-root-connections",           "Корень 3/5: статусы BLE/WiFi/USB",                     s_interfaces_status()),
    ("06-root-network",               "Корень 4/5: сеть, список соседей",                      s_neighbors(True)),
    ("07-root-network-empty",         "Корень 4/5: соседей нет",                               s_neighbors(False)),
    ("08-root-menu-radio",            "Корень 5/5: меню, выбран Радио",                        s_menu(0)),
    ("09-root-menu-battery",          "Корень 5/5: меню, прокрутка до Батареи",                s_menu(2)),
    ("10-root-menu-shutdown",         "Корень 5/5: меню, прокрутка до Выключить",              s_menu(9)),
    ("11-sub-interfaces-toggle",      "Подэкран Связи: выбор интерфейса (маркер BLE)",         s_interfaces_select(0)),
    ("12-sub-interfaces-toggle-wifi", "Подэкран Связи: маркер на WiFi",                        s_interfaces_select(1)),
    ("13-sub-radio",                  "Подэкран Радио: параметры и LNA",                       s_radio(True)),
    ("14-sub-radio-no-lna",           "Подэкран Радио: плата без управляемого LNA",            s_radio(False)),
    ("14b-sub-radio-actions",         "Действия Радио: профиль, RX Boost, LNA",                s_actions("Радио", "radio",
        ["Профиль: далее", "RX Boost: выкл", "LNA: выкл"], 0)),
    ("15-sub-power",                  "Подэкран Питание: профиль и состояние сна",             s_power(False)),
    ("16-sub-power-blocked",          "Подэкран Питание: сон держит BLE",                      s_power(True)),
    ("16b-sub-power-actions",         "Действия Питание: профиль вперёд/назад",                s_actions("Питание", "power",
        ["Профиль: далее", "Профиль: назад"], 0)),
    ("17-sub-battery",                "Подэкран Батарея: напряжение и множитель АЦП",          s_battery()),
    ("18-sub-battery-calibration",    "Подэкран калибровки: выбор операции",                   s_battery_cal(0)),
    ("18b-sub-battery-meter",         "Подэкран калибровки: маркер на Мультиметре",            s_battery_cal(1)),
    ("19-sub-battery-editor",         "Калибровка по мультиметру: правка эталона",             s_battery_edit()),
    ("19b-sub-battery-confirm",       "Калибровка: подтверждение, «Отмена» по умолчанию",      s_battery_confirm()),
    ("20-sub-noise-scan",             "Подэкран Шум: уровень, пик, шкала",                     s_scan()),
    ("21-sub-freq-scan-idle",         "Подэкран Частоты: ожидание",                            s_fscan("idle")),
    ("22-sub-freq-scan-running",      "Подэкран Частоты: идёт скан",                           s_fscan("run")),
    ("23-sub-freq-scan-result",       "Подэкран Частоты: результат",                           s_fscan("done")),
    ("24-sub-recent",                 "Подэкран Недавние: список и возраст",                   s_recent(True)),
    ("25-sub-recent-empty",           "Подэкран Недавние: пусто",                              s_recent(False)),
    ("26-sub-gps",                    "Подэкран GPS: включён, есть фикс",                      s_gps(True)),
    ("27-sub-gps-unavailable",        "Подэкран GPS: модуль недоступен",                       s_gps(False)),
    ("28-sub-beacon",                 "Подэкран Маяк: режим, интервал, цель",                  s_beacon()),
    ("28b-sub-beacon-actions",        "Действия Маяк: режим, интервал, стоп",                  s_actions("Маяк", "beacon",
        ["Режим: далее", "Интервал: далее", "Остановить маяк"], 0)),
    ("29-sub-screen-time",            "Подэкран Экран: Clock и Таймаут",                       s_setup()),
    ("29b-sub-about",                 "Подэкран О прошивке: версия, сборка, узел, аптайм",     s_about()),
    ("30-sub-time",                   "Подэкран Время: часы, пояс, источник",                    s_time()),
    ("31-sub-shutdown",               "Подэкран Выключить: подтверждение, «Отмена» по умолчанию", s_shutdown("ready")),
    ("32-sub-shutdown-apply",         "Подэкран Выключить: выбрано «Применить»",                s_shutdown_apply()),
    ("33-sub-shutdown-progress",      "Подэкран Выключить: процесс пошёл",                      s_shutdown("hibernating")),
    ("34-boot-splash",                "Заставка при загрузке",                                 s_splash()),
    ("35-message-preview",            "Просмотр сообщения (без шапки)",                        s_msg_preview()),
]

ALERTS = [
    ("31-alert-advert-sent",    "Всплывающее: advert отправлен",   alert(s_recent(True).copy(), "Advert sent!")),
    ("32-alert-power-mode",     "Всплывающее: смена профиля",      alert(s_power(False).copy(), "Eco max")),
    ("33-alert-radio-profile",  "Всплывающее: смена радио-профиля", alert(s_radio(True).copy(), "LongFast")),
    ("34-alert-polling",        "Всплывающее: опрос соседей",      alert(s_neighbors(True).copy(), "Polling...")),
    ("35-alert-adc-saved",      "Всплывающее: АЦП сохранён",       alert(s_battery().copy(), "ADC saved")),
]

def save(img, name):
    img.save(os.path.join(OUT, f"{name}.png"))
    img.resize((W * SCALE, H * SCALE), Image.Resampling.NEAREST).save(
        os.path.join(OUT, f"{name}@4x.png"))

def main():
    os.makedirs(OUT, exist_ok=True)
    for f in os.listdir(OUT):
        if f.endswith(".png"):
            os.remove(os.path.join(OUT, f))

    shots = SCREENS + ALERTS
    for name, _, img in shots:
        save(img, name)

    SC, COLS = 3, 3
    CW, CH = W * SC, H * SC + 20
    rows = (len(shots) + COLS - 1) // COLS
    sheet = Image.new("L", (COLS * CW + 14 * (COLS + 1), rows * CH + 14 * (rows + 1)), 24)
    d = ImageDraw.Draw(sheet)
    try:
        font = ImageFont.truetype("DejaVuSans.ttf", 13)
    except Exception:
        font = None
    for idx, (name, _, img) in enumerate(shots):
        r, c = divmod(idx, COLS)
        x, y = 14 + c * (CW + 14), 14 + r * (CH + 14)
        sheet.paste(img.resize((CW, H * SC), Image.Resampling.NEAREST), (x, y))
        d.rectangle([x - 1, y - 1, x + CW, y + H * SC], outline=90)
        label = name.split("-", 1)[0]
        if font:
            d.text((x + 2, y + H * SC + 1), label, fill=235, font=font)
        else:
            d.text((x + 2, y + H * SC + 2), label, fill=235)
    sheet.save(os.path.join(OUT, "00-contact-sheet.png"))

    lines = ["# Макеты экранов MeshCore (128x64 OLED)", "",
             "Сгенерировано из кода прошивки: `tools/ui_mockups.py`.", "",
             "- Латинский шрифт — 5x7 из Adafruit GFX (`glcdfont.c`)",
             "- Кириллица — лицо CYR_OLED из `src/helpers/ui/CyrillicGlyphs.inc`",
             "- Шапка: название экрана слева, счётчик `3/12` (или `<` на подэкранах) у x=101,",
             "  иконка батареи справа, разделительная линия на y=12",
             "", "| Файл | Экран | Описание |", "|---|---|---|"]
    for name, desc, _ in shots:
        lines.append(f"| `{name}.png` | {name.split('-', 1)[1]} | {desc} |")
    lines += ["", "Каждый PNG — 128x64 (1x) и `<имя>@4x.png` (увеличение x4 без сглаживания).",
              "", "Обзорный лист: `00-contact-sheet.png`.", ""]
    open(os.path.join(OUT, "README.md"), "w", encoding="utf-8").write("\n".join(lines))
    print(f"{len(shots)} screens -> {OUT}")

if __name__ == "__main__":
    main()
