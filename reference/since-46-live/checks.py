"""Live checks for #66 (since #46) in the nested Hyprland.

    python3 -I checks.py <id>...         e.g. r1 f3 c9
    python3 -I checks.py g1              the build gate: run it first, before launch.sh
    python3 -I checks.py all             every other check that needs no mouse
    python3 -I checks.py --mouse <id>... (mouse) checks: only with the author's OK

Each check sets up and cleans up its own windows, settings and state file,
then prints `PASS <id>` or `FAIL <id>: <what was seen>`.
"""
import os, re, shutil, subprocess, sys, tempfile, time

S = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, S)
import nest as n

BUILD_REF = "7041cc8"  # the build under test
HYPRLAND = "0.57.1"

# Plugin settings put in front of every check's own, e.g. 'right_rows = "unlimited"'.
# Empty by default: the checks run on the default settings, as #66 asks.
BASE = os.environ.get("SINCE46_BASE", "")

STEP = 0.05  # default `step`
TOL = 0.005  # fractions: about 13 px at 2560
PX = 3  # pixels

# ---- result collection ----

FAILS = []
CHECKS = {}  # id -> (function, needs the mouse, needs the nested instance)


class Stop(Exception):
    """Stops the whole run (a mouse safety stop)."""


def expect(ok, seen):
    if not ok:
        FAILS.append(seen)
    return ok


def check(mouse=False, nested=True):
    def wrap(fn):
        CHECKS[fn.__name__] = (fn, mouse, nested)
        return fn
    return wrap


def run_check(name):
    fn, needs_mouse, nested = CHECKS[name]
    FAILS.clear()
    stop = None
    try:
        if nested:
            reset()
        if needs_mouse:
            import mouse
            mouse.fullscreen()
            with mouse.Mouse() as m:
                fn(m)
        else:
            fn()
    except Exception as e:
        if type(e).__name__ == "Unsafe":
            stop = e
            FAILS.append(f"stopped for safety: {e}")
        else:
            FAILS.append(f"{type(e).__name__}: {e}")
    finally:
        if nested:
            try:
                reset()
            except Exception as e:
                FAILS.append(f"cleanup failed: {type(e).__name__}: {e}")
    print(f"PASS {name}" if not FAILS else f"FAIL {name}: " + "; ".join(FAILS), flush=True)
    if stop:
        raise Stop(stop)


# ---- setup helpers ----

def cfg(opts="", rules=""):
    """Replace the nested extra.lua with these plugin settings and rules, then reload."""
    body = ""
    opts = ", ".join(x for x in (BASE, opts) if x)
    if opts:
        body += "hl.config({ plugin = { threecol = { %s } } })\n" % opts
    body += rules
    n.extra(body)
    n.reload()


def rule(ws, opts):
    return "hl.workspace_rule({ workspace = '%s', layout_opts = { %s } })\n" % (ws, opts)


def setting(lua):
    """Change plugin settings at runtime with hl.config, without a reload."""
    return n.run("hl.config({ plugin = { threecol = { %s } } })" % lua)


def close_all():
    for c in n.clients():
        n.term(c["pid"])
    for _ in range(50):
        if not n.clients():
            break
        time.sleep(0.1)
    time.sleep(0.3)


def wipe_state():
    for p in (n.STATE, n.STATE_OFF, n.STATE_BAK):
        if os.path.exists(p):
            os.remove(p)


def reset():
    """No windows, no state file, default settings, workspace 1, no notifications."""
    close_all()
    wipe_state()
    cfg("")
    n.goto(1)
    n.clear_notes()


def windows(ws, titles, fresh=True):
    """Open `titles` on `ws`. `fresh` resets the workspace, as Kept sizes outlive its windows."""
    n.goto(ws)
    for t in titles:
        n.spawn(ws, t)
    if fresh:
        n.msg("reset")
    return n.geo(ws)


def four(ws=1, prefix="T"):
    """Master T1, Left T2, Stack T3 above T4 (with the default settings)."""
    return windows(ws, [f"{prefix}{i}" for i in range(1, 5)])


