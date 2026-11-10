"""Live checks for #85: the Binds table rows #81 left unchecked (swap next /
prev, cycle_next, the cyclenext / cycleprev / swapnext / swapprev messages),
plus focus after a window closes and the One-off resize.

    python3 -I live85.py all | <id>...
    python3 -I live85.py --mouse drop_swap drop_new_row drop_empty_zone

The drop checks move the author's real pointer (see
../since-46-live/README.md, "(mouse) checks"): ask the author first. `all`
leaves them out; the Drag resize rows are the harness's own c1-c3, c6, c7.

Default settings: four() gives Master T1, Left T2, Stack T3 above T4, so the
Chain is T2, T1, T3, T4, while Hyprland's window order is T1, T2, T3, T4 (the
order they opened). Floating F and G open after them, so the Cycle is T2, T1,
T3, T4, F, G. Switches change with runtime hl.config (c.setting), never a
reload. What threecol should do is worked out from the tiles' positions; where
Hyprland's behaviour runs, runs with the wrap switch on and off must match. No
check moves the mouse or presses a key.
"""
import os, sys, time

S = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, S)
import checks as c
import nest as n

expect, same, four, float_window, rect, PX = c.expect, c.same, c.four, c.float_window, c.rect, c.PX
MINE = []


def check(fn):
    MINE.append(fn.__name__)
    return c.check()(fn)


def mouse_check(fn):
    return c.check(mouse=True)(fn)


def b(v):
    return "true" if v else "false"


def switches(swap_next=True, cycle_next=True, wrap_swap_next=True, wrap_cycle_next=True):
    """Set binds.swap_next / cycle_next and wrap.swap_next / cycle_next at runtime, and check they took."""
    c.setting(f"binds = {{ swap_next = {b(swap_next)}, cycle_next = {b(cycle_next)} }}, "
              f"wrap = {{ swap_next = {b(wrap_swap_next)}, cycle_next = {b(wrap_cycle_next)} }}")
    for name, want in [("binds:swap_next", swap_next), ("binds:cycle_next", cycle_next),
                       ("wrap:swap_next", wrap_swap_next), ("wrap:cycle_next", wrap_cycle_next)]:
        got = n.option("plugin:threecol:" + name).splitlines()[0]
        expect(got == f"bool: {b(want)}", f"{name} reads {got!r}, want {b(want)}")


def swap_next(forward):
    return n.d("hl.dsp.window.swap({ %s = true })" % ("next" if forward else "prev"))


def cycle_next(forward=True, tiled=None, floating=None):
    args = [f"next = {b(forward)}"]
    if tiled is not None:
        args.append(f"tiled = {b(tiled)}")
    if floating is not None:
        args.append(f"floating = {b(floating)}")
    return n.d("hl.dsp.window.cycle_next({ %s })" % ", ".join(args))


def swapped(before, a, b_):
    after = dict(before)
    after[a], after[b_] = before[b_], before[a]
    return after


def chain(geo):
    """The Chain from positions: Left, Master, Right, each top to bottom."""
    return sorted(geo, key=lambda t: (geo[t][0], geo[t][1]))


def step(order, who, forward, wrap):
    """The window after (or before) `who` in `order`, or None at an end without wrap."""
    if len(order) < 2:
        return None
    i = order.index(who) + (1 if forward else -1)
    if 0 <= i < len(order):
        return order[i]
    return order[i % len(order)] if wrap else None


def with_floating():
    """four(), then floating F and G (opened in that order), F focused."""
    start = four()
    for t in ["F", "G"]:
        n.spawn(1, t)
        float_window(t)
    n.d(f"hl.dsp.window.move({{ x = 100, y = 100, relative = false, window = '{n.addr('G')}' }})")
    n.focus("F")
    expect(n.by_title("F")["floating"] and n.by_title("G")["floating"], "F or G isn't floating")
    return start


# ---- swap({ next | prev }) ----

def threecol_swap_next(wrap):
    four()
    switches(wrap_swap_next=wrap)
    for who in ["T1", "T2", "T3", "T4"]:
        for forward in (True, False):
            before = n.geo(1)
            n.focus(who)
            swap_next(forward)
            partner = step(chain(before), who, forward, wrap)
            want = swapped(before, who, partner) if partner else before
            got = n.geo(1)
            expect(same(want, got), f"swap {'next' if forward else 'prev'} from {who} (wrap.swap_next {wrap}): {got}, want partner {partner}")
            expect(n.active() == who, f"swap {'next' if forward else 'prev'} from {who}: focus on {n.active()}")


