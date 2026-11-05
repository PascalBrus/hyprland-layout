# Reset live checks (#45)

Agent run, 2026-11-05, of the live check in #45 (parent #33): SUPER+R didn't reset the workspace while a floating window was focused.

- Hyprland 0.57.1 (`acc1aa0`), in a **nested Hyprland** with the [side-zones-live](../side-zones-live/) harness (set it up as its RESULTS.md says). The author's session was never driven. Output 1280x720, gaps 5, borders 2, default widths.
- [`checks.py`](checks.py): three tiles on workspace 1, two `resize wider` on Master, then the setup below, then the SUPER+R action from `examples/binds.lua`. A pass is every tile back at its place before the resize.

**Result:** not a bug in threecol. The session that found it ran an old build.

## Cause

- The author's session loads hyprpm's build, and `hyprpm.toml` pins Hyprland 0.57.1 to threecol `9fd58e0`. That commit predates `e75ddbb`, which let reset run under floating and fullscreen windows. So the live check of #33 tested the code from before the fix.
- Hyprland doesn't route by focus. `Layout::CAlgorithm::layoutMsg` asks the floating algorithm first, and the default one returns no error, so the message always goes on to the tiled algorithm (`src/layout/algorithm/Algorithm.cpp`, `CAlgorithm::layoutMsg`, read in the 0.57.0 source).

## Checks

| Focused when SUPER+R is pressed | threecol `deb3234` | threecol `9fd58e0` (hyprpm, 0.57.1 pin) |
|---|---|---|
| A floating window on the workspace | pass | fail: Dividers stay resized |
| A fullscreen tile | pass | fail |
| A maximised tile | pass | fail |

## Left for the author

- hyprpm users on 0.57.1 still get `9fd58e0`, without the reset fix, until the 0.57.1 pin moves to a newer commit.
