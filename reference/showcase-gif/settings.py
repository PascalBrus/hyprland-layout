"""GIF 2, in the README's Settings section: every setting the tour doesn't
show, one at a time, with live value labels. Writes settings.pkl."""
import timeline as tl
from model import MIN_MASTER, MIN_ROW, MIN_WIDTH, STEP, WS
from timeline import (Space, at_frac, caption, cursor, cursor_alpha, dnd, fade_out, fill, hide_tags, hold, key,
                      mouse_drag, open_win, retile, set_focus, show_tags, shown_rect, spaces, switch_space, title)

ws1 = WS()
ws2 = WS(left_width=0.15, right_width=0.35)
ws3 = WS(left_rows=2, right_rows=2, fill_first="right")
ws4 = WS(left_rows=None, right_rows=None)
ws5 = WS(left_rows=None, right_rows=0)
for n, ws in enumerate((ws1, ws2, ws3, ws4, ws5), 1):
    spaces[str(n)] = Space(str(n), ws)
spaces["1"].visible = True
sp1, sp2 = spaces["1"], spaces["2"]


def at_min(v, shown, lo):
    """At the limit, once the tiles have caught up with the model."""
    return abs(v - lo) < 1e-6 and abs(shown - v) < 0.005


def width_tags(ws):
    """Each zone's width, as drawn this frame. Amber at min_width / min_master_width."""
    def fn():
        out = []
        for z, name, lo, limit in (("L", "left_width", MIN_WIDTH, "min_width"),
                                   ("M", "Master", MIN_MASTER, "min_master_width"),
                                   ("R", "right_width", MIN_WIDTH, "min_width")):
            if not ws.exists(z):
                continue
            x, _, w, _ = ws.zone_rect(z)
            # Read the width off a window in the zone, so the label moves with the tiles.
            i = ws.master if z == "M" else (ws.win[z][0] if ws.win[z] else None)
            mx, mw = (x, w) if i is None else shown_rect(i)[0::2]
            warn = at_min(w, mw, lo)
            x, w = mx, mw
            out.append(dict(text=f"{name} {w:.2f}" + (f" = {limit}" if warn else ""),
                            pos=at_frac(x + w / 2, 0.4), warn=warn))
        return out
    return fn


def row_tags(ws, z):
    """Each Row's height in zone z. Amber at min_row_height."""
    def fn():
        out = []
        for r in range(len(ws.win[z])):
            h_model = ws.row_rect(z, r)[3]
            x, y, w, h = shown_rect(ws.win[z][r])
            warn = at_min(h_model, h, MIN_ROW)
            out.append(dict(text=f"{h:.2f}" + (" = min_row_height" if warn else ""),
                            pos=at_frac(x + w / 2, y + h / 2), warn=warn))
        return out
    return fn


title("settings", "every threecol setting, one at a time", dur=1.5)

# 1. Widths -------------------------------------------------------------------
caption("Widths", "", "left_width = 0.25, right_width = 0.25: Master gets the rest")
fill(spaces["1"], (1, 2, 3, 4), 0.35)
show_tags(width_tags(ws1))
hold(1.6)

# 2. min_width and min_master_width ---------------------------------------------
caption("min_width", "", "min_width = 0.10: the smallest Left or Right width")
cursor.set(at_frac(0.5, 0.6))
cursor_alpha.to(1.0, tl.T, 0.25)
hold(0.2)
at = ws1.dl
mouse_drag(sp1, at_frac(0.25, 0.55), [(at_frac(0.02, 0.57), 1.1)], lambda dx, dy: ws1.move_left_divider(at, dx))
hold(0.9)
caption("min_master_width", "", "min_master_width = 0.20: the smallest Master width")
at = ws1.dl
mouse_drag(sp1, at_frac(ws1.dl, 0.55), [(at_frac(0.70, 0.53), 1.4)], lambda dx, dy: ws1.move_left_divider(at, dx))
hold(1.5)

# 3. min_row_height -------------------------------------------------------------
caption("min_row_height", "", "min_row_height = 0.10: the smallest Row height")
show_tags(row_tags(ws1, "R"))
at_h = list(ws1.h["R"])
start_y = at_h[0]
mouse_drag(sp1, at_frac(0.875, start_y), [(at_frac(0.875, 0.01), 1.1)],
           lambda dx, dy: ws1.move_edge("R", at_h, 1, dy))
