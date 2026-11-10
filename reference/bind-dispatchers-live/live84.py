"""Live checks for #84: the threecol dispatchers on tiles, floating and
fullscreen windows, Step resize on a floating window, and looping moves.

    python3 -I live84.py all | <id>...

Default settings: four() gives Master T1, Left T2 (Row limit 1), Stack T3
above T4, so the Chain is T2, T1, T3, T4. Switches change with runtime
hl.config (c.setting), never a reload. Expected places are written out by
hand from the rules in the README, not from the plugin's code. No check moves
the mouse or presses a key.
"""
import os, sys, time

S = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, S)
import checks as c
import nest as n

expect, same, four, float_window, rect, centre, PX = c.expect, c.same, c.four, c.float_window, c.rect, c.centre, c.PX
MINE = []
STEP = 0.05  # the default `step`


def check(fn):
    MINE.append(fn.__name__)
    return c.check()(fn)


def b(v):
    return "true" if v else "false"


def wrap_move(on):
    """Set wrap.move at runtime, the other wrap switches on, and check it took."""
    c.setting(f"wrap = {{ focus = true, swap = true, move = {b(on)}, swap_next = true, cycle_next = true }}")
    got = n.option("plugin:threecol:wrap:move").splitlines()[0]
    expect(got == f"bool: {b(on)}", f"wrap:move reads {got!r}, want {b(on)}")


def move_dir(d):
    return n.d(f"hl.dsp.window.move({{ direction = '{d}' }})")


def swapped(before, a, b):
    after = dict(before)
    after[a], after[b] = before[b], before[a]
    return after


def column(g, t):
    """The x of `t`'s column."""
    return g[t][0]


def work_area():
    """The nested output's size, which Step resize takes its share of. Gaps
    (5 px) change a 0.05 share by well under a pixel."""
    return c.width(), c.height()


def with_floating(extra=None):
    """four(), and floating F, focused; `extra` floating windows placed left of it."""
    start = four()
    for t in ["F"] + (extra or []):
        n.spawn(1, t)
        float_window(t)
    n.d(f"hl.dsp.window.move({{ x = 1200, y = 200, relative = false, window = '{n.addr('F')}' }})")
    n.d(f"hl.dsp.window.resize({{ x = 800, y = 300, relative = false, window = '{n.addr('F')}' }})")
    for t in extra or []:
        n.d(f"hl.dsp.window.move({{ x = 100, y = 200, relative = false, window = '{n.addr(t)}' }})")
    time.sleep(0.3)
    n.focus("F")
    expect(n.by_title("F")["floating"] and n.active() == "F", f"F isn't a focused floating window: {n.by_title('F')}")
    return start


# ---- the four threecol dispatcher rows ----

@check
def rows_on_a_tile():
    """focus l, resize wider / narrower, resize taller / shorter and reset, on tiles."""
    start = four()
    for m, who, want in [("focus l", "T1", "T2"), ("focus l", "T2", "T4"), ("focus r", "T4", "T2"), ("focus d", "T3", "T4"), ("focus u", "T3", "T4")]:
        n.focus(who)
        n.msg(m)
        expect(n.active() == want, f"{m} from {who}: on {n.active()}, want {want}")
    expect(same(start, n.geo(1)), f"focus moved tiles: {n.geo(1)}")

    W, H = work_area()
    n.focus("T2")
    n.msg("resize wider")
    g = n.geo(1)
    expect(abs(g["T2"][2] - start["T2"][2] - STEP * W) <= 2 * PX, f"resize wider on Left: width {start['T2'][2]} -> {g['T2'][2]}, want +{STEP * W:.0f}")
    n.msg("resize narrower")
    expect(same(start, n.geo(1)), f"resize narrower didn't undo wider: {n.geo(1)}")

    n.focus("T3")
    n.msg("resize taller")
    g = n.geo(1)
    expect(abs(g["T3"][3] - start["T3"][3] - STEP * H) <= 2 * PX, f"resize taller on T3: height {start['T3'][3]} -> {g['T3'][3]}, want +{STEP * H:.0f}")
    expect(abs(g["T4"][3] - start["T4"][3] + STEP * H) <= 2 * PX, f"resize taller on T3: T4 didn't give way: {g['T4']}")
    n.msg("resize shorter")
    expect(same(start, n.geo(1)), f"resize shorter didn't undo taller: {n.geo(1)}")

    n.focus("T2")
    n.msg("resize wider")
    n.focus("T3")
    n.msg("resize taller")
    expect(not same(start, n.geo(1)), "nothing to reset")
    n.msg("reset")
    expect(same(start, n.geo(1)), f"reset: {n.geo(1)}, want {start}")


