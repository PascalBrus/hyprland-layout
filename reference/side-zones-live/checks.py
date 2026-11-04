"""Live checks for #52 in the nested Hyprland. Run: python3 -I checks.py <check>..."""
import os, sys, json, time
S = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, S)
import nest as n

# sig.txt holds the nested instance's signature; without it, take the newest one.
SIG = os.path.join(S, "sig.txt")
n.pick(open(SIG).read().strip() if os.path.exists(SIG) else None)


def cfg(opts="", rules=""):
    body = ""
    if opts:
        body += "hl.config({ plugin = { threecol = { %s } } })\n" % opts
    body += rules
    n.extra(body)
    n.reload()


def clean():
    for c in n.clients():
        os.kill(c["pid"], 15)
    for _ in range(50):
        if not n.clients():
            break
        time.sleep(0.1)
    time.sleep(0.3)
    if os.path.exists(n.STATE):
        os.remove(n.STATE)
    n.clear_notes()


def mon():
    m = json.loads(n.hc("monitors", "-j"))[0]
    return m["width"], m["height"]


def info(t):
    c = n.by_title(t)
    return dict(ws=c["workspace"]["id"], fl=c["floating"], at=tuple(c["at"]), size=tuple(c["size"]))


def centred(t):
    c = info(t)
    W, H = mon()
    cx, cy = c["at"][0] + c["size"][0] / 2, c["at"][1] + c["size"][1] / 2
    return abs(cx - W / 2) <= 2 and abs(cy - H / 2) <= 2


def addr(t):
    return "address:" + n.by_title(t)["address"]


def toggle_float(t):
    n.d(f"hl.dsp.window.float({{ action = 'toggle', window = '{addr(t)}' }})")


def move_ws(t, ws):
    n.d(f"hl.dsp.window.move({{ workspace = '{ws}', follow = false, window = '{addr(t)}' }})")


def report(name, ok, detail=""):
    print(("PASS " if ok else "FAIL ") + name + (" :: " + str(detail) if detail else ""))


def floats(ws=1):
    return sorted(c["title"] for c in n.clients(ws) if c["floating"])


# ---- acceptance criteria ----

def ac_bounded():
    clean(); cfg('left_rows = 1, right_rows = 2')
    for t in ["T1", "T2", "T3", "T4"]:
        n.spawn(1, t)
    tiles = n.geo(1)
    n.spawn(1, "T5")
    i = info("T5")
    report("AC1 full: 5th window floats, centred", i["fl"] and centred("T5") and n.geo(1) == tiles,
           f"T5={i} mon={mon()} tiles_unchanged={n.geo(1) == tiles}")
    toggle_float("T5"); time.sleep(0.4)
    i = info("T5")
    report("AC2 toggle to tiled while full floats again", i["fl"] and n.geo(1) == tiles, f"T5={i}")
    n.kill("T2")
    i = info("T5")
    report("AC3 floated window stays floating after a tile closes", i["fl"], f"T5={i} tiles={n.geo(1)}")
    n.spawn(1, "T6")  # fills the freed Row
    full = n.geo(1)
    n.spawn(2, "X1")
    n.goto(1)
    move_ws("X1", 1); time.sleep(0.4)
    i = info("X1")
    report("AC4 window moved in with no room floats", i["ws"] == 1 and i["fl"] and n.geo(1) == full,
           f"X1={i} T6 tiled={'T6' in full}")
    print("   notes:", n.notes())


def ac_reload_lower():
    clean(); cfg('left_rows = 2, right_rows = 2')
    for t in ["T1", "T2", "T3", "T4", "T5"]:
        n.spawn(1, t)
    n.focus("T5")
    before = n.geo(1)
    cfg('left_rows = 1, right_rows = 1')
    after = n.geo(1)
    act = n.active()
    report("AC5 reload lower limit floats pushed-out windows", floats() == sorted(set(before) - set(after)) and len(after) == 3,
           f"before={before} after={after} floating={floats()} active={act}")
    report("AC5 focus not lost", act == "T5", f"active={act}")
    print("   notes:", n.notes())


def ac_left_zero():
    clean(); cfg('')
    for t in ["T1", "T2", "T3"]:
        n.spawn(1, t)
    n.focus("T2")
    n.d("hl.dsp.window.resize({ x = 150, y = 0, relative = true })")  # move Left|Master Divider
    moved = n.geo(1)
    print("   moved Left:", moved, "state:", n.state())
    cfg('left_rows = 0')
    g = n.geo(1)
    print("   left_rows=0:", g, "floating:", floats())
    m = g.get("T1")
    report("AC6 Master reaches left edge", m and m[0] <= 10, g)
    n.focus("T1")
    w0 = n.geo(1)["T1"]
    n.d("hl.dsp.window.resize({ x = -100, y = 0, relative = true })")
    w1 = n.geo(1)["T1"]
    report("AC6 resize on Master moves only Master|Right (left edge stays)", w1[0] == w0[0], f"{w0} -> {w1}")
    n.d("hl.dsp.window.resize({ x = 100, y = 0, relative = true })")
    cfg('')
    back = n.geo(1)
    report("AC6 left_rows back restores saved Divider (Master starts where it did)", back["T1"][0] == moved["T1"][0],
           f"moved={moved} back={back}")
    print("   notes:", n.notes())


