"""Timeline engine: drives the model and records one plain-data descriptor per
frame. A shot script (tour.py, settings.py) imports it, fills `spaces`, runs
its shots and calls save()."""
import pickle
import sys

OUT_W = int(sys.argv[1]) if len(sys.argv) > 1 else 1600
OUT_H = OUT_W * 1440 // 5120
FPS = 25  # GIF delays are in 1/100 s: 4 cs per frame
DT = 1 / FPS
U = OUT_W / 1600  # visual unit: 1 at 1600 px wide

GOUT, GIN = 7 * U, 3.5 * U  # gaps (output px), inner gap is per side
AREA = (GOUT - GIN, GOUT - GIN, OUT_W - 2 * (GOUT - GIN), OUT_H - 2 * (GOUT - GIN))
ANIM = 0.42


# ---------------------------------------------------------------- easing
def out_quint(p):
    return 1 - (1 - p) ** 5


def in_out_cubic(p):
    return 4 * p * p * p if p < 0.5 else 1 - (-2 * p + 2) ** 3 / 2


def out_back(p, k=1.4):
    p -= 1
    return 1 + (k + 1) * p ** 3 + k * p ** 2


def lerp(a, b, k):
    if isinstance(a, tuple):
        return tuple(x + (y - x) * k for x, y in zip(a, b))
    return a + (b - a) * k


class Tween:
    def __init__(s, v):
        s.a = s.b = v
        s.t0, s.d, s.e = 0.0, 0.0, out_quint

    def get(s, t):
        if s.d <= 0 or t >= s.t0 + s.d:
            return s.b
        return lerp(s.a, s.b, s.e(max(0.0, (t - s.t0) / s.d)))

    def to(s, v, t, d, e=out_quint):
        s.a, s.b, s.t0, s.d, s.e = s.get(t), v, t, d, e

    def set(s, v):
        s.a = s.b = v
        s.d = 0


# ---------------------------------------------------------------- geometry
def px(r):
    x, y, w, h = r
    nx, ny = AREA[0] + x * AREA[2], AREA[1] + y * AREA[3]
    return (nx + GIN, ny + GIN, w * AREA[2] - 2 * GIN, h * AREA[3] - 2 * GIN)


def unpx(r):
    """px() undone: a window's drawn rect back to area fractions."""
    x, y, w, h = r
    return ((x - GIN - AREA[0]) / AREA[2], (y - GIN - AREA[1]) / AREA[3], (w + 2 * GIN) / AREA[2], (h + 2 * GIN) / AREA[3])


def frac(p):
    return ((p[0] - AREA[0]) / AREA[2], (p[1] - AREA[1]) / AREA[3])


def at_frac(fx, fy):
    return (AREA[0] + fx * AREA[2], AREA[1] + fy * AREA[3])


# ---------------------------------------------------------------- state
T = 0.0
frames = []
wins = {}
spaces = {}
focused = None
cursor = Tween((OUT_W * 0.5, OUT_H * 0.55))
cursor_alpha = Tween(0.0)
pressed = None  # "left" / "right"
indicator = Tween((0, 0, 0, 0))
indicator_alpha = Tween(0.0)
dragged = None
captions = []  # dicts: title, sub, code, t0, t1, keep_title
keycaps = []  # dicts: keys, t0, t1
title_card = []  # (t0, t1, title, sub)
tag_fn = None  # () -> [dict(text, pos, warn)]: value labels drawn over the tiles
tag_alpha = Tween(0.0)
global_fade = Tween(1.0)


class Win:
    def __init__(s, i, ws, content):
        s.i, s.ws, s.content = i, ws, content
        s.rect = Tween((0, 0, 0, 0))
        s.alpha = Tween(0.0)
        s.scale = Tween(1.0)
        s.focus = Tween(0.0)
        s.gone_at = None
        s.floating = False
        s.badge = None


class Space:
    def __init__(s, name, model):
        s.name, s.model = name, model
        s.offset = Tween(0.0)
        s.visible = False


def step():
    global T
    frames.append(snapshot(T))
    T += DT


def hold(sec):
    for _ in range(max(1, round(sec * FPS))):
        step()


def retile(sp, dur=ANIM, skip=()):
    for i, r in sp.model.rects().items():
        if i in skip:
            continue
        wins[i].rect.to(px(r), T, dur)


def set_focus(i):
    global focused
    focused = i
    for w in wins.values():
        w.focus.to(1.0 if w.i == i else 0.0, T, 0.18)


content_counter = [0]


FLOAT_SIZE = (0.26, 0.56)  # a new floating window's own size, as area fractions


