"""Live checks for #82: focusmaster / swapwithmaster arguments, ignored messages.

    python3 -I live82.py all | <id>...

Default settings: four() gives Master T1, Left T2, Stack T3 above T4.
Focus and positions come from `hyprctl clients -j` (nest.active / nest.geo).
"""
import os, sys

S = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, S)
import checks as c
import nest as n

expect, same, four, float_window = c.expect, c.same, c.four, c.float_window
MINE = []


def check(fn):
    MINE.append(fn.__name__)
    return c.check()(fn)


def swapped(before, a, b):
    """`before` with windows a and b trading places."""
    after = dict(before)
    after[a], after[b] = before[b], before[a]
    return after


def focus_is(t, what):
    expect(n.active() == t, f"{what}: focus on {n.active()}, want {t}")


def geo_is(want, what):
    got = n.geo(1)
    expect(same(want, got), f"{what}: tiles {got}, want {want}")


def floating_f():
    n.spawn(1, "F")
    float_window("F")
    n.focus("F")
    expect(n.by_title("F")["floating"] and n.active() == "F", f"F isn't a focused floating window: {n.by_title('F')}")


# ---- focusmaster ----

@check
def fm_from_other():
    """:614-615: every argument focuses Master from another window."""
    start = four()
    for m in ["focusmaster", "focusmaster auto", "focusmaster master", "focusmaster previous"]:
        n.focus("T4")
        n.msg(m)
        focus_is("T1", m)
    geo_is(start, "positions")


@check
def fm_auto_on_master():
    """:622-630, :640: auto on Master focuses the first window after Master in Fill order."""
    four()
    for m in ["focusmaster", "focusmaster auto"]:
        n.focus("T1")
        n.msg(m)
        focus_is("T2", m)


@check
def fm_auto_fill_first_right():
    """With fill_first right, the first window after Master is Right's top Row."""
    c.cfg('fill_first = "right"')
    four()  # T1 Master, T2 T3 T4 Right Stack
    n.focus("T1")
    n.msg("focusmaster")
    focus_is("T2", "fill_first right")
    # T3 to Right's top: the first window after Master is now T3.
    n.focus("T3")
    n.msg("swapprev")
    n.focus("T1")
    n.msg("focusmaster")
    focus_is("T3", "after swapping T3 to the top")


@check
def fm_master_on_master():
    """:632-633: master on Master stays."""
    four()
    n.focus("T1")
    n.msg("focusmaster master")
    focus_is("T1", "focusmaster master")


@check
def fm_previous():
    """:617-618, :635-638: previous goes back where it came from; else auto."""
    four()
    n.focus("T1")
    n.msg("focusmaster previous")
    focus_is("T2", "nothing saved")
    n.focus("T4")
    n.msg("focusmaster previous")
    focus_is("T1", "from T4")
    n.msg("focusmaster previous")
    focus_is("T4", "back")
    # auto doesn't save
    n.focus("T3")
    n.msg("focusmaster")
    n.msg("focusmaster previous")
    focus_is("T4", "auto from T3 left T4 saved")
    # the saved window gone
    n.kill("T4")
    n.focus("T1")
    n.msg("focusmaster previous")
    focus_is("T2", "saved window closed")


@check
def fm_from_floating():
    """focusmaster works from a floating window."""
    start = four()
    floating_f()
    n.msg("focusmaster")
    focus_is("T1", "from F")
    geo_is(start, "positions")


@check
def fm_previous_from_floating():
    """:617-618, :637: previous from a floating window, then previous on Master falls back to auto."""
    four()
    n.focus("T4")
    n.msg("focusmaster previous")
    floating_f()
    n.msg("focusmaster previous")
    focus_is("T1", "from F")
    n.msg("focusmaster previous")
    focus_is("T2", "on Master after F")


# ---- swapwithmaster ----

@check
def sm_from_other():
    """:573-578: auto, master or none swap with Master; focus stays on the window."""
    for m in ["swapwithmaster", "swapwithmaster auto", "swapwithmaster master", "swapwithmaster ignoremaster"]:
        c.reset()
        start = four()
        n.focus("T4")
        n.msg(m)
        geo_is(swapped(start, "T1", "T4"), m)
        focus_is("T4", m)


@check
def sm_child():
    """:575-578: child focuses the old Master, now in the window's Row."""
    start = four()
    n.focus("T3")
    n.msg("swapwithmaster child")
    geo_is(swapped(start, "T1", "T3"), "child")
    focus_is("T1", "child")


@check
def sm_on_master():
    """:579-589: on Master, swap with the first window after Master; master focuses the new Master."""
    for m, want in [("swapwithmaster", "T1"), ("swapwithmaster auto", "T1"), ("swapwithmaster child", "T1"),
                    ("swapwithmaster master", "T2")]:
        c.reset()
        start = four()
        n.focus("T1")
        n.msg(m)
        geo_is(swapped(start, "T1", "T2"), m)
        focus_is(want, m)


@check
def sm_ignoremaster_on_master():
    """:571, :579: ignoremaster on Master does nothing."""
    start = four()
    for m in ["swapwithmaster ignoremaster", "swapwithmaster master ignoremaster", "swapwithmaster child ignoremaster"]:
        n.focus("T1")
        n.msg(m)
        geo_is(start, m)
        focus_is("T1", m)


@check
def sm_into_hole():
    """threecol's own: into a Master Hole the window moves; child has no old Master, so focus stays."""
    for m in ["swapwithmaster", "swapwithmaster child"]:
        c.reset()
        start = four()
        n.kill("T1")  # Master Hole
        n.focus("T4")
        n.msg(m)
        got = n.geo(1)
        expect(same({"T4": start["T1"]}, {"T4": got.get("T4")}), f"{m}: T4 at {got.get('T4')}, want Master {start['T1']}")
        focus_is("T4", m)


@check
def sm_from_floating():
    """:559-560: not from a floating window."""
    start = four()
    floating_f()
    f = n.by_title("F")["at"]
    for m in ["swapwithmaster", "swapwithmaster child", "swapwithmaster master"]:
        n.msg(m)
        geo_is(start, m)
        focus_is("F", m)
        expect(n.by_title("F")["floating"] and n.by_title("F")["at"] == f, f"{m}: F changed {n.by_title('F')}")


# ---- ignored ----

IGNORED = ["addmaster", "removemaster", "orientationleft", "orientationright", "orientationtop", "orientationbottom",
           "orientationcenter", "orientationnext", "orientationprev", "orientationcycle left top", "mfact 0.1",
           "mfact exact 0.5", "togglesplit", "swapsplit", "preselect l"]


@check
def ignored():
    """Ignored messages change nothing, with no notification, from Master, a side zone and a floating window."""
    start = four()
    n.spawn(1, "F")
    float_window("F")
    f = n.by_title("F")
    n.clear_notes()
    for who in ["T1", "T3", "F"]:
        for m in IGNORED:
            n.focus(who)
            r = n.msg(m)
            expect(r in ("ok", ""), f"{m!r} on {who} answered {r!r}")
            geo_is(start, f"{m} on {who}")
            focus_is(who, f"{m} on {who}")
            g = n.by_title("F")
            expect(g["floating"] and g["at"] == f["at"] and g["size"] == f["size"], f"{m} on {who}: F changed {g}")
    expect(not n.notes(), f"notifications: {n.notes()!r}")
    expect(c.saved() is None, f"state file entry written: {c.saved()}")


if __name__ == "__main__":
    names = MINE if sys.argv[1:] == ["all"] else sys.argv[1:]
    n.pick(open(os.path.join(S, "sig.txt")).read().strip())
    for x in names:
        c.run_check(x)
