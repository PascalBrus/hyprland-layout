# Live results: bind dispatchers (#79)

## #82: `focusmaster` / `swapwithmaster`, ignored messages (2026-11-09)

- Hyprland 0.57.1, commit `acc1aa0bfd4bd575dcb0f76c35ccf12b660a1ab9`, nested instance, default `plugin:threecol:*` values (one check sets `fill_first = "right"`).
- threecol at `d09ac8a`, loaded from `build/threecol.so` built from it.
- Run with [`live82.py`](live82.py). Focus and positions read from `hyprctl clients -j`. Setup: Master T1, Left T2, Stack T3 above T4; F a floating window.
- Line numbers are Hyprland's `src/layout/algorithm/tiled/master/MasterAlgorithm.cpp` at that commit.
- A negative control (an expected focus and swap that are wrong) fails, so the checks can fail.

**Result: 14 of 14 pass.**

| Check | Message and arguments | Master layout | Seen | Result |
|---|---|---|---|---|
| `fm_from_other` | `focusmaster` none / `auto` / `master` / `previous`, from T4 | `:614-615` | Focus T1, nothing moves | PASS |
| `fm_auto_on_master` | `focusmaster` none / `auto`, on Master | `:622-630`, `:640` | Focus T2, the first window after Master in Fill order | PASS |
| `fm_auto_fill_first_right` | `focusmaster`, on Master, `fill_first = "right"` | `:622-630` | Focus on Right's top Row (T2, then T3 after swapping it to the top) | PASS |
| `fm_master_on_master` | `focusmaster master`, on Master | `:632-633` | Focus stays on T1 | PASS |
| `fm_previous` | `focusmaster previous` | `:617-618`, `:635-638` | Nothing saved: T2. From T4: T1, then back to T4. `auto` from T3 leaves T4 saved. T4 closed: T2 | PASS |
| `fm_from_floating` | `focusmaster`, from F | `:614-615` | Focus T1, nothing moves | PASS |
| `fm_previous_from_floating` | `focusmaster previous` from T4, then from F, then on Master | `:617-618`, `:637` | F replaces T4 as saved, so on Master it falls back to auto: T2 | PASS |
| `sm_from_other` | `swapwithmaster` none / `auto` / `master` / `ignoremaster`, from T4 | `:571-578` | T4 and T1 trade places, focus stays on T4 | PASS |
| `sm_child` | `swapwithmaster child`, from T3 | `:575-578` | T3 and T1 trade places, focus on T1 | PASS |
| `sm_on_master` | `swapwithmaster` none / `auto` / `child` / `master`, on Master | `:579-589` | T1 and T2 trade places; focus stays on T1, with `master` goes to T2 | PASS |
| `sm_ignoremaster_on_master` | `swapwithmaster [auto\|master\|child] ignoremaster`, on Master | `:571`, `:579` | Nothing moves, focus stays | PASS |
| `sm_into_hole` | `swapwithmaster` none / `child`, from T4, T1 closed (a Master Hole) | none: threecol's own | T4 moves into Master, focus stays on T4 | PASS |
| `sm_from_floating` | `swapwithmaster` none / `child` / `master`, from F | `:559-560` | Nothing moves, F stays floating and focused | PASS |
| `ignored` | `addmaster`, `removemaster`, `orientationleft` / `right` / `top` / `bottom` / `center` / `next` / `prev`, `orientationcycle left top`, `mfact 0.1`, `mfact exact 0.5`, `togglesplit`, `swapsplit`, `preselect l`, on T1, T3 and F | `:689-796`, dwindle | Nothing moves, focus stays, `hyprctl` answers `ok`, no notification, no saved Dividers | PASS |

Not checked live: `focusmaster` with no focused window. It now does nothing, as the master layout does (`:600-601`), but a layout message only reaches the focused workspace, so there is no easy way to send one with nothing focused there. This case is covered in the plugin code only.

Bugs found and fixed in `d09ac8a`, both with unit tests:

1. `focusmaster previous` from a floating window kept the tile saved before. The master layout saves the floating window (`:617-618`), so `previous` on Master then falls back to auto (`:637-638`). threecol now does the same.
2. With no focused window, `focusmaster` focused Master. The master layout does nothing (`:600-601`). threecol now does nothing too.

One small difference is left: if the floating window that `focusmaster previous` came from is tiled later, the master layout goes back to it, but threecol falls back to auto.