def width():
    m = n.monitor()
    return m["width"] / m["scale"]


def height():
    m = n.monitor()
    return m["height"] / m["scale"]


def saved(ws=1):
    """The state file's (left_width, right_width) for workspace `ws`, or None."""
    return n.entries(n.state()).get(str(ws))


def near(a, b, tol=TOL):
    return a is not None and b is not None and all(abs(x - y) <= tol for x, y in zip(a, b))


def same(g1, g2, tol=PX):
    return g1.keys() == g2.keys() and all(all(abs(a - b) <= tol for a, b in zip(g1[t], g2[t])) for t in g1)


def fmt(t):
    return "(" + ", ".join(f"{x:.3f}" if isinstance(x, float) else str(x) for x in t) + ")" if t else str(t)


def float_window(t):
    n.d(f"hl.dsp.window.float({{ action = 'toggle', window = '{n.addr(t)}' }})")
    time.sleep(0.3)


def rect(t):
    c = n.by_title(t)
    return (c["at"][0], c["at"][1], c["size"][0], c["size"][1]) if c else None


def centre(r):
    return r[0] + r[2] / 2, r[1] + r[3] / 2


def master_centred(t="T1"):
    cx = centre(rect(t))[0]
    return abs(cx - width() / 2) <= PX, cx


def side_widths(ws=1, master="T1"):
    """(Left, Right) zone widths in px from the tiles' rectangles; 0 for an empty side."""
    g = n.geo(ws)
    m = g[master]
    left = [r for t, r in g.items() if r[0] + r[2] <= m[0]]
    right = [r for t, r in g.items() if r[0] >= m[0] + m[2]]
    return (max((r[2] for r in left), default=0), max((r[2] for r in right), default=0))


# ---- G1: gate ----

@check(nested=False)
def g1():
    v = subprocess.run(["pkg-config", "--modversion", "hyprland"], capture_output=True, text=True).stdout.strip()
    expect(v == HYPRLAND, f"Hyprland headers are {v!r}, not {HYPRLAND}")
    src = tempfile.mkdtemp(prefix="g1-src-", dir=S)
    build = os.path.join(S, "build")
    try:
        archive = subprocess.run(["git", "-C", n.REPO, "archive", BUILD_REF], capture_output=True, check=True).stdout
        subprocess.run(["tar", "-x", "-C", src], input=archive, check=True)
        shutil.rmtree(build, ignore_errors=True)
        for cmd in (["cmake", "-S", src, "-B", build, "-DCMAKE_BUILD_TYPE=Release"],
                    ["cmake", "--build", build, "-j"]):
            r = subprocess.run(cmd, capture_output=True, text=True)
            if not expect(r.returncode == 0, f"{' '.join(cmd[:2])} failed: {(r.stdout + r.stderr)[-400:]}"):
                return
        r = subprocess.run(["ctest", "--test-dir", build], capture_output=True, text=True)
        summary = next((l for l in r.stdout.splitlines() if "tests passed" in l), r.stdout[-300:])
        expect(r.returncode == 0 and "100% tests passed" in summary, f"ctest: {summary}")
        expect(os.path.exists(os.path.join(build, "threecol.so")), "no build/threecol.so")
    finally:
        shutil.rmtree(src, ignore_errors=True)


# ---- #37: hyprctl reload keeps the layout ----

@check()
def r1():
    four()
    n.focus("T3")
    n.msg("resize taller")
    before = n.geo(1)
    if not expect(abs(before["T3"][3] - before["T4"][3]) > PX, f"resize taller left the Rows equal: {before}"):
        return
    n.reload()
    after = n.geo(1)
    expect(same(before, after), f"before reload {before}, after {after}")


def hole_survives(closed):
    four()
    n.kill(closed)
    before = n.geo(1)
    expect(len(before) == 3, f"after closing {closed}: {before}")
    n.reload()
    after = n.geo(1)
    expect(same(before, after), f"before reload {before}, after {after}")


@check()
def r2():
    hole_survives("T1")


@check()
def r3():
    hole_survives("T2")


