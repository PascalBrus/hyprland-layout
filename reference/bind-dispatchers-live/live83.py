"""Live checks for #83: direction focus and swap, their switches, looping and edges.

    python3 -I live83.py all | <id>...

Default settings: four() gives Master T1, Left T2, Stack T3 above T4, so the
Chain is T2, T1, T3, T4. Switches change with runtime hl.config (c.setting),
never a reload, so each check also shows they take effect straight away.

What threecol should do is worked out from the tiles' positions alone (the
spec: the Chain is Left, Master, Right, each top to bottom), not from the
plugin's code. Where Hyprland's behaviour runs, the checks compare runs with
the switches on and off: they must match.
"""
import json, os, sys, time

S = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, S)
import checks as c
import nest as n

expect, same, four, float_window = c.expect, c.same, c.four, c.float_window
MINE = []
DIRS = ["left", "right", "up", "down"]


def check(fn):
    MINE.append(fn.__name__)
    return c.check()(fn)


def switches(focus=True, swap=True, wrap_focus=True, wrap_swap=True):
    """Set binds.focus / swap and wrap.focus / swap at runtime, and check they took."""
    b = lambda v: "true" if v else "false"
    c.setting(f"binds = {{ focus = {b(focus)}, swap = {b(swap)} }}, wrap = {{ focus = {b(wrap_focus)}, swap = {b(wrap_swap)} }}")
    for name, want in [("binds:focus", focus), ("binds:swap", swap), ("wrap:focus", wrap_focus), ("wrap:swap", wrap_swap)]:
        got = n.option("plugin:threecol:" + name).splitlines()[0]
        expect(got == f"bool: {b(want)}", f"{name} reads {got!r}, want {b(want)}")


def focus_dir(d):
    return n.d(f"hl.dsp.focus({{ direction = '{d}' }})")


def swap_dir(d):
    return n.d(f"hl.dsp.window.swap({{ direction = '{d}' }})")


def swapped(before, a, b):
    after = dict(before)
    after[a], after[b] = before[b], before[a]
    return after


def model(geo):
    """(Chain, zone of each tile) from positions: zones by x, the Chain by (x, y)."""
    chain = sorted(geo, key=lambda t: (geo[t][0], geo[t][1]))
    xs = sorted({geo[t][0] for t in geo})
    zone = {t: xs.index(geo[t][0]) for t in geo}
    return chain, zone


def expected(geo, who, d, wrap):
    """Where threecol's focus in direction d goes from `who`, or None. Up or
    down, Master is alone in its zone, so it goes nowhere."""
    chain, zone = model(geo)
    if d in ("left", "right"):
        order, forward = chain, d == "right"
    else:
        order = sorted((t for t in geo if zone[t] == zone[who]), key=lambda t: geo[t][1])
        forward = d == "down"
    if len(order) < 2:
        return None
    i = order.index(who) + (1 if forward else -1)
    if 0 <= i < len(order):
        return order[i]
    return order[i % len(order)] if wrap else None


# ---- threecol's behaviour ----

def threecol_focus(wrap):
    start = four()
    switches(wrap_focus=wrap)
    for who in start:
        for d in DIRS:
            n.focus(who)
            focus_dir(d)
            want = expected(start, who, d, wrap) or who
            expect(n.active() == want, f"focus {d} from {who} (wrap.focus {wrap}): on {n.active()}, want {want}")
    expect(same(start, n.geo(1)), f"focus moved tiles: {n.geo(1)}")


@check
def focus_loops():
    """binds.focus on, wrap.focus on: all 16 window x direction cases, loops at both Chain ends and a zone's ends."""
    threecol_focus(True)


@check
def focus_no_wrap():
    """wrap.focus off: the same 16 cases, stopping at the ends."""
    threecol_focus(False)


def threecol_swap(wrap):
    four()
    switches(wrap_swap=wrap)
    for who in ["T1", "T2", "T3", "T4"]:
        for d in DIRS:
            before = n.geo(1)
            n.focus(who)
            swap_dir(d)
            partner = expected(before, who, d, wrap)
            want = swapped(before, who, partner) if partner else before
            got = n.geo(1)
            expect(same(want, got), f"swap {d} from {who} (wrap.swap {wrap}): {got}, want partner {partner}")
            expect(n.active() == who, f"swap {d} from {who}: focus on {n.active()}")


