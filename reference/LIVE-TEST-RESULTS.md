# Live test results (2026-11-04)

The agent run of [`docs/live-test.md`](../docs/live-test.md) (#32), following [`LIVE-TEST-HANDOFF.md`](LIVE-TEST-HANDOFF.md).

- Hyprland 0.57.0, commit `0aa6745f705f2c4b0a5004eb019ef76e5bbdb426`. threecol 0.1.0 at `5bbd1a1`, loaded from `build/threecol.so` (current with the source).
- DP-1 5120x1440, default `plugin:threecol:*` values. Test windows: `ghostty` on workspaces 8–10 only.
- Tools: [`livetest.py`](livetest.py) for `hyprctl`, [`uinput.py`](uinput.py) for mouse and keys (new: right-drag, pause mid-drag, held keys, refuses SUPER+ALT+L, checks the active workspace before every press and step).

**Result (after the author's pass below):** 118 items pass, 3 open (2 failures, 1 left for the author: the README GIF). 3 bugs (one found under an item that otherwise passes), 1 question, 3 notes from the second pass, 2 from the author's pass.

First pass alone: 97 pass, 24 open. After the second pass: 112 pass, 9 open.

## Bugs

### 1. `hyprctl reload` re-places every window

Checklist: Holes and close rules, "`hyprctl reload` keeps every window in its Zone and Row, and keeps Holes".

Steps (workspace with 4 windows: Master T1, Left T2, Stack T3 above T4):

1. (a) `resize taller` on T3 so the Rows are unequal; or (b) close T1 to leave a Master Hole; or (c) close T2 to leave a Left Hole.
2. `hyprctl reload`.

Seen:

- (a) Row heights go back to equal (744/607 → 676/675).
- (b) Every window moves: T2 (Left) → Master, T3 (Stack top) → Left, T4 → whole Stack. The Hole is gone.
- (c) T3 → Left, T4 → whole Stack. The Hole is gone.
- Saved Dividers do survive.

It hits every threecol workspace, not just the test one. On the author's workspace 4, the final reload swapped the two windows' zones. It looks like the layout is rebuilt from scratch on reload and the windows are re-added in Fill order; `src/plugin/ThreecolAlgorithm.cpp` keeps no Zone or Row state across that.

### 2. Mouse resize on a fullscreen or maximised window moves a Divider

Checklist: Mouse resize, "Resize on a fullscreen or maximised window does nothing".

Steps: 4 windows, focus Master T1, `hl.dsp.window.fullscreen({ mode = 'maximized' })` (or `'fullscreen'`), then SUPER+right-drag from (2000, 700) to (2200, 760).

Seen: at the press, before any movement, T1 is no longer fullscreen (`fullscreen` 1 or 2 → 0). The drag then moves the Left|Master Divider by +200 px and saves it to the state file. Same in both modes.

The fullscreen state probably ends inside Hyprland's mouse-bind handling, before threecol sees a resize. So the fix may be in threecol (refuse to start a drag resize on a window that was fullscreen at the press), or the item's expected result may need to change.

### 3. Closing the dragged window mid-drag moves the window under the cursor

Checklist: found under Drop indicator, "It disappears on release and when the drag ends any other way". The indicator does disappear, but:

Steps: 4 windows (Master T1, Left T2, Stack T3 above T4). SUPER+left-drag T4, hover over Master (2560, 700), close T4 (SIGTERM) while still holding the button, then release.

Seen: as soon as T4 closes, T1 moves out of Master into T4's old Row at the bottom of the Stack, leaving a Master Hole. Hovering over Left instead moves T2 into the Stack and leaves a Left Hole. Hovering over T4's own zone (Stack top) works: T4's Row just closes. Reproduced 3 out of 3 times.

Expected: T4's place follows the close rules and no other window moves. Where to look: `m_dragged` in `ThreecolAlgorithm.cpp` isn't cleared when the dragged target dies, and Hyprland may hand the still-held drag to the window now under the cursor.

## Question

1. **`window.resize` sign.** `hl.dsp.window.resize({ x = -120, y = 0, relative = true })` on a Stack window moves the Master|Stack Divider 120 px **left**, so the Stack gets wider. That matches the checklist ("moves the same Divider by the given amount", like a mouse drag), but in Hyprland `x` is usually a change in the window's size, where −120 would make it narrower. On Left and for Row heights the two meanings agree, so only the Stack (and Master's right side) differ. Which is intended? The README says "Moves the focused window's Divider or Row edge by that amount", which supports the current behaviour.

## README/code mismatches and notes

- **`focusmaster` from a floating window** focuses Master (`ThreecolAlgorithm.cpp:438`). The README says a message on a floating window "does nothing and reports no error" (README line 132). Known; confirmed live.
- **`persist` only changes on a config load.** Set at runtime with `hl.config`, the other seven settings apply straight away, but `persist` is only read in `SavedDividers.cpp` `configLoaded()`. A runtime `persist = false` still writes the file. Matches the README ("apply as soon as the config reloads"), but it's the one setting that behaves differently at runtime.
- **The author's `keybinds.lua`** `threecol_active()` doesn't check `has_fullscreen`, unlike `examples/binds.lua`. On a fullscreen workspace SUPER+HJKL send the layout message, which threecol ignores, so the keys do nothing instead of falling back.
- **Resize keys on dwindle** do nothing, but dwindle returns its own error (`Unknown dwindle layoutmsg: resize wider`). The README says "the resize and reset keys do nothing on other layouts", which is true for the layout.

## Second pass: nested Hyprland (2026-11-04)

The items left for the author that need only a config edit, a restart or a plugin unload, run in a **nested Hyprland** (a second instance running as a window on the author's workspace 4) with its own config and `XDG_STATE_HOME` in the agent's scratchpad. Nothing was sent to the author's session except launching that window (silently, without focus). No synthetic keys or mouse: binds from `examples/binds.lua` were run by recording each bind's action at config load and calling it through `hyprctl dispatch`.

- Nested Hyprland **0.57.1** (`acc1aa0`), the version now installed (the author's session still runs 0.57.0). threecol `9fd58e0`, a fresh clone built Release with tests off. The full suite also passes against the 0.57.1 headers (215/215).
- 15 more items pass (see the checklist, notes marked "nested").

### Notes

1. **Hyprland 0.57.1 is installed but not pinned.** The session runs 0.57.0; the next login runs 0.57.1. `build/threecol.so` (built for `0aa6745`) is then refused with the mismatch notification and every workspace uses dwindle until it's rebuilt. `hyprpm.toml` has no `commit_pins` entry for `acc1aa0bfd4bd575dcb0f76c35ccf12b660a1ab9`; `9fd58e0` builds and passes the nested run on it.
2. **Out-of-range global settings are dropped silently.** `hl.config({ plugin = { threecol = { left_width = 2.0, stack_width = -1, min_width = 'abc' } } })`: Hyprland drops each value (range 0–1 from `Config.cpp`), `getoption` shows the default with `set: false`, and there's no config error and no threecol notice. Only values that pass Hyprland's check reach threecol's report. The checklist item asks threecol to list what it ignored or adjusted, so it passes, but the user gets no feedback for these.
3. **Float noise in the notice.** `left_rows 99 doesn't fit min_row_height 0.10000000149011612; using 9`: the value is a float widened to double. `{:.3g}` (as the width warning already uses) would print `0.1`.

## Third pass: the author (2026-11-04)

The author, on the live Hyprland 0.57.1 session (`acc1aa0`), guided by a wizard script (not committed). Test windows on workspace 9 only; the state file was backed up first and matched the backup afterwards.

- **Held keys.** Temporary runtime binds with the same flags as `examples/binds.lua`, each counting its calls and acting only on workspace 9. Held ~2 s: SUPER+ALT+H fired 18× (Left 1269 → 840 px), K 10×, J 11×; SUPER+R once. SUPER+ALT+L wasn't held (it locks the author's screen); it has the same flags as H.
- **Visual.** Drop indicator and new windows (no visible jump) look right.
- **hyprpm.** `hyprpm update` built the 0.57.1 headers; `hyprpm add https://github.com/PascalBrus/hyprland-layout` read the manifest from `master`, logged `commit pin 9fd58e0… matched hl`, and built with `-DTHREECOL_TESTS=OFF` (no GoogleTest in the log); `hyprpm enable threecol` loaded it. `hyprpm update -f` rebuilt headers and threecol at the same pin and loaded it.
- **Start-up.** The author's `hyprland.lua` was switched from `hl.plugin.load(build/threecol.so)` to the README's `hyprpm reload` hook. After a login, threecol was loaded from `/var/cache/hyprpm/pascal.brus/threecol/threecol.so` and every workspace used it. The author kept the hyprpm setup (backup of the old config in `reference/testwizard/.run/`).

### Notes

1. **hyprpm needs current Hyprland build deps.** The first `hyprpm update` failed: building the 0.57.1 headers needs `hyprland-protocols >= 0.8.0`, and the machine had a stale AUR `hyprland-protocols-git` 0.7.0. Replacing it with `extra/hyprland-protocols` 0.8.0 fixed it. This is hyprpm building Hyprland itself, not threecol; the README's dependency line only covers the plugin build.
2. **With hyprpm, the session runs the pinned build, not `build/`.** Local rebuilds no longer load at login; for development, load `build/threecol.so` by hand or switch the config back.

## Left for the author

Not run, and why (after the third pass):

| Item | Why |
|---|---|
| README GIF plays on GitHub | needs recording; shot list in the checklist's Showcase section |

Drop indicator screenshots from this run (scratchpad of the agent session, not committed): `ind_master_tl.png` (corner close-up: rounded cyan border, tint), `swap.png`, `hole.png`, `fullleft_add.png`, `noop_own.png` (no indicator), `after_release.png` and `after_close.png` (indicator gone), in `/tmp/claude-1000/-home-pascal-brus-Projects-work-hyprland-layout/5d2fd6c1-72d1-499a-b159-05db69748fa0/scratchpad/shots/`.

## Cleanup and side effects

- All 13 test windows closed by pid; workspaces 8–10 are empty. Back on workspace 1.
- State file restored from the backup taken at the start; it still matched after the final reload (only the workspace 5 entry).
- `hyprctl reload` done; every `plugin:threecol:*` is back to its default with `set: false`; the runtime workspace rules (dwindle on 8, `layout_opts` on 8 and 10) are gone.
- **Workspace 4 (the author's):** one `resize narrower` landed there by mistake. Focus was on workspace 4 between two dispatches; the harness has guarded every layout message since. Its Dividers are 0.275 / 0.275 (Left 1396 px, Master 2286 px), and the final reload (bug 1) swapped its two windows' zones. The author chose to fix it themselves (SUPER+R there resets it).
- Workspaces 1–3 were unchanged by the final reload. Five earlier test reloads happened before the snapshot; with no Holes or unequal Rows on 1–3, bug 1 most likely left them as they were.