@check()
def r4():
    start = four()
    n.focus("T3")
    n.msg("move l")
    n.focus("T4")
    n.msg("move u")
    before = n.geo(1)
    if not expect(not same(start, before), f"the moves changed nothing: {before}"):
        return
    n.reload()
    after = n.geo(1)
    expect(same(before, after), f"before reload {before}, after {after}")


@check()
def r5():
    # Workspace 2: Zones changed, a Hole, unequal Row heights.
    windows(2, ["A1", "A2", "A3", "A4", "A5"])
    n.focus("A3")
    n.msg("move l")
    n.focus("A4")
    n.msg("resize taller")
    n.kill("A2")
    before = n.geo(2)
    heights = sorted(r[3] for t, r in before.items() if r[0] > width() / 2)
    expect(len(before) == 4 and heights[-1] - heights[0] > PX, f"workspace 2 set-up didn't take: {before}")
    four(1)
    n.reload()
    n.goto(2)
    after = n.geo(2)
    expect(same(before, after), f"workspace 2 before reload {before}, after {after}")


# ---- #39: closing the dragged window (mouse) ----

def drop_close(m, target):
    """Drag T4 over `target` (title), close T4 while holding, release."""
    four()
    n.focus("T4")
    base = n.shot("base")
    others = {t: r for t, r in n.geo(1).items() if t != "T4"}
    x, y = centre(rect("T4"))
    m.press(x, y, m.BTN_LEFT)
    m.glide_to(*centre(rect(target)))
    time.sleep(0.4)
    seen = n.magenta_gain(base, n.shot("mid"))
    expect(seen > 0.02, f"the detector can't see the drop indicator mid-drag ({seen:.1%} of pixels)")
    n.kill("T4")
    m.release()
    time.sleep(0.5)
    after = n.geo(1)
    left = n.magenta_gain(base, n.shot("after"))
    expect("T4" not in after and n.by_title("T4") is None, f"T4 still there: {n.by_title('T4')}")
    expect(same({t: after[t] for t in ("T1", "T2") if t in after}, {t: others[t] for t in ("T1", "T2")}),
           f"T1/T2 moved: before {others}, after {after}")
    t3, o3 = after.get("T3"), others["T3"]
    expect(t3 and abs(t3[0] - o3[0]) <= PX and abs(t3[2] - o3[2]) <= PX and t3[3] > o3[3] + PX,
           f"T4's Row didn't close into T3: T3 before {o3}, after {t3}")
    expect(left < 0.02, f"drop indicator still shown ({left:.1%} of pixels turned magenta; see shots/after.ppm)")


@check(mouse=True)
def d1(m):
    drop_close(m, "T1")


@check(mouse=True)
def d2(m):
    drop_close(m, "T2")


@check(mouse=True)
def d3(m):
    drop_close(m, "T3")


# ---- #40: messages from a floating window ----

def with_float():
    four()
    n.spawn(1, "F")
    float_window("F")
    n.focus("F")
    expect(n.by_title("F")["floating"] and n.active() == "F", f"F isn't a focused floating window: {n.by_title('F')}")


@check()
def f1():
    with_float()
    n.msg("focusmaster")
    expect(n.active() == "T1", f"focus is on {n.active()}, not T1")


@check()
def f2():
    start = four()
    n.focus("T2")
    n.msg("resize wider")
    expect(not same(start, n.geo(1)) and saved() is not None, f"resize didn't take: {n.geo(1)}, saved {saved()}")
    n.spawn(1, "F")
    float_window("F")
    n.focus("F")
    n.bind("SUPER + R")
    after = n.geo(1)
    expect(same(start, after), f"after SUPER+R {after}, settings give {start}")
    expect(saved() is None, f"saved entry kept: {saved()}")


