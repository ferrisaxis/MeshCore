#!/usr/bin/env python3
"""
Pixel-accurate 128x64 mockups of every MeshCore companion screen and state.

Rendering rules are taken from the firmware itself:
  * 5x7 Adafruit GFX font (parsed from the real glcdfont.c in .pio libdeps)
  * 12px title bar: node name left, 24x10 battery icon right
  * page dots at y=14 (2x2 px, active 4x4 px)
  * content area y = 18..63
  * alert popup: y=H/3, pad=H/32, box 124x21, text at y+6
  * meshcore splash logo 128x13 parsed from ui-new/icons.h

Usage:  python3 tools/ui_mockups.py
Output: docs/ui-mockups/<NN>-<name>.png  (1x, 128x64)
        docs/ui-mockups/<NN>-<name>@4x.png
        docs/ui-mockups/00-contact-sheet.png
        docs/ui-mockups/README.md
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

# ----------------------------------------------------------------- font
def load_glcd_font():
    path = glob.glob(os.path.join(ROOT, ".pio/libdeps/*/Adafruit GFX Library/glcdfont.c"))[0]
    src = open(path, encoding="utf-8", errors="ignore").read()
    body = src[src.index("{"):]
    nums = re.findall(r"0x([0-9a-fA-F]{2})", body)
    data = bytes(int(n, 16) for n in nums)
    return [data[i * 5:(i + 1) * 5] for i in range(256)]

GLYPHS = load_glcd_font()

def load_logo():
    src = open(os.path.join(ROOT, "examples/companion_radio/ui-new/icons.h"),
               encoding="utf-8", errors="ignore").read()
    start = src.index("meshcore_logo")
    body = src[src.index("{", start):]
    body = body[:body.index("}")]
    nums = re.findall(r"0x([0-9a-fA-F]{2})", body)
    return bytes(int(n, 16) for n in nums)

LOGO = load_logo()

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

def text_w(s, size=1):
    return len(s) * 6 * size

def draw_text(px, x, y, s, size=1):
    cx = x
    for ch in s:
        draw_char(px, cx, y, ch, size)
        cx += 6 * size

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
    bytes_per_row = (w + 7) // 8
    for row in range(h):
        for col in range(w):
            byte = data[row * bytes_per_row + col // 8]
            if byte & (0x80 >> (col % 8)):
                px_set(px, x + col, y + row)

def ellipsize(s, max_chars):
    if len(s) <= max_chars:
        return s
    return s[:max_chars - 2] + ".."

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
def battery(px, pct=78, charging=False):
    iw, ih = 24, 10
    ix, iy = W - iw - 5, 0
    rect(px, ix, iy, iw, ih)
    fill_rect(px, ix + iw, iy + ih // 4, 3, ih // 2)
    fill_rect(px, ix + 2, iy + 2, (pct * (iw - 4)) // 100, ih - 4)
    if charging:               # 8x8 bolt just left of the battery
        bx, by = ix - 9, iy + 1
        for i, col in enumerate([0b00011000, 0b00110000, 0b01100000, 0b11111110,
                                 0b00011000, 0b00110000, 0b01100000, 0b11000000]):
            for bit in range(8):
                if col & (0x80 >> bit):
                    px_set(px, bx + bit, by + i)

def page_dots(px, index, total):
    y = 14
    x0 = W // 2 - 5 * (total - 1)
    for i in range(total):
        x = x0 + i * 10
        if i == index:
            fill_rect(px, x - 1, y - 1, 4, 4)
        else:
            fill_rect(px, x, y, 2, 2)

TOTAL = 11   # visible pages in this build

def screen(name=None, dot=None, pct=78, charging=False):
    img, px = canvas()
    if name:
        draw_text(px, 0, 2, name, 1)
        battery(px, pct, charging)
    if dot is not None:
        page_dots(px, dot, TOTAL)
    return img, px

def alert(img, text):
    """Overlay the firmware alert popup on top of a rendered screen."""
    px = img.load()
    y = H // 3          # 21
    p = H // 32         # 2
    fill_rect(px, p, y, W - p * 2, y, val=0)      # popup background = black
    rect(px, p, y, W - p * 2, y)
    draw_centered(px, y + p * 3, text, 1)
    return img

# ----------------------------------------------------------------- screens
NODE = "FDSKY"

def s_main_connected():
    img, px = screen(NODE, 0)
    draw_centered(px, 20, "MSG 3/5", 2)
    draw_centered(px, 43, "192.168.1.42", 1)
    draw_centered(px, 53, "14:32   05.11", 1)
    return img

def s_main_pin():
    img, px = screen(NODE, 0)
    draw_centered(px, 20, "MSG 3/5", 2)
    draw_centered(px, 43, "Pin:123456", 2)
    draw_centered(px, 53, "14:32   05.11", 1)
    return img

def s_splash():
    img, px = canvas()
    draw_xbm(px, (W - 128) // 2, 3, LOGO, 128, 13)
    draw_text(px, (W - text_w("https://meshcore.io")) // 2, 22, "https://meshcore.io", 1)
    draw_centered(px, 35, "v1.18.0-0.2.0", 1)
    draw_centered(px, 48, "by Termin36", 1)
    return img

def s_msg_preview():
    img, px = canvas()
    age = "2m"
    draw_text(px, 0, 0, "MO Mytischi Mira 37", 1)
    draw_right(px, 0, age, 1)
    fill_rect(px, 0, 11, W, 1)
    body = "Privet! Ja na meste, vidim tebja po mesh. Kogda vyhodish?"
    for i, line in enumerate(wrap(body, 21)[:5]):
        draw_text(px, 0, 14 + i * 10, line, 1)
    return img

def s_recent(full=True):
    img, px = screen(NODE, 1)
    if full:
        for i, (nm, t) in enumerate([("MO Mytischi-4", "12s"), ("CAO Taganka", "4m"),
                                     ("Zelenograd 23", "1h"), ("MO Klyazma", "3h")]):
            y = 20 + i * 11
            draw_text(px, 0, y, ellipsize(nm, (W - text_w(t) - 1) // 6), 1)
            draw_right(px, y, t, 1)
    else:
        draw_centered(px, 28, "no adverts", 1)
        draw_centered(px, 20 + 4 * 11, "advert: hold", 1)
    return img

def s_neighbors(full=True):
    img, px = screen(NODE, 2)
    if full:
        for i, (nm, snr) in enumerate([("MO Mytischi Mira 37", "+9"), ("FDSKY-2", "+7"),
                                       ("CAO Moscow-City", "+5"), ("TROYAN-Repeater", "+3")]):
            y = 20 + i * 11
            draw_text(px, 0, y, ellipsize(nm, (W - text_w(snr) - 1) // 6), 1)
            draw_right(px, y, snr, 1)
    else:
        draw_centered(px, 28, "no neighbors", 1)
        draw_centered(px, 53, "poll: hold", 1)
    return img

def s_radio(lna=True):
    img, px = screen(NODE, 3)
    draw_text(px, 0, 20, "FQ: 868.731   SF: 7", 1)
    draw_text(px, 0, 31, "BW: 62.50     CR: 7", 1)
    draw_text(px, 0, 42, "TX: 10dBm  LNA: on" if lna else "TX: 10dBm", 1)
    draw_text(px, 0, 53, "LongFast", 1)
    draw_right(px, 53, "3x: profile", 1)
    return img

def s_power(blocked=False):
    img, px = screen(NODE, 4)
    draw_text(px, 0, 18, "Power", 1)
    draw_right(px, 18, "Eco max", 1)
    draw_text(px, 0, 30, "CPU on", 1)
    draw_right(px, 30, "RX on", 1)
    draw_text(px, 0, 42, "LNA on", 1)
    draw_centered(px, 54, "BLE needs CPU on" if blocked else "mode: hold", 1)
    return img

def s_scan():
    img, px = screen(NODE, 5)
    draw_centered(px, 18, "-97", 2)
    draw_right(px, 18, "PKT", 1)
    draw_text(px, 0, 36, "NL -105", 1)
    draw_right(px, 36, "pk -89", 1)
    bx, by, bw, bh = 2, 48, W - 4, 8
    rect(px, bx, by, bw, bh)
    fill_rect(px, bx + 1, by + 1, max(((-97 + 120) * bw) // 70 - 2, 1), bh - 2)
    return img

def s_fscan(state="done"):
    img, px = screen(NODE, 6)
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

def s_interfaces_status():
    img, px = screen(NODE, 7)
    y = 18
    for name, st in [("BLE", "ON"), ("WiFi", "OFF"), ("USB", "ON")]:
        draw_text(px, 0, y, f"{name:<6}{st}", 1)
        y += 12
    draw_text(px, 0, y, "Pin:123456", 1)
    return img

def s_interfaces_select(sel):
    img, px = screen(NODE, 7)
    y = 18
    for i, (name, st) in enumerate([("BLE", "ON"), ("WiFi", "OFF"), ("USB", "ON")]):
        mark = ">" if i == sel else " "
        draw_text(px, 0, y, f"{mark}{name:<6}{st}", 1)
        y += 12
    return img

def s_gps(available=True):
    img, px = screen(NODE, 8)
    draw_text(px, 0, 20, "module", 1)
    draw_right(px, 20, "on" if available else "off", 1)
    if available:
        draw_text(px, 0, 32, "fix yes", 1)
        draw_text(px, 0, 44, "sats 7", 1)
    else:
        draw_text(px, 0, 32, "Can't access GPS", 1)
    return img

def s_beacon():
    img, px = screen(NODE, 9)
    draw_text(px, 0, 20, "mode", 1)
    draw_right(px, 20, "2", 1)
    draw_text(px, 0, 32, "interval", 1)
    draw_right(px, 32, "30m", 1)
    draw_text(px, 0, 44, "target", 1)
    draw_right(px, 44, "55.9165", 1)
    return img

def s_setup():
    img, px = screen(NODE, 10)
    draw_text(px, 10, 22, "Clock", 1)
    draw_right(px, 22, "on", 1, margin=8)
    draw_centered(px, 52, "hold", 1)
    return img

def s_shutdown(state="ready"):
    img, px = screen(NODE, 0)
    if state == "hibernating":
        draw_centered(px, 34, "hibernating...", 1)
        return img
    cx, cy, r = W // 2, 30, 11
    for a in range(0, 360):
        if 250 < a < 290:
            continue
        px_set(px, cx + int(r * math.cos(math.radians(a))), cy + int(r * math.sin(math.radians(a))))
    fill_rect(px, cx - 1, cy - r - 3, 2, 12)
    draw_centered(px, 53, "hibernate: hold", 1)
    return img

# ----------------------------------------------------------------- build
SCREENS = [
    ("01-main-screen-connected",        "Главный экран: телефон подключён, показан IP",            s_main_connected()),
    ("02-main-screen-pin",              "Главный экран: подключения нет, показан PIN",              s_main_pin()),
    ("03-boot-splash",                  "Заставка при загрузке: лого, версия",                       s_splash()),
    ("04-message-preview",              "Превью входящего сообщения (долгое нажатие на главном)",   s_msg_preview()),
    ("05-recent-nodes",                 "RECENT: недавно слышанные ноды и возраст",                 s_recent(True)),
    ("06-recent-nodes-empty",           "RECENT: список пуст, подсказка advert",                    s_recent(False)),
    ("07-neighbors-snr",                "NEIGHBORS: соседи и SNR",                                  s_neighbors(True)),
    ("08-neighbors-empty",              "NEIGHBORS: пусто, подсказка poll",                         s_neighbors(False)),
    ("09-radio-settings-lna",           "RADIO: частота/SF/BW/CR, мощность, LNA доступен",          s_radio(True)),
    ("10-radio-settings-no-lna",        "RADIO: плата без управляемого LNA",                        s_radio(False)),
    ("11-power-profile",                "POWER: профиль питания и подсказка",                       s_power(False)),
    ("12-power-profile-blocked",        "POWER: профиль упирается в ресурс (BLE держит CPU)",       s_power(True)),
    ("13-noise-scan",                   "SCAN: уровень шума, пик, шкала",                           s_scan()),
    ("14-freq-scan-idle",               "FSCAN: ожидание запуска",                                  s_fscan("idle")),
    ("15-freq-scan-running",            "FSCAN: идёт сканирование, прогресс",                       s_fscan("run")),
    ("16-freq-scan-result",             "FSCAN: результат, топ-3 частоты",                          s_fscan("done")),
    ("17-interfaces-status",            "Экран интерфейсов: BLE/WiFi/USB ON-OFF + PIN",             s_interfaces_status()),
    ("18-interfaces-select-ble",        "Подэкран выбора: маркер на BLE",                           s_interfaces_select(0)),
    ("19-interfaces-select-wifi",       "Подэкран выбора: маркер на WiFi",                          s_interfaces_select(1)),
    ("20-interfaces-select-usb",        "Подэкран выбора: маркер на USB",                           s_interfaces_select(2)),
    ("21-gps-status",                   "GPS: модуль включён, есть фикс",                           s_gps(True)),
    ("22-gps-unavailable",              "GPS: модуль недоступен",                                   s_gps(False)),
    ("23-beacon",                       "BEACON: режим, интервал, цель",                            s_beacon()),
    ("24-setup",                        "SETUP: Clock (на OLED одна строка)",                       s_setup()),
    ("25-shutdown",                     "SHUTDOWN: иконка питания",                                 s_shutdown("ready")),
    ("26-shutdown-hibernating",         "SHUTDOWN: идёт выключение",                                s_shutdown("hibernating")),
]

# alert popups, rendered as overlays on their parent screen
ALERTS = [
    ("27-alert-advert-sent", "Всплывающее подтверждение: advert отправлен",
     alert(s_recent(True).copy(), "Advert sent!")),
    ("28-alert-power-mode", "Всплывающее подтверждение: смена профиля питания",
     alert(s_power(False).copy(), "Eco max")),
    ("29-alert-radio-profile", "Всплывающее подтверждение: смена радио-профиля",
     alert(s_radio(True).copy(), "LongFast")),
    ("30-alert-polling", "Всплывающее подтверждение: опрос соседей",
     alert(s_neighbors(True).copy(), "Polling...")),
]

def save(img, name):
    img.save(os.path.join(OUT, f"{name}.png"))
    img.resize((W * SCALE, H * SCALE), Image.Resampling.NEAREST).save(
        os.path.join(OUT, f"{name}@4x.png"))

def main():
    os.makedirs(OUT, exist_ok=True)
    # drop previously generated files so stale names do not linger
    for f in os.listdir(OUT):
        if f.endswith(".png"):
            os.remove(os.path.join(OUT, f))

    all_shots = [(n, d, i) for n, d, i in SCREENS] + [(n, d, i) for n, d, i in ALERTS]
    for name, _, img in all_shots:
        save(img, name)

    # contact sheet
    SC = 3
    CW, CH = W * SC, H * SC + 20
    COLS = 3
    rows = (len(all_shots) + COLS - 1) // COLS
    sheet = Image.new("L", (COLS * CW + 14 * (COLS + 1), rows * CH + 14 * (rows + 1)), 24)
    d = ImageDraw.Draw(sheet)
    try:
        font = ImageFont.truetype("DejaVuSans.ttf", 13)
    except Exception:
        font = None
    for idx, (name, _, img) in enumerate(all_shots):
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

    # README index
    lines = ["# Макеты экранов MeshCore (128x64 OLED)", "",
             "Сгенерировано из кода прошивки: `tools/ui_mockups.py`.", "",
             "Шрифт — настоящий 5x7 из Adafruit GFX (glcdfont.c), координаты — как в `UITask.cpp`.", "",
             "| Файл | Экран | Описание |", "|---|---|---|"]
    for name, desc, _ in all_shots:
        lines.append(f"| `{name}.png` | {name.split('-',1)[1]} | {desc} |")
    lines += ["", "Каждый PNG — 128x64 (1x) и `<имя>@4x.png` (увеличение x4 без сглаживания).",
              "", "Обзорный лист: `00-contact-sheet.png`.", ""]
    open(os.path.join(OUT, "README.md"), "w", encoding="utf-8").write("\n".join(lines))

    print(f"{len(all_shots)} screens -> {OUT}")

if __name__ == "__main__":
    main()
