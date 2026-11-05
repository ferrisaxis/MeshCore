#!/usr/bin/env python3
"""
Render 128x64 OLED mockups of the MeshCore companion UI, pixel-accurate to the
firmware: same 5x7 Adafruit GFX font, same coordinates, same title bar, page
dots and battery icon.

Output: docs/ui-mockups/*.png  (1x) and *_4x.png (scaled), plus contact sheets.
"""
import os
import re
import glob
from PIL import Image, ImageDraw

W, H = 128, 64
SCALE = 4
OUT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "docs", "ui-mockups")
os.makedirs(OUT, exist_ok=True)

# ---------------------------------------------------------------- font
def load_glcd_font():
    path = glob.glob("/home/firid/MeshCore/.pio/libdeps/*/Adafruit GFX Library/glcdfont.c")[0]
    src = open(path, encoding="utf-8", errors="ignore").read()
    body = src[src.index("{"):]
    nums = re.findall(r"0x([0-9a-fA-F]{2})", body)
    data = bytes(int(n, 16) for n in nums)
    glyphs = [data[i * 5:(i + 1) * 5] for i in range(256)]
    return glyphs

GLYPHS = load_glcd_font()

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
                        xx, yy = x + col * size + dx, y + row * size + dy
                        if 0 <= xx < W and 0 <= yy < H:
                            px[xx, yy] = 255

def text_w(s, size=1):
    return len(s) * 6 * size

def draw_text(px, x, y, s, size=1):
    cx = x
    for ch in s:
        draw_char(px, cx, y, ch, size)
        cx += 6 * size

