"""Live-test helpers for threecol. Drives Hyprland via hyprctl; only touches windows it spawned."""
import json, os, subprocess, time

SPAWNED = {}  # title -> pid


def hc(*args):
    return subprocess.run(["hyprctl", *args], capture_output=True, text=True).stdout.strip()


def d(expr):
    out = hc("dispatch", expr)
    time.sleep(0.15)
    return out


def clients(ws=None):
    cs = json.loads(hc("clients", "-j"))
    if ws is not None:
        cs = [c for c in cs if c["workspace"]["id"] == ws]
    return cs


def by_title(t):
    for c in clients():
        if c["title"] == t:
            return c
    return None


def addr(t):
    c = by_title(t)
    if not c:
        raise RuntimeError(f"no window titled {t}")
    return c["address"]


def spawn(ws, title, wait=True):
    d(f"hl.dsp.exec_cmd('ghostty --gtk-single-instance=false --title={title}', {{ workspace = '{ws} silent' }})")
    if not wait:
        return
    for _ in range(100):
        c = by_title(title)
        if c and c["workspace"]["id"] == ws:
            SPAWNED[title] = c["pid"]
            time.sleep(0.4)
            return c
        time.sleep(0.1)
    raise RuntimeError(f"{title} never appeared")


def kill(title):
    c = by_title(title)
    if c:
        try:
            os.kill(c["pid"], 15)
        except ProcessLookupError:
            pass
    for _ in range(50):
        if not by_title(title):
            break
        time.sleep(0.1)
    time.sleep(0.3)
    SPAWNED.pop(title, None)


def cleanup():
    """Close every window spawn() started in this process, by pid."""
    for pid in set(SPAWNED.values()):
        try:
            os.kill(pid, 15)
        except ProcessLookupError:
            pass
    SPAWNED.clear()
    time.sleep(0.8)


def focus(title):
    d(f"hl.dsp.focus({{ window = 'address:{addr(title)}' }})")


def active():
    return json.loads(hc("activewindow", "-j")).get("title")


def msg(m):
    return d(f"hl.dsp.layout('{m}')")


def geo(ws):
    """title -> (x, y, w, h) for tiled windows."""
    return {c["title"]: (*c["at"], *c["size"]) for c in clients(ws) if not c["floating"]}


def show(ws):
    g = geo(ws)
    for t, r in sorted(g.items(), key=lambda kv: (kv[1][0], kv[1][1])):
        print(f"  {t:6} x={r[0]:5} y={r[1]:5} w={r[2]:5} h={r[3]:5}")
    return g


def goto(ws):
    d(f"hl.dsp.focus({{ workspace = {ws} }})")
    time.sleep(0.3)


def state():
    p = os.path.join(os.environ.get("XDG_STATE_HOME") or os.path.expanduser("~/.local/state"), "threecol", "dividers")
    try:
        return open(p).read()
    except FileNotFoundError:
        return None
