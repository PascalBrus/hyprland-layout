# Live checks for the bind dispatchers (#79)

Checks for the #79 sub-issues, run in the nested Hyprland of [`../since-46-live/`](../since-46-live/), whose `checks.py` and `nest.py` they reuse. Results are in [`RESULTS.md`](RESULTS.md), for the #85 PR.

| File | Checks |
|---|---|
| `live82.py` | #82: `focusmaster` / `swapwithmaster` arguments, ignored messages |
| `live83.py` | #83: direction focus and swap, `binds.focus` / `binds.swap`, `wrap.focus` / `wrap.swap`, `noloop`, edges, a hook that fails |
| `hookblocker.cpp` | For `live83.py hook_failure`: a test plugin that holds the `moveFocus` hook. Build it as its header says, into the harness directory |

## Run

1. `reference/since-46-live/up.sh "$H"` sets up the harness in `$H` with `build/threecol.so` and these check files, and starts the nested Hyprland. See [`../since-46-live/README.md`](../since-46-live/README.md#set-up). For `live83.py hook_failure`, also build `hookblocker.so` into `$H/`.
2. `cd "$H" && python3 -I live82.py all`, or name checks, e.g. `python3 -I live82.py sm_child ignored`. The scripts drive only the instance in `$H/sig.txt`.
3. `reference/since-46-live/down.sh "$H"` stops it. After a rebuild, run `down.sh` then `up.sh` again to load the new build.

No check moves the mouse or presses a key.
