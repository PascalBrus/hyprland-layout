"""A small Python port of threecol's core rules (src/core/Workspace.cpp),
enough for the shots in the GIF. Rects are fractions of the workspace area."""
import copy

MIN_WIDTH, MIN_MASTER, MIN_ROW, STEP = 0.10, 0.20, 0.10, 0.05
ADD_AREA = 0.25


def _fit(heights, total, lo):
    """RowHeights.cpp fit(): scaled to share `total`, none below `lo`."""
    n = len(heights)
    if n == 0:
        return []
    if n * lo > total + 1e-9:
        return [total / n] * n
    at_min = [False] * n
    while True:
        free = total - lo * sum(at_min)
        share = sum(h for h, m in zip(heights, at_min) if not m)
        out, clamped = [], False
        for h, m in zip(heights, at_min):
            out.append(lo if m else (h * free / share if share > 0 else free / at_min.count(False)))
        for k in range(n):
            if not at_min[k] and out[k] < lo - 1e-9:
                at_min[k] = clamped = True
        if not clamped:
            return out


class WS:
    def __init__(s, left_width=0.25, right_width=0.25, left_rows=1, right_rows=None, fill_first="left"):
        s.lw0, s.rw0 = left_width, right_width
        s.rows = {"L": left_rows, "R": right_rows}  # None = unlimited (Stack)
        s.fill_first = fill_first
        s.master = None
        s.win = {"L": [], "R": []}
        s.h = {"L": [], "R": []}
        s.reset()

    # -- shape --
    def exists(s, z):
        return z == "M" or s.rows[z] != 0

    def reset(s):
        s.dl = s.lw0 if s.exists("L") else 0.0
        s.dr = s.rw0 if s.exists("R") else 0.0
        for z in "LR":
            n = len(s.win[z])
            s.h[z] = [1 / n] * n if n else []

    def has_room(s, z):
        if z == "M":
            return s.master is None
        return s.exists(z) and (s.rows[z] is None or len(s.win[z]) < s.rows[z])

    def next_zone(s):
        if s.master is None:
            return "M"
        order = "LR" if s.fill_first == "left" else "RL"
        for z in order:
            if s.has_room(z):
                return z
        return None

    def zone_of(s, i):
        if s.master == i:
            return "M"
        for z in "LR":
            if i in s.win[z]:
                return z
        return None

    def windows(s):
        out = list(s.win["L"])
        if s.master is not None:
            out.append(s.master)
        return out + list(s.win["R"])

    # -- rows --
    def _insert(s, z, row, i):
        n = len(s.h[z]) + 1
        s.h[z] = [x * (n - 1) / n for x in s.h[z]]
        s.h[z].insert(row, 1 / n)
        s.win[z].insert(row, i)

    def take_out(s, i):
        if s.master == i:
            s.master = None
        for z in "LR":
            if i in s.win[z]:
                r = s.win[z].index(i)
                s.win[z].pop(r)
                s.h[z].pop(r)
                t = sum(s.h[z])
                if t > 0:
                    s.h[z] = [x / t for x in s.h[z]]

    def add(s, i):
        z = s.next_zone()
        if z == "M":
            s.master = i
        elif z:
            s._insert(z, len(s.win[z]), i)
        return z

    remove = take_out

    def end_nearest_master(s, z):
        return "bottom" if z == "L" else "top"

    def join_near_master(s, z, i):
        s.take_out(i)
        s._insert(z, 0 if s.end_nearest_master(z) == "top" else len(s.win[z]), i)

    def join_bottom(s, z, i):
        s.take_out(i)
        s._insert(z, len(s.win[z]), i)

    def slot_swap(s, a, b):
        za, zb = s.zone_of(a), s.zone_of(b)
        def put(z, old, new):
            if z == "M":
                s.master = new
            else:
                s.win[z][s.win[z].index(old)] = new
        # swap via a placeholder
        put(za, a, -1)
        put(zb, b, a)
        if za == "M":
            s.master = b
        else:
            s.win[za][s.win[za].index(-1)] = b

    def to_master(s, i):
        if s.master is not None:
            s.slot_swap(i, s.master)
        else:
            s.take_out(i)
            s.master = i

    # -- geometry --
    def zone_rect(s, z):
        if z == "L":
            return (0.0, 0.0, s.dl, 1.0)
        if z == "M":
            return (s.dl, 0.0, 1 - s.dl - s.dr, 1.0)
        return (1 - s.dr, 0.0, s.dr, 1.0)

    def row_rect(s, z, row, heights=None):
        hs = heights if heights is not None else s.h[z]
        x, _, w, _ = s.zone_rect(z)
        top = sum(hs[:row])
        return (x, top, w, hs[row])

    def rect_of(s, i):
        z = s.zone_of(i)
        if z == "M":
            return s.zone_rect("M")
        return s.row_rect(z, s.win[z].index(i))

    def rects(s):
        return {i: s.rect_of(i) for i in s.windows()}

    # -- chain --
    def chain(s):
        return s.windows()

    def focus_step(s, i, d):
        c = s.chain()
        k = c.index(i) + (1 if d == "r" else -1)
        return c[k] if 0 <= k < len(c) else i

    def move(s, i, d):
        z = s.zone_of(i)
        fwd = d == "r"
        if z in "LR":
            ws = s.win[z]
            r = ws.index(i) + (1 if fwd else -1)
            if 0 <= r < len(ws):
                s.slot_swap(i, ws[r])
                return True
            master_fwd = z == "L"
            if master_fwd != fwd:
                return False
            s.to_master(i)
            return True
        side = "R" if fwd else "L"
        if s.has_room(side):
            s.join_near_master(side, i)
            return True
        ws = s.win[side]
        if not ws:
            return False
        s.slot_swap(i, ws[0] if s.end_nearest_master(side) == "top" else ws[-1])
        return True

    # -- resize --
    def resize(s, z, by):
        """Workspace::resize: the zone wider (by > 0) or narrower, stopping at
        min_width and min_master_width."""
        def moved(w, by, lo, hi):
            return max(w, min(w + by, hi)) if by > 0 else min(w, max(w + by, lo))
        if z == "L":
            s.dl = moved(s.dl, by, MIN_WIDTH, 1 - s.dr - MIN_MASTER)
        elif z == "R":
            s.dr = moved(s.dr, by, MIN_WIDTH, 1 - s.dl - MIN_MASTER)
        else:
            count = int(s.exists("L")) + int(s.exists("R"))
            if count == 0:
                return
            if by > 0:
                if s.exists("L"):
                    s.dl = moved(s.dl, -by / count, MIN_WIDTH, s.dl)
                if s.exists("R"):
                    s.dr = moved(s.dr, -by / count, MIN_WIDTH, s.dr)
            else:
                share = min(-by, max(0.0, 1 - s.dl - s.dr - MIN_MASTER)) / count
                if s.exists("L"):
                    s.dl += share
                if s.exists("R"):
                    s.dr += share

    def resize_row(s, i, by):
        """RowHeights::resize: the window's Row taller or shorter, the others
        giving way in proportion, no Row below min_row_height."""
        z = s.zone_of(i)
        if z not in ("L", "R") or len(s.h[z]) < 2:
            return
        hs, n, row = list(s.h[z]), len(s.h[z]), s.win[z].index(i)
        h = hs[row]
        if by > 0:
            grown = h + max(0.0, min(by, 1 - h - (n - 1) * MIN_ROW))
        else:
            grown = h - max(0.0, min(-by, h - MIN_ROW))
        rest = _fit(hs[:row] + hs[row + 1:], 1 - grown, MIN_ROW)
        s.h[z] = rest[:row] + [grown] + rest[row:]

    def narrower_master(s):
        s.resize("M", -STEP)

    def move_left_divider(s, at, by):
        s.dl = max(MIN_WIDTH, min(at + by, 1 - MIN_MASTER - s.dr))

    def move_right_divider(s, at, by):
        """`by` is the cursor's move: right makes the Right zone narrower."""
        s.dr = max(MIN_WIDTH, min(at - by, 1 - MIN_MASTER - s.dl))

    def move_edge(s, z, at, edge, by):
        a, b = at[edge - 1], at[edge]
        by = max(MIN_ROW - a, min(by, b - MIN_ROW))
        hs = list(at)
        hs[edge - 1] = a + by
        hs[edge] = b - by
        s.h[z] = hs

    # -- drag and drop --
    def zone_at(s, x, y):
        if not (0 <= x < 1 and 0 <= y < 1):
            return None
        if x < s.zone_rect("M")[0]:
            return "L"
        if x < s.zone_rect("R")[0]:
            return "M"
        return "R"

    def row_at(s, z, y):
        ws = s.win[z]
        for r in range(len(ws) - 1):
            _, ry, _, rh = s.row_rect(z, r)
            if y < ry + rh:
                return r
        return len(ws) - 1

    def apply_drop(s, i, x, y):
        z = s.zone_at(x, y)
        if not z or not s.zone_of(i):
            return False
        if z == "M":
            if s.master == i:
                return False
            s.to_master(i)
            return True
        ws = s.win[z]
        if not ws:
            s.join_bottom(z, i)
            return True
        r = s.row_at(z, y)
        onto = ws[r]
        if onto == i:
            return False
        rr = s.row_rect(z, r)
        if r + 1 == len(ws) and y >= rr[1] + rr[3] * (1 - ADD_AREA):
            if i in ws:
                ws.remove(i)
                ws.append(i)
                return True
            if s.has_room(z):
                s.join_bottom(z, i)
                return True
        s.slot_swap(i, onto)
        return True

    def drop_indicator(s, i, x, y):
        after = copy.deepcopy(s)
        if not after.apply_drop(i, x, y):
            return None
        return after.rect_of(i)

    def window_at(s, x, y):
        for i, r in s.rects().items():
            if r[0] <= x < r[0] + r[2] and r[1] <= y < r[1] + r[3]:
                return i
        return None