@check()
def f3():
    with_float()
    before, f = n.geo(1), rect("F")
    for m in ["focus l", "focus r", "focus u", "focus d", "move l", "move r", "move u", "move d",
              "swapwithmaster", "resize wider", "resize narrower", "resize taller", "resize shorter"]:
        r = n.msg(m)
        expect(r in ("ok", ""), f"{m!r} answered {r!r}")
        expect(same(before, n.geo(1)) and rect("F") == f and n.active() == "F" and n.by_title("F")["floating"],
               f"{m!r} changed something: tiles {n.geo(1)}, F {rect('F')}, focus {n.active()}")
    expect(not n.notes(), f"notifications: {n.notes()!r}")
    expect(n.state() is None, f"state file written: {n.state()!r}")


# ---- #41: persist at runtime ----

@check()
def p1():
    four()
    n.focus("T2")
    n.msg("resize wider")
    saved_text = n.state()
    n.write_state(n.STATE_OFF, {"8": (0.2, 0.2)})  # an older dividers.off
    before = n.geo(1)
    setting("persist = false")
    expect(n.state() is None, f"dividers still there: {n.state()!r}")
    expect(n.read(n.STATE_OFF) == saved_text, f"dividers.off is {n.read(n.STATE_OFF)!r}, not the old dividers {saved_text!r}")
    expect(same(before, n.geo(1)), f"windows moved: {before} -> {n.geo(1)}")
    n.focus("T2")
    n.msg("resize wider")
    resized = n.geo(1)
    expect(not same(before, resized), f"resize did nothing: {resized}")
    expect(n.state() is None and n.read(n.STATE_OFF) == saved_text,
           f"resize wrote a file: dividers {n.state()!r}, dividers.off {n.read(n.STATE_OFF)!r}")
    n.kill("T2")
    n.spawn(1, "T5")
    after = n.geo(1)
    want = dict(resized)
    want["T5"] = want.pop("T2")
    expect(same(want, after), f"close and reopen lost the resize: {resized} -> {after}")


@check()
def p2():
    n.write_state(n.STATE, {"7": (0.2, 0.2)})  # a workspace that isn't open
    n.reload()
    four()
    setting("persist = false")
    expect(n.state() is None and "7" in n.entries(n.read(n.STATE_OFF)),
           f"persist = false didn't hide the file: dividers {n.state()!r}, dividers.off {n.read(n.STATE_OFF)!r}")
    n.focus("T2")
    n.msg("resize wider")
    setting("persist = true")
    e = n.entries(n.state())
    expect(n.state() is not None and not os.path.exists(n.STATE_OFF),
           f"not renamed back: dividers {n.state()!r}, dividers.off {n.read(n.STATE_OFF)!r}")
    expect(near(e.get("1"), (0.25 + STEP, 0.25 + STEP)), f"on-screen workspace 1 saved as {fmt(e.get('1'))}, want 0.30 0.30")
    expect(near(e.get("7"), (0.2, 0.2)), f"workspace 7 entry is {fmt(e.get('7'))}, want 0.2 0.2")


@check()
def p3():
    four()
    n.focus("T2")
    n.msg("resize wider")
    setting("persist = false")
    hidden = n.read(n.STATE_OFF)
    expect(hidden is not None, "persist = false left no dividers.off")
    n.write_state(n.STATE, {"7": (0.2, 0.2)})
    setting("persist = true")
    expect(n.read(n.STATE_OFF) == hidden, f"dividers.off changed: {hidden!r} -> {n.read(n.STATE_OFF)!r}")
    expect(near(n.entries(n.state()).get("7"), (0.2, 0.2)), f"dividers didn't win: {n.state()!r}")


@check()
def p4():
    dflt1 = four()
    dflt2 = windows(2, ["A1", "A2", "A3"])
    n.focus("A2")
    n.msg("resize wider")
    resized2 = n.geo(2)
    n.goto(1)
    n.focus("T1")
    n.msg("resize wider")
    resized1 = n.geo(1)
    expect(not same(dflt1, resized1) and not same(dflt2, resized2), "resizes didn't take")
    cfg("persist = false")
    g1 = n.geo(1)
    n.goto(2)
    g2 = n.geo(2)
    expect(same(dflt1, g1) and same(dflt2, g2), f"persist = false on reload didn't reset: ws1 {g1} (want {dflt1}), ws2 {g2} (want {dflt2})")
    n.goto(1)
    cfg("persist = true")
    g1 = n.geo(1)
    n.goto(2)
    g2 = n.geo(2)
    expect(same(resized1, g1) and same(resized2, g2),
           f"persist = true on reload didn't load the saved Dividers: ws1 {g1} (want {resized1}), ws2 {g2} (want {resized2})")