hold(1.0)

# 4. step -----------------------------------------------------------------------
caption("step", "", f"step = {STEP:.2f}: one SUPER + ALT + H / J / K / L press")
cursor_alpha.to(0.0, tl.T, 0.3)
set_focus(3)
hold(0.4)
for _ in range(4):
    key("SUPER", "ALT", "K", dur=0.4)
    ws1.resize_row(3, STEP)
    retile(sp1, dur=0.25)
    hold(0.45)
show_tags(width_tags(ws1))
for _ in range(3):
    key("SUPER", "ALT", "H", dur=0.4)
    ws1.resize("R", -STEP)
    retile(sp1, dur=0.25)
    hold(0.45)
hold(0.5)

# 5. persist and reset ------------------------------------------------------------
caption("persist", "", "persist = true: resized Dividers survive reloads and reboots")
hold(2.0)
caption("Reset", "", "SUPER + R: this workspace back to its defaults")
hold(0.3)
key("SUPER", "R", dur=0.9)
ws1.reset()
retile(sp1)
hold(1.4)

# 6. Different widths per workspace, and its own reset ------------------------------
hide_tags()
caption("Per-workspace widths", "", "workspace 2 · layout_opts = { left_width = 0.15, right_width = 0.35 }")
key("SUPER", "2", dur=0.6)
switch_space("1", "2")
fill(spaces["2"], (11, 12, 13, 14), 0.35)
show_tags(width_tags(ws2))
hold(1.2)
cursor.set(at_frac(0.5, 0.6))
cursor_alpha.to(1.0, tl.T, 0.25)
at = ws2.dr
mouse_drag(sp2, at_frac(1 - ws2.dr, 0.55), [(at_frac(0.80, 0.55), 0.9)], lambda dx, dy: ws2.move_right_divider(at, dx))
cursor_alpha.to(0.0, tl.T, 0.3)
hold(0.5)
caption("Per-workspace widths", "", "SUPER + R: back to this workspace's own widths, not the global ones")
hold(0.3)
key("SUPER", "R", dur=0.9)
ws2.reset()
retile(sp2)
hold(1.6)

# 7. fill_first, and both sides full ---------------------------------------------
hide_tags()
caption("fill_first", "", 'workspace 3 · left_rows = 2, right_rows = 2, fill_first = "right"')
key("SUPER", "3", dur=0.6)
switch_space("2", "3")
fill(spaces["3"], (21, 22, 23, 24, 25), 0.55, badges=True)
hold(0.8)
caption("Both sides full", "", "no room left: the next window floats, centred, at its own size")
hold(0.3)
open_win(spaces["3"], 26, badge="6")
hold(1.6)

# 8. Two Stacks -------------------------------------------------------------------
caption("Two Stacks", "", 'workspace 4 · left_rows = "unlimited", right_rows = "unlimited"')
key("SUPER", "4", dur=0.6)
switch_space("3", "4")
fill(spaces["4"], (31, 32, 33, 34), 0.45)
hold(0.6)
caption("Two Stacks", "", 'fill_first = "left" fills the Left Stack: drag windows to the Right one')
cursor.set(at_frac(0.5, 0.6))
cursor_alpha.to(1.0, tl.T, 0.25)
dnd(spaces["4"], 34, [at_frac(0.12, 0.72), (at_frac(0.5, 0.6), 0.6), (at_frac(0.88, 0.5), 0.55)])
hold(0.3)
dnd(spaces["4"], 33, [at_frac(0.12, 0.6), (at_frac(0.5, 0.7), 0.6), (at_frac(0.88, 0.93), 0.55)])
cursor_alpha.to(0.0, tl.T, 0.3)
hold(1.2)

# 9. No Right zone ----------------------------------------------------------------
caption("No Right zone", "", 'workspace 5 · left_rows = "unlimited", right_rows = 0')
key("SUPER", "5", dur=0.6)
switch_space("4", "5")
fill(spaces["5"], (41, 42, 43, 44), 0.45)
hold(1.5)

fade_out("5")
tl.save("settings.pkl")
