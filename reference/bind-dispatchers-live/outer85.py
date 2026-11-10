"""Live checks for #85 that need the author's own mouse binds: window.drag()
and window.resize() as mouse binds (SUPER + left / right button), run in the
author's REAL session on its workspace 9, with the threecol build it has
loaded. The nested harness can't do these: the outer session owns SUPER +
mouse, so it would catch the drag first.

    python3 -I outer85.py --yes [<id>...]     # default: every check

Ask the author first; they keep their hands off until it's done.

Safety, checked before every press, warp and drag step (any failure releases
everything and stops the run):
- The uinput device has SUPER and the two mouse buttons only, so it can't send
  SUPER+ALT+L (which locks the author's screen) or any other key.
- Workspace 9 is active and holds only this script's windows.
- The pointer stays on workspace 9's monitor.

The checks open their own test windows on workspace 9 (titles O1 … O4), and
on the way out close them, reset workspace 9's Dividers and put back the
threecol dividers file as it was, then show the workspace that was active.
"""
import json, os, shutil, subprocess, sys, time

S = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(os.path.realpath(__file__), "..", "..", ".."))
sys.path.insert(0, os.path.join(REPO, "reference"))
import uinput

WS = 9
PX = 3
STATE = os.path.join(os.environ.get("XDG_STATE_HOME") or os.path.expanduser("~/.local/state"), "threecol", "dividers")
MINE = {}  # title -> pid
FAILS = []
CHECKS = {}


class Unsafe(RuntimeError):
    pass


def hc(*args):
    return subprocess.run(["hyprctl", *args], capture_output=True, text=True).stdout.strip()


def hj(*args):
    return json.loads(hc(*args, "-j") or "null")


def d(expr):
    out = hc("dispatch", expr)
    time.sleep(0.25)
    return out


def clients():
    return [c for c in hj("clients") if c["workspace"]["id"] == WS]


def by_title(t):
    return next((c for c in clients() if c["title"] == t), None)


def geo():
    return {c["title"]: (c["at"][0], c["at"][1], c["size"][0], c["size"][1]) for c in clients() if not c["floating"]}


def rect(t):
    c = by_title(t)
    return (c["at"][0], c["at"][1], c["size"][0], c["size"][1])


def centre(r):
    return r[0] + r[2] / 2, r[1] + r[3] / 2


def monitor():
    ws = next(w for w in hj("workspaces") if w["id"] == WS)
    return next(m for m in hj("monitors") if m["name"] == ws["monitor"])


def same(a, b):
    return a.keys() == b.keys() and all(all(abs(x - y) <= PX for x, y in zip(a[t], b[t])) for t in a)


def swapped(g, a, b):
    out = dict(g)
    out[a], out[b] = g[b], g[a]
    return out


def expect(ok, seen):
    if not ok:
        FAILS.append(seen)


def safe():
    """None if the rules hold now, else what is wrong."""
    try:
        ws = hj("activeworkspace")["id"]
        if ws != WS:
            return f"workspace {ws} is active, not {WS}"
        pids = set(MINE.values())
        others = [c["class"] for c in clients() if c["pid"] not in pids]
        if others:
            return f"workspace {WS} holds windows that aren't the test's: {others}"
        m = monitor()
        x, y = map(float, hc("cursorpos").split(","))
        if not (m["x"] + 4 <= x <= m["x"] + m["width"] / m["scale"] - 4 and m["y"] + 4 <= y <= m["y"] + m["height"] / m["scale"] - 4):
            return f"pointer at {x:.0f},{y:.0f} is off workspace {WS}'s monitor"
        return None
    except Exception as e:
        return f"couldn't check the safety rules ({type(e).__name__}: {e})"


