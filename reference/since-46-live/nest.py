"""Drive a nested Hyprland via hyprctl. Never touches the outer session.

Copied from reference/side-zones-live/nest.py (#52) and extended: Lua runs,
the state file and its hidden copies, notices, options, screenshots.
"""
import json, os, re, subprocess, time

S = os.path.dirname(os.path.abspath(__file__))
REPO = "<REPO>"
NEST = os.path.join(S, "nest")
STATE_DIR = os.path.join(NEST, "state", "threecol")
STATE = os.path.join(STATE_DIR, "dividers")
STATE_OFF = STATE + ".off"
STATE_BAK = STATE + ".bak"
OUT = os.path.join(S, "out.txt")
SHOTS = os.path.join(S, "shots")
HIS = None
OUTER = os.environ.get("HYPRLAND_INSTANCE_SIGNATURE")
SPAWNED = {}


def pick(sig=None):
    """Target the nested instance `sig`, or the newest live one that isn't the outer session."""
    global HIS
    if not OUTER:
        raise RuntimeError("HYPRLAND_INSTANCE_SIGNATURE isn't set, so the outer session can't be told apart; run from inside it")
    if sig:
        HIS = sig
    else:
        env = dict(os.environ, HYPRLAND_INSTANCE_SIGNATURE=OUTER)
        live = json.loads(subprocess.run(["hyprctl", "instances", "-j"], capture_output=True, text=True, env=env).stdout or "[]")
        found = sorted((i for i in live if i["instance"] != OUTER), key=lambda i: i["time"], reverse=True)
        if not found:
            raise RuntimeError("no nested Hyprland found; start it with launch.sh")
        HIS = found[0]["instance"]
    assert HIS != OUTER, "refusing to drive the outer session"
    return HIS


def hc(*args):
    assert OUTER and HIS and HIS != OUTER, "refusing to touch the outer session"
    env = dict(os.environ, HYPRLAND_INSTANCE_SIGNATURE=HIS)
    return subprocess.run(["hyprctl", *args], capture_output=True, text=True, env=env).stdout.strip()


def d(expr):
    out = hc("dispatch", expr)
    time.sleep(0.2)
    return out


def run(body):
    """Run a Lua statement in the nested instance. Returns hyprctl's answer."""
    return d("function() %s end" % body)


def lua(body):
    """Run Lua in the nested instance; body writes with w(...). Returns what it wrote."""
    if os.path.exists(OUT):
        os.remove(OUT)
    r = hc("dispatch", "function() local f=io.open('%s','w'); local function w(...) for _,x in ipairs({...}) do f:write(tostring(x)) end f:write('\\n') end; %s; f:close() end" % (OUT, body))
    time.sleep(0.2)
    return open(OUT).read().strip() if os.path.exists(OUT) else "ERR " + r


def notes():
    """Text of every notification shown, newest last."""
    return lua("for _,n in ipairs(hl.notification.get()) do w(n:get_text()) end")


def clear_notes():
    lua("for _,n in ipairs(hl.notification.get()) do n:dismiss() end")


def warnings():
    """The `- …` lines of threecol's settings notices."""
    return [l[2:] for l in notes().splitlines() if l.startswith("- ")]


def clients(ws=None):
    cs = json.loads(hc("clients", "-j") or "[]")
    return [c for c in cs if ws is None or c["workspace"]["id"] == ws]


def by_title(t):
    return next((c for c in clients() if c["title"] == t), None)


def spawn(ws, title):
    d(f"hl.dsp.exec_cmd('ghostty --gtk-single-instance=false --title={title}', {{ workspace = '{ws} silent' }})")
    for _ in range(100):
        c = by_title(title)
        if c and c["workspace"]["id"] == ws:
            SPAWNED[title] = c["pid"]
            time.sleep(0.4)
            return c
        time.sleep(0.1)
    raise RuntimeError(f"{title} never appeared")


def term(pid):
    """SIGTERM one nested client. Never pid <= 0, which would signal a whole group."""
    if pid > 0:
        try:
            os.kill(pid, 15)
        except ProcessLookupError:
            pass


def kill(title, wait=True):
    c = by_title(title)
    if c:
        term(c["pid"])
    if not wait:
        return
    for _ in range(50):
        if not by_title(title):
            break
        time.sleep(0.1)
    time.sleep(0.3)