def open_win(sp, i, badge=None):
    """Opens window i on sp. With no room it floats, centred, at its own size."""
    z = sp.model.add(i)
    w = Win(i, sp.name, content_counter[0])
    content_counter[0] += 1
    w.badge = badge
    wins[i] = w
    if z:
        w.rect.set(px(sp.model.rect_of(i)))
    else:
        w.floating = True
        fw, fh = FLOAT_SIZE
        w.rect.set(px(((1 - fw) / 2, (1 - fh) / 2, fw, fh)))
    w.alpha.to(1.0, T, 0.28)
    w.scale.set(0.82)
    w.scale.to(1.0, T, 0.45, out_quint)
    retile(sp, skip=(i,))
    set_focus(i)


def close_win(sp, i, refocus=None):
    w = wins[i]
    w.alpha.to(0.0, T, 0.24)
    w.scale.to(0.85, T, 0.26)
    w.gone_at = T + 0.3
    if not w.floating:
        sp.model.remove(i)
    retile(sp)
    set_focus(refocus)


def title(name, sub, dur=1.7):
    title_card.append((T, T + dur, name, sub))
    hold(dur + 0.1)


def caption(title, sub="", code=""):
    keep = bool(captions) and captions[-1]["title"] == title and captions[-1]["t1"] is None
    if captions and captions[-1]["t1"] is None:
        captions[-1]["t1"] = T
        captions[-1]["keep_next"] = keep
    captions.append(dict(title=title, sub=sub, code=code, t0=T, t1=None, keep_title=keep, keep_next=False))


def end_caption():
    if captions and captions[-1]["t1"] is None:
        captions[-1]["t1"] = T


def key(*keys, dur=0.75):
    for k in keycaps:
        if k["t1"] is None or k["t1"] > T:
            k["t1"] = T
            k["cut"] = True
    keycaps.append(dict(keys=list(keys), t0=T, t1=T + dur if dur else None, cut=False))


def key_up():
    for k in keycaps:
        if k["t1"] is None:
            k["t1"] = T


def move_cursor(to, dur, each=None, follow=True, e=in_out_cubic):
    cursor.to(to, T, dur, e)
    for _ in range(max(1, round(dur * FPS))):
        if each:
            each(cursor.get(T))
        elif follow:
            follow_focus(cursor.get(T))
        step()
    if each:
        each(cursor.get(T))


def current_space():
    return next(sp for sp in spaces.values() if sp.visible and sp.offset.b == 0)


def follow_focus(p):
    sp = current_space()
    i = sp.model.window_at(*frac(p))
    if i is not None and i != focused:
        set_focus(i)


def switch_space(frm, to):
    a, b = spaces[frm], spaces[to]
    b.visible = True
    b.offset.set(OUT_W + 0.0)
    a.offset.to(-OUT_W - 0.0, T, 0.55, in_out_cubic)
    b.offset.to(0.0, T, 0.55, in_out_cubic)
    set_focus(None)
    hold(0.55)
    a.visible = False


# ---------------------------------------------------------------- snapshot
def snapshot(t):
    out = dict(t=t, wins=[], indicator=None, captions=[], keycap=None, cursor=None, title=None)
    order = []
    for sp in spaces.values():
        if not sp.visible:
            continue
        off = sp.offset.get(t)
        for w in wins.values():
            if w.ws != sp.name or (w.gone_at is not None and t >= w.gone_at):
                continue
            order.append(((w.i == dragged) * 2 + w.floating, off, w))
    for _, off, w in sorted(order, key=lambda o: o[0]):
        x, y, ww, hh = w.rect.get(t)
        out["wins"].append(dict(rect=(x + off, y, ww, hh), alpha=w.alpha.get(t), scale=w.scale.get(t),
                                focus=w.focus.get(t), content=w.content, active=(w.i == focused),
                                dragged=(w.i == dragged), floating=w.floating, badge=w.badge))
    ia = indicator_alpha.get(t)
    if ia > 0.01:
        out["indicator"] = dict(rect=indicator.get(t), alpha=ia)
    for c in captions:
        t1 = c["t1"]
        a_in = min(1.0, max(0.0, (t - c["t0"] - 0.12) / 0.25))
        a_out = 1.0 if t1 is None else min(1.0, max(0.0, 1 - (t - t1) / 0.18))
        if t < c["t0"] or a_out <= 0:
            continue
        if c["keep_next"] and t1 is not None and t >= t1:
            continue  # the next caption, with the same title, draws the box and title
        # A title shared with the caption before stays; otherwise it fades in and out.
        title_a = (1.0 if c["keep_title"] else a_in) * (1.0 if c["keep_next"] else a_out)
        out["captions"].append(dict(title=c["title"], sub=c["sub"], code=c["code"], title_a=title_a,
                                    sub_a=a_in * a_out, box_a=title_a))
    for k in reversed(keycaps):
        if t < k["t0"]:
            continue
        t1 = k["t1"]
        if t1 is not None and t >= t1 + (0.0 if k.get("cut") else 0.25):
            continue
        age = t - k["t0"]
        a = min(1.0, age / 0.08)
        if t1 is not None and t > t1:
            a *= max(0.0, 1 - (t - t1) / 0.25)
        pop = 1 + 0.12 * max(0.0, 1 - age / 0.16)
        out["keycap"] = dict(keys=k["keys"], alpha=a, pop=pop, held=(t1 is None or t < t1) and age > 0.1)
        break
    ca = cursor_alpha.get(t)
    if ca > 0.01:
        out["cursor"] = dict(pos=cursor.get(t), alpha=ca, pressed=pressed)
    for (t0, t1, name, sub) in title_card:
        if t0 <= t <= t1:
            a = min(1.0, (t - t0) / 0.35, (t1 - t) / 0.35)
            out["title"] = dict(alpha=max(0.0, a), name=name, sub=sub)
    out["tags"] = []
    ta = tag_alpha.get(t)
    if tag_fn and ta > 0.01:
        out["tags"] = [dict(tg, alpha=ta) for tg in tag_fn()]
    out["fade"] = global_fade.get(t)
    return out