class Mouse(uinput.Input):
    KEYS = (uinput.Input.KEY_LEFTMETA,)  # SUPER only: no ALT, no letters
    MOD = uinput.Input.KEY_LEFTMETA

    def __init__(self):
        super().__init__(allowed=(WS,))

    def check(self):
        why = safe()
        if why:
            self.release_all()
            raise Unsafe(why + "; released everything")

    def _set(self, code, value):
        if value:
            if code not in (self.BTN_LEFT, self.BTN_RIGHT, self.MOD):
                raise Unsafe(f"refusing to press key {code}")
            self.check()
        super()._set(code, value)

    def key(self, *a, **k):
        raise Unsafe("this helper sends no keys")

    def warp(self, x, y):
        m = monitor()
        if not (m["x"] + 4 <= x <= m["x"] + m["width"] / m["scale"] - 4 and m["y"] + 4 <= y <= m["y"] + m["height"] / m["scale"] - 4):
            self.release_all()
            raise Unsafe(f"refusing to warp to {x:.0f},{y:.0f}, off the monitor; released everything")
        self.check()
        hc("dispatch", f"hl.dsp.cursor.move({{ x = {x:.0f}, y = {y:.0f} }})")
        time.sleep(0.15)

    def glide_to(self, x, y, step=40, delay=0.01):
        for _ in range(600):
            self.check()
            cx, cy = map(float, hc("cursorpos").split(","))
            dx, dy = x - cx, y - cy
            if abs(dx) < 2 and abs(dy) < 2:
                return cx, cy
            self.move_by(max(-step, min(step, dx)), max(-step, min(step, dy)))
            time.sleep(delay)
        self.release_all()
        raise Unsafe(f"glide to {x:.0f},{y:.0f} didn't arrive; released everything")

    def press(self, x, y, button):
        self.warp(x, y)
        self._set(self.MOD, 1)
        time.sleep(0.1)
        self._set(button, 1)
        time.sleep(0.2)

    def drag(self, x1, y1, x2, y2, button):
        self.press(x1, y1, button)
        end = self.glide_to(x2, y2)
        time.sleep(0.3)
        self.release()
        time.sleep(0.5)
        return end

    def release(self):
        self.release_all()


# ---- setup ----

def spawn(title):
    d(f"hl.dsp.exec_cmd('ghostty --gtk-single-instance=false --title={title}', {{ workspace = '{WS}' }})")
    for _ in range(100):
        c = by_title(title)
        if c:
            MINE[title] = c["pid"]
            time.sleep(0.4)
            return
        time.sleep(0.1)
    raise RuntimeError(f"{title} didn't open")


def close_all():
    for t, pid in list(MINE.items()):
        if pid > 0:
            try:
                os.kill(pid, 15)
            except ProcessLookupError:
                pass
        MINE.pop(t)
    for _ in range(50):
        if not clients():
            break
        time.sleep(0.1)
    time.sleep(0.3)


def four():
    """O1 … O4 on workspace 9, Dividers reset: Master O1, Left O2, Stack O3 above O4."""
    close_all()
    d(f"hl.dsp.focus({{ workspace = {WS} }})")
    for t in ["O1", "O2", "O3", "O4"]:
        spawn(t)
    d("hl.dsp.layout('reset')")
    g = geo()
    ok = g["O2"][0] < g["O1"][0] < g["O3"][0] == g["O4"][0] and g["O3"][1] < g["O4"][1]
    if not ok:
        raise RuntimeError(f"not Master O1, Left O2, Stack O3 above O4 (another workspace rule or setting?): {g}")
    return g


def check(fn):
    CHECKS[fn.__name__] = fn
    return fn


# ---- window.drag() ----

def drop(m, who, x, y):
    d(f"hl.dsp.focus({{ window = 'address:{by_title(who)['address']}' }})")
    m.drag(*centre(rect(who)), x, y, m.BTN_LEFT)
    expect(not by_title(who)["floating"], f"{who} floats after the drop")
    return geo()


@check
def drop_swap(m):
    """SUPER + left drag O4 onto O2: they swap."""
    start = four()
    r = rect("O2")
    g = drop(m, "O4", r[0] + r[2] / 2, r[1] + r[3] * 0.4)
    expect(same(swapped(start, "O4", "O2"), g), f"O4 onto O2: {g}, want O4 and O2 swapped from {start}")


@check
def drop_new_row(m):
    """O2 dropped onto the bottom quarter of the Stack joins it as its bottom Row."""
    start = four()
    r = rect("O4")
    g = drop(m, "O2", r[0] + r[2] / 2, r[1] + r[3] * 0.9)
    expect(g["O2"][0] == g["O3"][0] and g["O3"][1] < g["O4"][1] < g["O2"][1], f"O2 onto the Stack's bottom: {g}, want Stack O3, O4, O2")
    expect(same({"O1": g["O1"]}, {"O1": start["O1"]}), f"Master moved: {start['O1']} -> {g['O1']}")