@check
def swap_next_loops():
    """binds.swap_next on, wrap.swap_next on: 8 swaps in a row, each with the next / previous on the Chain, looping."""
    threecol_swap_next(True)


@check
def swap_next_no_wrap():
    """wrap.swap_next off: the same 8, none past an end of the Chain."""
    threecol_swap_next(False)


def trace(steps, focus_only=False):
    """Run `steps` (who, action) and note focus and every window's place after each."""
    out = []
    for who, act in steps:
        if who:
            n.focus(who)
        act()
        # Tiles' places as shares of the output, rounded: its size can change
        # after a reload (it follows the nested window), which isn't a
        # difference. Floating windows open at a size that follows it too, and
        # no step moves them, so only their state counts.
        W, H = c.width(), c.height()
        frac = lambda xy: (round(xy[0] / W, 2), round(xy[1] / H, 2))
        place = lambda x: ((), ()) if x["floating"] else (frac(x["at"]), frac(x["size"]))
        cs = [] if focus_only else sorted((x["title"], *place(x), x["floating"], x["fullscreen"]) for x in n.clients())
        out.append((who, n.active(), cs))
    return out


def compare(setup, steps, what, combos, focus_only=False):
    """`steps` after `setup` give the same trace with every combination in `combos`."""
    traces = []
    for combo in combos:
        c.reset()
        c.settle_size()
        setup()
        switches(**combo)
        traces.append(trace(steps, focus_only))
    for combo, t in zip(combos[1:], traces[1:]):
        for i, (x, y) in enumerate(zip(traces[0], t)):
            if x != y:
                expect(False, f"{what}: {combo} differs from {combos[0]} at step {i} ({x[0]}): {x[1:]} vs {y[1:]}")
                break
    return traces


SWAP_STEPS = [("T1", lambda: swap_next(True)), ("T1", lambda: swap_next(True)), ("T3", lambda: swap_next(False)),
              ("T4", lambda: swap_next(True)), ("T2", lambda: swap_next(False))]


@check
def swap_next_hyprland():
    """binds.swap_next off: Hyprland's swap next / prev, the same with wrap.swap_next on and off, and not the Chain."""
    off, _ = compare(four, SWAP_STEPS, "swap next, binds off",
                     [dict(swap_next=False), dict(swap_next=False, wrap_swap_next=False)])
    on, = compare(four, SWAP_STEPS, "swap next, binds on", [dict()])
    expect(on != off, "binds.swap_next off changed nothing")
    # From T1, the Chain's next is T3; Hyprland's window order gives T2.
    c.reset()
    start = four()
    switches(swap_next=False)
    n.focus("T1")
    swap_next(True)
    expect(same(swapped(start, "T1", "T2"), n.geo(1)), f"binds off, swap next from T1: {n.geo(1)}, want Hyprland's T2")


# ---- cycle_next() ----

def cycle_order(start):
    return chain(start) + ["F", "G"]


def threecol_cycle(wrap):
    start = with_floating()
    switches(wrap_cycle_next=wrap)
    order = cycle_order(start)
    for who in order:
        for forward in (True, False):
            n.focus(who)
            cycle_next(forward)
            want = step(order, who, forward, wrap) or who
            expect(n.active() == want, f"cycle_next next={forward} from {who} (wrap.cycle_next {wrap}): on {n.active()}, want {want}")
    expect(same(start, n.geo(1)), f"cycle_next moved tiles: {n.geo(1)}")


@check
def cycle_next_loops():
    """binds.cycle_next on, wrap.cycle_next on: from each of the 6 windows both ways along the Cycle, looping."""
    threecol_cycle(True)


@check
def cycle_next_no_wrap():
    """wrap.cycle_next off: the same 12, stopping at the Cycle's ends (T2 back, G forward)."""
    threecol_cycle(False)


@check
def cycle_next_tiled_only():
    """tiled = true: the Chain alone from a tile, looping; from a floating window, Hyprland's."""
    start = with_floating()
    order = chain(start)
    for wrap in (True, False):
        switches(wrap_cycle_next=wrap)
        for who in order:
            for forward in (True, False):
                n.focus(who)
                cycle_next(forward, tiled=True)
                want = step(order, who, forward, wrap) or who
                expect(n.active() == want, f"cycle_next tiled next={forward} from {who} (wrap {wrap}): on {n.active()}, want {want}")
    steps = [("F", lambda: cycle_next(True, tiled=True)), ("G", lambda: cycle_next(False, tiled=True))]
    on, off = compare(with_floating, steps, "cycle_next tiled from floating", [dict(), dict(cycle_next=False)])
    print("  tiled from F, G: " + ", ".join(s[1] for s in on))