@check
def swap_loops():
    """binds.swap on, wrap.swap on: 16 swaps in a row, each against the positions before it."""
    threecol_swap(True)


@check
def swap_no_wrap():
    """wrap.swap off: 16 swaps in a row, none past an end."""
    threecol_swap(False)


@check
def focus_message_noloop():
    """focus l/r/u/d loops; noloop, or wrap.focus off, stops that one message."""
    start = four()
    for m, who, want in [("focus l", "T2", "T4"), ("focus r", "T4", "T2"), ("focus d", "T4", "T3"), ("focus u", "T3", "T4"),
                         ("focus l noloop", "T2", "T2"), ("focus r noloop", "T4", "T4"), ("focus d noloop", "T4", "T4"),
                         ("focus u noloop", "T3", "T3"), ("focus r noloop", "T2", "T1"), ("focus d noloop", "T3", "T4")]:
        n.focus(who)
        n.msg(m)
        expect(n.active() == want, f"{m} from {who}: on {n.active()}, want {want}")
    switches(wrap_focus=False)
    for m, who in [("focus l", "T2"), ("focus r", "T4"), ("focus d", "T4"), ("focus u", "T3")]:
        n.focus(who)
        n.msg(m)
        expect(n.active() == who, f"{m} from {who}, wrap.focus off: on {n.active()}")
    expect(same(start, n.geo(1)), f"focus moved tiles: {n.geo(1)}")


# ---- Hyprland's behaviour ----

# Hyprland's direction focus on four() where its pick is clear: (from, direction, to).
HYPR_FOCUS = [("T2", "right", "T1"), ("T1", "left", "T2"), ("T3", "left", "T1"), ("T4", "left", "T1"),
              ("T3", "down", "T4"), ("T4", "up", "T3"),
              # Its edges, one monitor: with nothing that way, it looks again in the
              # same direction from the screen's far edge (ConfigActions.cpp:487-534
              # at 0.57.1 and 0.57.0). Gaps keep a full-height window from counting
              # as touching both edges (:497-501), so Master wraps too. Seen live,
              # then checked against the source. All but T4 right differ from threecol's.
              ("T2", "left", "T3"), ("T3", "right", "T2"), ("T4", "right", "T2"), ("T4", "down", "T2"),
              ("T3", "up", "T2"), ("T1", "up", "T2"), ("T1", "down", "T2"), ("T2", "up", "T1"), ("T2", "down", "T1")]


@check
def focus_hyprland():
    """binds.focus off: Hyprland's direction focus, and wrap.focus makes no difference."""
    start = four()
    for wrap in (True, False):
        switches(focus=False, wrap_focus=wrap)
        for who, d, want in HYPR_FOCUS:
            n.focus(who)
            focus_dir(d)
            expect(n.active() == want, f"binds.focus off, wrap {wrap}: focus {d} from {who} on {n.active()}, want {want}")
        n.focus("T1")
        focus_dir("right")
        expect(n.active() in ("T3", "T4"), f"binds.focus off: focus right from T1 on {n.active()}")
    expect(same(start, n.geo(1)), f"focus moved tiles: {n.geo(1)}")


@check
def swap_hyprland():
    """binds.swap off: Hyprland's direction swap, and wrap.swap makes no difference."""
    for wrap in (True, False):
        c.reset()
        four()
        switches(swap=False, wrap_swap=wrap)
        for who, d, partner in [("T3", "right", None), ("T2", "left", None), ("T4", "down", None), ("T1", "up", None),
                                ("T2", "right", "T1"), ("T4", "up", "T3")]:
            before = n.geo(1)
            n.focus(who)
            swap_dir(d)
            want = swapped(before, who, partner) if partner else before
            expect(same(want, n.geo(1)), f"binds.swap off, wrap {wrap}: swap {d} from {who}: {n.geo(1)}, want partner {partner}")