@check
def rows_on_a_floating_window():
    """focus l is Hyprland's; Step resize changes F around its centre, never the tiles; reset resets the tiles."""
    start = with_floating(["G"])
    n.msg("focus l")
    expect(n.active() == "G", f"focus l from floating F: on {n.active()}, want Hyprland's pick G")
    n.focus("F")

    W, H = work_area()
    for m, i, sign in [("resize wider", 2, 1), ("resize narrower", 2, -1), ("resize taller", 3, 1), ("resize shorter", 3, -1)]:
        before = rect("F")
        n.msg(m)
        time.sleep(0.2)
        after = rect("F")
        share = STEP * (W if i == 2 else H)
        expect(abs(after[i] - before[i] - sign * share) <= PX, f"{m}: F {before} -> {after}, want {'+' if sign > 0 else '-'}{share:.0f}")
        expect(all(abs(a - b) <= PX for a, b in zip(centre(before), centre(after))), f"{m}: centre {centre(before)} -> {centre(after)}")
        expect(same(start, n.geo(1)), f"{m} on F moved tiles: {n.geo(1)}")
        expect(n.active() == "F" and n.by_title("F")["floating"], f"{m}: F no longer focused and floating")

    n.focus("T2")
    n.msg("resize wider")
    expect(not same(start, n.geo(1)), "resize wider on T2 did nothing")
    n.focus("F")
    n.msg("reset")
    expect(same(start, n.geo(1)), f"reset from F: {n.geo(1)}, want {start}")


@check
def floating_shrinks_to_one_step():
    """resize narrower on F stops at one step of the width, centre kept, and then does nothing."""
    with_floating()
    W, _ = work_area()
    first = rect("F")
    for _ in range(12):
        n.msg("resize narrower")
        time.sleep(0.1)
    time.sleep(0.2)
    last = rect("F")
    print(f"  F {first} -> {last}, one step {STEP * W:.0f} px")
    expect(abs(last[2] - STEP * W) <= PX, f"F stopped at width {last[2]}, want one step, {STEP * W:.0f}")
    expect(all(abs(a - b) <= PX for a, b in zip(centre(first), centre(last))), f"centre {centre(first)} -> {centre(last)}")
    n.msg("resize narrower")
    time.sleep(0.2)
    expect(rect("F") == last, f"another narrower changed F: {rect('F')}")


def with_fullscreen():
    four()
    n.focus("T2")
    n.d(f"hl.dsp.window.fullscreen({{ action = 'set', window = '{n.addr('T2')}' }})")
    time.sleep(0.3)
    expect(n.by_title("T2")["fullscreen"], "T2 isn't fullscreen")


@check
def focus_under_fullscreen():
    """focus l / r / u / d under fullscreen run Hyprland's direction focus:
    the same as focus({ direction }) with binds.focus off."""
    seen = {}
    for how in ("message", "hyprland"):
        for d, m in [("left", "focus l"), ("right", "focus r"), ("up", "focus u"), ("down", "focus d")]:
            c.reset()
            with_fullscreen()
            if how == "message":
                n.msg(m)
            else:
                c.setting("binds = { focus = false }")
                n.d(f"hl.dsp.focus({{ direction = '{d}' }})")
            time.sleep(0.2)
            seen[how, d] = (n.active(), n.by_title("T2")["fullscreen"])
    for d in ["left", "right", "up", "down"]:
        expect(seen["message", d] == seen["hyprland", d], f"focus {d} under fullscreen: message {seen['message', d]}, Hyprland's {seen['hyprland', d]}")
    print("  " + ", ".join(f"{d}: {seen['message', d][0]}" for d in ["left", "right", "up", "down"]))
    # threecol's own would follow the Chain from T2: left to T4, right to T1.
    expect(seen["message", "left"][0] != "T4" or seen["message", "right"][0] != "T1", "focus under fullscreen followed the Chain")


# ---- window.move ----

