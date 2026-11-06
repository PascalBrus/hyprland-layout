"""Mouse input for the (mouse) checks: moves the author's REAL pointer.

Extends reference/uinput.py for the safety rules in #66:

1. The nested Hyprland runs fullscreen on the outer workspace 9 (`fullscreen()`).
2. The modifier is ALT, never SUPER: the outer session owns SUPER + mouse.
   The uinput device only has ALT and the two buttons, so it cannot send
   SUPER at all, let alone SUPER+ALT+L (which locks the author's screen).
3. Before every press and every drag step, `check()` makes sure the outer
   workspace 9 is active and the pointer is inside the nested window. If not,
   it releases every button and key and raises `Unsafe`.

Coordinates passed in are the nested instance's own; `to_outer` maps them.

    import mouse
    mouse.fullscreen()
    with mouse.Mouse() as m:
        m.press(x, y, m.BTN_RIGHT)   # ALT + right button down at nested (x, y)
        m.glide_to(x2, y2)           # drag, checked on every step
        m.release()
"""
import json, os, subprocess, sys, time

S = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, S)
import nest as n

for p in (os.path.join(n.REPO, "reference"), os.path.dirname(S)):
    if os.path.exists(os.path.join(p, "uinput.py")):
        sys.path.insert(0, p)
        break
import uinput

WORKSPACE = 9
MARGIN = 4  # px kept clear of the nested window's edge


class Unsafe(RuntimeError):
    pass


def outer(*args):
    """hyprctl against the outer session. Used only to launch, fullscreen and read state."""
    if not n.OUTER or n.OUTER == n.HIS:
        raise Unsafe("no outer HYPRLAND_INSTANCE_SIGNATURE, or it is the nested one")
    env = dict(os.environ, HYPRLAND_INSTANCE_SIGNATURE=n.OUTER)
    return subprocess.run(["hyprctl", *args], capture_output=True, text=True, env=env).stdout.strip()


def outer_json(*args):
    return json.loads(outer(*args, "-j") or "null")


def nested_window():
    """The outer client that is the nested Hyprland (matched by pid)."""
    pid = n.instance()["pid"]
    w = next((c for c in outer_json("clients") if c["pid"] == pid), None)
    if not w:
        raise Unsafe(f"no outer window with the nested Hyprland's pid {pid}")
    return w


def cursor():
    x, y = outer("cursorpos").split(",")
    return float(x), float(y)


def to_outer(x, y):
    """Nested layout coordinates -> outer layout coordinates."""
    w, m = nested_window(), n.monitor()
    sx = w["size"][0] / (m["width"] / m["scale"])
    sy = w["size"][1] / (m["height"] / m["scale"])
    return w["at"][0] + (x - m["x"]) * sx, w["at"][1] + (y - m["y"]) * sy


def to_nested(x, y):
    w, m = nested_window(), n.monitor()
    sx = w["size"][0] / (m["width"] / m["scale"])
    sy = w["size"][1] / (m["height"] / m["scale"])
    return m["x"] + (x - w["at"][0]) / sx, m["y"] + (y - w["at"][1]) / sy


def inside(x, y, w=None):
    """Is outer point (x, y) inside the nested window, MARGIN px clear of its edge?"""
    w = w or nested_window()
    (ax, ay), (sw, sh) = w["at"], w["size"]
    return ax + MARGIN <= x <= ax + sw - MARGIN and ay + MARGIN <= y <= ay + sh - MARGIN


def safe():
    """None if rules 1 and 3 hold right now, else what is wrong. Any error counts as unsafe."""
    try:
        return _safe()
    except Exception as e:
        return f"couldn't check the safety rules ({type(e).__name__}: {e})"


def _safe():
    ws = outer_json("activeworkspace")["id"]
    if ws != WORKSPACE:
        return f"outer workspace {ws} is active, not {WORKSPACE}"
    w = nested_window()
    if w["workspace"]["id"] != WORKSPACE:
        return f"the nested window is on outer workspace {w['workspace']['id']}, not {WORKSPACE}"
    x, y = cursor()
    if not inside(x, y, w):
        return f"pointer at {x:.0f},{y:.0f} is outside the nested window at {w['at']} size {w['size']}"
    return None