# ---- #42 / #43: settings notice ----

NOISE = re.compile(r"\d\.\d*?(\d)\1{3,}\d*|\d\.\d{5,}")  # 0.30000000000000004, 0.1000001, 0.0999999


@check()
def n1():
    four()
    cfg("left_rows = 99")
    w = n.warnings()
    want = "left_rows 99 doesn't fit min_row_height 0.1; using 9"
    expect(want in w, f"want {want!r}, notice lines {w!r}")
    for opts in ["left_rows = 99",
                 "left_width = 0.05, right_width = 0.05",
                 "left_width = 0.45, right_width = 0.45, min_master_width = 0.3",
                 "left_width = 0.15, right_width = 0.35",
                 "left_width = 0.3, right_width = 0.3, centre_master = false, min_master_width = 0.7",
                 "right_rows = 7, min_row_height = 0.3"]:
        n.clear_notes()
        cfg(opts)
        w = n.warnings()
        expect(w, f"{opts}: no warning to check")
        for line in w:
            expect(not NOISE.search(line), f"float noise: {line!r}")


@check()
def o1():
    dflt = four()
    cfg("left_width = 2.0, right_width = -1")
    w = n.warnings()
    expect(any(l.startswith("left_width 2") for l in w), f"no left_width 2 warning: {w!r}")
    expect(any(l.startswith("right_width -1") for l in w), f"no right_width -1 warning: {w!r}")
    expect(same(dflt, n.geo(1)), f"defaults not used: {n.geo(1)}, defaults give {dflt}")
    n.clear_notes()
    cfg("min_width = 'abc'")
    notes, errs, opt = n.notes(), n.hc("configerrors"), n.option("plugin:threecol:min_width")
    expect("min_width" not in notes, f"a notification names min_width: {notes!r}")
    expect("min_width" not in errs, f"configerrors names min_width: {errs!r}")
    expect("set: false" in opt, f"getoption: {opt!r}")


# ---- #63 / #64: Centred Master ----

def rdrag(m, x, y, dx, dy=0, button=None):
    """Drag-resize (ALT + right button) from (x, y) by (dx, dy). Returns the cursor's travel."""
    end = m.drag(x, y, x + dx, y + dy, button or m.BTN_RIGHT)
    time.sleep(0.3)
    return end[0] - x, end[1] - y


def expect_saved(want, what, ws=1):
    got = saved(ws)
    return expect(near(got, want), f"{what}: saved {fmt(got)}, want {fmt(want)}")


def sr():
    n.bind("SUPER + R")


@check(mouse=True)
def c1(m):
    four()
    r = rect("T1")
    rdrag(m, r[0] + r[2] * 0.25, r[1] + r[3] / 2, -0.10 * width())
    expect_saved((0.15, 0.15), "Master's left half 10% left")
    ok, cx = master_centred()
    expect(ok, f"Master's centre at {cx}, screen centre {width() / 2}")


@check(mouse=True)
def c2(m):
    four()
    rdrag(m, *centre(rect("T2")), 0.10 * width())
    expect_saved((0.35, 0.35), "Left 10% right")


@check(mouse=True)
def c3(m):
    four()
    r = rect("T1")
    rdrag(m, r[0] + r[2] * 0.75, r[1] + r[3] / 2, 0.10 * width())
    expect_saved((0.15, 0.15), "Master's right half 10% right")
    sr()
    rdrag(m, *centre(rect("T3")), -0.10 * width())
    expect_saved((0.35, 0.35), "Right 10% left")


