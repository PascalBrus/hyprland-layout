"""GIF 1, the tour at the top of the README: the features, then workspaces
with three, two and one columns. Writes tour.pkl for render.py."""
import timeline as tl
from model import WS
from timeline import (Space, at_frac, caption, close_win, cursor, cursor_alpha, dnd, fade_out, fill, hold, key,
                      mouse_drag, open_win, retile, set_focus, spaces, switch_space, title)

ws1 = WS()
ws2 = WS(left_rows=2)
ws3 = WS(left_rows=None, right_rows=1, fill_first="right")
ws4 = WS(left_rows=0)
ws5 = WS(left_rows=0, right_rows=0)
for n, ws in enumerate((ws1, ws2, ws3, ws4, ws5), 1):
    spaces[str(n)] = Space(str(n), ws)
spaces["1"].visible = True
sp1 = spaces["1"]

title("threecol", "a three-zone tiling layout for Hyprland, built for ultrawides", dur=1.6)

# 1. Fill order -------------------------------------------------------------
caption("Fill order", "Master first, then Left, then down the Stack")
hold(0.35)
for i in (1, 2, 3, 4):
    open_win(sp1, i)
    hold(0.8)
hold(0.3)

# 2. Holes ------------------------------------------------------------------
caption("Holes", "close Master: nothing slides in")
hold(0.3)
key("SUPER", "M")
set_focus(1)
hold(0.6)
close_win(sp1, 1, refocus=4)
hold(1.3)
caption("Holes", "the next new window fills the Hole")
hold(0.35)
open_win(sp1, 5)
hold(1.1)

# 3. Mouse resize -------------------------------------------------------------
caption("Mouse resize", "SUPER + right-drag a Divider or a Row edge")
cursor.set(at_frac(0.5, 0.62))
cursor_alpha.to(1.0, tl.T, 0.25)
hold(0.3)
at = ws1.dl
mouse_drag(sp1, at_frac(0.235, 0.5), [(at_frac(0.335, 0.52), 0.9), (at_frac(0.295, 0.5), 0.5)],
           lambda dx, dy: ws1.move_left_divider(at, dx))
hold(0.25)
at_h = list(ws1.h["R"])
mouse_drag(sp1, at_frac(0.86, 0.45), [(at_frac(0.86, 0.68), 0.75), (at_frac(0.86, 0.30), 0.85)],
           lambda dx, dy: ws1.move_edge("R", at_h, 1, dy))
hold(0.4)

# 4. Keyboard resize and reset -------------------------------------------------
caption("Keyboard resize", "SUPER + ALT + H: the focused zone gets narrower")
cursor_alpha.to(0.0, tl.T, 0.3)
hold(0.3)
key("SUPER", "M")
set_focus(ws1.master)
hold(0.55)
for _ in range(3):
    key("SUPER", "ALT", "H", dur=0.42)
    ws1.narrower_master()
    retile(sp1, dur=0.3)
    hold(0.42)
hold(0.3)
caption("Keyboard resize", "SUPER + R: back to the settings")
hold(0.3)
key("SUPER", "R", dur=0.9)
ws1.reset()
retile(sp1)
hold(1.2)

# 5. Drag-and-drop ---------------------------------------------------------
caption("Drag-and-drop", "SUPER + left-drag onto a window to swap")
cursor.set(at_frac(0.55, 0.62))
cursor_alpha.to(1.0, tl.T, 0.25)
hold(0.25)
# Stack's bottom window over Master: swap.
dnd(sp1, 4, [at_frac(0.87, 0.78), (at_frac(0.62, 0.62), 0.6), (at_frac(0.48, 0.55), 0.55)])
hold(0.7)
caption("Drag-and-drop", "onto a zone's bottom quarter: add it there")
hold(0.2)
# Left window onto the Stack's add area: a new bottom Row, Left becomes a Hole.
dnd(sp1, 2, [at_frac(0.03, 0.06), (at_frac(0.5, 0.45), 0.7), (at_frac(0.8, 0.6), 0.55), (at_frac(0.8, 0.93), 0.55)])
hold(0.9)

# 6. Chain focus and move ---------------------------------------------------
caption("Chain focus and move", "SUPER + H / L walks the Chain: Left, Master, Stack")
cursor_alpha.to(0.0, tl.T, 0.3)
hold(0.4)
for d in ("H", "H", "H", "L", "H"):
    key("SUPER", d, dur=0.5)
    set_focus(ws1.focus_step(tl.focused, "l" if d == "H" else "r"))
    hold(0.5)
hold(0.2)
caption("Chain focus and move", "SUPER + SHIFT + L moves Master into the Stack, leaving a Hole")
hold(0.3)
key("SUPER", "SHIFT", "L", dur=0.8)
ws1.move(tl.focused, "r")
retile(sp1)
hold(1.4)


# 7. Left with several Rows -------------------------------------------------
caption("Several Left Rows", "", "workspace 2 · layout_opts = { left_rows = 2 }")
key("SUPER", "2", dur=0.6)
switch_space("1", "2")
hold(0.1)
fill(spaces["2"], (11, 12, 13, 14, 15), 0.5)
hold(0.6)

# 8. Per-workspace shape ----------------------------------------------------
caption("Per-workspace shape", "", 'workspace 3 · left_rows = "unlimited", right_rows = 1, fill_first = "right"')
key("SUPER", "3", dur=0.6)
switch_space("2", "3")
hold(0.1)
fill(spaces["3"], (21, 22, 23, 24), 0.5)
hold(0.8)

# 9. Two columns and one ----------------------------------------------------
caption("Two columns", "", "workspace 4 · left_rows = 0: no Left zone, Master reaches the edge")
key("SUPER", "4", dur=0.6)
switch_space("3", "4")
hold(0.1)
fill(spaces["4"], (31, 32, 33), 0.5)
hold(0.8)
caption("One column", "", "workspace 5 · left_rows = 0, right_rows = 0: Master only")
key("SUPER", "5", dur=0.6)
switch_space("4", "5")
hold(0.1)
open_win(spaces["5"], 41)
hold(0.9)
caption("One column", "", "workspace 5 · left_rows = 0, right_rows = 0: more windows float")
hold(0.3)
open_win(spaces["5"], 42)
hold(1.4)

fade_out("5")
tl.save("tour.pkl")