@check
def cycle_next_floating_only():
    """floating = true: Hyprland's, whatever the switches. tiled and floating
    both true count as neither, as Hyprland reads them (ConfigActions.cpp:1789-1792):
    the Cycle."""
    steps = [(w, lambda f=f: cycle_next(f, floating=True)) for w in ["T1", "F", "G"] for f in (True, False)]
    on, off, nowrap = compare(with_floating, steps, "cycle_next floating",
                              [dict(), dict(cycle_next=False), dict(wrap_cycle_next=False)])
    print("  " + ", ".join(f"{s[0]}->{s[1]}" for s in on))
    c.reset()
    with_floating()
    for who, want in [("T1", "T3"), ("T4", "F"), ("G", "T2")]:
        n.focus(who)
        cycle_next(True, tiled=True, floating=True)
        expect(n.active() == want, f"cycle_next tiled and floating from {who}: on {n.active()}, want the Cycle's {want}")


CYCLE_STEPS = [(w, lambda f=f: cycle_next(f)) for w in ["T1", "T2", "T4", "F", "G"] for f in (True, False)]


@check
def cycle_next_hyprland():
    """binds.cycle_next off: Hyprland's cycle_next, the same with wrap.cycle_next on and off, and not the Cycle."""
    off, _ = compare(with_floating, CYCLE_STEPS, "cycle_next, binds off",
                     [dict(cycle_next=False), dict(cycle_next=False, wrap_cycle_next=False)])
    on, = compare(with_floating, CYCLE_STEPS, "cycle_next, binds on", [dict()])
    # From T1 next: the Cycle gives T3, Hyprland's window order T2.
    expect(on[0][1] == "T3" and off[0][1] == "T2", f"cycle_next from T1: threecol {on[0][1]}, Hyprland {off[0][1]}; want T3, T2")


# ---- the Chain messages ----

@check
def chain_messages():
    """cyclenext / cycleprev and swapnext / swapprev step along the Chain and loop;
    noloop, wrap.cycle_next off and wrap.swap_next off stop them at an end."""
    start = four()
    order = chain(start)
    for wrap in (True, False):
        switches(wrap_cycle_next=wrap)
        for suffix in ("", " noloop"):
            loops = wrap and not suffix
            for who in order:
                for m in ("cyclenext", "cycleprev"):
                    n.focus(who)
                    n.msg(m + suffix)
                    want = step(order, who, m == "cyclenext", loops) or who
                    expect(n.active() == want, f"{m}{suffix} from {who} (wrap.cycle_next {wrap}): on {n.active()}, want {want}")
    expect(same(start, n.geo(1)), f"cyclenext moved tiles: {n.geo(1)}")

    for wrap in (True, False):
        switches(wrap_swap_next=wrap)
        for suffix in ("", " noloop"):
            loops = wrap and not suffix
            for who in ["T1", "T2", "T3", "T4"]:
                for m in ("swapnext", "swapprev"):
                    before = n.geo(1)
                    n.focus(who)
                    n.msg(m + suffix)
                    partner = step(chain(before), who, m == "swapnext", loops)
                    want = swapped(before, who, partner) if partner else before
                    expect(same(want, n.geo(1)), f"{m}{suffix} from {who} (wrap.swap_next {wrap}): {n.geo(1)}, want partner {partner}")
                    expect(n.active() == who, f"{m}{suffix} from {who}: focus on {n.active()}")


@check
def chain_messages_tiles_only():
    """From a floating window the Chain messages do nothing; from a tile they skip floating windows."""
    start = with_floating()
    f = rect("F")
    for m in ["cyclenext", "cycleprev", "swapnext", "swapprev"]:
        n.focus("F")
        n.msg(m)
        expect(n.active() == "F" and rect("F") == f and same(start, n.geo(1)), f"{m} from F: on {n.active()}, F {rect('F')}, tiles {n.geo(1)}")
    n.focus("T4")
    n.msg("cyclenext")
    expect(n.active() == "T2", f"cyclenext from T4 with floating windows: on {n.active()}, want T2")