@check()
def c4():
    four()
    n.focus("T2")
    n.bind("SUPER + ALT + H")
    expect_saved((0.25 - STEP, 0.25 - STEP), "SUPER+ALT+H on Left")
    sr()
    n.focus("T2")
    n.msg("resize wider")
    expect_saved((0.25 + STEP, 0.25 + STEP), "resize wider on Left")
    sr()
    n.focus("T1")
    n.msg("resize wider")
    expect_saved((0.25 - STEP / 2, 0.25 - STEP / 2), "resize wider on Master")
    sr()
    n.focus("T1")
    n.bind("SUPER + ALT + H")
    expect_saved((0.25 + STEP / 2, 0.25 + STEP / 2), "SUPER+ALT+H on Master")


@check()
def c5():
    four()
    dx = round(0.10 * width())
    for t in ("T2", "T1", "T3"):
        sr()
        n.focus(t)
        n.d(f"hl.dsp.window.resize({{ x = {dx}, y = 0, relative = true }})")
        got = saved()
        expect(got and abs(got[0] - got[1]) <= TOL and abs(got[0] - 0.25) > TOL,
               f"window.resize x = {dx} on {t}: saved {fmt(got)}, want both sides moved equally")
        ok, cx = master_centred()
        expect(ok, f"window.resize on {t}: Master's centre at {cx}")


@check(mouse=True)
def c6(m):
    four()
    x, y = centre(rect("T2"))
    rdrag(m, x, y, 20 - x)  # far past min_width
    expect_saved((0.10, 0.10), "Left dragged past min_width")
    ok, cx = master_centred()
    expect(ok, f"past min_width: Master's centre at {cx}")
    sr()
    x, y = centre(rect("T2"))
    rdrag(m, x, y, 0.25 * width())  # Master would be 0.10 < min_master_width
    expect_saved((0.40, 0.40), "Left dragged past min_master_width")
    ok, cx = master_centred()
    expect(ok, f"past min_master_width: Master's centre at {cx}")


@check(mouse=True)
def c7(m):
    results = {}
    for mode, opts in (("on", ""), ("off", "centre_master = false")):
        if mode == "off":
            reset()
        cfg(opts)
        start = four()
        r = rect("T3")
        rdrag(m, r[0] + r[2] / 2, r[1] + r[3] - 15, 0, 0.10 * height())
        down = n.geo(1)
        r = rect("T3")
        rdrag(m, r[0] + r[2] / 2, r[1] + r[3] - 15, 0, -0.20 * height())
        up = n.geo(1)
        results[mode] = (down, up)
        expect(down["T3"][3] > start["T3"][3] + PX and up["T3"][3] < start["T3"][3] - PX,
               f"centre_master {mode}: Row edge didn't follow: start {start['T3']}, down {down['T3']}, up {up['T3']}")
        expect(all(down[t][0] == start[t][0] and down[t][2] == start[t][2] for t in start),
               f"centre_master {mode}: a Row drag moved a Divider: {start} -> {down}")
    expect(same(results["on"][0], results["off"][0]) and same(results["on"][1], results["off"][1]),
           f"on {results['on']}, off {results['off']}")


@check()
def c8():
    cfg("", rule(3, "right_rows = 0"))
    windows(3, ["T1", "T2"])
    want = "workspace 3: centre_master needs both side zones, but right_rows is 0; using centre_master = false"
    expect(want in n.warnings(), f"want {want!r}, notice lines {n.warnings()!r}")
    m0 = rect("T1")
    n.focus("T2")
    n.msg("resize wider")
    got, m1 = saved(3), rect("T1")
    expect(got and near((got[0],), (0.25 + STEP,)), f"workspace 3 Left resize: saved {fmt(got)}, want left 0.30")
    expect(abs((m1[0] + m1[2]) - (m0[0] + m0[2])) <= PX and m1[0] > m0[0] + PX,
           f"Master {m0} -> {m1}: want only its left edge to move")
    close_all()
    n.clear_notes()
    cfg("right_rows = 0")
    windows(1, ["T1", "T2"])
    want = "centre_master needs both side zones, but right_rows is 0; using centre_master = false"
    expect(want in n.warnings(), f"want {want!r}, notice lines {n.warnings()!r}")


