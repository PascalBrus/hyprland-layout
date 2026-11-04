# Handoff: run the live test checklist (#32)

Your job: run every item in [`docs/live-test.md`](../docs/live-test.md) that an agent can run on the author's live Hyprland session. Tick what passes, write down what fails, and leave a list of the items only the author can run. Don't fix bugs; file them (or list them for the author) with the item, the steps and what you saw.

This runs on the author's **real session**. Their own windows are on workspaces 1–3. Work only on empty workspaces (8–10), and only touch windows you started.

## What's already known (2026-11-04)

- Hyprland 0.57.0, commit `0aa6745f705f2c4b0a5004eb019ef76e5bbdb426`. `hyprctl plugin list` shows threecol 0.1.0 loaded.
- `~/.config/hypr/hyprland.lua` loads `~/Projects/work/hyprland-layout/build/threecol.so`, and `core/general.lua` sets `layout = "threecol"`. All workspaces are threecol. Rebuild with `cmake --build build -j` if you need a fresh `.so`; a running session keeps the old one until reloaded.
- Monitor DP-1, 5120x1440, scale 1. A bar reserves 66 px at the top. `gaps_in` 5, `gaps_out` 0 5 5 5, `border_size` 2, `rounding` 12. All `plugin:threecol:*` values are at their defaults (`set: false`).
- Terminal: `ghostty` only (no kitty). Start it with `--gtk-single-instance=false`, or it joins the author's ghostty process.
- `/dev/uinput` is writable and the user is in group `input`. `grim` is installed. No ydotool or wtype.
- State file: `~/.local/state/threecol/dividers` exists and holds the author's real entry for workspace 5. **Back it up before you start and restore it at the end.**
- Smoke test passed: four windows on workspace 9 gave Master x=1290 w=2541, Left x=7 w=1269, Stack x=3845 w=1268, two Stack Rows of 676 and 675 px.

## Safety rules

1. **Never send SUPER+ALT+L.** In the author's config it locks the screen (`core/keybinds.lua:42`). Use `hyprctl dispatch "hl.dsp.layout('resize wider')"` instead.
2. Kill only windows you started, by **pid**. Never kill the shared ghostty pid.
3. Don't edit anything in `~/.config/hypr` without asking the author first. That includes adding `examples/binds.lua` or workspace rules.
4. Don't run `hyprctl plugin unload`, reboot, or log out. Those items are for the author.
5. Settings changed at runtime are undone with `hyprctl reload`. Do that at the end, and check `hyprctl getoption plugin:threecol:*` is back to defaults.
6. Clean up at the end: close your windows, restore the state file, go back to workspace 1.

## Tools

- [`reference/livetest.py`](livetest.py): small helpers. Import it from a script and run with `python3 -I`.
  - `spawn(ws, "T1")`: open a ghostty titled `T1` on workspace `ws` (silent); waits for it.
  - `geo(ws)` / `show(ws)`: `title -> (x, y, w, h)` of the tiled windows.
  - `focus("T1")`, `active()`, `msg("focus l")` (layout message), `d(lua_expr)` (any dispatch), `goto(ws)`, `kill("T1")`, `cleanup()`, `state()` (state file text).
- [`reference/drag.py`](drag.py) `x1 y1 x2 y2`: SUPER+left-drag through a temporary uinput device. Proven on the old Lua layout.
- **To build:** a uinput helper based on `drag.py` that can also:
  - right-drag (`BTN_RIGHT` = 0x111) for the mouse resize items
  - pause mid-drag, so the script can close, float or add a window, or take a `grim` screenshot, before release
  - press and hold a key, for "repeats when held" (only on safe keys, see rule 1)
- `hyprctl dispatch "<lua expr>"` runs Lua. Dispatchers are `hl.dsp.*` (`focus`, `layout`, `window.move`, `window.resize`, `window.float`, `window.fullscreen`, `window.close`, `window.swap`, `cursor.move`, `exec_cmd`). Their argument tables aren't typed in `/usr/share/hypr/stubs/hl.meta.lua`, so try each one on a throwaway window first.
- `grim -g "x,y wxh" file.png`, then look at the image, for anything visual (drop indicator, notifications).

