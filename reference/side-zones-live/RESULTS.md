# Side zones live checks (#52)

Agent run, 2026-11-04, of the live checks in #52 (parent #46) and its follow-up comment.

- Hyprland 0.57.1 (`acc1aa0`), threecol at `59d2f2d`, built fresh (316/316 unit tests pass).
- Run in a **nested Hyprland**, so the author's session was never driven. It ran as a floating window on the outer workspace 2, so the tiled windows there didn't move. Output 2560x720 (2136x1257 for part of the run, after the nested window was resized), gaps 5, borders 2, default widths.
- The nested instance has its own config and `XDG_STATE_HOME`. Test windows are `ghostty` started inside it.
- Harness: [`nest.py`](nest.py) (the #32 one, plus a guard that refuses the outer session's signature) and [`checks.py`](checks.py) (one function per check). Replace `<HARNESS>` in `launch.sh` and `nest/hyprland.lua` with a working directory holding `build/threecol.so` and `nest/extra.lua`, and `<REPO>` with this checkout. `checks.py` targets the instance in `sig.txt`, or the newest nested one.

**Result:** every check the harness can reach passes (8 acceptance criteria, 6 from the comment). No bugs found. The mouse and visual items, and one decision, are left for the author.

## Acceptance criteria

| # | Check | Result |
|---|---|---|
| 1 | Both sides bounded (`left_rows = 1`, `right_rows = 2`) and full: a new window floats at its own size (800x600), centred | pass. Tiles unchanged |
| 2 | Toggling it back to tiled while still full floats it again | pass |
| 3 | A floated overflow window stays floating when a tiled window closes | pass |
| 4 | A window moved in from another workspace with no room floats | pass. It keeps its current size (its old tile size) and is centred. With the default widths that puts it exactly over Master, so it can look tiled at first |
| 5 | `hyprctl reload` from 2/2 to 1/1 floats the pushed-out windows | pass. The bottom Row of each side floated, the others filled their side. Focus stayed on the focused window. **Stuck drag: not checked** (needs a mouse) |
| 6 | `left_rows = 0`: Master reaches the left edge, resize on Master moves only Master\|Right, setting it back restores the saved Divider | pass, including a Divider restored from the state file after a restart. **"No Divider to grab" by mouse: not checked** |
| 7 | Mirrored (`left_rows = "unlimited"`, `right_rows = 1`, `fill_first = "right"`) is the mirror image, and `move l` from Right mirrors the default `move r` from Left | pass, after SUPER+R (saved Dividers are per side, so a moved Left Divider shows up in both) |
| 8 | `fill_first = "right"` fills Right first | pass, with the default rows and with both bounded (1/2: T2, T3 Right, then T4 Left) |

## From the issue comment

| Check | Result |
|---|---|
| An untouched config still shows a Right Stack on 0.57.1 | pass |
| Switching a 6-window dwindle workspace to threecol with `layout_opts = { left_rows = 1, right_rows = 1 }` floats the extras without a crash | pass: 3 tiled, 3 floating. **One-frame flash: not checked** (visual) |
| A window pushed out while being dragged behaves sensibly when dropped | **not checked** (needs a mouse) |
| Fill 1/2, lower to 1/1, open one more: tiles laid out again, new window floats | pass: the remaining three are full height |
| `left_rows = 0` / `right_rows = 0` accepted globally and in `layout_opts` | pass: no notifications, no `configerrors`. Both at 0: Master fills the workspace, the rest float |
| Reload to 0 floats windows with no room | pass (1/1 to 0/1: Left's window floated) |
| A returning zone uses its saved Divider, including one from the state file | pass (left_rows 0 at start-up, then reload to 1: Master starts at the same x as before the restart) |
| Keyboard resize on Master with one side missing moves the other Divider a full step | pass: Master width +107 then −107 px, step 0.05 × 2136 |
| **Decision:** with one side missing, a mouse drag on Master's missing-side half moves the other Divider | open, for the author |

## Left for the author

- Mouse: no Left\|Master Divider to grab with `left_rows = 0`. No stuck drag after a reload that floats windows. A window pushed out mid-drag, then dropped.
- Visual: a one-frame flash when a workspace switches to a both-bounded threecol.
- The decision above.