@check()
def c9():
    dflt = four()
    cfg("left_width = 0.20, right_width = 0.30")
    want = "centre_master needs equal side widths, but left_width is 0.2 and right_width is 0.3; using 0.25 each"
    expect(want in n.warnings(), f"want {want!r}, notice lines {n.warnings()!r}")
    expect(same(dflt, n.geo(1)), f"layout {n.geo(1)}, want 0.25 | 0.50 | 0.25 {dflt}")


def unequal():
    """Workspace 1 with centre_master off and Left resized: Left 0.30, Right 0.25. Returns Master's width."""
    four()
    n.focus("T2")
    n.msg("resize wider")
    return rect("T1")[2]


def expect_centred(mw, what, entry=True):
    ok, cx = master_centred()
    expect(ok and abs(rect("T1")[2] - mw) <= PX,
           f"{what}: Master width {rect('T1')[2]} (was {mw}), centre {cx} (screen {width() / 2})")
    lw, rw = side_widths()
    expect(abs(lw - rw) <= PX, f"{what}: side widths {lw} and {rw}")
    if entry:
        expect_saved((0.275, 0.275), what)


@check()
def c10():
    # By config reload, with saved Dividers.
    cfg("centre_master = false")
    mw = unequal()
    expect_saved((0.25 + STEP, 0.25), "set-up")
    cfg("")
    expect_centred(mw, "reload on")
    cfg("centre_master = false")
    expect_centred(mw, "reload off again")

    # By runtime hl.config, with saved Dividers.
    reset()
    cfg("centre_master = false")
    mw = unequal()
    setting("centre_master = true")
    expect_centred(mw, "runtime on")
    setting("centre_master = false")
    expect_centred(mw, "runtime off again")

    # By runtime hl.config, Kept sizes only.
    reset()
    cfg("centre_master = false, persist = false")
    mw = unequal()
    setting("centre_master = true")
    expect_centred(mw, "runtime on, persist off", entry=False)
    setting("centre_master = false")
    expect_centred(mw, "runtime off again, persist off", entry=False)

    # A workspace that isn't open: centred and rewritten when first laid out.
    reset()
    cfg("centre_master = false")
    windows(4, ["A1", "A2", "A3"])
    n.focus("A2")
    n.msg("resize wider")
    close_all()
    n.goto(1)
    cfg("")
    expect(near(saved(4), (0.25 + STEP, 0.25)), f"workspace 4 entry {fmt(saved(4))} changed before it was laid out")
    windows(4, ["A1", "A2", "A3"], fresh=False)
    expect(near(saved(4), (0.275, 0.275)), f"workspace 4 entry {fmt(saved(4))} after it was laid out, want 0.275 0.275")
    ok, cx = master_centred("A1")
    expect(ok, f"workspace 4: Master's centre at {cx}")


@check()
def c11():
    dflt = four()
    cfg("left_width = 0.20, right_width = 0.30")
    n.clear_notes()
    n.focus("T2")
    n.msg("resize wider")
    expect(not same(dflt, n.geo(1)), "resize didn't take")
    sr()
    expect(same(dflt, n.geo(1)), f"after SUPER+R {n.geo(1)}, want 0.25 | 0.50 | 0.25 {dflt}")
    expect(not n.notes(), f"new notification: {n.notes()!r}")


@check()
def c12():
    for what, opts, rules in (("globally", "centre_master = false", ""),
                              ("layout_opts false", "", rule(1, "centre_master = false")),
                              ("layout_opts 0", "", rule(1, "centre_master = 0"))):
        reset()
        cfg(opts, rules)
        start = four()
        n.focus("T2")
        n.msg("resize wider")
        g = n.geo(1)
        expect_saved((0.25 + STEP, 0.25), what)
        expect(g["T3"][0] == start["T3"][0] and g["T1"][0] > start["T1"][0] + PX,
               f"{what}: want only Left|Master to move: {start} -> {g}")
        expect(not n.warnings(), f"{what}: warnings {n.warnings()!r}")


# ---- #46 leftovers (from #52) ----

