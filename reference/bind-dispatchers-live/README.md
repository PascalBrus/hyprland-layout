# Live checks for the bind dispatchers (#79)

Checks for the #79 sub-issues, run in the nested Hyprland of [`../since-46-live/`](../since-46-live/), whose `checks.py` and `nest.py` they reuse. Results are in [`RESULTS.md`](RESULTS.md), for the #85 PR.

| File | Checks |
|---|---|
| `live82.py` | #82: `focusmaster` / `swapwithmaster` arguments, ignored messages |
| `live83.py` | #83: direction focus and swap, `binds.focus` / `binds.swap`, `wrap.focus` / `wrap.swap`, `noloop`, edges, a hook that fails |
| `live84.py` | #84: the threecol dispatchers on tiles, floating and fullscreen windows, floating Step resize, looping `window.move` and `move l/r/u/d`, `wrap.move`, `noloop` |
| `live85.py` | #85 (the #81 rows): `window.swap({ next \| prev })`, `window.cycle_next()` with its `tiled` / `floating` arguments, the `cyclenext` / `cycleprev` / `swapnext` / `swapprev` messages, `binds.swap_next` / `binds.cycle_next`, `wrap.swap_next` / `wrap.cycle_next`, `noloop`; focus after a window closes, One-off resize, the ignored messages #82 didn't send; with `--mouse`, the `window.drag()` drops |
| `live90.py` | #90: duplicate `hl.bind`s, `hl.unbind` spellings and a bind's `:remove()`, repeated `hl.config` (nested `wrap` / `binds`), `getoption` names of the switches, repeated `hl.workspace_rule` for one workspace. Results in [`research/lua-config-precedence.md`](../../research/lua-config-precedence.md) |
| `outer85.py` | #85: `window.drag()` drops and mouse Drag resize, in the author's **real** session on its workspace 9 with their SUPER + mouse binds (the nested one can't get SUPER + mouse). Moves the real pointer: ask first, then `python3 -I outer85.py --yes` |
| `hookblocker.cpp` | For `live83.py hook_failure`: a test plugin that holds the `moveFocus` hook. Build it as its header says, into the harness directory |

## Run

1. `reference/since-46-live/up.sh "$H"` sets up the harness in `$H` with `build/threecol.so`, links these check files into it (edit them here; they run there as they are), and starts the nested Hyprland. See [`../since-46-live/README.md`](../since-46-live/README.md#set-up). For `live83.py hook_failure`, also build `hookblocker.so` into `$H/`.
2. `cd "$H" && python3 -I live82.py all`, or name checks, e.g. `python3 -I live82.py sm_child ignored`. The scripts drive only the instance in `$H/sig.txt`.
3. `reference/since-46-live/down.sh "$H"` stops it. After a rebuild, run `down.sh` then `up.sh` again to load the new build.

No check presses a key. Only the `live85.py` drop checks move the mouse, and only with `--mouse`: ask the author first, as in [`../since-46-live/README.md`](../since-46-live/README.md#mouse-checks).
