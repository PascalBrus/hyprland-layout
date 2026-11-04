"""Checks the GIF's Python model against tests/core: same settings, same
expected rects. The core tests use a 5120x1440 area; here rects are fractions
of it, so (0, 0, 1280, 720) is (0, 0, 0.25, 0.5).

Run: python3 test_model.py
"""
import math

from model import WS

W, H = 5120, 1440


def px(*r):
    return (r[0] / W, r[1] / H, r[2] / W, r[3] / H)


LEFT_ZONE = px(0, 0, 1280, 1440)
MASTER_ZONE = px(1280, 0, 2560, 1440)
RIGHT_ZONE = px(3840, 0, 1280, 1440)
AREA = px(0, 0, 5120, 1440)


def near(a, b):
    assert a is not None and b is not None, (a, b)
    assert all(math.isclose(x, y, abs_tol=1e-9) for x, y in zip(a, b)), (a, b)


def filled(ws, n):
    for i in range(1, n + 1):
        ws.add(i)
    return ws


# FillOrderTest ---------------------------------------------------------------
def test_fill_first_right_fills_right_before_left():
    ws = filled(WS(left_rows=None, right_rows=1, fill_first="right"), 4)
    near(ws.rect_of(1), MASTER_ZONE)
    near(ws.rect_of(2), RIGHT_ZONE)
    near(ws.rect_of(3), px(0, 0, 1280, 720))
    near(ws.rect_of(4), px(0, 720, 1280, 720))


def test_master_takes_a_missing_left_zones_width():
    ws = filled(WS(left_rows=0), 2)
    near(ws.rect_of(1), px(0, 0, 3840, 1440))
    near(ws.rect_of(2), RIGHT_ZONE)


def test_master_takes_a_missing_right_zones_width():
    ws = filled(WS(left_rows=None, right_rows=0), 2)
    near(ws.rect_of(1), px(1280, 0, 3840, 1440))
    near(ws.rect_of(2), LEFT_ZONE)


def test_with_both_sides_missing_the_next_window_is_not_placed():
    ws = WS(left_rows=0, right_rows=0)
    assert ws.add(1)
    assert ws.add(2) is None
    near(ws.rect_of(1), AREA)
    assert ws.zone_of(2) is None


# RowLimitTest / SideZones: both sides bounded and full ------------------------
def test_a_window_with_no_room_is_not_placed():
    ws = filled(WS(left_rows=2, right_rows=2, fill_first="right"), 5)
    near(ws.rect_of(2), px(3840, 0, 1280, 720))
    near(ws.rect_of(4), px(0, 0, 1280, 720))
    assert ws.add(6) is None


# KeyboardResizeTest ----------------------------------------------------------
def test_narrowing_the_stack_stops_at_the_minimum_width():
    ws = filled(WS(), 3)
    ws.resize("R", -0.1)  # 0.25 -> 0.15
    ws.resize("R", -0.1)  # would be 0.05; stops at 0.10
    near(ws.rect_of(1), px(1280, 0, 3328, 1440))
    near(ws.rect_of(3), px(4608, 0, 512, 1440))


# RowLimitTest: Row heights ----------------------------------------------------
def test_taller_grows_the_row_and_the_others_give_way_in_proportion():
    ws = filled(WS(left_rows=3), 4)  # 2, 3 and 4 share Left, 480 px each
    ws.resize_row(3, 0.05)
    near(ws.rect_of(2), px(0, 0, 1280, 444))
    near(ws.rect_of(3), px(0, 444, 1280, 552))
    near(ws.rect_of(4), px(0, 996, 1280, 444))


def test_shorter_stops_at_the_minimum_row_height():
    ws = filled(WS(left_rows=2), 3)
    for _ in range(20):
        ws.resize_row(2, -0.05)
    near(ws.rect_of(2), px(0, 0, 1280, 144))
    near(ws.rect_of(3), px(0, 144, 1280, 1296))


# MouseResizeTest -------------------------------------------------------------
def test_the_stack_stops_at_its_minimum_width():
    ws = filled(WS(), 3)
    ws.move_right_divider(ws.dr, 2000 / W)
    near(ws.rect_of(3), px(4608, 0, 512, 1440))
    near(ws.rect_of(1), px(1280, 0, 3328, 1440))


def test_reset_puts_the_dividers_back_to_the_layout_opts():
    ws = filled(WS(left_width=0.3), 3)
    ws.resize("M", 0.05)
    ws.resize("R", 0.05)
    ws.reset()
    near(ws.rect_of(2), px(0, 0, 1536, 1440))
    near(ws.rect_of(1), px(1536, 0, 2304, 1440))
    near(ws.rect_of(3), px(3840, 0, 1280, 1440))


if __name__ == "__main__":
    tests = [(n, f) for n, f in sorted(globals().items()) if n.startswith("test_")]
    for name, f in tests:
        f()
        print("ok", name)
    print(len(tests), "passed")
