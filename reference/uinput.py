#!/usr/bin/env python3
"""Mouse and keyboard input for live tests, through a temporary uinput device.

Extends drag.py: left- or right-drags that can pause mid-drag, and key presses
that can be held. Every press and every drag step first checks that the active
workspace is one of `allowed`; if not, it lets go of everything and raises.

    from uinput import Input
    with Input() as ui:
        ui.press(x, y, button=ui.BTN_RIGHT)   # SUPER + right button down at (x, y)
        ui.glide_to(x2, y2)                   # drag; pause here as long as needed
        ui.release()
        ui.key(ui.KEY_H, mods=[ui.KEY_LEFTMETA], hold=0.8)

SUPER+ALT+L is refused: it locks the screen in the author's config.
"""
import fcntl, json, os, struct, subprocess, time

UI_SET_EVBIT, UI_SET_KEYBIT, UI_SET_RELBIT = 0x40045564, 0x40045565, 0x40045566
UI_DEV_SETUP, UI_DEV_CREATE, UI_DEV_DESTROY = 0x405C5503, 0x5501, 0x5502
EV_SYN, EV_KEY, EV_REL = 0, 1, 2
REL_X, REL_Y = 0, 1


class WrongWorkspace(RuntimeError):
    pass


def _hyprctl(*args):
    return subprocess.run(["hyprctl", *args], capture_output=True, text=True).stdout


def cursor():
    x, y = _hyprctl("cursorpos").strip().split(",")
    return float(x), float(y)


def active_workspace():
    return json.loads(_hyprctl("activeworkspace", "-j"))["id"]


class Input:
    BTN_LEFT, BTN_RIGHT = 0x110, 0x111
    KEY_LEFTMETA, KEY_LEFTALT, KEY_LEFTSHIFT = 125, 56, 42
    KEY_H, KEY_J, KEY_K, KEY_L, KEY_M, KEY_R = 35, 36, 37, 38, 50, 19
    KEYS = (KEY_LEFTMETA, KEY_LEFTALT, KEY_LEFTSHIFT, KEY_H, KEY_J, KEY_K, KEY_L, KEY_M, KEY_R)

    def __init__(self, allowed=(8, 9, 10)):
        self.allowed = allowed
        self.down = []  # keys and buttons currently held, in press order

    def __enter__(self):
        self.fd = os.open("/dev/uinput", os.O_WRONLY | os.O_NONBLOCK)
        for ev in (EV_KEY, EV_REL):
            fcntl.ioctl(self.fd, UI_SET_EVBIT, ev)
        for k in (self.BTN_LEFT, self.BTN_RIGHT, *self.KEYS):
            fcntl.ioctl(self.fd, UI_SET_KEYBIT, k)
        for r in (REL_X, REL_Y):
            fcntl.ioctl(self.fd, UI_SET_RELBIT, r)
        fcntl.ioctl(self.fd, UI_DEV_SETUP, struct.pack("HHHH80sI", 3, 0x1234, 0x5679, 1, b"claude-test-input", 0))
        fcntl.ioctl(self.fd, UI_DEV_CREATE)
        time.sleep(1.0)  # let Hyprland pick up the device
        return self

    def __exit__(self, *exc):
        try:
            self.release_all()
        finally:
            fcntl.ioctl(self.fd, UI_DEV_DESTROY)
            os.close(self.fd)

    # --- low level ---

    def _emit(self, t, c, v):
        os.write(self.fd, struct.pack("llHHi", 0, 0, t, c, v))

    def _syn(self):
        self._emit(EV_SYN, 0, 0)

    def _set(self, code, value):
        self._emit(EV_KEY, code, value)
        self._syn()
        if value:
            self.down.append(code)
        elif code in self.down:
            self.down.remove(code)

    def check(self):
        ws = active_workspace()
        if ws not in self.allowed:
            self.release_all()
            raise WrongWorkspace(f"active workspace is {ws}, not one of {self.allowed}; released everything")

    def release_all(self):
        for code in reversed(list(self.down)):
            self._set(code, 0)
            time.sleep(0.05)

    # --- pointer ---

    def warp(self, x, y):
        _hyprctl("dispatch", f"hl.dsp.cursor.move({{ x = {x}, y = {y} }})")
        time.sleep(0.15)

    def move_by(self, dx, dy):
        self._emit(EV_REL, REL_X, int(dx))
        self._emit(EV_REL, REL_Y, int(dy))
        self._syn()

    def glide_to(self, x, y, step=40, delay=0.01):
        """Small relative steps, corrected against the real cursor (acceleration-proof)."""
        for i in range(600):
            if i % 10 == 0:
                self.check()
            cx, cy = cursor()
            dx, dy = x - cx, y - cy
            if abs(dx) < 2 and abs(dy) < 2:
                return cursor()
            self.move_by(max(-step, min(step, dx)), max(-step, min(step, dy)))
            time.sleep(delay)
        return cursor()

    def press(self, x, y, button=BTN_LEFT, mod=KEY_LEFTMETA):
        """Warp to (x, y), hold `mod`, then press `button`. The drag stays held until release()."""
        self.warp(x, y)
        self.check()
        if mod:
            self._set(mod, 1)
            time.sleep(0.1)
        self._set(button, 1)
        time.sleep(0.2)

    def release(self):
        self.release_all()
        time.sleep(0.3)

    def drag(self, x1, y1, x2, y2, button=BTN_LEFT):
        self.press(x1, y1, button)
        self.glide_to(x2, y2)
        time.sleep(0.3)
        self.release()
        return cursor()

    # --- keyboard ---

    def key(self, code, mods=(), hold=0.05):
        """Press `code` with `mods` held, keep it down `hold` seconds, then release."""
        if code == self.KEY_L and self.KEY_LEFTMETA in mods and self.KEY_LEFTALT in mods:
            raise ValueError("SUPER+ALT+L locks the screen in the author's config; refusing")
        self.check()
        for m in mods:
            self._set(m, 1)
            time.sleep(0.03)
        self._set(code, 1)
        time.sleep(hold)
        self.release_all()
        time.sleep(0.15)
