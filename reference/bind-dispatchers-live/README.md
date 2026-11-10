# Live checks for the bind dispatchers (#79)

Checks for the #79 sub-issues, run in the nested Hyprland of [`../since-46-live/`](../since-46-live/), whose `checks.py` and `nest.py` they reuse. Results are in [`RESULTS.md`](RESULTS.md), for the #85 PR.

| File | Checks |
|---|---|
| `live82.py` | #82: `focusmaster` / `swapwithmaster` arguments, ignored messages |
| `live83.py` | #83: direction focus and swap, `binds.focus` / `binds.swap`, `wrap.focus` / `wrap.swap`, `noloop`, edges, a hook that fails |
| `hookblocker.cpp` | For `live83.py hook_failure`: a test plugin that holds the `moveFocus` hook. Build it as its header says, into the harness directory |

## Run

1. Set up the harness as [`../since-46-live/README.md`](../since-46-live/README.md) says, but copy the build under test to `<HARNESS>/build/threecol.so` instead of running G1.
2. Copy the check file in: `cp live82.py "$H/"` (or `live83.py`, and for its `hook_failure` check build `hookblocker.so` into `$H/`).
3. Launch the nested Hyprland, then write its signature to `$H/sig.txt` (the newest `hyprctl instances` entry that isn't the outer session). The script only drives that instance.
4. `python3 -I live82.py all`, or name checks, e.g. `python3 -I live82.py sm_child ignored`.

No check moves the mouse or presses a key.