@check
def ignored_more():
    """Master and dwindle messages the #82 check didn't send do nothing either, with no notification."""
    start = with_floating()
    f = rect("F")
    for who in ["T1", "T3", "F"]:
        for m in ["rollnext", "rollprev", "rotatesplit", "movetoroot", "splitratio 0.1", "splitratio exact 0.3"]:
            n.focus(who)
            out = n.msg(m)
            expect(out.strip() == "ok", f"{m} on {who}: hyprctl answered {out!r}")
            expect(same(start, n.geo(1)) and rect("F") == f and n.active() == who, f"{m} on {who}: tiles {n.geo(1)}, F {rect('F')}, focus {n.active()}")
    expect(not n.notes(), f"notifications: {n.notes()!r}")


# ---- fullscreen and another layout ----

def with_fullscreen():
    with_floating()
    n.focus("T2")
    n.d(f"hl.dsp.window.fullscreen({{ action = 'set', window = '{n.addr('T2')}' }})")
    time.sleep(0.3)
    expect(n.by_title("T2")["fullscreen"], "T2 isn't fullscreen")


NEXT_STEPS = [(None, lambda: cycle_next(True)), (None, lambda: cycle_next(False)), ("T2", lambda: swap_next(True)), ("T2", lambda: swap_next(False))]
COMBOS = [dict(), dict(swap_next=False, cycle_next=False), dict(wrap_swap_next=False, wrap_cycle_next=False),
          dict(swap_next=False, cycle_next=False, wrap_swap_next=False, wrap_cycle_next=False)]


@check
def fullscreen():
    """Under fullscreen, swap next / prev and cycle_next are Hyprland's, whatever
    the switches. Each step starts from a fresh fullscreen T2: focusing another
    window ends fullscreen, and then threecol's behaviour runs again."""
    for i in range(len(NEXT_STEPS)):
        t = compare(with_fullscreen, NEXT_STEPS[i:i + 1], f"fullscreen step {i}", COMBOS)
        print(f"  step {i}: on {t[0][0][1]}")
    # threecol's Cycle would go T2 -> T1 (next) and T2 -> G (prev).
    c.reset()
    with_fullscreen()
    cycle_next(True)
    expect(n.active() != "T1", "cycle_next under fullscreen followed the Cycle")


def dwindle():
    c.cfg(rules="hl.workspace_rule({ workspace = '2', layout = 'dwindle' })\n")
    n.goto(2)
    for t in ["D1", "D2", "D3", "D4"]:
        n.spawn(2, t)
    expect(n.layout(2) == "dwindle", f"workspace 2 is {n.layout(2)}")


@check
def other_layout():
    """On a dwindle workspace, swap next / prev and cycle_next are Hyprland's, whatever the switches."""
    # Dwindle places windows differently run to run, so only focus is compared.
    steps = [("D1", lambda: cycle_next(True)), ("D3", lambda: cycle_next(False)), ("D4", lambda: cycle_next(True)),
             ("D1", lambda: cycle_next(True, tiled=True))]
    t = compare(dwindle, steps, "dwindle", COMBOS, focus_only=True)
    print("  " + ", ".join(s[1] for s in t[0]))
    # Swaps change places, which dwindle doesn't keep run to run: check only
    # that each one swaps two windows and focus stays.
    for combo in COMBOS:
        c.reset()
        dwindle()
        switches(**combo)
        for who, forward in [("D2", True), ("D2", False)]:
            before = n.geo(2)
            n.focus(who)
            swap_next(forward)
            got = n.geo(2)
            moved = [x for x in got if got[x] != before[x]]
            expect(len(moved) == 2 and n.active() == who, f"dwindle swap next={forward} from {who} {combo}: moved {moved}, focus {n.active()}")


# ---- not a dispatcher: focus after a window closes; One-off resize ----

@check
def close_focus():
    """Closing the focused window focuses the one after it on the Chain, or before it if it was last."""
    for who, want in [("T2", "T1"), ("T1", "T3"), ("T3", "T4"), ("T4", "T3")]:
        c.reset()
        four()
        n.focus(who)
        n.kill(who)
        time.sleep(0.3)
        expect(n.active() == want, f"closed {who}: focus on {n.active()}, want {want}")