def draw_text_centered(px, y, s, size=1):
    draw_text(px, (W - text_w(s, size)) // 2, y, s, size)

def draw_text_right(px, y, s, size=1, margin=1):
    draw_text(px, W - text_w(s, size) - margin, y, s, size)

def fill_rect(px, x, y, w, h):
    for i in range(x, x + w):
        for j in range(y, y + h):
            if 0 <= i < W and 0 <= j < H:
                px[i, j] = 255

def rect(px, x, y, w, h):
    for i in range(x, x + w):
        for j in (y, y + h - 1):
            if 0 <= i < W and 0 <= j < H:
                px[i, j] = 255
    for j in range(y, y + h):
        for i in (x, x + w - 1):
            if 0 <= i < W and 0 <= j < H:
                px[i, j] = 255

def draw_xbm_box(px, x, y, w, h):
    rect(px, x, y, w, h)

def fit(s, max_chars):
    """Truncate like drawTextEllipsized does in the firmware."""
    if len(s) <= max_chars:
        return s
    if max_chars <= 2:
        return s[:max_chars]
    return s[:max_chars - 2] + ".."

# ---------------------------------------------------------------- chrome
def battery(px, pct=78):
    iw, ih = 24, 10
    ix, iy = W - iw - 5, 0
    rect(px, ix, iy, iw, ih)
    fill_rect(px, ix + iw, iy + ih // 4, 3, ih // 2)
    fill_rect(px, ix + 2, iy + 2, (pct * (iw - 4)) // 100, ih - 4)

def page_dots(px, index, total):
    y = 14
    x0 = W // 2 - 5 * (total - 1)
    for i in range(total):
        x = x0 + i * 10
        if i == index:
            fill_rect(px, x - 1, y - 1, 4, 4)
        else:
            fill_rect(px, x, y, 2, 2)

def new_screen(name="FDSKY", pct=78):
    img = Image.new("L", (W, H), 0)
    px = img.load()
    draw_text(px, 0, 2, name, 1)
    battery(px, pct)
    return img, px

# total visible pages in this build
TOTAL = 11

# ---------------------------------------------------------------- screens
def sc_first(connected=True):
    img, px = new_screen()
    page_dots(px, 0, TOTAL)
    draw_text_centered(px, 20, "MSG 3/5", 2)
    if connected:
        draw_text_centered(px, 43, "192.168.1.42", 1)
    else:
        draw_text_centered(px, 43, "Pin:123456", 2)
    draw_text_centered(px, 53, "14:32   05.11", 1)
    return img

def sc_recent():
    img, px = new_screen()
    page_dots(px, 1, TOTAL)
    rows = [("MO Mytischi-4", "12s"), ("CAO Taganka", "4m"), ("Zelenograd 23", "1h"),
            ("MO Klyazma", "3h")]
    y = 20
    for name, t in rows:
        maxc = (W - text_w(t, 1) - 1) // 6
        draw_text(px, 0, y, fit(name, maxc), 1)
        draw_text_right(px, y, t, 1)
        y += 11
    return img

def sc_neighbors():
    img, px = new_screen()
    page_dots(px, 2, TOTAL)
    rows = [("MO Mytischi Mira 37", "+9"), ("FDSKY-2", "+7"), ("CAO Moscow-City", "+5"),
            ("TROYAN-Repeater", "+3")]
    y = 20
    for name, snr in rows:
        maxc = (W - text_w(snr, 1) - 1) // 6
        draw_text(px, 0, y, fit(name, maxc), 1)
        draw_text_right(px, y, snr, 1)
        y += 11
    return img

def sc_radio(lna_supported=True):
    img, px = new_screen()
    page_dots(px, 3, TOTAL)
    draw_text(px, 0, 20, "FQ: 868.731   SF: 7", 1)
    draw_text(px, 0, 31, "BW: 62.50     CR: 7", 1)
    draw_text(px, 0, 42, "TX: 10dBm  LNA: on" if lna_supported else "TX: 10dBm", 1)
    draw_text(px, 0, 53, "LongFast", 1)
    draw_text_right(px, 53, "3x: profile", 1)
    return img

def sc_power():
    img, px = new_screen()
    page_dots(px, 4, TOTAL)
    draw_text(px, 0, 18, "Power", 1)
    draw_text_right(px, 18, "Eco max", 1)
    draw_text(px, 0, 30, "CPU on", 1)
    draw_text_right(px, 30, "RX on", 1)
    draw_text(px, 0, 42, "LNA on", 1)
    draw_text_centered(px, 54, "mode: hold", 1)
    return img

def sc_scan():
    img, px = new_screen()
    page_dots(px, 5, TOTAL)
    draw_text_centered(px, 18, "-97", 2)
    draw_text_right(px, 18, "PKT", 1)
    draw_text(px, 0, 36, "NL -105", 1)
    draw_text_right(px, 36, "pk -89", 1)
    bx, by, bw, bh = 2, 48, W - 4, 8
    rect(px, bx, by, bw, bh)
    fill = int((-97 + 120) * bw / 70)
    fill_rect(px, bx + 1, by + 1, max(fill - 2, 1), bh - 2)
    return img

def sc_fscan():
    img, px = new_screen()
    page_dots(px, 6, TOTAL)
    draw_text(px, 0, 18, "now 868.731", 1)
    for i, f in enumerate(["868.731  -97 *", "869.525 -104", "867.900 -106"]):
        draw_text(px, 0, 29 + i * 11, f, 1)
    return img

def sc_bluetooth():
    img, px = new_screen()
    page_dots(px, 7, TOTAL)
    y = 18
    for name, state in [("BLE", "ON"), ("WiFi", "OFF"), ("USB", "ON")]:
        draw_text(px, 0, y, f"{name:<6}{state}", 1)
        y += 12
    draw_text(px, 0, y, "Pin:123456", 1)
    return img

def sc_connections(sel=0):
    img, px = new_screen()
    page_dots(px, 7, TOTAL)   # same dot position: sub-screen of BLUETOOTH
    y = 18
    for i, (name, state) in enumerate([("BLE", "ON"), ("WiFi", "OFF"), ("USB", "ON")]):
        mark = ">" if i == sel else " "
        draw_text(px, 0, y, f"{mark}{name:<6}{state}", 1)
        y += 12
    return img

def sc_gps():
    img, px = new_screen()
    page_dots(px, 8, TOTAL)
    draw_text(px, 0, 20, "module", 1)
    draw_text_right(px, 20, "on", 1)
    draw_text(px, 0, 32, "fix yes", 1)
    draw_text(px, 0, 44, "sats 7", 1)
    return img

def sc_beacon():
    img, px = new_screen()
    page_dots(px, 9, TOTAL)
    draw_text(px, 0, 20, "mode", 1)
    draw_text_right(px, 20, "2", 1)
    draw_text(px, 0, 32, "interval", 1)
    draw_text_right(px, 32, "30m", 1)
    draw_text(px, 0, 44, "target", 1)
    draw_text_right(px, 44, "55.9165", 1)
    return img

def sc_setup():
    img, px = new_screen()
    page_dots(px, 10, TOTAL)
    draw_text(px, 10, 22, "Clock", 1)
    draw_text_right(px, 22, "on", 1, margin=8)
    draw_text_centered(px, 52, "hold", 1)
    return img

def sc_shutdown():
    img, px = new_screen()
    page_dots(px, 0, TOTAL)
    # simple power icon: circle with a gap + stem
    cx, cy, r = W // 2, 30, 11
    for a in range(0, 360):
        if 250 < a < 290:
            continue
        import math
        x = cx + int(r * math.cos(math.radians(a)))
        y = cy + int(r * math.sin(math.radians(a)))
        if 0 <= x < W and 0 <= y < H:
            px[x, y] = 255
    fill_rect(px, cx - 1, cy - r - 3, 2, 12)
    draw_text_centered(px, 53, "hibernate:", 1)
    draw_text_centered(px, 53, "hibernate: hold", 1)
    return img

# ---------------------------------------------------------------- output
def save(img, name):
    p1 = os.path.join(OUT, f"{name}.png")
    img.save(p1)
    img.resize((W * SCALE, H * SCALE), Image.Resampling.NEAREST).save(os.path.join(OUT, f"{name}_4x.png"))
    return p1

SCREENS = [
    ("01_first_connected", sc_first(True)),
    ("02_first_nopin_bt", sc_first(False)),
    ("03_recent", sc_recent()),
    ("04_neighbors", sc_neighbors()),
    ("05_radio", sc_radio()),
    ("06_power", sc_power()),
    ("07_scan", sc_scan()),
    ("08_fscan", sc_fscan()),
    ("09_bluetooth", sc_bluetooth()),
    ("10_connections", sc_connections(1)),
    ("11_gps", sc_gps()),
    ("12_beacon", sc_beacon()),
    ("13_setup", sc_setup()),
    ("14_shutdown", sc_shutdown()),
]

for name, img in SCREENS:
    save(img, name)

# contact sheet 2 columns x N, each cell 128x64 scaled 4x with caption strip
SC = 4
CW, CH = W * SC, H * SC + 20
COLS = 2
rows = (len(SCREENS) + COLS - 1) // COLS
sheet = Image.new("L", (COLS * CW + 14 * (COLS + 1), rows * CH + 14 * (rows + 1)), 24)
d = ImageDraw.Draw(sheet)
try:
    from PIL import ImageFont
    FONT = ImageFont.truetype("DejaVuSans.ttf", 15)
except Exception:
    FONT = None
for idx, (name, img) in enumerate(SCREENS):
    r, c = divmod(idx, COLS)
    x = 14 + c * (CW + 14)
    y = 14 + r * (CH + 14)
    sheet.paste(img.resize((CW, H * SC), Image.Resampling.NEAREST), (x, y))
    d.rectangle([x - 1, y - 1, x + CW, y + H * SC], outline=90)
    if FONT:
        d.text((x + 2, y + H * SC + 1), name, fill=230, font=FONT)
    else:
        d.text((x + 2, y + H * SC + 2), name, fill=230)
sheet.save(os.path.join(OUT, "contact_sheet.png"))
print("saved:", OUT)
for f in sorted(os.listdir(OUT)):
    print(" ", f)