def geo(ws):
    return {c["title"]: (c["at"][0], c["at"][1], c["size"][0], c["size"][1])
            for c in clients(ws) if not c["floating"]}


def addr(t):
    return "address:" + by_title(t)["address"]


def focus(t):
    d(f"hl.dsp.focus({{ window = '{addr(t)}' }})")


def msg(m):
    return d(f"hl.dsp.layout('{m}')")


def goto(ws):
    d(f"hl.dsp.focus({{ workspace = {ws} }})")


def layout(ws):
    for w in json.loads(hc("workspaces", "-j") or "[]"):
        if w["id"] == ws:
            return w["tiledLayout"]


def read(path):
    return open(path).read() if os.path.exists(path) else None


def state():
    return read(STATE)


def entries(text):
    """workspace name -> (left_width, right_width) from a dividers file's text."""
    out = {}
    for line in (text or "").splitlines():
        parts = line.split(" ", 2)
        if len(parts) == 3 and not line.startswith("#"):
            try:
                out[parts[2]] = (float(parts[0]), float(parts[1]))
            except ValueError:
                pass
    return out


def write_state(path, saved):
    """Write a dividers file with `saved`: workspace name -> (left, right)."""
    os.makedirs(STATE_DIR, exist_ok=True)
    body = "threecol dividers 1\n# left_width right_width workspace\n"
    body += "".join(f"{l} {r} {name}\n" for name, (l, r) in saved.items())
    open(path, "w").write(body)


def reload():
    hc("reload")
    time.sleep(1.0)


def bind(key):
    """Run the action examples/binds.lua bound to `key`, as a key press would. No key is sent."""
    r = hc("dispatch", "function() local b = BINDS['%s']; if type(b.a) == 'function' then b.a() else hl.dispatch(b.a) end end" % key)
    time.sleep(0.25)
    return r


def extra(text):
    open(os.path.join(NEST, "extra.lua"), "w").write(text + "\n")


def active():
    return json.loads(hc("activewindow", "-j") or "{}").get("title")


def monitor():
    return json.loads(hc("monitors", "-j"))[0]


def option(name):
    """`hyprctl getoption` as text."""
    return hc("getoption", name)


def alive():
    return "Hyprland" in hc("version")


_INSTANCE = {}


def instance():
    """This nested instance's entry in `hyprctl instances` (pid, wl_socket)."""
    if HIS not in _INSTANCE:
        _INSTANCE[HIS] = next(i for i in json.loads(hc("instances", "-j")) if i["instance"] == HIS)
    return _INSTANCE[HIS]


def shot(name):
    """Screenshot the nested output (through its own Wayland socket) as PPM: (w, h, bytes)."""
    os.makedirs(SHOTS, exist_ok=True)
    path = os.path.join(SHOTS, name + ".ppm")
    env = dict(os.environ, WAYLAND_DISPLAY=instance()["wl_socket"])
    try:
        subprocess.run(["grim", "-t", "ppm", path], env=env, check=True, capture_output=True, timeout=10)
    except subprocess.TimeoutExpired:
        raise RuntimeError("grim timed out: the nested window must be on screen (the mouse checks make it fullscreen)")
    data = open(path, "rb").read()
    m = re.match(rb"P6\s+(\d+)\s+(\d+)\s+(\d+)\s", data)
    w, h = int(m.group(1)), int(m.group(2))
    return w, h, data[m.end():]


def magenta_gain(before, after):
    """Share of pixels that turned magenta-ish from `before` to `after`.

    The nested config makes the active border solid magenta; the drop
    indicator fills its box with that colour at 0.2 alpha, so a leftover
    indicator shows as a large area that got more magenta. Text and the
    moving focus border stay far below the threshold the checks use.
    """
    (w, h, a), (w2, h2, b) = before, after
    if (w, h) != (w2, h2):
        raise RuntimeError(f"screenshot sizes differ: {w}x{h} vs {w2}x{h2}")
    n, hits = w * h, 0
    for i in range(0, n * 3, 3 * 4):  # every 4th pixel is plenty
        ma = (a[i] + a[i + 2]) / 2 - a[i + 1]
        mb = (b[i] + b[i + 2]) / 2 - b[i + 1]
        if mb - ma > 25:
            hits += 1
    return hits / (n / 4)
