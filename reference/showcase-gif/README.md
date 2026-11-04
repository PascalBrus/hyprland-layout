# Showcase GIFs (#44)

Generators for the two README GIFs. They are rendered, not recorded: a Python
port of the layout rules places the windows, and Pillow draws each frame.

- `docs/threecol.gif`, the tour at the top of the README: Fill order, Holes,
  mouse and keyboard resize, drag-and-drop, the Chain, then workspaces with
  `left_rows = 2`, a mirrored shape, two columns (`left_rows = 0`) and one
  column (`left_rows = 0, right_rows = 0`). About 45 s.
- `docs/threecol-settings.gif`, in the Settings section: every setting the
  tour doesn't show, with live value labels. About 49 s.

Rebuild both with `./build.sh` (Python 3 with Pillow and NumPy, the JetBrains
Mono Nerd and Noto Sans fonts, ffmpeg). Each GIF is about 8 MB. The font
paths at the top of `render.py` are Arch's; change them on other systems.

| File | What it is |
|---|---|
| `model.py` | The layout rules from `src/core`: Fill order, Row limits and missing zones, Row heights, moves, Divider and Row-edge clamps, keyboard resize, drop and add area |
| `test_model.py` | Checks the model against `tests/core` expectations: `python3 test_model.py` |
| `timeline.py` | The engine: tweens, captions, keycaps, cursor, value labels, floating windows, workspace switches |
| `tour.py`, `settings.py` | One shot list per GIF |
| `render.py` | Draws the frames: `python3 render.py tour.pkl frames/` |

What isn't faithful to a live session:

- Gaps are about twice the real ones at this scale. The border gradient and rounding are made up.
- Workspace switches slide horizontally. A window's open and close have no keycap.
- A floating window's own size is fixed (`FLOAT_SIZE` in `timeline.py`).
- Terminal text is made up and holds nothing personal.
- `hyprctl reload` isn't shown: it re-places windows until #37 is fixed.