## How to check results

Use window rectangles from `hyprctl clients -j`, never screenshots, unless the item is visual. Derive Zone and Row from the rectangles. With the defaults, the Left zone starts near x=7, Master near x=1290 and the Stack near x=3845. Compare with a tolerance of a few pixels for gaps and rounding. For Divider items, read the state file before and after.

## Plan by section

**A** = `hyprctl` only, **B** = needs uinput, **C** = needs a screenshot, **U** = author only.

| Section | How | Notes |
|---|---|---|
| Install and load | mostly U | Agent can run: `hyprctl version` vs `commit_pins` in `hyprpm.toml`; the full build + `ctest` command; "binds no keys" by checking `hyprctl binds -j` has nothing threecol adds. Everything else is U |
| Selecting the layout | A | Check `tiledLayout` in `hyprctl workspaces -j`. Per-workspace rule: try `hl.workspace_rule` through `hyprctl dispatch` at runtime; if that doesn't work, it's U (config edit). Rollback to `lua:threecol` is U |
| Zones and Fill order | A | "No visible jump" for a new window: compare its first reported size with its final one. The visual part is U |
| Holes and close rules | A | Move-in from another workspace: `window.move` to workspace 9. Un-float: `window.float` toggle. Reload: `hyprctl reload`, then compare `geo` before and after |
| Settings | A | Set values at runtime with `hyprctl dispatch "hl.config({ plugin = { threecol = { … } } })"`; first check that this works and triggers a relayout. Notification: C. `layout_opts` needs a workspace rule (see above) |
| Keyboard resize and reset | A + B | Behaviour via `msg("resize wider")` etc. and `msg("reset")`. "Repeats when held" and "SUPER+R doesn't repeat" are bind options: only testable once `examples/binds.lua` is loaded, which needs the author's OK (rule 3). Otherwise U |
| Saved Dividers | A | Back up the state file first. Rename: `hyprctl dispatch` a workspace rename, then check the file. Reboot and resolution/bar change: U |
| Mouse resize | B | Right-drag helper. Check "no jump on grab" by comparing the rectangles at press and after the first small move. Mid-drag changes need the pause step. `window.resize` via `hyprctl dispatch` is A |
| Row heights | A | Use `msg("resize taller")` to make heights unequal first, then move or close windows |
| Left with several Rows | A | Set `left_rows` at runtime as in Settings |
| Chain focus and move | A | `msg("focus l")`, `msg("move r")`, `focusmaster`, `swapwithmaster`; native `window.move({ direction = … })`. The bind fallback (floating, fullscreen, empty, other layout) is in the author's own `keybinds.lua`; test it by checking what the dispatches do, and note that the author's `threecol_active()` doesn't check fullscreen, unlike `examples/binds.lua`. "Don't repeat when held" is U |
| Drag-and-drop | B | `drag.py`. A float toggle (`window.float`) never swaps: A |
| Drop indicator | B + C | Pause mid-drag and `grim` the area. Check colour, fill, rounding, and that the rectangle matches where the drop lands. Show the screenshots to the author for the "looks right" part |
| Binds and README | partly A | Compare the README settings table with `hyprctl getoption` for each value. The snippet-in-a-fresh-config item and the GIF are U |

## Known issue to keep in mind

The README says layout messages on a floating window do nothing, but `focusmaster` works from a floating window (`src/plugin/ThreecolAlgorithm.cpp:438`). If the SUPER+M item shows this, report it as a README/code mismatch; don't count it as a layout bug.

## What to hand back

1. `docs/live-test.md` with the items you ran ticked (`[x]`), and failures marked with a short note.
2. A list of failures, each with steps and what you saw, ready to file as bugs.
3. The list of items left for the author (every **U** above, plus anything you couldn't run and why).
4. Confirmation that cleanup is done (windows closed, state file restored, settings back to defaults).
