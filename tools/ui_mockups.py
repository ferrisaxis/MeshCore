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
    img, px = screen("Messages", 2)
    draw_text(px, 0, 20, "In memory: 5", 1)
    draw_text(px, 0, 32, "To phone: 3", 1)
    draw_text(px, 0, 44, "hold: read", 1)
    return img

def s_menu(sel=0):
    items = ["Radio", "Power", "Battery", "Noise", "Freqs", "Recent", "Sensors",
             "GPS: off", "Beacon", "Time", "Display", "Firmware", "Power off"]
    img, px = screen("Menu", 5)
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

def s_sensors(rows=None):
    img, px = screen("Sensors", sub=True)
    if rows is None:
        rows = [("vbat", "3.87 V"), ("temp", "21.4 C"), ("pressure", "1013 hPa")]
    if not rows:
        draw_centered(px, 30, "no sensors", 1)
    for i, (n, v) in enumerate(rows[:3]):
        draw_text(px, 0, 18 + i * 12, n, 1)
        draw_right(px, 18 + i * 12, v, 1)
    draw_text(px, 0, 54, "hold: refresh", 1)
    return img

def s_recent(full=True):
    img, px = screen("Recent", sub=True)
    if full:
        for i, (nm, t) in enumerate([("MO Mytischi-4", "12s"), ("CAO Taganka", "4m"),
                                     ("Zelenograd 23", "1h"), ("MO Klyazma", "3h")]):
            y = 20 + i * 11
            draw_text(px, 0, y, ellipsize(nm, W - text_w(t) - 4), 1)
            draw_right(px, y, t, 1)
    else:
        draw_centered(px, 28, "no adverts", 1)
        draw_centered(px, 20 + 4 * 11, "In list: 0", 1)
    return img

def s_neighbors(full=True):
    img, px = screen("Network", 4)
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
    img, px = screen("Radio", sub=True)
    draw_text(px, 0, 20, "FQ: 868.731   SF: 7", 1)
    draw_text(px, 0, 31, "BW: 62.50     CR: 7", 1)
    draw_text(px, 0, 42, "TX: 10dBm  LNA: on" if lna else "TX: 10dBm", 1)
    draw_text(px, 0, 53, "LongFast", 1)
    draw_right(px, 53, "RXB: on", 1)
    return img

def s_power(blocked=False):
    img, px = screen("Power", sub=True)
    draw_text(px, 0, 18, "Mode", 1)
    draw_right(px, 18, "Eco max", 1)
    draw_text(px, 0, 30, "Sleep: yes", 1)
    draw_right(px, 30, "RX: on", 1)
    draw_text(px, 0, 42, "LNA: on", 1)
    if blocked:
        draw_centered(px, 54, "BLE holds CPU", 1)
    return img

def s_battery():
    img, px = screen("Battery", sub=True)
    draw_text(px, 0, 21, "Battery / ADC", 1)
    draw_text(px, 0, 32, "Voltage: 4.012 V", 1)
    draw_text(px, 0, 43, "ADC: 5.420", 1)
    draw_centered(px, 53, "calibrate: long press", 1)
    return img

def s_battery_cal(sel=0):
    img, px = screen("Battery", sub=True)
    draw_text(px, 0, 18, "ADC calibration", 1)
    items = ["Full 4.20 V", "Multimeter", "Reset ADC"]
    for i, name in enumerate(items):
        y = 30 + i * 11
        draw_text(px, 4, y, ">" if i == sel else " ", 1)
        draw_text(px, 14, y, name, 1)
    return img

def s_battery_edit():
    img, px = screen("Battery", sub=True)
    draw_text(px, 0, 18, "Multimeter", 1)
    draw_text(px, 0, 32, "4.150 V", 1)
    draw_text(px, 0, 46, "click: +10 mV", 1)
    draw_text(px, 0, 56, "hold: next", 1)
    return img

def s_battery_confirm():
    img, px = screen("Battery", sub=True)
    draw_text(px, 0, 18, "Confirm", 1)
    draw_text(px, 0, 30, "ref 4.150 V", 1)
    draw_text(px, 4, 42, "> Cancel", 1)
    draw_text(px, 4, 54, "Apply", 1)
    return img

