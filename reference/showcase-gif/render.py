"""Renders the frame descriptors from a shot script to PNGs (supersampled, then
downscaled).

Run: python3 render.py tour.pkl frames/"""
import os
import pickle
import sys
from multiprocessing import Pool

import numpy as np
from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageFont

SS = 2
PKL = sys.argv[1] if len(sys.argv) > 1 else "tour.pkl"
OUT_DIR = sys.argv[2] if len(sys.argv) > 2 else "frames"
DATA = pickle.load(open(PKL, "rb"))
OUT_W, OUT_H = DATA["out"]
U = DATA["U"]
RW, RH = OUT_W * SS, OUT_H * SS
K = U * SS  # render px per visual unit

MONO = "/usr/share/fonts/TTF/JetBrainsMonoNerdFontMono-Regular.ttf"
MONO_B = "/usr/share/fonts/TTF/JetBrainsMonoNerdFontMono-Bold.ttf"
SANS = "/usr/share/fonts/noto/NotoSans-Regular.ttf"
SANS_B = "/usr/share/fonts/noto/NotoSans-Bold.ttf"

BORDER = 2 * K
ROUND = 10 * K
TERM_BG = (14, 16, 23, 255)
INACTIVE = np.array([62, 67, 82], np.float32)
ACT1 = np.array([51, 204, 255], np.float32)  # Hyprland's default cyan …
ACT2 = np.array([178, 108, 255], np.float32)  # … into violet

C = dict(fg=(201, 209, 217), dim=(110, 118, 129), green=(126, 231, 135), blue=(121, 192, 255),
         yellow=(227, 179, 65), magenta=(210, 168, 255), cyan=(86, 212, 221), red=(255, 123, 114))

# Each terminal: lines of (colour, text) segments. The last line holds the prompt cursor.
P = [("green", "~"), ("blue", " $ ")]
CONTENTS = [
    [P + [("fg", "ls")], [("blue", "docs  examples  src  tests  "), ("fg", "README.md  GLOSSARY.md")], P],
    [P + [("fg", "git log --oneline -3")], [("yellow", "cb91ecb "), ("fg", "README: diagrams of every setting")],
     [("yellow", "8185cff "), ("fg", "Merge pull request #53")], [("yellow", "61194f6 "), ("fg", "Drop compiled Python files")], P],
    [P + [("fg", "uptime")], [("fg", " 10:42:07 up 3 days,  2:14,  load 0.31")], P],
    [P + [("fg", "echo $XDG_SESSION_TYPE")], [("fg", "wayland")], P],
    [P + [("fg", "cd src/core && ls")], [("cyan", "DividerStore.cpp  RowHeights.cpp  Settings.cpp  Workspace.cpp")], P],
    [P + [("fg", "date")], [("fg", "Wed Nov  4 10:42:31 CET 2026")], P],
    [P + [("fg", "hyprctl version | head -1")], [("magenta", "Hyprland 0.57.1")], P],
    [P + [("fg", "cat notes.md")], [("dim", "# today")], [("fg", "- ultrawide, three zones")], [("fg", "- holes stay holes")], P],
    [P + [("fg", "ls ~/Pictures")], [("blue", "wallpapers  screenshots")], P],
    [P + [("fg", "echo $SHELL")], [("fg", "/bin/sh")], P],
    [P + [("fg", "nproc")], [("fg", "22")], P],
    [P + [("fg", "ping -c1 localhost")], [("dim", "64 bytes from ::1: icmp_seq=1 time=0.04 ms")], P],
    [P + [("fg", "df -h /")], [("dim", "Filesystem  Size  Used Avail Use%")], [("fg", "/dev/nvme0  1.8T  612G  1.2T  34%")], P],
    [P + [("fg", "echo hello")], [("fg", "hello")], P],
]

_cache = {}


def font(path, size):
    k = (path, size)
    if k not in _cache:
        _cache[k] = ImageFont.truetype(path, size)
    return _cache[k]