def trace(steps, names):
    """Run `steps` (who, kind, direction) and note where focus is and every
    window's place after each. Windows go by `names`: title -> name."""
    real = {v: k for k, v in names.items()}
    out = []
    for who, kind, d in steps:
        if who:
            n.focus(real[who])
        (focus_dir if kind == "focus" else swap_dir)(d)
        cs = sorted((names[x["title"]], tuple(x["at"]), tuple(x["size"]), x["floating"], x["fullscreen"]) for x in n.clients())
        out.append((who, kind, d, names.get(n.active()), cs))
    return out


def by_place():
    """Names P1, P2, … for the windows by where they start, left to right, top to bottom."""
    cs = sorted(n.clients(), key=lambda x: (x["at"][0], x["at"][1]))
    return {x["title"]: f"P{i}" for i, x in enumerate(cs, 1)}


def by_title():
    return {x["title"]: x["title"] for x in n.clients()}


COMBOS = [dict(), dict(focus=False, swap=False), dict(wrap_focus=False, wrap_swap=False),
          dict(focus=False, swap=False, wrap_focus=False, wrap_swap=False)]


def compare(setup, steps, what, names=by_title):
    """`steps` after `setup` give the same trace with every combination of the switches."""
    traces = []
    for combo in COMBOS:
        c.reset()
        setup()
        switches(**combo)
        traces.append(trace(steps, names()))
    for combo, t in zip(COMBOS[1:], traces[1:]):
        for a, b in zip(traces[0], t):
            if a != b:
                expect(False, f"{what}: {combo} differs at {a[:3]}: {a[3:]} vs {b[3:]}")
                break
    return traces[0]


def with_floating():
    """four(), and floating F and G: Hyprland's focus from a floating window
    goes to floating windows, so G gives it somewhere to go."""
    four()
    for t in ["F", "G"]:
        n.spawn(1, t)
        float_window(t)
    n.d(f"hl.dsp.window.move({{ x = 100, y = 100, relative = false, window = '{n.addr('G')}' }})")
    n.focus("F")


@check
def floating():
    """From a floating window, focus and swap in every direction are Hyprland's, whatever the switches."""
    steps = [(w, "focus", d) for w in ["F", "G"] for d in DIRS] + [("F", "swap", d) for d in DIRS]
    t = compare(with_floating, steps, "floating")
    expect(any(s[3] not in (s[0], None) for s in t[:8]), f"focus never left F or G: {[s[3] for s in t[:8]]}")


def with_fullscreen():
    four()
    n.focus("T2")
    n.d(f"hl.dsp.window.fullscreen({{ action = 'set', window = '{n.addr('T2')}' }})")
    time.sleep(0.3)
    expect(n.by_title("T2")["fullscreen"], "T2 isn't fullscreen")


@check
def fullscreen():
    """Under a fullscreen window, focus and swap are Hyprland's, whatever the switches."""
    steps = [(None, "focus", d) for d in DIRS] + [("T2", "swap", d) for d in DIRS]
    t = compare(with_fullscreen, steps, "fullscreen")
    expect(all(any(w[4] for w in s[4]) for s in t), "fullscreen ended during the steps")
    # threecol's focus would go along the Chain from T2: T1 right, T4 left.
    expect(t[0][3] != "T4" and t[1][3] != "T1", f"focus under fullscreen followed the Chain: {t[0][3]}, {t[1][3]}")


def dwindle():
    c.cfg(rules="hl.workspace_rule({ workspace = '2', layout = 'dwindle' })\n")
    n.goto(2)
    for t in ["D1", "D2", "D3"]:
        n.spawn(2, t)
    expect(n.layout(2) == "dwindle", f"workspace 2 is {n.layout(2)}")


@check
def other_layout():
    """On a dwindle workspace, focus and swap are Hyprland's, whatever the switches."""
    steps = [(w, k, d) for w in ["P1", "P2", "P3"] for k in ("focus", "swap") for d in DIRS]
    t = compare(dwindle, steps, "dwindle", names=by_place)
    expect(len({s[3] for s in t}) > 1, f"focus never moved on dwindle: {[s[3] for s in t]}")


# ---- edges ----

SECOND = "hl.monitor({ output = 'HL2', mode = '1280x720', position = '2560x0', scale = 1 })\n"