@check
def drop_empty_zone(m):
    """With Left emptied (O2 closed), O4 dropped into the Left zone goes there."""
    start = four()
    os.kill(MINE.pop("O2"), 15)
    time.sleep(0.8)
    x, y = centre(start["O2"])
    g = drop(m, "O4", x, y)
    expect(abs(g["O4"][0] - start["O2"][0]) <= PX and abs(g["O4"][2] - start["O2"][2]) <= PX, f"O4 into empty Left: {g['O4']}, want Left's place {start['O2']}")


# ---- window.resize() as a mouse bind: Drag resize ----

@check
def drag_resize_divider(m):
    """SUPER + right drag in Left, 10% of the width to the right: Left|Master follows the
    cursor; Master|Right stays (centre_master is off in this session)."""
    if hc("getoption", "plugin:threecol:centre_master").splitlines()[0] != "bool: false":
        raise RuntimeError("centre_master isn't false here; this check expects it off")
    start = four()
    W = monitor()["width"] / monitor()["scale"]
    x, y = centre(start["O2"])
    end = m.drag(x, y, x + 0.10 * W, y, m.BTN_RIGHT)
    g = geo()
    travel = end[0] - x
    expect(abs(g["O2"][2] - start["O2"][2] - travel) <= 2 * PX, f"Left width {start['O2'][2]} -> {g['O2'][2]}, the cursor moved {travel:.0f}")
    expect(abs(g["O3"][0] - start["O3"][0]) <= PX and abs(g["O3"][2] - start["O3"][2]) <= PX, f"Right moved: {start['O3']} -> {g['O3']}")
    d("hl.dsp.layout('reset')")
    expect(same(start, geo()), f"reset after the drag: {geo()}")


@check
def drag_resize_row(m):
    """SUPER + right drag in the Stack's top Row, 60 px down: the Row edge follows, O4 gives way."""
    start = four()
    x, y = start["O3"][0] + start["O3"][2] / 2, start["O3"][1] + start["O3"][3] * 0.75
    end = m.drag(x, y, x, y + 60, m.BTN_RIGHT)
    g = geo()
    travel = end[1] - y
    expect(abs(g["O3"][3] - start["O3"][3] - travel) <= 2 * PX, f"O3 height {start['O3'][3]} -> {g['O3'][3]}, the cursor moved {travel:.0f}")
    expect(abs(g["O4"][3] - start["O4"][3] + travel) <= 2 * PX, f"O4 didn't give way: {start['O4']} -> {g['O4']}")
    expect(abs(g["O2"][2] - start["O2"][2]) <= PX, f"a Divider moved: Left {start['O2']} -> {g['O2']}")


def main():
    if "--yes" not in sys.argv[1:]:
        sys.exit("this moves the author's real pointer on their workspace 9: ask them, then pass --yes")
    names = [a for a in sys.argv[1:] if a != "--yes"] or list(CHECKS)
    if any(c["workspace"]["id"] == WS for c in hj("clients")):
        sys.exit(f"workspace {WS} isn't empty")
    back = hj("activeworkspace")["id"]
    saved = open(STATE).read() if os.path.exists(STATE) else None
    try:
        with Mouse() as m:
            for name in names:
                FAILS.clear()
                stop = None
                try:
                    CHECKS[name](m)
                except Unsafe as e:
                    stop = e
                    FAILS.append(f"stopped for safety: {e}")
                except Exception as e:
                    FAILS.append(f"{type(e).__name__}: {e}")
                print(f"PASS {name}" if not FAILS else f"FAIL {name}: " + "; ".join(FAILS), flush=True)
                if stop:
                    break
    finally:
        if clients():
            d(f"hl.dsp.focus({{ workspace = {WS} }})")
            d("hl.dsp.layout('reset')")
        close_all()
        time.sleep(0.5)
        if saved is not None:
            open(STATE, "w").write(saved)
        d(f"hl.dsp.focus({{ workspace = {back} }})")


if __name__ == "__main__":
    main()
