"""Live checks for #90: how Hyprland 0.57's Lua config treats repeats.

    python3 -I live90.py all | <id>...

Duplicate hl.bind calls, hl.unbind and a bind's :remove(), repeated
hl.config({ plugin = { threecol = ... } }) calls (nested wrap / binds tables
too), the getoption names of the nested switches, and repeated
hl.workspace_rule calls for one workspace. Each check writes its Lua into the
nested extra.lua and reloads, then reads hyprctl binds / getoption /
workspacerules or the tiles' positions. No check presses a key, so whether
both duplicate binds fire is left to the source (research note for #90).
"""
import json, os, sys

S = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, S)
import checks as c
import nest as n

expect, same = c.expect, c.same
MINE = []
SUPER = 64


def check(fn):
    MINE.append(fn.__name__)
    return c.check()(fn)


def load(lua):
    """Replace extra.lua with `lua` and reload."""
    n.extra(lua)
    n.reload()


def f9():
    """The SUPER + F9 binds, in registration order, as hyprctl binds -j lists them."""
    return [b for b in json.loads(n.hc("binds", "-j")) if b["key"] == "F9" and b["modmask"] == SUPER]


def opt(name):
    """getoption -j as a dict, or the raw text when there's no such option."""
    out = n.hc("getoption", name, "-j")
    try:
        return json.loads(out)
    except ValueError:
        return out


TWO = ('hl.bind("SUPER + F9", hl.dsp.exec_cmd("true"), { description = "first" })\n'
       'hl.bind("SUPER + F9", hl.dsp.layout("reset"), { description = "second" })\n')


@check
def dup_binds_kept():
    load(TWO)
    got = f9()
    expect([b["description"] for b in got] == ["first", "second"], f"SUPER + F9 binds: {[(b['description'], b['dispatcher']) for b in got]}")
    expect([b["dispatcher"] for b in got] == ["HL.Dispatcher(exec_cmd)", "HL.Dispatcher(layout)"],
           f"dispatchers: {[b['dispatcher'] for b in got]}")


@check
def unbind_all_before():
    # Other case and spacing than the binds; then a bind after the unbind.
    load(TWO + 'hl.unbind("super+f9")\nhl.bind("SUPER + F9", hl.dsp.exec_cmd("true"), { description = "after" })\n')
    got = [b["description"] for b in f9()]
    expect(got == ["after"], f"after unbind + rebind: {got}")


@check
def unbind_other_spelling():
    # MOD4 is SUPER's modmask, but the display string differs.
    load(TWO + 'hl.unbind("MOD4 + F9")\n')
    got = [b["description"] for b in f9()]
    expect(got == ["first", "second"], f"after unbind MOD4 + F9: {got}")


@check
def bind_handle_remove():
    load('local b = hl.bind("SUPER + F9", hl.dsp.exec_cmd("true"), { description = "first" })\n'
         'hl.bind("SUPER + F9", hl.dsp.layout("reset"), { description = "second" })\nb:remove()\n')
    got = [b["description"] for b in f9()]
    expect(got == ["second"], f"after :remove() of the first: {got}")


@check
def config_merges_leaves():
    load('hl.config({ plugin = { threecol = { left_width = 0.3, wrap = { focus = false } } } })\n'
         'hl.config({ plugin = { threecol = { wrap = { swap = false }, binds = { focus = false } } } })\n'
         'hl.config({ plugin = { threecol = { left_width = 0.35 } } })\n')
    want = {
        "plugin:threecol:left_width": ("float", 0.35, True),
        "plugin:threecol:wrap:focus": ("bool", False, True),
        "plugin:threecol:wrap:swap": ("bool", False, True),
        "plugin:threecol:wrap:move": ("bool", True, False),
        "plugin:threecol:binds:focus": ("bool", False, True),
        "plugin:threecol:binds:swap": ("bool", True, False),
    }
    for name, (kind, value, set_) in want.items():
        o = opt(name)
        ok = isinstance(o, dict) and abs(float(o.get(kind, -9)) - float(value)) < 1e-6 and o["set"] == set_
        expect(ok, f"{name}: {o}, want {kind} {value} set {set_}")


@check
def getoption_names():
    load('hl.config({ plugin = { threecol = { wrap = { focus = false } } } })\n')
    for name in ("plugin:threecol:wrap:focus", "plugin.threecol.wrap.focus"):
        o = opt(name)
        expect(isinstance(o, dict) and o.get("bool") is False, f"{name}: {o}")
    for name in ("plugin:threecol:wrap", "plugin:threecol"):
        o = opt(name)
        expect(o == "no such option", f"{name}: {o}")


@check
def ws_rule_merges():
    one = c.rule(2, "left_width = 0.35, right_width = 0.15")
    split = c.rule(2, "left_width = 0.4") + c.rule(2, "left_width = 0.35, right_width = 0.15")
    load(one)
    whole = c.four(ws=2)
    c.close_all()
    load(split)
    merged = c.four(ws=2)
    expect(same(whole, merged), f"one rule {whole} vs two rules {merged}")
    rules = [r for r in json.loads(n.hc("workspacerules", "-j")) if r["workspaceString"] == "2"]
    expect(len(rules) == 1, f"workspacerules for '2': {rules}")
    expect(all("layout_opts" not in r and "layoutopts" not in r for r in rules), f"layout_opts shown: {rules}")


if __name__ == "__main__":
    args = sys.argv[1:]
    names = MINE if args == ["all"] else args
    n.pick(open(os.path.join(S, "sig.txt")).read().strip())
    for x in names:
        c.run_check(x)