# ---------------------------------------------------------------- helpers for shots
def fill(sp, ids, gap, badges=False):
    """Opens windows one by one, `gap` seconds apart; badges number them."""
    for n, i in enumerate(ids, 1):
        open_win(sp, i, badge=str(n) if badges else None)
        hold(gap)


def shown_rect(i):
    """Window i's rect as drawn this frame, mid-animation included, in area fractions."""
    return unpx(wins[i].rect.get(T))


def show_tags(fn):
    global tag_fn
    tag_fn = fn
    tag_alpha.to(1.0, T, 0.25)


def hide_tags():
    tag_alpha.to(0.0, T, 0.25)


def mouse_drag(sp, start, path, update, button="right"):
    """Presses at `start`, follows `path` [(point, seconds)], calling
    update(dx, dy) with the cursor's move in area fractions on every frame."""
    global pressed
    move_cursor(start, 0.6)
    hold(0.15)
    pressed = button
    key("SUPER", button + "-drag", dur=None)
    f0 = frac(start)

    def each(p):
        f = frac(p)
        update(f[0] - f0[0], f[1] - f0[1])
        retile(sp, dur=0.12)

    for p, d in path:
        move_cursor(p, d, each=each)
    pressed = None
    key_up()


def dnd(sp, i, path):
    """SUPER+left-drags window i along `path` (grab point, then (point, seconds)
    pairs) with the drop indicator showing, and drops it."""
    global dragged, pressed
    grab = path[0]
    move_cursor(grab, 0.6)
    hold(0.15)
    pressed = "left"
    key("SUPER", "left-drag", dur=None)
    dragged = i
    set_focus(i)
    w = wins[i]
    x, y, ww, hh = w.rect.get(T)
    off = (grab[0] - x, grab[1] - y)

    def each(p):
        w.rect.set((p[0] - off[0], p[1] - off[1], ww, hh))
        r = sp.model.drop_indicator(i, *frac(p))
        if r is None:
            indicator_alpha.to(0.0, T, 0.12)
        else:
            target = px(r)
            if indicator_alpha.b < 0.5:
                indicator.set(target)
            elif indicator.b != target:
                indicator.to(target, T, 0.16)
            indicator_alpha.to(1.0, T, 0.12)

    for p, d in path[1:]:
        move_cursor(p, d, each=each)
        hold(0.05)
    hold(0.35)
    sp.model.apply_drop(i, *frac(cursor.get(T)))
    pressed = None
    key_up()
    dragged = None
    indicator_alpha.to(0.0, T, 0.18)
    retile(sp)
    set_focus(i)


def fade_out(name, dur=0.5):
    """Everything on workspace `name` fades back to the empty desktop."""
    end_caption()
    hide_tags()
    for w in wins.values():
        if w.ws == name:
            w.alpha.to(0.0, T, dur, in_out_cubic)
            w.scale.to(0.94, T, dur, in_out_cubic)
    hold(dur + 0.25)


def save(path):
    with open(path, "wb") as f:
        pickle.dump(dict(frames=frames, out=(OUT_W, OUT_H), fps=FPS, U=U), f)
    print(path, len(frames), "frames", round(T, 2), "s")