@check(mouse=True)
def l1(m):
    cfg("left_rows = 0, centre_master = false")
    start = windows(1, ["T1", "T2", "T3"])
    r = start["T1"]
    travel, _ = rdrag(m, r[0] + r[2] * 0.25, r[1] + r[3] / 2, -0.10 * width())
    g = n.geo(1)
    expect(abs(g["T1"][0] - r[0]) <= PX, f"Master's left edge moved: {r} -> {g['T1']}")
    moved = (g["T1"][0] + g["T1"][2]) - (r[0] + r[2])
    expect(abs(moved - travel) <= 2 * PX, f"Master|Right moved {moved} px, the cursor {travel:.0f} px")


@check(mouse=True)
def l2(m):
    cfg("left_rows = 1, right_rows = 2")
    four()
    r = rect("T1")
    x, y = r[0] + r[2] * 0.25, r[1] + r[3] / 2
    m.press(x, y, m.BTN_RIGHT)
    m.glide_to(x - 0.05 * width(), y)
    cfg("left_rows = 1, right_rows = 1")  # floats T4 mid-drag
    m.glide_to(x - 0.08 * width(), y)
    m.release()
    time.sleep(0.4)
    expect(n.by_title("T4")["floating"], "the reload didn't float T4")
    after = n.geo(1)
    tx, ty = x + 0.15 * width(), y + 0.10 * height()
    end = m.glide_to(tx, ty)  # no button held
    time.sleep(0.4)
    expect(same(after, n.geo(1)), f"moving the pointer resized: {after} -> {n.geo(1)}")
    expect(abs(end[0] - tx) <= PX and abs(end[1] - ty) <= PX, f"pointer stuck at {end}, want {tx:.0f},{ty:.0f}")


@check(mouse=True)
def l3(m):
    cfg("left_rows = 1, right_rows = 2")
    four()
    n.focus("T4")
    base = n.shot("base")
    x, y = centre(rect("T4"))
    m.press(x, y, m.BTN_LEFT)
    tx, ty = centre(rect("T1"))
    m.glide_to(tx, ty)
    cfg("left_rows = 1, right_rows = 1")  # T4 has no room now
    others = {t: r for t, r in n.geo(1).items() if t != "T4"}
    m.release()
    time.sleep(0.6)
    if not expect(n.alive(), "Hyprland is gone"):
        return
    t4 = n.by_title("T4")
    expect(t4 and t4["floating"], f"T4 isn't floating: {t4}")
    if t4:
        (ax, ay), (w, h) = t4["at"], t4["size"]
        expect(ax <= tx <= ax + w and ay <= ty <= ay + h, f"T4 at {t4['at']} size {t4['size']}, dropped at {tx:.0f},{ty:.0f}")
    after = {t: r for t, r in n.geo(1).items() if t != "T4"}
    expect(same(others, after), f"other windows moved: {others} -> {after}")
    left = n.magenta_gain(base, n.shot("after"))
    expect(left < 0.02, f"drop indicator still shown ({left:.1%} of pixels turned magenta; see shots/after.ppm)")


# ---- runner ----

if __name__ == "__main__":
    args = sys.argv[1:]
    allow_mouse = "--mouse" in args
    names = [a for a in args if a != "--mouse"]
    if names == ["all"]:
        # Not g1: it rebuilds the .so the nested instance has loaded. Run it first, on its own.
        names = [k for k, (_, mse, nst) in CHECKS.items() if nst and (allow_mouse or not mse)]
    unknown = [x for x in names if x not in CHECKS]
    if unknown or not names:
        sys.exit(f"unknown check(s) {unknown}; known: {' '.join(CHECKS)}")
    refused = [x for x in names if CHECKS[x][1] and not allow_mouse]
    if refused:
        sys.exit(f"{' '.join(refused)} move the real pointer; run them with --mouse, and only with the author's OK")
    if any(CHECKS[x][2] for x in names):
        sig = os.path.join(S, "sig.txt")
        n.pick(open(sig).read().strip() if os.path.exists(sig) else None)
    try:
        for x in names:
            run_check(x)
    except Stop as e:
        sys.exit(f"stopped: {e}")