def fullscreen():
    """Rule 1: move the nested window to the outer workspace 9, show it and make it fullscreen."""
    w = nested_window()
    others = [c["class"] for c in outer_json("clients") if c["workspace"]["id"] == WORKSPACE and c["address"] != w["address"]]
    if others:
        raise Unsafe(f"outer workspace {WORKSPACE} isn't empty: {others}")
    a = "address:" + w["address"]
    if w["workspace"]["id"] != WORKSPACE:
        outer("dispatch", f"hl.dsp.window.move({{ workspace = '{WORKSPACE}', follow = false, window = '{a}' }})")
    outer("dispatch", f"hl.dsp.focus({{ workspace = {WORKSPACE} }})")
    time.sleep(0.3)
    outer("dispatch", f"hl.dsp.focus({{ window = '{a}' }})")
    if not nested_window()["fullscreen"]:
        outer("dispatch", f"hl.dsp.window.fullscreen({{ action = 'set', window = '{a}' }})")
    time.sleep(1.0)
    w = nested_window()
    if w["workspace"]["id"] != WORKSPACE or not w["fullscreen"]:
        raise Unsafe(f"nested window not fullscreen on {WORKSPACE}: workspace {w['workspace']['id']}, fullscreen {w['fullscreen']}")
    if outer_json("activeworkspace")["id"] != WORKSPACE:
        raise Unsafe(f"outer workspace {WORKSPACE} isn't active")
    return w


class Mouse(uinput.Input):
    # Only these reach the device: no SUPER, no letter keys.
    KEYS = (uinput.Input.KEY_LEFTALT,)
    MOD = uinput.Input.KEY_LEFTALT

    def __enter__(self):
        super().__enter__()
        try:
            self._check_mapping()
        except BaseException:
            self.__exit__()
            raise
        return self

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
        raise Unsafe("the mouse helper sends no keys")

    def _warp_outer(self, x, y):
        if not inside(x, y):
            self.release_all()
            raise Unsafe(f"refusing to warp to {x:.0f},{y:.0f}, outside the nested window; released everything")
        self.check()  # rule 1 before moving the pointer at all
        outer("dispatch", f"hl.dsp.cursor.move({{ x = {x:.0f}, y = {y:.0f} }})")
        time.sleep(0.15)

    def warp(self, x, y):
        """Put the pointer on nested (x, y)."""
        self._warp_outer(*to_outer(x, y))

    def _check_mapping(self):
        m = n.monitor()
        cx, cy = m["x"] + m["width"] / m["scale"] / 2, m["y"] + m["height"] / m["scale"] / 2
        self.warp(cx, cy)
        nx, ny = map(float, n.hc("cursorpos").split(","))
        if abs(nx - cx) > 3 or abs(ny - cy) > 3:
            raise Unsafe(f"coordinate mapping is off: put the pointer on nested {cx:.0f},{cy:.0f}, nested sees {nx:.0f},{ny:.0f}")

    def glide_to(self, x, y, step=40, delay=0.01):
        """Glide to nested (x, y) in small relative steps, checking before each one."""
        tx, ty = to_outer(x, y)
        if not inside(tx, ty):
            self.release_all()
            raise Unsafe(f"refusing to glide to nested {x:.0f},{y:.0f}: outside the nested window; released everything")
        for _ in range(600):
            self.check()
            cx, cy = cursor()
            dx, dy = tx - cx, ty - cy
            if abs(dx) < 2 and abs(dy) < 2:
                return to_nested(cx, cy)
            # Never a step that could land outside, allowing 2x for pointer
            # acceleration: near the edge, steps shrink to 2 px.
            mx, my = max(-step, min(step, dx)), max(-step, min(step, dy))
            if not inside(cx + 2 * mx, cy + 2 * my):
                mx, my = max(-2, min(2, dx)), max(-2, min(2, dy))
            self.move_by(mx, my)
            time.sleep(delay)
        self.release_all()
        raise Unsafe(f"glide to nested {x:.0f},{y:.0f} didn't arrive (at {to_nested(*cursor())}); released everything")

    def press(self, x, y, button=uinput.Input.BTN_LEFT):
        """Warp to nested (x, y), hold ALT, press `button`. Held until release()."""
        self.warp(x, y)
        self._set(self.MOD, 1)
        time.sleep(0.1)
        self._set(button, 1)
        time.sleep(0.2)

    def drag(self, x1, y1, x2, y2, button=uinput.Input.BTN_LEFT):
        self.press(x1, y1, button)
        self.glide_to(x2, y2)
        time.sleep(0.3)
        self.release()
        return to_nested(*cursor())