def second_monitor():
    """A headless monitor right of the nested one, with window M on it."""
    if not any(m["name"] == "HL2" for m in json.loads(n.hc("monitors", "-j"))):
        n.hc("output", "create", "headless", "HL2")
        time.sleep(0.5)
    c.cfg(rules=SECOND)
    mons = {m["name"]: m for m in json.loads(n.hc("monitors", "-j"))}
    expect(mons["HL2"]["x"] == mons["WAYLAND-1"]["width"], f"HL2 at {mons['HL2']['x']}")
    n.d("hl.dsp.focus({ monitor = 'HL2' })")
    ws = mons["HL2"]["activeWorkspace"]["id"]
    n.spawn(ws, "M")
    n.goto(1)
    return ws


@check
def edges():
    """At threecol's edges nothing hands over to Hyprland: not to the monitor on the right."""
    try:
        second_monitor()
        four()
        # Looping on, from the right end: back to the Chain's start, not to M.
        for kind, who, want in [("focus", "T4", "T2"), ("focus", "T3", "T4")]:
            n.focus(who)
            focus_dir("right")
            expect(n.active() == want, f"focus right from {who}: on {n.active()}, want {want}")
        switches(wrap_focus=False, wrap_swap=False)
        before = n.geo(1)
        for who, d in [("T4", "right"), ("T1", "up"), ("T1", "down"), ("T2", "up"), ("T2", "left"), ("T4", "down")]:
            n.focus(who)
            focus_dir(d)
            expect(n.active() == who, f"wrap off: focus {d} from {who} went to {n.active()}")
            swap_dir(d)
            expect(same(before, n.geo(1)) and n.by_title(who)["workspace"]["id"] == 1, f"wrap off: swap {d} from {who} moved: {n.geo(1)}")
        # Control: Hyprland's own does reach M.
        switches(focus=False, wrap_focus=False)
        n.focus("T4")
        focus_dir("right")
        expect(n.active() == "M", f"control, binds.focus off: focus right from T4 on {n.active()}, want M")
    finally:
        n.hc("output", "remove", "HL2")
        time.sleep(0.5)


# ---- a hook that fails ----

BLOCKER = os.path.join(S, "hookblocker.so")  # built from hookblocker.cpp
PLUGIN = os.path.join(S, "build", "threecol.so")


def load(path):
    r = n.hc("plugin", "load", path)
    time.sleep(0.5)
    return r


@check
def hook_failure():
    """Another plugin holds moveFocus: one notification at load, focus stays
    Hyprland's, and the other three hooks still work."""
    expect(os.path.exists(BLOCKER), f"build {BLOCKER} first (see hookblocker.cpp)")
    c.close_all()
    try:
        n.hc("plugin", "unload", PLUGIN)
        time.sleep(0.5)
        n.clear_notes()
        expect("ok" in load(BLOCKER), "hookblocker didn't load")
        expect("ok" in load(PLUGIN), "threecol didn't load")
        mine = [x for x in n.notes().splitlines() if "[threecol]" in x]
        print("  notification:", mine)
        expect(len(mine) == 1 and "focus({ direction }) keeps Hyprland's" in mine[0] and "swap" not in mine[0] and "cycle_next" not in mine[0],
               f"notifications: {mine}")
        c.cfg("")  # workspace 1 back on threecol
        start = four()
        # Hyprland's focus right from T3 wraps to the screen's left edge: T2.
        # threecol's would go to T4.
        n.focus("T3")
        focus_dir("right")
        expect(n.active() == "T2", f"focus right from T3: on {n.active()}, want Hyprland's T2")
        # swap still threecol's: right from T3 swaps with T4 (Hyprland's: nothing).
        n.focus("T3")
        swap_dir("right")
        expect(same(swapped(start, "T3", "T4"), n.geo(1)), f"swap right from T3: {n.geo(1)}")
    finally:
        c.close_all()
        n.hc("plugin", "unload", PLUGIN)
        n.hc("plugin", "unload", BLOCKER)
        time.sleep(0.5)
        load(PLUGIN)
        n.clear_notes()
        expect("hookblocker" not in n.hc("plugin", "list"), "hookblocker still loaded")


if __name__ == "__main__":
    names = MINE if sys.argv[1:] == ["all"] else sys.argv[1:]
    n.pick(open(os.path.join(S, "sig.txt")).read().strip())
    for x in names:
        c.run_check(x)