@check
def one_off_resize():
    """window.resize({ x, y }) on a tile moves its zone's Divider and its Row
    edge by the amount, as a drag would: +x moves it right, +y down. So on the
    Right zone +x makes it narrower, as in the master layout
    (MasterAlgorithm.cpp:351). With Centred Master, the other Divider mirrors it."""
    start = four()
    n.d(f"hl.dsp.window.resize({{ x = 100, y = 0, relative = true, window = '{n.addr('T4')}' }})")
    time.sleep(0.3)
    g = n.geo(1)
    expect(abs(g["T4"][2] - start["T4"][2] + 100) <= 2 * PX, f"T4 x +100: width {start['T4'][2]} -> {g['T4'][2]}, want -100")
    expect(g["T3"][2] == g["T4"][2], f"T3 didn't follow T4's zone width: {g['T3']} vs {g['T4']}")
    expect(abs(g["T2"][2] - g["T4"][2]) <= 2 * PX, f"Centred Master: Left {g['T2']} didn't mirror Right {g['T4']}")
    c.reset()
    start = four()
    n.d(f"hl.dsp.window.resize({{ x = 100, y = 0, relative = true, window = '{n.addr('T2')}' }})")
    time.sleep(0.3)
    g = n.geo(1)
    expect(abs(g["T2"][2] - start["T2"][2] - 100) <= 2 * PX, f"T2 x +100: width {start['T2'][2]} -> {g['T2'][2]}, want +100")
    c.reset()
    start = four()
    n.d(f"hl.dsp.window.resize({{ x = 0, y = 60, relative = true, window = '{n.addr('T3')}' }})")
    time.sleep(0.3)
    g = n.geo(1)
    expect(abs(g["T3"][3] - start["T3"][3] - 60) <= 2 * PX, f"T3 y +60: height {start['T3'][3]} -> {g['T3'][3]}")
    expect(abs(g["T4"][3] - start["T4"][3] + 60) <= 2 * PX, f"T4 didn't give way: {start['T4']} -> {g['T4']}")


# ---- window.drag(), with the mouse ----

def drag_to(m, who, x, y):
    """Drag `who` from its centre to (x, y) with ALT + left button; the drop
    indicator must show on the way. Returns the tiles' places after the drop."""
    n.focus(who)
    base = n.shot("base")
    m.press(*c.centre(rect(who)), m.BTN_LEFT)
    m.glide_to(x, y)
    time.sleep(0.4)
    seen = n.magenta_gain(base, n.shot("mid"))
    expect(seen > 0.02, f"no drop indicator mid-drag ({seen:.1%} of pixels)")
    m.release()
    time.sleep(0.5)
    left = n.magenta_gain(base, n.shot("after"))
    expect(left < 0.02, f"drop indicator still shown after the drop ({left:.1%})")
    expect(not n.by_title(who)["floating"], f"{who} floats after the drop")
    return n.geo(1)


@mouse_check
def drop_swap(m):
    """Dropping T4 onto T2 swaps them."""
    start = four()
    r = rect("T2")
    g = drag_to(m, "T4", r[0] + r[2] * 0.5, r[1] + r[3] * 0.4)
    expect(same(swapped(start, "T4", "T2"), g), f"T4 onto T2: {g}, want T4 and T2 swapped")


@mouse_check
def drop_new_row(m):
    """Dropping T2 onto the bottom quarter of the Stack adds it as a Row there."""
    start = four()
    r = rect("T4")
    g = drag_to(m, "T2", r[0] + r[2] * 0.5, r[1] + r[3] * 0.9)
    expect(g["T2"][0] == g["T3"][0] and g["T2"][1] > g["T4"][1] > g["T3"][1], f"T2 onto the Stack's bottom: {g}, want Stack T3, T4, T2")
    expect(g["T1"] == start["T1"], f"Master moved: {g['T1']}")


@mouse_check
def drop_empty_zone(m):
    """With Left emptied (T2 closed), dropping T4 into the Left zone puts it there."""
    start = four()
    n.kill("T2")
    time.sleep(0.4)
    x, y = start["T2"][0] + start["T2"][2] / 2, start["T2"][1] + start["T2"][3] / 2
    g = drag_to(m, "T4", x, y)
    expect(abs(g["T4"][0] - start["T2"][0]) <= PX and abs(g["T4"][2] - start["T2"][2]) <= PX, f"T4 into empty Left: {g['T4']}, want Left's place {start['T2']}")


if __name__ == "__main__":
    args = [a for a in sys.argv[1:] if a != "--mouse"]
    mouse_ok = "--mouse" in sys.argv[1:]
    for a in args:
        if a != "all" and c.CHECKS[a][1] and not mouse_ok:
            sys.exit(f"{a} moves the real pointer: ask the author, then pass --mouse")
    names = MINE if args == ["all"] else args
    n.pick(open(os.path.join(S, "sig.txt")).read().strip())
    for x in names:
        c.run_check(x)
