"""Drive a nested Hyprland via hyprctl. Never touches the outer session (except launch)."""
import json, os, subprocess, time

S = os.path.dirname(os.path.abspath(__file__))
NEST = os.path.join(S, "nest")
STATE = os.path.join(NEST, "state", "threecol", "dividers")
OUT = os.path.join(S, "out.txt")
HIS = None
OUTER = os.environ.get("HYPRLAND_INSTANCE_SIGNATURE")
SPAWNED = {}


def pick(sig=None):
    global HIS
    if sig:
        HIS = sig
    else:
        rt = os.path.join(os.environ["XDG_RUNTIME_DIR"], "hypr")
        HIS = [x for x in sorted(os.listdir(rt), key=lambda x: os.path.getmtime(os.path.join(rt, x)), reverse=True)
               if x.startswith("acc1aa0") and x != OUTER][0]
    return HIS


def hc(*args):
    assert HIS and HIS != OUTER, "refusing to touch the outer session"
    env = dict(os.environ, HYPRLAND_INSTANCE_SIGNATURE=HIS)
    return subprocess.run(["hyprctl", *args], capture_output=True, text=True, env=env).stdout.strip()


def d(expr):
    out = hc("dispatch", expr)
    time.sleep(0.2)
    return out


def lua(body):
    """Run Lua in the nested instance; body writes with w(...). Returns what it wrote."""
    if os.path.exists(OUT):
        os.remove(OUT)
    r = hc("dispatch", "function() local f=io.open('%s','w'); local function w(...) for _,x in ipairs({...}) do f:write(tostring(x)) end f:write('\\n') end; %s; f:close() end" % (OUT, body))
    time.sleep(0.2)
    return open(OUT).read().strip() if os.path.exists(OUT) else "ERR " + r


def notes():
    return lua("for _,n in ipairs(hl.notification.get()) do w(n:get_text()) end")


def clear_notes():
    lua("for _,n in ipairs(hl.notification.get()) do n:dismiss() end")


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


def kill(title):
    c = by_title(title)
    if c:
        os.kill(c["pid"], 15)
    for _ in range(50):
        if not by_title(title):
            break
        time.sleep(0.1)
    time.sleep(0.3)


def geo(ws):
    return {c["title"]: (c["at"][0], c["at"][1], c["size"][0], c["size"][1])
            for c in clients(ws) if not c["floating"]}


def focus(t):
    d(f"hl.dsp.focus({{ window = 'address:{by_title(t)['address']}' }})")


def msg(m):
    return d(f"hl.dsp.layout('{m}')")


def goto(ws):
    d(f"hl.dsp.focus({{ workspace = {ws} }})")


def layout(ws):
    for w in json.loads(hc("workspaces", "-j") or "[]"):
        if w["id"] == ws:
            return w["tiledLayout"]


def state():
    return open(STATE).read() if os.path.exists(STATE) else None


def reload():
    hc("reload")
    time.sleep(1.0)


def bind(key):
    """Run the action examples/binds.lua bound to `key`, as a key press would."""
    r = hc("dispatch", "function() local b = BINDS['%s']; if type(b.a) == 'function' then b.a() else hl.dispatch(b.a) end end" % key)
    time.sleep(0.25)
    return r


def extra(text):
    open(os.path.join(NEST, "extra.lua"), "w").write(text + "\n")


def active():
    return json.loads(hc("activewindow", "-j") or "{}").get("title")


def full(ws):
    return {c["title"]: (c["fullscreen"], c["floating"]) for c in clients(ws)}