def moves(send):
    """The looping move cases on four(), with `send(who, d)` making the move."""
    start = four()
    # Off the left end: T2 joins the bottom of the Stack, leaving a Left Hole.
    send("T2", "left")
    g = n.geo(1)
    expect(column(g, "T2") == column(g, "T3") and g["T2"][1] > g["T4"][1], f"T2 left: {g}, want T2 at the Stack's bottom")
    expect(g["T1"] == start["T1"], f"T2 left moved Master: {g['T1']}")
    expect(n.active() == "T2", f"T2 left: focus on {n.active()}")
    # Off the right end into that Left Hole: back where it was.
    send("T2", "right")
    expect(same(start, n.geo(1)), f"T2 right into the Left Hole: {n.geo(1)}, want {start}")
    # Off the right end with Left full (Row limit 1): T4 swaps with T2.
    send("T4", "right")
    expect(same(swapped(start, "T4", "T2"), n.geo(1)), f"T4 right: {n.geo(1)}, want T4 and T2 swapped")
    # Off the left end again: the Stack has room, so T4 joins its bottom as a
    # new Row rather than swapping back, leaving a Left Hole.
    send("T4", "left")
    g = n.geo(1)
    expect(column(g, "T4") == column(g, "T3") and g["T4"][1] > g["T2"][1] > g["T3"][1], f"T4 left: {g}, want Stack T3, T2, T4")
    c.close_all()
    start = four()
    # Up / down at the Stack's edges swap with the other edge.
    send("T4", "down")
    expect(same(swapped(start, "T4", "T3"), n.geo(1)), f"T4 down: {n.geo(1)}, want T4 and T3 swapped")
    send("T4", "up")
    expect(same(start, n.geo(1)), f"T4 up (now top): {n.geo(1)}, want {start}")
    # Up / down on Master and on the lone Left Row: nothing.
    for who, d in [("T1", "up"), ("T1", "down"), ("T2", "up"), ("T2", "down")]:
        send(who, d)
        expect(same(start, n.geo(1)), f"{who} {d}: {n.geo(1)}, want nothing moved")


def no_loop_moves(send):
    """At the ends, with looping off, nothing moves; inside, moves still work."""
    start = four()
    for who, d in [("T2", "left"), ("T4", "right"), ("T4", "down"), ("T3", "up")]:
        send(who, d)
        expect(same(start, n.geo(1)), f"{who} {d} with looping off: {n.geo(1)}, want nothing moved")
    send("T3", "down")
    expect(same(swapped(start, "T3", "T4"), n.geo(1)), f"T3 down inside the Stack: {n.geo(1)}")


def by_dispatcher(who, d):
    n.focus(who)
    move_dir(d)


def by_message(suffix=""):
    def send(who, d):
        n.focus(who)
        n.msg(f"move {d[0]}{suffix}")
    return send


@check
def move_loops():
    """window.move({ direction }) loops, wrap.move on (the default)."""
    wrap_move(True)
    moves(by_dispatcher)


@check
def move_no_wrap():
    """window.move({ direction }) with wrap.move off stops at the ends."""
    wrap_move(False)
    no_loop_moves(by_dispatcher)


@check
def move_message_loops():
    """move l / r / u / d loop like window.move; noloop and wrap.move off stop them."""
    moves(by_message())
    c.reset()
    no_loop_moves(by_message(" noloop"))
    c.reset()
    wrap_move(False)
    no_loop_moves(by_message())


@check
def move_left_missing():
    """left_rows = 0: Master is the Chain's left end. Off it, T1 joins the Stack's
    bottom, leaving a Master Hole; off the right end it fills that Hole."""
    c.cfg("left_rows = 0")
    start = c.windows(1, ["T1", "T2", "T3"])
    expect(start["T1"][0] < start["T2"][0] and start["T2"][0] == start["T3"][0], f"not Master T1, Right T2 T3: {start}")
    by_dispatcher("T1", "left")
    g = n.geo(1)
    expect(column(g, "T1") == column(g, "T2") and g["T1"][1] > g["T3"][1], f"T1 left: {g}, want T1 at the Stack's bottom")
    by_dispatcher("T1", "right")
    expect(same(start, n.geo(1)), f"T1 right into the Master Hole: {n.geo(1)}, want {start}")
    by_dispatcher("T3", "right")
    expect(same(swapped(start, "T3", "T1"), n.geo(1)), f"T3 right: {n.geo(1)}, want T3 and T1 swapped")


@check
def move_floating_and_fullscreen():
    """window.move on a floating window or under fullscreen leaves the tiles alone."""
    start = with_floating()
    f = rect("F")
    for d in ["left", "right", "up", "down"]:
        n.msg(f"move {d[0]}")
        expect(same(start, n.geo(1)) and rect("F") == f, f"move {d[0]} on F: tiles {n.geo(1)}, F {rect('F')}")
    c.reset()
    with_fullscreen()
    before = n.geo(1)
    for d in ["left", "right", "up", "down"]:
        move_dir(d)
        n.msg(f"move {d[0]}")
        expect(same(before, n.geo(1)) and n.by_title("T2")["fullscreen"], f"move {d} under fullscreen: {n.geo(1)}")


if __name__ == "__main__":
    names = MINE if sys.argv[1:] == ["all"] else sys.argv[1:]
    n.pick(open(os.path.join(S, "sig.txt")).read().strip())
    for x in names:
        c.run_check(x)