def s_scan():
    img, px = screen("Noise", sub=True)
    draw_centered(px, 18, "-97", 2)
    draw_right(px, 18, "PKT", 1)
    draw_text(px, 0, 36, "NL -105", 1)
    draw_right(px, 36, "pk -89", 1)
    bx, by, bw, bh = 2, 48, W - 4, 8
    rect(px, bx, by, bw, bh)
    fill_rect(px, bx + 1, by + 1, max(((-97 + 120) * bw) // 70 - 2, 1), bh - 2)
    return img

def s_fscan(state="done"):
    img, px = screen("Freqs", sub=True)
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
    img, px = screen("Links", 3)
    rows = [("BLE", "on", "no"), ("WiFi", "off", ""), ("USB", "on", "?")]
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
    img, px = screen("Interfaces", sub=True)
    y = 18
    for i, (name, st) in enumerate([("BLE", "on"), ("WiFi", "off"), ("USB", "on")]):
        draw_text(px, 0, y, ">" if i == sel else " ", 1)
        draw_text(px, 10, y, name, 1)
        draw_text(px, 46, y, st, 1)
        y += 12
    draw_text(px, 0, y, "ports: --", 1)
    return img

def s_gps(available=True):
    img, px = screen("GPS", sub=True)
    draw_text(px, 0, 20, "module", 1)
    draw_right(px, 20, "on" if available else "off", 1)
    if available:
        draw_text(px, 0, 32, "fix: yes", 1)
        draw_right(px, 32, "7 sats", 1)
        draw_text(px, 0, 44, "pos", 1)
        draw_right(px, 44, "55.9165 37.7601", 1)
        draw_text(px, 0, 56, "alt", 1)
        draw_right(px, 56, "156.20", 1)
    else:
        draw_text(px, 0, 32, "powered off", 1)
    return img

def s_beacon():
    img, px = screen("Beacon", sub=True)
    draw_text(px, 0, 18, "group", 1)
    draw_right(px, 18, "30m", 1)
    draw_text(px, 0, 30, "FDSKY-Base", 1)
    draw_text(px, 0, 42, "55.9165  37.7601", 1)
    draw_text(px, 0, 54, "sent 4m ago", 1)
    return img

def s_beacon_no_fix():
    img, px = screen("Beacon", sub=True)
    draw_text(px, 0, 18, "group", 1)
    draw_right(px, 18, "30m", 1)
    draw_text(px, 0, 30, "FDSKY-Base", 1)
    draw_text(px, 0, 42, "no fix", 1)
    draw_text(px, 0, 54, "not sent", 1)
    return img

def s_setup():
    img, px = screen("Display", sub=True)
    draw_text(px, 10, 20, "Clock", 1)
    draw_right(px, 20, "on", 1)
    draw_text(px, 10, 32, "Language", 1)
    draw_right(px, 32, "RU", 1)
    draw_text(px, 10, 44, "Timeout", 1)
    draw_right(px, 44, "60 s", 1)
    draw_centered(px, 56, "hold: edit", 1)
    return img

def s_clock_nodes(names):
    img, px = screen("Nodes", sub=True)
    if not names:
        draw_text(px, 0, 22, "no nodes set", 1)
        draw_text(px, 0, 40, "hold: menu", 1)
        return img
    for i, n in enumerate(names[:3]):
        draw_text(px, 0, 18 + i * 12, n, 1)
    if len(names) > 3:
        draw_right(px, 54, f"+{len(names) - 3} more", 1)
    draw_text(px, 0, 54, "hold: menu", 1)
    return img

def s_node_picker(names, sel=0):
    img, px = screen("Pick node", sub=True)
    total = len(names)
    maxrows = 3
    start = sel
    if start + maxrows > total:
        start = total - maxrows if total > maxrows else 0
    rows = names[start:start + maxrows]
    for r, n in enumerate(rows):
        idx = start + r
        is_sel = idx == sel
        if is_sel:
            draw_text(px, 0, 18 + r * 12, ">", 1)
        draw_text(px, 10, 18 + r * 12, n[:19], 1)
    draw_text(px, 0, 54, "hold: add", 1)
    draw_right(px, 54, f"{sel + 1}/{total}", 1)
    return img

def s_actions(title, page, items, sel=0):
    img, px = screen(title, sub=True)
    for i, name in enumerate(items[:4]):
        y = 18 + i * 12
        draw_text(px, 0, y, ">" if i == sel else " ", 1)
        draw_text(px, 10, y, name, 1)
    draw_text(px, 0, 54, "hold: run", 1)
    return img

def s_about():
    img, px = screen("Firmware", sub=True)
    draw_centered(px, 18, "v1.18.0-0.2.0", 1)
    draw_text(px, 0, 30, "build: Nov  6 2026", 1)
    draw_text(px, 0, 42, "node FDSKY", 1)
    draw_text(px, 0, 54, "uptime 1h 12m", 1)
    return img

def s_shutdown(state="ready"):
    img, px = screen("Power off", sub=True)
    if state == "hibernating":
        draw_centered(px, 34, "powering off", 1)
        return img
    draw_text(px, 0, 20, "Power off?", 1)
    draw_text(px, 4, 34, "> Cancel", 1)      # safe choice, preselected
    draw_text(px, 4, 46, "Apply", 1)
    draw_text(px, 0, 56, "click: pick  hold: ok", 1)
    return img

def s_shutdown_apply():
    img, px = screen("Power off", sub=True)
    draw_text(px, 0, 20, "Power off?", 1)
    draw_text(px, 4, 34, "Cancel", 1)
    draw_text(px, 4, 46, "> Apply", 1)
    draw_text(px, 0, 56, "click: pick  hold: ok", 1)
    return img

def s_time():
    img, px = screen("Time", sub=True)
    draw_text(px, 0, 18, "14:32  Thu, 05.11", 1)
    draw_text(px, 0, 30, "tz: +3 h", 1)
    draw_right(px, 30, "lang: RU", 1)
    draw_text(px, 0, 42, "source: set", 1)
    draw_text(px, 0, 54, "hold: menu", 1)
    return img

# ----------------------------------------------------------------- build
SCREENS = [
    ("01-root-home-wifi-ip",          "Root 1/5: home, Wi-Fi up, IP shown",         s_main_connected()),
    ("02-root-home-ble-client",       "Root 1/5: USB/BLE client attached, no Wi-Fi",      s_main_usb()),
    ("03-root-home-pin",              "Root 1/5: no connection, PIN",                      s_main_pin()),
    ("04-root-messages",              "Root 2/5: message counters",                        s_messages()),
    ("05-root-connections",           "Root 3/5: BLE/WiFi/USB status",                     s_interfaces_status()),
    ("06-root-network",               "Root 4/5: network, neighbor list",                      s_neighbors(True)),
    ("07-root-network-empty",         "Root 4/5: no neighbors",                               s_neighbors(False)),
    ("08-root-menu-radio",            "Root 5/5: menu, Radio selected",                        s_menu(0)),
    ("09-root-menu-battery",          "Root 5/5: menu scrolled to Battery",                s_menu(2)),
    ("10-root-menu-shutdown",         "Root 5/5: menu scrolled to Power off",              s_menu(9)),
    ("11-sub-interfaces-toggle",      "Links: interface selection (BLE)",         s_interfaces_select(0)),
    ("12-sub-interfaces-toggle-wifi", "Links: marker on WiFi",                        s_interfaces_select(1)),
    ("13-sub-radio",                  "Radio: parameters and LNA",                       s_radio(True)),
    ("14-sub-radio-no-lna",           "Radio: board without a controllable LNA",            s_radio(False)),
    ("14b-sub-radio-actions",         "Radio actions: profile, RX Boost, LNA",                s_actions("Radio", "radio",
        ["Profile: next", "RX Boost: off", "LNA: off"], 0)),
    ("15-sub-power",                  "Power: profile and sleep state",             s_power(False)),
    ("16-sub-power-blocked",          "Power: sleep blocked by BLE",                      s_power(True)),
    ("16b-sub-power-actions",         "Power actions: profile forward/back",                s_actions("Power", "power",
        ["Profile: next", "Profile: back"], 0)),
    ("17-sub-battery",                "Battery: voltage and ADC multiplier",          s_battery()),
    ("18-sub-battery-calibration",    "ADC calibration: operation list",                   s_battery_cal(0)),
    ("18b-sub-battery-meter",         "ADC calibration: Multimeter highlighted",            s_battery_cal(1)),
    ("19-sub-battery-editor",         "Multimeter calibration: reference editor",             s_battery_edit()),
    ("19b-sub-battery-confirm",       "Calibration: confirmation, Cancel preselected",      s_battery_confirm()),
    ("20-sub-noise-scan",             "Noise: level, peak, bar",                     s_scan()),
    ("21-sub-freq-scan-idle",         "Freq scan: idle",                            s_fscan("idle")),
    ("22-sub-freq-scan-running",      "Freq scan: running",                           s_fscan("run")),
    ("23-sub-freq-scan-result",       "Freq scan: result",                           s_fscan("done")),
    ("24-sub-recent",                 "Recent: list with age",                   s_recent(True)),
    ("25-sub-recent-empty",           "Recent: empty",                              s_recent(False)),
    ("26-sub-gps",                    "GPS: on, fix acquired",                      s_gps(True)),
    ("27-sub-gps-unavailable",        "GPS: module not responding",                       s_gps(False)),
    ("28-sub-beacon",                 "Beacon: mode, interval, target",                  s_beacon()),
    ("25a-sub-sensors",              "Sensors: voltage, temperature, pressure",              s_sensors()),
    ("25b-sub-sensors-empty",        "Sensors: no data",                                   s_sensors([])),
    ("29c-sub-clock-nodes",          "Time nodes: two trusted nodes",                    s_clock_nodes(["FDSKY-Base", "Tower-2"])),
    ("29d-sub-clock-nodes-empty",    "Time nodes: none set",                              s_clock_nodes([])),
    ("29e-sub-node-picker",          "Contact picker for a time node",                      s_node_picker(["FDSKY-Base", "Tower-2", "Home-rep", "Neighbor-3"], 1)),
    ("29f-sub-time-actions",         "Time actions: request and nodes",                        s_actions("Time", "time",
        ["Request time", "Time nodes"], 0)),
    ("28c-sub-beacon-no-fix",         "Beacon: no GPS fix",                                  s_beacon_no_fix()),
    ("28b-sub-beacon-actions",        "Beacon actions: mode, interval, stop",                  s_actions("Beacon", "beacon",
        ["Mode: next", "Interval: next", "Stop beacon"], 0)),
    ("29-sub-screen-time",            "Display: Clock and Timeout",                       s_setup()),
    ("29b-sub-about",                 "Firmware: version, build, node, uptime",     s_about()),
    ("30-sub-time",                   "Time: clock, timezone, source",                    s_time()),
    ("31-sub-shutdown",               "Power off: confirmation, Cancel preselected", s_shutdown("ready")),
    ("32-sub-shutdown-apply",         "Power off: Apply selected",                s_shutdown_apply()),
    ("33-sub-shutdown-progress",      "Power off: in progress",                      s_shutdown("hibernating")),
    ("34-boot-splash",                "Boot splash",                                 s_splash()),
    ("35-message-preview",            "Message preview (no header)",                        s_msg_preview()),
]

ALERTS = [
    ("31-alert-advert-sent",    "Alert: advert sent",   alert(s_recent(True).copy(), "Advert sent!")),
    ("32-alert-power-mode",     "Alert: profile changed",      alert(s_power(False).copy(), "Eco max")),
    ("33-alert-radio-profile",  "Alert: radio profile changed", alert(s_radio(True).copy(), "LongFast")),
    ("34-alert-polling",        "Alert: neighbor poll",      alert(s_neighbors(True).copy(), "Polling...")),
    ("35-alert-adc-saved",      "Alert: ADC saved",       alert(s_battery().copy(), "ADC saved")),
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

    lines = ["# MeshCore screen mockups (128x64 OLED)", "",
             "Generated from the firmware code: `tools/ui_mockups.py`.", "",
             "- Latin font: 5x7 from Adafruit GFX (`glcdfont.c`)",
             "- Cyrillic glyphs come from `src/helpers/ui/CyrillicGlyphs.inc` (messages only)",
             "- Header: screen title on the left, counter `3/12` (or `<` on sub-screens) at x=101,",
             "  battery indicator on the right, rule at y=12",
             "", "| File | Screen | Description |", "|---|---|---|"]
    for name, desc, _ in shots:
        lines.append(f"| `{name}.png` | {name.split('-', 1)[1]} | {desc} |")
    lines += ["", "Each PNG is 128x64 (1x) plus `<name>@4x.png` (4x, no smoothing).",
              "", "Overview sheet: `00-contact-sheet.png`.", ""]
    open(os.path.join(OUT, "README.md"), "w", encoding="utf-8").write("\n".join(lines))
    print(f"{len(shots)} screens -> {OUT}")

if __name__ == "__main__":
    main()