FS = round(10.5 * K)
LH = round(FS * 1.45)
PAD = round(9 * K)


def content_img(i):
    k = ("content", i)
    if k in _cache:
        return _cache[k]
    f = font(MONO, FS)
    lines = CONTENTS[i % len(CONTENTS)]
    img = Image.new("RGBA", (RW, RH), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    cw = f.getlength("M")
    cur = None
    for n, line in enumerate(lines):
        x = 0
        for col, txt in line:
            d.text((x, n * LH), txt, font=f, fill=C[col] + (255,))
            x += cw * len(txt)
        cur = (x, n * LH)
    _cache[k] = (img, cur, cw)
    return _cache[k]


def rounded_mask(w, h, r, inset=0.0):
    m = Image.new("L", (w, h), 0)
    ImageDraw.Draw(m).rounded_rectangle((inset, inset, w - 1 - inset, h - 1 - inset), radius=max(0, r - inset), fill=255)
    return m


def gradient(w, h, f):
    gx = np.linspace(0, 1, w, dtype=np.float32)[None, :]
    gy = np.linspace(0, 1, h, dtype=np.float32)[:, None]
    # 45 degrees in screen space, independent of the tile's aspect
    t = (gx * w + (1 - gy) * h) / (w + h)
    act = ACT1[None, None, :] * (1 - t[..., None]) + ACT2[None, None, :] * t[..., None]
    col = INACTIVE[None, None, :] * (1 - f) + act * f
    return Image.fromarray(np.clip(col, 0, 255).astype(np.uint8), "RGB")


def composite(canvas, img, x, y):
    """alpha_composite that tolerates negative / off-canvas positions."""
    w, h = img.size
    x0, y0 = max(0, x), max(0, y)
    x1, y1 = min(canvas.width, x + w), min(canvas.height, y + h)
    if x1 <= x0 or y1 <= y0:
        return
    canvas.alpha_composite(img, dest=(x0, y0), source=(x0 - x, y0 - y, x1 - x, y1 - y))


def draw_tile(canvas, w, t):
    x, y, ww, hh = [v * SS for v in w["rect"]]
    W, H = round(ww), round(hh)
    if W < 12 or H < 12 or w["alpha"] <= 0.01:
        return
    tile = Image.new("RGBA", (W, H), TERM_BG)
    img, cur, cw = content_img(w["content"])
    tile.alpha_composite(img, dest=(PAD, PAD), source=(0, 0, max(1, W - PAD), max(1, H - PAD)))
    d = ImageDraw.Draw(tile)
    cx, cy = PAD + cur[0], PAD + cur[1] + round(FS * 0.12)
    if cx + cw < W - BORDER and cy + FS * 1.15 < H - BORDER:
        box = (cx, cy, cx + cw, cy + FS * 1.18)
        if w["active"]:
            if (t % 1.0) < 0.6:
                d.rectangle(box, fill=(201, 209, 217, 255))
        else:
            d.rectangle(box, outline=(110, 118, 129, 255), width=max(1, round(K)))
    outer = rounded_mask(W, H, ROUND)
    inner = rounded_mask(W, H, ROUND, BORDER)
    ring = ImageChops.subtract(outer, inner)
    tile.paste(gradient(W, H, w["focus"]), (0, 0), ring)
    tile.putalpha(outer)
    s = w["scale"]
    X, Y = x, y
    if abs(s - 1) > 1e-3:
        sw, sh = max(1, round(W * s)), max(1, round(H * s))
        tile = tile.resize((sw, sh), Image.BILINEAR)
        X, Y = x + (W - sw) / 2, y + (H - sh) / 2
    if w["badge"]:
        draw_badge(tile, w["badge"])
    a = w["alpha"] * (0.9 if w["dragged"] else 1.0)
    if a < 0.999:
        al = tile.getchannel("A").point(lambda v: int(v * a))
        tile.putalpha(al)
    if w["dragged"] or w["floating"]:
        sh = Image.new("RGBA", (tile.width + 40 * SS, tile.height + 40 * SS), (0, 0, 0, 0))
        sm = Image.new("L", sh.size, 0)
        sm.paste(tile.getchannel("A").point(lambda v: int(v * 0.55)), (20 * SS, 26 * SS))
        sm = sm.filter(ImageFilter.GaussianBlur(9 * SS))
        sh.putalpha(sm)
        composite(canvas, sh, round(X) - 20 * SS, round(Y) - 20 * SS)
    composite(canvas, tile, round(X), round(Y))


def draw_badge(tile, text):
    """A big order number in the middle of the tile."""
    f = font(SANS_B, round(min(tile.width, tile.height) * 0.42))
    d = ImageDraw.Draw(tile)
    d.text((tile.width / 2, tile.height / 2), text, font=f, fill=(240, 244, 250, 200), anchor="mm")


def draw_tag(canvas, tg):
    """A value label: a small pill centred on `pos`, amber when at a limit."""
    f = font(MONO_B, round(11 * K))
    text = tg["text"]
    padx, h = round(9 * K), round(22 * K)
    w = round(f.getlength(text) + 2 * padx)
    warn = tg.get("warn")
    edge = (255, 196, 92, 255) if warn else (255, 255, 255, 40)
    img = pill((w, h), h // 2, (10, 12, 19, 225), edge)
    ImageDraw.Draw(img).text((w / 2, h / 2), text, font=f, fill=(255, 205, 110, 255) if warn else (236, 240, 247, 255),
                             anchor="mm")
    margin = round(10 * K)  # kept on screen, even over a zone narrower than the label
    x = min(max(tg["pos"][0] * SS - w / 2, margin), RW - margin - w)
    y = tg["pos"][1] * SS - h / 2
    composite(canvas, with_alpha(img, tg["alpha"]), round(x), round(y))


def draw_indicator(canvas, ind):
    x, y, ww, hh = [v * SS for v in ind["rect"]]
    W, H = round(ww), round(hh)
    if W < 8 or H < 8:
        return
    g = gradient(W, H, 1.0)
    outer = rounded_mask(W, H, ROUND)
    inner = rounded_mask(W, H, ROUND, BORDER)
    ring = ImageChops.subtract(outer, inner)
    a = ind["alpha"]
    mask = ImageChops.add(ring, inner.point(lambda v: int(v * 0.2)))  # border + 20 % fill
    mask = mask.point(lambda v: int(v * a))
    img = g.convert("RGBA")
    img.putalpha(mask)
    composite(canvas, img, round(x), round(y))


def pill(size, r, fill, outline=None):
    img = Image.new("RGBA", size, (0, 0, 0, 0))
    ImageDraw.Draw(img).rounded_rectangle((0, 0, size[0] - 1, size[1] - 1), radius=r, fill=fill,
                                          outline=outline, width=max(1, round(K)))
    return img


def with_alpha(img, a):
    if a >= 0.999:
        return img
    img = img.copy()
    img.putalpha(img.getchannel("A").point(lambda v: int(v * a)))
    return img


def draw_caption(canvas, c):
    ft = font(SANS_B, round(15 * K))
    fs = font(SANS, round(11.5 * K))
    fc = font(MONO, round(10.5 * K))
    pad_x, pad_y = round(14 * K), round(10 * K)
    dot = round(8 * K)
    tw = ft.getlength(c["title"])
    sub, sf = (c["code"], fc) if c["code"] else (c["sub"], fs)
    sw = sf.getlength(sub) if sub else 0
    w = round(pad_x * 2 + dot + 9 * K + max(tw, sw))
    h = round(pad_y * 2 + 15 * K * 1.25 + (14.5 * K if sub else 0))
    x, y = round(18 * K), RH - round(18 * K) - h
    if c["box_a"] > 0.01:
        box = pill((w, h), round(10 * K), (10, 12, 19, 215), (255, 255, 255, 26))
        composite(canvas, with_alpha(box, c["box_a"]), x, y)
    layer = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    tx = pad_x + dot + 9 * K
    if c["title_a"] > 0.01:
        lay = Image.new("RGBA", (w, h), (0, 0, 0, 0))
        dd = ImageDraw.Draw(lay)
        dy = pad_y + 15 * K * 0.62
        g = gradient(dot, dot, 1.0).convert("RGBA")
        g.putalpha(rounded_mask(dot, dot, dot // 2))
        lay.alpha_composite(g, dest=(pad_x, round(dy - dot / 2)))
        dd.text((tx, pad_y), c["title"], font=ft, fill=(240, 244, 250, 255))
        composite(canvas, with_alpha(lay, c["title_a"]), x, y)
    if sub and c["sub_a"] > 0.01:
        col = (121, 192, 255, 255) if c["code"] else (160, 168, 182, 255)
        d.text((tx, pad_y + 15 * K * 1.3), sub, font=sf, fill=col)
        composite(canvas, with_alpha(layer, c["sub_a"]), x, y)


def draw_keycap(canvas, k):
    f = font(MONO_B, round(12 * K * k["pop"]))
    fp = font(SANS_B, round(12 * K * k["pop"]))
    kh = round(26 * K * k["pop"])
    padx = round(9 * K)
    gap = round(7 * K)
    widths = [round(f.getlength(s) + 2 * padx) for s in k["keys"]]
    plus = fp.getlength("+")
    total = sum(widths) + (len(widths) - 1) * (plus + 2 * gap)
    W = round(total + 2 * gap * 2)
    H = kh + round(14 * K)
    layer = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    bg = pill((W, H), round(10 * K), (10, 12, 19, 215), (255, 255, 255, 26))
    layer.alpha_composite(bg)
    d = ImageDraw.Draw(layer)
    x = gap * 2
    y0 = (H - kh) // 2
    for n, (s, kw) in enumerate(zip(k["keys"], widths)):
        d.rounded_rectangle((x, y0 + 2 * K, x + kw, y0 + kh), radius=round(6 * K), fill=(6, 7, 11, 255))
        top = (40, 46, 62, 255) if not k["held"] else (36, 60, 84, 255)
        d.rounded_rectangle((x, y0, x + kw, y0 + kh - 2 * K), radius=round(6 * K), fill=top,
                            outline=(92, 104, 130, 255) if not k["held"] else (51, 204, 255, 255), width=max(1, round(K)))
        d.text((x + kw / 2, y0 + (kh - 2 * K) / 2), s, font=f, fill=(236, 240, 247, 255), anchor="mm")
        x += kw
        if n < len(widths) - 1:
            d.text((x + gap + plus / 2, H / 2), "+", font=fp, fill=(130, 138, 152, 255), anchor="mm")
            x += plus + 2 * gap
    composite(canvas, with_alpha(layer, k["alpha"]), round(RW / 2 - W / 2), RH - round(18 * K) - H)


ARROW = [(0, 0), (0, 17), (4.2, 13.2), (7.2, 19.8), (9.8, 18.7), (6.9, 12.3), (12.3, 12.3)]


def draw_cursor(canvas, c):
    x, y = c["pos"][0] * SS, c["pos"][1] * SS
    s = 1.15 * K
    layer = Image.new("RGBA", (round(60 * K), round(60 * K)), (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    o = 20 * K
    if c["pressed"]:
        r = 11 * K
        col = (51, 204, 255) if c["pressed"] == "left" else (178, 108, 255)
        d.ellipse((o - r, o - r, o + r, o + r), fill=col + (70,), outline=col + (230,), width=round(1.5 * K))
    pts = [(o + px * s, o + py * s) for px, py in ARROW]
    shadow = Image.new("L", layer.size, 0)
    ImageDraw.Draw(shadow).polygon([(p[0] + 1.5 * K, p[1] + 2 * K) for p in pts], fill=110)
    shadow = shadow.filter(ImageFilter.GaussianBlur(2 * K))
    sh = Image.new("RGBA", layer.size, (0, 0, 0, 0))
    sh.putalpha(shadow)
    layer = Image.alpha_composite(sh, layer)
    d = ImageDraw.Draw(layer)
    d.polygon(pts, fill=(255, 255, 255, 255), outline=(10, 10, 14, 255), width=max(1, round(1.2 * K)))
    composite(canvas, with_alpha(layer, c["alpha"]), round(x - o), round(y - o))


def draw_title(canvas, tc):
    f1 = font(SANS_B, round(46 * K))
    f2 = font(SANS, round(15 * K))
    layer = Image.new("RGBA", (RW, RH), (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    d.text((RW / 2, RH * 0.45), tc["name"], font=f1, fill=(240, 244, 250, 255), anchor="mm")
    d.text((RW / 2, RH * 0.45 + 40 * K), tc["sub"],
           font=f2, fill=(150, 160, 178, 255), anchor="mm")
    bw, bh = round(150 * K), round(3 * K)
    g = gradient(bw, bh, 1.0).convert("RGBA")
    g.putalpha(rounded_mask(bw, bh, bh // 2))
    layer.alpha_composite(g, dest=(round(RW / 2 - bw / 2), round(RH * 0.45 + 25 * K)))
    composite(canvas, with_alpha(layer, tc["alpha"]), 0, 0)


def background():
    yy, xx = np.mgrid[0:RH, 0:RW].astype(np.float32)
    xn, yn = xx / RW, yy / RH
    base = np.array([9, 11, 17], np.float32) * (1 - xn[..., None] * 0.4) + np.array([20, 14, 34], np.float32) * (xn[..., None] * 0.4)
    def glow(cx, cy, rx, ry, col, k):
        d2 = ((xn - cx) / rx) ** 2 + ((yn - cy) / ry) ** 2
        return np.exp(-d2)[..., None] * np.array(col, np.float32) * k
    img = base + glow(0.82, 0.1, 0.35, 0.9, (90, 50, 160), 0.35) + glow(0.15, 0.95, 0.35, 0.8, (20, 110, 140), 0.3)
    return Image.fromarray(np.clip(img, 0, 255).astype(np.uint8), "RGB").convert("RGBA")


BG = None


def render(n):
    global BG
    if BG is None:
        BG = background()
    fr = DATA["frames"][n]
    canvas = BG.copy()
    for w in fr["wins"]:
        if not w["dragged"] and not w["floating"]:
            draw_tile(canvas, w, fr["t"])
    for tg in fr["tags"]:
        draw_tag(canvas, tg)
    for w in fr["wins"]:
        if w["floating"] and not w["dragged"]:
            draw_tile(canvas, w, fr["t"])
    if fr["indicator"]:
        draw_indicator(canvas, fr["indicator"])
    for w in fr["wins"]:
        if w["dragged"]:
            draw_tile(canvas, w, fr["t"])
    if fr["title"]:
        draw_title(canvas, fr["title"])
    for c in fr["captions"]:
        draw_caption(canvas, c)
    if fr["keycap"]:
        draw_keycap(canvas, fr["keycap"])
    if fr["cursor"]:
        draw_cursor(canvas, fr["cursor"])
    out = canvas.convert("RGB").resize((OUT_W, OUT_H), Image.LANCZOS)
    out.save(os.path.join(OUT_DIR, f"{n:05d}.png"), compress_level=1)


if __name__ == "__main__":
    os.makedirs(OUT_DIR, exist_ok=True)
    which = range(len(DATA["frames"]))
    if len(sys.argv) > 3:
        which = [int(a) for a in sys.argv[3:]]
    with Pool(20) as p:
        p.map(render, which, chunksize=8)
    print("rendered", len(which))