def ac_mirror():
    clean(); cfg('')
    for t in ["T1", "T2", "T3", "T4"]:
        n.spawn(1, t)
    n.bind("SUPER + R")
    dflt = n.geo(1)
    clean(); cfg('left_rows = "unlimited", right_rows = 1, fill_first = "right"')
    for t in ["T1", "T2", "T3", "T4"]:
        n.spawn(1, t)
    n.bind("SUPER + R")
    mir = n.geo(1)
    W, _ = mon()
    def flip(r):
        return (W - r[0] - r[2], r[1], r[2], r[3])
    ok = all(abs(flip(dflt[t])[0] - mir[t][0]) <= 2 and abs(dflt[t][2] - mir[t][2]) <= 2 and dflt[t][1] == mir[t][1]
             for t in dflt)
    report("AC7 mirrored layout is the mirror image", ok, f"default={dflt} mirrored={mir}")
    # moves: from Master, move l should go into the Left Stack (mirror of move r into the Right Stack)
    n.focus("T2")  # Right (single)
    n.msg("move l")
    after = n.geo(1)
    print("   mirrored 'move l' from Right(T2):", after)
    clean(); cfg('')
    for t in ["T1", "T2", "T3", "T4"]:
        n.spawn(1, t)
    n.bind("SUPER + R")
    n.focus("T2")  # Left (single)
    n.msg("move r")
    da = n.geo(1)
    ok = all(abs(flip(da[t])[0] - after[t][0]) <= 2 and da[t][1] == after[t][1] for t in da)
    report("AC7 mirrored move is the mirror of the default move", ok, f"default move r={da}")
    print("   notes:", n.notes())


def ac_fill_first():
    clean(); cfg('fill_first = "right"')
    for t in ["T1", "T2", "T3"]:
        n.spawn(1, t)
    g = n.geo(1)
    W, _ = mon()
    report("AC8 fill_first right (default rows): T2,T3 go Right", all(g[t][0] > W / 2 for t in ["T2", "T3"]), g)
    clean(); cfg('left_rows = 1, right_rows = 2, fill_first = "right"')
    for t in ["T1", "T2", "T3", "T4"]:
        n.spawn(1, t)
    g = n.geo(1)
    report("AC8 fill_first right (both bounded): T2,T3 Right, T4 Left",
           g["T2"][0] > W / 2 and g["T3"][0] > W / 2 and g["T4"][0] < 20, g)
    print("   notes:", n.notes())




# ---- extra checks from the issue comment ----

def ex_switch_layout():
    clean(); cfg('', "hl.workspace_rule({ workspace = '3', layout = 'dwindle' })\n")
    for t in ["D1", "D2", "D3", "D4", "D5", "D6"]:
        n.spawn(3, t)
    print("   dwindle:", n.layout(3), len(n.geo(3)), "tiled")
    cfg('', "hl.workspace_rule({ workspace = '3', layout = 'threecol', layout_opts = { left_rows = 1, right_rows = 1 } })\n")
    time.sleep(0.5)
    alive = "Hyprland" in n.hc("version")
    report("EX2 switch to both-bounded threecol floats extras, no crash",
           alive and n.layout(3) == "threecol" and len(n.geo(3)) == 3 and len(floats(3)) == 3,
           f"layout={n.layout(3)} tiled={n.geo(3)} floating={floats(3)}")
    print("   notes:", n.notes())


def ex_lower_then_open():
    clean(); cfg('left_rows = 1, right_rows = 2')
    for t in ["T1", "T2", "T3", "T4"]:
        n.spawn(1, t)
    cfg('left_rows = 1, right_rows = 1')
    g = n.geo(1)
    _, H = mon()
    relaid = all(r[3] > H - 20 for r in g.values())
    n.spawn(1, "T5")
    report("EX4 lower a limit: tiles laid out again, new window floats",
           len(g) == 3 and relaid and info("T5")["fl"] and n.geo(1) == g, f"after lower={g} floating={floats()}")
    print("   notes:", n.notes())


def ex_zero_accepted():
    for o, r in [('left_rows = 0', ''), ('right_rows = 0', ''), ('left_rows = 0, right_rows = 0', ''),
                 ('', "hl.workspace_rule({ workspace = '1', layout_opts = { left_rows = 0 } })\n"),
                 ('', "hl.workspace_rule({ workspace = '1', layout_opts = { right_rows = 0 } })\n")]:
        clean(); cfg(o, r)
        for t in ["T1", "T2", "T3"]:
            n.spawn(1, t)
        errs = n.hc("configerrors")
        print(f"   {o or r.strip()}: tiled={n.geo(1)} floating={floats()} notes={n.notes()!r} configerrors={errs!r}")
    # reload to 0 floats windows with no room
    clean(); cfg('left_rows = 1, right_rows = 1')
    for t in ["T1", "T2", "T3"]:
        n.spawn(1, t)
    cfg('left_rows = 0, right_rows = 1')
    report("EX5 reload to 0 floats the window with no room", floats() == ["T2"], f"tiled={n.geo(1)} floating={floats()}")


def ex_keyboard_missing():
    clean(); cfg('left_rows = 0')
    for t in ["T1", "T2"]:
        n.spawn(1, t)
    n.bind("SUPER + R")
    n.focus("T1")
    W, _ = mon()
    w0 = n.geo(1)["T1"][2]
    n.bind("SUPER + ALT + L"); w1 = n.geo(1)["T1"][2]
    n.bind("SUPER + ALT + H"); w2 = n.geo(1)["T1"][2]
    step = 0.05 * W
    report("EX7 keyboard resize on Master with Left missing moves a full step",
           abs(abs(w1 - w0) - step) <= 3 and abs(abs(w2 - w1) - step) <= 3, f"widths {w0} -> {w1} -> {w2}, step={step}")


if __name__ == "__main__":
    for name in sys.argv[1:]:
        print("==", name)
        globals()[name]()
