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

## #83: direction focus and swap, switches, looping, edges (2026-11-10)

- **Hyprland 0.57.2**, commit `11cbc6c577b84ffb1edf4df3fc5b6128c3ae6947`, nested instance. The installed package moved to 0.57.2 since #82; the README cites 0.57.1 (`acc1aa0b`), and `moveFocus` (`ConfigActions.cpp:448-536`) is the same at 0.57.0 and 0.57.1.
- threecol built from the #83 commit, loaded from `build/threecol.so`. Default `plugin:threecol:*` values; every switch changed at runtime with `hl.config`, never a reload, and read back with `hyprctl getoption`.
- Run with [`live83.py`](live83.py). Setup: Master T1, Left T2, Stack T3 above T4 (Chain T2, T1, T3, T4). Where threecol's behaviour runs, the expected window is worked out from the tiles' positions alone. Where Hyprland's runs, runs with every combination of `binds.focus` / `binds.swap` and `wrap.focus` / `wrap.swap` must match.
- A negative control (expecting `wrap.focus = false` results with looping on) fails, so the checks can fail.

**Result: 12 of 12 pass.**

| Check | Dispatcher | Switches | Seen | Result |
|---|---|---|---|---|
| `focus_loops` | `focus({ direction })`, all 4 directions from all 4 tiles | `binds.focus` on, `wrap.focus` on | left / right one step along the Chain, T4 right → T2 and T2 left → T4; up / down inside the Stack, T4 down → T3, T3 up → T4; up / down on Master and on the lone Left Row stay | PASS |
| `focus_no_wrap` | the same 16 | `wrap.focus` off | The same, but T4 right, T2 left, T4 down and T3 up stay | PASS |
| `swap_loops` | `window.swap({ direction })`, 16 in a row | `binds.swap` on, `wrap.swap` on | Each swaps with the window focus would go to from the positions before it, looping; focus stays on the swapped window | PASS |
| `swap_no_wrap` | the same 16 | `wrap.swap` off | No swap past an end | PASS |
| `focus_message_noloop` | `layout("focus l/r/u/d")`, with and without `noloop` | `wrap.focus` on, then off | Loops by default; `noloop` stops just that message at an end and steps normally elsewhere; `wrap.focus` off stops it too | PASS |
| `focus_hyprland` | `focus({ direction })` | `binds.focus` off, `wrap.focus` on and off | Hyprland's: geometry picks, and at the screen edge its own wrap from the far edge (`ConfigActions.cpp:487-534`); `wrap.focus` changes nothing | PASS |
| `swap_hyprland` | `window.swap({ direction })` | `binds.swap` off, `wrap.swap` on and off | Hyprland's: geometry swaps, nothing at the screen edge; `wrap.swap` changes nothing | PASS |
| `floating` | focus from two floating windows, swap from one, every direction | all 4 combinations | Identical every time; focus moves between the floating windows | PASS |
| `fullscreen` | focus and swap, every direction, T2 fullscreen | all 4 combinations | Identical every time, fullscreen holds, focus doesn't follow the Chain | PASS |
| `other_layout` | focus and swap, every direction, 3 windows on a dwindle workspace | all 4 combinations | Identical every time (windows named by position, as dwindle places them differently run to run) | PASS |
| `edges` | focus and swap at threecol's edges, with a headless monitor right of the nested one holding window M | looping on, then `wrap.focus` / `wrap.swap` off | Looping: T4 right → T2, not M. Off: T4 right, T4 down, T2 left, up / down on Master and on the lone Left Row: focus stays, nothing swaps, nothing leaves the monitor. Control with `binds.focus` off: T4 right → M | PASS |
| `hook_failure` | `hookblocker.so` loaded first, holding `moveFocus`; then threecol | defaults | One notification: `[threecol] focus({ direction }) keeps Hyprland's behaviour: threecol could not hook it (x86_64 only, one plugin per hook)`. Focus right from T3 is Hyprland's (T2, its edge wrap), swap right from T3 still threecol's (with T4) | PASS |

Settings reset with `hyprctl reload` in the nested instance at the end; every `binds` and `wrap` switch read back `true`.

Found and fixed in the #83 commit:

1. The README said Hyprland's direction focus goes to the next monitor and stops there. It also wraps: with no window and no monitor that way, it takes the window nearest the screen's far edge, unless `general:no_focus_fallback` (`ConfigActions.cpp:487-534`). The README row now says so. Found when `focus_hyprland` first expected focus to stay at the edges.
2. With one hook failing, the notification read "focus({ direction }) keep Hyprland's behaviour: threecol could not hook them". It now says "keeps" and "it" for one; `hook_failure` was run again on the fixed build.

Not checked: several real monitors (the `edges` check uses one headless monitor).

## #84: threecol dispatchers off tiles, floating Step resize, looping moves (2026-11-10)

- Hyprland 0.57.2, commit `11cbc6c577b84ffb1edf4df3fc5b6128c3ae6947`, nested instance.
- threecol built from `2d283da`, loaded from `build/threecol.so`. Default `plugin:threecol:*` values; `wrap.move` and `binds.focus` changed at runtime with `hl.config` and read back with `hyprctl getoption`; one check sets `left_rows = 0`.
- Run with [`live84.py`](live84.py). Setup: Master T1, Left T2 (Row limit 1), Stack T3 above T4 (Chain T2, T1, T3, T4); F (and G) floating. Positions and sizes from `hyprctl clients -j`. Expected places written out by hand from the README rules.
- `move_loops` and `move_no_wrap` send the same moves and expect opposite results, so each one shows the other's checks can fail.

**Result: 9 of 9 pass.**

| Check | Dispatcher | Seen | Result |
|---|---|---|---|
| `rows_on_a_tile` | `focus l/r/u/d`, `resize wider/narrower/taller/shorter`, `reset`, on tiles | Focus walks the Chain and loops (T2 left → T4, T4 right → T2, T3 up → T4); wider / taller change the zone or Row by one step (128 / 36 px), the other Row giving way; narrower / shorter undo them; `reset` puts everything back | PASS |
| `rows_on_a_floating_window` | the same, from floating F | `focus l` is Hyprland's (to floating G); each Step resize changes F by one step share (128 px wide, 36 px tall) with its centre kept to the pixel, tiles untouched, F still floating and focused; `reset` from F resets a resized tile layout | PASS |
| `floating_shrinks_to_one_step` | `resize narrower` ×12 on F, 800 px wide | Stops at 128 px (one step of 2560), centre kept: (1124, 173, 800, 300) → (1460, 173, 128, 300); one more does nothing | PASS |
| `focus_under_fullscreen` | `focus l/r/u/d` messages, T2 fullscreen | The same as Hyprland's `focus({ direction })` with `binds.focus` off, in every direction: focus stays on fullscreen T2, which stays fullscreen; not the Chain | PASS |
| `move_loops` | `window.move({ direction })`, `wrap.move` on | T2 left → bottom of the Stack, a Left Hole; T2 right → back into that Hole; T4 right → swaps with T2 (Left full); T4 left → bottom of the Stack (it has room), not a swap back; T4 down / T3 up swap with the other edge; up / down on Master and the lone Left Row do nothing; focus stays on the moved window | PASS |
| `move_no_wrap` | the same at the ends, `wrap.move` off | Nothing moves at the ends; T3 down inside the Stack still swaps | PASS |
| `move_message_loops` | `move l/r/u/d`, then with `noloop`, then `wrap.move` off | The same as `window.move` looping; `noloop` and `wrap.move` off each stop it at the ends | PASS |
| `move_left_missing` | `window.move`, `left_rows = 0` | Master T1 left → bottom of the Stack, a Master Hole; T1 right (Chain's right end) → fills the Master Hole; T3 right → swaps with Master | PASS |
| `move_floating_and_fullscreen` | `move l/r/u/d` on F; `window.move` and `move` under fullscreen | Tiles and F untouched; fullscreen holds | PASS |

No bugs found in the plugin. A first run expected T4 left (after swapping into the full Left) to swap back; it joins the Stack's bottom instead, as the rules say, since the Stack has room. The check was wrong and now expects that.

Not checked: the drag and mouse-resize rows (`window.drag`, `window.resize` as a mouse bind). They move the real pointer, so they wait for the author's yes.
