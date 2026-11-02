# Handoff: turn the `threecol` Hyprland layout into a git repo

## Goal of the next session
Create a git repository for the custom Hyprland Lua layout **threecol** (built in the previous
session) so it can be versioned and reused. Decide with the user where the repo lives and whether
the config should keep loading the layout from its current path (e.g. symlink/clone into
`~/.config/hypr/custom/`) or from the repo directly. **Ask before moving files**, because the live
Hyprland session loads them on every reload.

`~/.config/hypr` is **not** a git repo today. Don't run `git init` there unless the user asks. The
directory holds unrelated configs and backups (`dms/`, `.migration-attempt-1/`, `*.backup*`).

## Environment
- Arch Linux, Hyprland **0.57.0** with the **Lua config** (`~/.config/hypr/hyprland.lua`, not `.conf`).
- One monitor: DP-1, 5120x1440 ultrawide. Fish is the login shell; commands run in bash.
- Lua API stubs: `/usr/share/hypr/stubs/hl.meta.lua`. Read these for API facts, because the online wiki may lag behind.
- C++ headers for the Lua layout provider: `/usr/include/hyprland/src/config/lua/layout/`.

## What the layout does
Three fixed columns: **left ¼ | middle ½ (master) | right ¼ (stack)**. Change the width with `SIDE = 0.25`.
- Every window remembers its column. Opening order: middle → left → bottom of the right stack.
- Closing a window leaves its spot empty, and the next new window fills it. Nothing slides over.
  The user asked for this explicitly.
- A single window sits only in the middle half, with empty space on both sides.
- State is keyed by `window.stable_id`, **not** `address`. Hyprland reuses addresses, which caused a bug
  where new windows opened in the right stack.
- State lives in Lua memory, so `hyprctl reload` resets it and windows are re-sorted by opening order.

### Keybinds (all handled by the layout through layoutmsgs)
| Bind | layoutmsg | Behaviour |
|---|---|---|
| SUPER+H/L | `focus l/r` | focus along the chain left ↔ middle ↔ right-top ↔ … ↔ right-bottom (skips empty spots) |
| SUPER+J/K | `focus d/u` | focus within the right stack |
| SUPER+SHIFT+H/L | `move l/r` | swap along the same chain, or move into an empty spot |
| SUPER+SHIFT+J/K | `move d/u` | reorder within the right stack |
| SUPER+M | `focusmaster` | focus the middle window (the tile-selector's `M` uses this too) |
| SUPER+SHIFT+M | `swapwithmaster` | swap the focused window with the middle one; focus stays on the moved window |
| SUPER+left-drag | `drop <stable_id>` (internal) | drop onto a window to swap, or onto an empty column to move there |

The HJKL binds fall back to Hyprland's normal focus/move on floating windows, empty workspaces and other layouts.

## Files (all in `~/.config/hypr/`)
- `custom/threecol.lua`: **the layout** (~300 lines, self-documenting header). It registers with
  `hl.layout.register("threecol", {recalculate, layout_msg})` and returns a module `M` with `M.arm_drag()`.
- `hyprland.lua` line 10: `require("custom.threecol")`. This must come **before** `core.general` and `core.keybinds`.
- `core/general.lua` line 22: `layout = "lua:threecol"`. A registered Lua layout needs the **`lua:` prefix**.
  Without it Hyprland silently falls back to dwindle.
- `core/keybinds.lua` lines ~84-107: the `threecol_active()` helper and the HJKL focus/move binds.
  Line 121: the drag-arming bind. It **must be registered before** the `SUPER + mouse:272` drag bind on
  line 122, or it never fires.
- `core/layout.lua`: the old master-layout settings (center orientation). These are unused now but harmless.
- `custom/keybinds-tile-selector.lua` and `custom/scripts/`: the user's existing tile selector (SUPER+I).
  It isn't part of threecol but uses `focusmaster`.

The integration points in `core/` mean the repo needs either those snippets documented (README /
example) or the binds moved into the layout module (e.g. an `M.setup_binds()` function). Discuss this with the user.

## Hyprland Lua API findings (not documented elsewhere)
- `ctx.targets` order is Hyprland's own. Removing and re-adding a target appends it, so the layout keeps its own ordering.
- `target:place(box)` applies gaps. `ctx.area` already excludes reserved space.
- Recalculate is **never called with 0 targets**.
- Drag of a tiled window: Hyprland floats it for the duration (`window.update_state` fires with `{float=true}`,
  and later `{float=false}` on release). The `ctx.targets` count drops during the drag. A plain float toggle
  produces the **same** events, so drag detection is armed by a `non_consuming` SUPER+click bind and requires
  the float to start within 2s of the click.
- A `release=true` bind on `SUPER + mouse:272` does **not** fire while the drag bind is active.
- `hl.is_key_down` works for keys (`"Super_L"`, keycode 133) but not for mouse buttons.
- `hyprctl dispatch "<lua expr>"` runs Lua, e.g.
  `hyprctl dispatch "hl.dsp.layout('focus l')"` or `hl.dsp.exec_cmd('cmd', { workspace = '9 silent' })`.
- Ghostty opens new windows in the existing process unless you pass `--gtk-single-instance=false`, which also ignores workspace rules.

## Testing approach used
- Spawn throwaway terminals on empty workspace 9:
  `hyprctl dispatch "hl.dsp.exec_cmd('ghostty --gtk-single-instance=false --title=T1', { workspace = '9 silent' })"`.
  Inspect them with `hyprctl clients -j | jq`, then kill them by **pid**. Never kill the shared ghostty pid,
  because the user's own terminals use it.
- `/tmp/threecol-handoff/drag.py x1 y1 x2 y2`: simulates SUPER+left-drag through a temporary uinput device
  (`/dev/uinput` is writable; no ydotool is installed). Run it with `python3 -I`. A good candidate for
  `tests/` or `tools/` in the repo.
- Scenarios that were verified: opening sequence; closing the left/middle leaves a hole that the next
  window refills; all focus and move directions; focusmaster/swapwithmaster; drag swaps (middle↔right-top,
  right-bottom→left, onto itself = no-op, into an empty column, a lone master dragged to the left/right/middle);
  a float toggle never swaps.

## Open ideas the user hasn't asked for yet
- No resize support (SUPER+right-drag / `resizeTarget` isn't exposed to Lua). The column widths are fixed.
- If the middle closes, it stays empty. The user was offered "top-right window fills the middle" but hasn't answered.

## Suggested skills
- **dave-writing-for-agents**: if you write an AGENTS.md/CLAUDE.md for the new repo.
- **dave-domain-modeling**: optional, for a short GLOSSARY/ADR on the column model and drag detection.
- **dave-pr**: if the repo gets pushed and a PR is opened.
- **dave-wizard**: only if the user needs guided manual steps (e.g. creating a remote on their Git host).
