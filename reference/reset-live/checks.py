"""Live checks for #45: SUPER+R reset from a floating, fullscreen and maximised window.

Uses the side-zones-live harness. Run: python3 -I checks.py <nested instance signature>
"""
import os, sys, time
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "side-zones-live"))
import nest as n
n.pick(sys.argv[1])
n.goto(1)
for t in ("A", "B", "C"):
    n.spawn(1, t)
base = n.geo(1)
print("base", base)

def resized():
    n.focus("A"); n.msg("resize wider"); n.msg("resize wider")
    g = n.geo(1); assert g != base, "resize did nothing"; return g

def check(name, setup, teardown):
    resized(); setup(); time.sleep(0.3)
    print(name, "active:", n.active(), "full:", n.full(1))
    n.bind("SUPER + R")
    teardown(); time.sleep(0.3)
    g = n.geo(1)
    print(name, "PASS" if g == base else f"FAIL {g}")

# floating window focused
def float_setup():
    n.spawn(1, "F"); n.focus("F"); n.d("hl.dsp.window.float({ action = 'enable' })"); n.focus("F")
def float_teardown():
    n.kill("F")
check("floating", float_setup, float_teardown)

def fs(mode):
    def s(): n.focus("B"); n.d(f"hl.dsp.window.fullscreen({{ mode = '{mode}' }})")
    def t(): n.focus("B"); n.d(f"hl.dsp.window.fullscreen({{ mode = '{mode}' }})")
    return s, t
check("fullscreen", *fs("fullscreen"))
check("maximised", *fs("maximized"))
print("notes:", n.notes())
