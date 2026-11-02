#!/usr/bin/env python3
"""Simulate SUPER + left-drag from (x1,y1) to (x2,y2) via a temporary uinput device.
Usage: drag.py x1 y1 x2 y2"""
import fcntl, os, struct, subprocess, sys, time

UI_SET_EVBIT, UI_SET_KEYBIT, UI_SET_RELBIT = 0x40045564, 0x40045565, 0x40045566
UI_DEV_SETUP, UI_DEV_CREATE, UI_DEV_DESTROY = 0x405C5503, 0x5501, 0x5502
EV_SYN, EV_KEY, EV_REL = 0, 1, 2
REL_X, REL_Y = 0, 1
BTN_LEFT, KEY_LEFTMETA = 0x110, 125

x1, y1, x2, y2 = map(int, sys.argv[1:5])

fd = os.open("/dev/uinput", os.O_WRONLY | os.O_NONBLOCK)
for ev in (EV_KEY, EV_REL):
    fcntl.ioctl(fd, UI_SET_EVBIT, ev)
for k in (BTN_LEFT, KEY_LEFTMETA):
    fcntl.ioctl(fd, UI_SET_KEYBIT, k)
for r in (REL_X, REL_Y):
    fcntl.ioctl(fd, UI_SET_RELBIT, r)
fcntl.ioctl(fd, UI_DEV_SETUP, struct.pack("HHHH80sI", 3, 0x1234, 0x5678, 1, b"claude-test-drag", 0))
fcntl.ioctl(fd, UI_DEV_CREATE)


def emit(t, c, v):
    os.write(fd, struct.pack("llHHi", 0, 0, t, c, v))


def syn():
    emit(EV_SYN, 0, 0)


def cursor():
    out = subprocess.run(["hyprctl", "cursorpos"], capture_output=True, text=True).stdout
    x, y = out.strip().split(",")
    return float(x), float(y)


def warp(x, y):
    subprocess.run(["hyprctl", "dispatch", f"hl.dsp.cursor.move({{ x = {x}, y = {y} }})"], capture_output=True)


def glide_to(x, y):
    # small relative steps, corrected against the real cursor position (acceleration-proof)
    for _ in range(400):
        cx, cy = cursor()
        dx, dy = x - cx, y - cy
        if abs(dx) < 3 and abs(dy) < 3:
            return
        emit(EV_REL, REL_X, int(max(-40, min(40, dx))))
        emit(EV_REL, REL_Y, int(max(-40, min(40, dy))))
        syn()
        time.sleep(0.01)


try:
    time.sleep(1.0)  # let Hyprland pick up the device
    warp(x1, y1)
    time.sleep(0.2)
    emit(EV_KEY, KEY_LEFTMETA, 1); syn(); time.sleep(0.1)
    emit(EV_KEY, BTN_LEFT, 1); syn(); time.sleep(0.2)
    glide_to(x2, y2)
    time.sleep(0.3)
    emit(EV_KEY, BTN_LEFT, 0); syn(); time.sleep(0.1)
    emit(EV_KEY, KEY_LEFTMETA, 0); syn(); time.sleep(0.3)
    print("dropped at", cursor())
finally:
    fcntl.ioctl(fd, UI_DEV_DESTROY)
    os.close(fd)
