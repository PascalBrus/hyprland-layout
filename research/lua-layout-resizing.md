# How can a Lua layout support mouse and keyboard resizing?

Research for [#2](https://github.com/PascalBrus/hyprland-layout/issues/2). Target: Hyprland **0.57.0** with the Lua config, as running here
(`hyprctl version`: tag v0.57.0, commit `0aa6745f705f2c4b0a5004eb019ef76e5bbdb426`).
Source links point at the `v0.57.0` tag on GitHub, which is the commit the live session was built from.
Vocabulary follows `GLOSSARY.md` (Zone, Master zone, Left zone, Stack zone, Hole).

## Answer in short

- **Hyprland never tells a Lua layout how far a window was resized.** Every resize of a tiled window, whether by mouse drag,
  `hl.dsp.window.resize({x, y, relative})`, `resize_on_border` or the trackpad gesture, ends in
  `CLuaTiledAlgorithm::resizeTarget`, which throws away the delta and the grabbed corner and just calls `recalculate()`.
  The layout re-places the window where it was, so the window snaps back on every tick.
- **Mouse resize is still possible** by wrapping the SUPER+right-drag bind in a Lua function. The function marks a resize as
  in progress, starts Hyprland's normal resize drag, and asks to be called again when the button is released
  (`return { request_release = true }`). During the drag Hyprland calls `recalculate` on every mouse tick, and `recalculate` can
  read `hl.get_cursor_pos()` and work out the new zone sizes itself. This gives live feedback, Hyprland's resize cursor and
  focus on the grabbed window, without polling.
- **Keyboard resize must be a `layout_msg`** (e.g. `hl.dsp.layout("resize l +0.02")`). Hyprland's own
  `hl.dsp.window.resize({ x, y, relative = true })` does reach the layout, but without its delta, so the layout can't act on it.
- Upstream has no newer API: `main` has no change to the Lua layout provider since v0.57.0, and the one PR that would have
  added a `resize_target(ctx, target, delta, corner)` callback was closed unmerged.

Everything below is read from source. None of the workarounds were tried in the live session (that would need test windows
and config changes, both out of scope for this ticket).

## 1. What the Lua layout API offers

| Fact | Source |
|---|---|
| A provider has only two callbacks: `recalculate(ctx)` (required) and `layout_msg(ctx, msg)` (optional). | `/usr/share/hypr/stubs/hl.meta.lua:496-499` |
| The C++ provider struct stores only those two call contexts; there is no resize callback. | `/usr/include/hyprland/src/config/lua/layout/LuaLayoutProvider.hpp:19-25`; registration only looks up `recalculate`: [`LuaLayoutProvider.cpp#L231-L242`](https://github.com/hyprwm/Hyprland/blob/v0.57.0/src/config/lua/layout/LuaLayoutProvider.cpp#L231-L242) |
| `ctx` holds `area`, `targets` and the helpers `grid_cell`, `column`, `row`, `split`. No workspace, no reason for the recalculate, no resize info. | `hl.meta.lua:487-494`; [`LuaLayoutContext.cpp#L153-L175`](https://github.com/hyprwm/Hyprland/blob/v0.57.0/src/config/lua/layout/LuaLayoutContext.cpp#L153-L175) |
| A target has `index`, `window`, `box`, `place()`, `set_box()`. | `hl.meta.lua:479-485` |
| The C++ class does override `resizeTarget(Δ, target, corner)` … | `LuaLayoutProvider.hpp:35` |
| … but its body is just `recalculate();`. Δ and corner are dropped. | [`LuaLayoutProvider.cpp#L47-L49`](https://github.com/hyprwm/Hyprland/blob/v0.57.0/src/config/lua/layout/LuaLayoutProvider.cpp#L47-L49) |
| Lua events: no resize or drag event exists (`window.update_state` is the closest; it only reports float/fullscreen-style state). | `hl.meta.lua:5-40` |
| Useful helpers: `hl.get_cursor_pos()`, `hl.is_key_down()`, `hl.timer(fn, {timeout, type})`, `hl.get_active_window()`. | `hl.meta.lua:925, 929, 945, 949, 519-522` |
| `hl.get_cursor_pos()` returns the live pointer position in global layout coordinates. | [`LuaBindingsQuery.cpp#L340-L352`](https://github.com/hyprwm/Hyprland/blob/v0.57.0/src/config/lua/bindings/LuaBindingsQuery.cpp#L340-L352) |
| Each layout callback has a 50 ms budget, a bind callback 100 ms, a timer callback 50 ms. | [`LuaCaller.hpp#L37-L42`](https://github.com/hyprwm/Hyprland/blob/v0.57.0/src/config/lua/LuaCaller.hpp#L37-L42) |

## 2. How a resize reaches the layout

All resize paths go through `CLayoutManager::resizeTarget` → `CSpace::resizeTarget` → `CAlgorithm::resizeTarget`, which
sends **floating** windows to the floating algorithm and **tiled** windows to the tiled one (ours):

- [`LayoutManager.cpp#L63-L82`](https://github.com/hyprwm/Hyprland/blob/v0.57.0/src/layout/LayoutManager.cpp#L63-L82) (pseudotiled windows are handled here and never reach the layout, L64-L78)
- [`Space.cpp#L131-L136`](https://github.com/hyprwm/Hyprland/blob/v0.57.0/src/layout/space/Space.cpp#L131-L136)
- [`Algorithm.cpp#L124-L129`](https://github.com/hyprwm/Hyprland/blob/v0.57.0/src/layout/algorithm/Algorithm.cpp#L124-L129)

So floating windows keep Hyprland's normal resize no matter what threecol does.

Callers of `resizeTarget`:

| Path | What happens | Source |
|---|---|---|
| SUPER+right-drag (`hl.dsp.window.resize()` with no args, on a `mouse = true` bind) | `dsp_mouseResize` → `Actions::mouse("resizewindow 0")` → `beginDragTarget(window, MBIND_RESIZE)`. | [`LuaBindingsDispatchers.cpp#L1050-L1055`](https://github.com/hyprwm/Hyprland/blob/v0.57.0/src/config/lua/bindings/LuaBindingsDispatchers.cpp#L1050-L1055), [`#L669-L675`](https://github.com/hyprwm/Hyprland/blob/v0.57.0/src/config/lua/bindings/LuaBindingsDispatchers.cpp#L669-L675), [`ConfigActions.cpp#L1695-L1737`](https://github.com/hyprwm/Hyprland/blob/v0.57.0/src/config/shared/actions/ConfigActions.cpp#L1695-L1737) |
| … each mouse tick of that drag | For a tiled target: `g_layoutManager->resizeTarget(TICKDELTA, target, m_grabbedCorner)`. Ticks are throttled to about the monitor refresh rate for resize mode. | [`DragController.cpp#L473-L476`](https://github.com/hyprwm/Hyprland/blob/v0.57.0/src/layout/supplementary/DragController.cpp#L473-L476), [`#L385-L386`](https://github.com/hyprwm/Hyprland/blob/v0.57.0/src/layout/supplementary/DragController.cpp#L385-L386) |
| Keyboard `hl.dsp.window.resize({ x, y, relative })` | `Actions::resize` → `resizeTarget(delta, window)` once. | [`LuaBindingsDispatchers.cpp#L1060-L1069`](https://github.com/hyprwm/Hyprland/blob/v0.57.0/src/config/lua/bindings/LuaBindingsDispatchers.cpp#L1060-L1069), [`ConfigActions.cpp#L680-L698`](https://github.com/hyprwm/Hyprland/blob/v0.57.0/src/config/shared/actions/ConfigActions.cpp#L680-L698) |
| `general.resize_on_border` click | `beginDragTarget(w, MBIND_RESIZE)`, then the same drag ticks. Off in this config (`core/general.lua:15`, `hyprctl getoption general:resize_on_border` → false). | [`InputManager.cpp#L867-L877`](https://github.com/hyprwm/Hyprland/blob/v0.57.0/src/managers/input/InputManager.cpp#L867-L877) |
| Trackpad resize gesture | `resizeTarget(delta, …)`. | [`ResizeGesture.cpp#L20`](https://github.com/hyprwm/Hyprland/blob/v0.57.0/src/managers/input/trackpad/gestures/ResizeGesture.cpp#L20) |

What the drag controller does for a **tiled** resize (useful for the workaround):

- It does **not** float the window. Only `MBIND_MOVE` floats a tiled window, which is why the drag-and-drop detection sees
  `window.update_state {float=true}` and a resize would not. [`DragController.cpp#L135-L145`](https://github.com/hyprwm/Hyprland/blob/v0.57.0/src/layout/supplementary/DragController.cpp#L135-L145)
- It picks the grabbed corner from the quadrant of the window the cursor is in and sets the matching `nw/ne/sw/se-resize`
  cursor. [`#L216-L231`](https://github.com/hyprwm/Hyprland/blob/v0.57.0/src/layout/supplementary/DragController.cpp#L216-L231)
- It focuses and raises the grabbed window, so `hl.get_active_window()` is the resized window for the whole drag.
  [`#L241-L244`](https://github.com/hyprwm/Hyprland/blob/v0.57.0/src/layout/supplementary/DragController.cpp#L241-L244)
- It tells the client it is being resized (`setResizing(true)`), and back on `dragEnd`. [`#L246-L247`](https://github.com/hyprwm/Hyprland/blob/v0.57.0/src/layout/supplementary/DragController.cpp#L246-L247), [`#L252`](https://github.com/hyprwm/Hyprland/blob/v0.57.0/src/layout/supplementary/DragController.cpp#L252) ff.
- Any mouse button event or **any key event** ends the drag. [`keybinds/Manager.cpp#L223`](https://github.com/hyprwm/Hyprland/blob/v0.57.0/src/keybinds/Manager.cpp#L223), [`#L353`](https://github.com/hyprwm/Hyprland/blob/v0.57.0/src/keybinds/Manager.cpp#L353)

**Result today:** threecol's `recalculate` runs on every tick and puts the window back in its fixed zone box, so a SUPER+right-drag
on a tiled window does nothing visible, and the keyboard resize dispatcher does nothing at all.

## 3. Upstream: is there a newer API?

- `v0.57.0` is the newest tag (2026-10-28). `main` is 18 commits ahead (checked 2026-11-02, head `15b8b8e8`), and none touch
  `src/config/lua/layout/` (`git log v0.57.0..origin/main -- src/config/lua/layout` is empty). The only commits ever to that
  directory are the original API ([#14258](https://github.com/hyprwm/Hyprland/pull/14258)) and a ctx argument fix (#15061).
- The v0.57.0 release notes list no Lua layout resize feature (Lua additions there: `reload_config`, `get_devices`, minimize
  event, `window.update_state`). <https://github.com/hyprwm/Hyprland/releases/tag/v0.57.0>
- [hyprwm/Hyprland#16485](https://github.com/hyprwm/Hyprland/pull/16485) "layout/lua: pass resizeTarget's delta and corner to an
  optional resize_target" proposed exactly the missing hook: `resize_target(ctx, target, delta, corner)`, corner bits
  top 1, bottom 2, left 4, right 8, called before `recalculate`. It was **closed unmerged on 2026-10-04** by the bot, because
  the author wasn't a vouched contributor, not for technical reasons. The idea may come back later; don't plan on it.

## 4. Mouse resize: workarounds

### 4a. Recommended: wrap Hyprland's resize drag, compute sizes in `recalculate`

How it works, step by step, with the source for each step:

1. Replace the `SUPER + mouse:273` bind (today `hl.dsp.window.resize()` in `core/keybinds.lua:123`) with a Lua function.
   A function bind is called with **no arguments**, and its return table is read for `pass_event`, `ok` and
   **`request_release`**. [`LuaBindingsToplevel.cpp#L210-L223`](https://github.com/hyprwm/Hyprland/blob/v0.57.0/src/config/lua/bindings/LuaBindingsToplevel.cpp#L210-L223),
   [`LuaBindingsInternal.cpp#L389-L413`](https://github.com/hyprwm/Hyprland/blob/v0.57.0/src/config/lua/bindings/LuaBindingsInternal.cpp#L389-L413)
2. If the press returns `request_release = true`, Hyprland stores the bind as a release callback on that mouse button and calls
   the **same function again** when the button goes up.
   [`keybinds/Manager.cpp#L604-L612`](https://github.com/hyprwm/Hyprland/blob/v0.57.0/src/keybinds/Manager.cpp#L604-L612) (store), [`#L458-L463`](https://github.com/hyprwm/Hyprland/blob/v0.57.0/src/keybinds/Manager.cpp#L458-L463) (fire on release).
   The function gets no "pressed" flag, so it tracks press and release with its own toggle.
   Calling `hl.dispatch(hl.dsp.window.resize())` inside the press also sets the release request automatically, because the mouse
   dispatchers call `requestBindRelease` while a bind is being pressed.
   [`LuaBindingsDispatchers.cpp#L36-L46`](https://github.com/hyprwm/Hyprland/blob/v0.57.0/src/config/lua/bindings/LuaBindingsDispatchers.cpp#L36-L46)
3. **On press:** if the window under the cursor is a threecol tile, send `hl.dsp.layout("resize_begin")` so the layout gets a
   `ctx` and can record the cursor position, the zone sizes at the start, and which edge to move (the layout decides this
   itself, e.g. the nearest inner edge of the grabbed zone). Then dispatch `hl.dsp.window.resize()` to start Hyprland's drag.
   For floating windows only the second step runs, so they keep Hyprland's resize.
4. **During the drag:** Hyprland calls `resizeTarget` → `recalculate` once per tick (section 2). While a resize is in progress,
   `recalculate` reads `hl.get_cursor_pos()`, sets the new Left/Stack width or Stack height from it, then places the windows as
   usual. The tile follows the cursor live.
5. **On release** (the second call): clear the in-progress state and keep the new sizes. Hyprland has already ended the drag
   itself on the button release (`Manager.cpp#L353`).

Trade-offs:

- (+) No polling: the drag drives the updates at monitor rate. Hyprland supplies the resize cursor, focus and raise, the
  client's resizing state, and the floating-window behaviour.
- (+) Start and end are both explicit, so no time-based guess like the 2 s window used for drag-and-drop.
- (−) The cursor shape shows Hyprland's quadrant corner (`nw/ne/sw/se`), which may not match the edge threecol moves.
- (−) `recalculate` can't tell a drag tick from any other recalculate (`ctx` has no reason), so the in-progress state must be
  cleared reliably on release. A key press ends the Hyprland drag (`Manager.cpp#L223`) but not the Lua state; the release
  callback still fires when the button comes up, so this is fine as long as the release path always clears the state.
- (−) `layout_msg` goes to the active workspace of the **focused monitor**, not the workspace under the cursor
  ([`LayoutManager.cpp#L91-L104`](https://github.com/hyprwm/Hyprland/blob/v0.57.0/src/layout/LayoutManager.cpp#L91-L104)). Fine on the single DP-1 monitor here.
- (−) The bind wrapper needs to know whether threecol is active and the window is tiled, like `threecol_active()` in
  `core/keybinds.lua:85-88` does for HJKL. The wrapper should look at the window under the cursor, since the drag acts on that
  window, not on the focused one.
- (−) `resize_on_border` and the trackpad gesture would still snap back, because they bypass the bind. Not an issue while
  `resize_on_border = false` and no resize gesture is configured.

### 4b. Own drag without Hyprland's resize

Same bind with `request_release = true`, but instead of dispatching `hl.dsp.window.resize()`, start an
`hl.timer(fn, { timeout = 16, type = "repeat" })` that reads `hl.get_cursor_pos()` and sends a `layout_msg`, and stop it on
release (`timer:set_enabled(false)`). Timer API: [`LuaBindingsToplevel.cpp#L412-L466`](https://github.com/hyprwm/Hyprland/blob/v0.57.0/src/config/lua/bindings/LuaBindingsToplevel.cpp#L412-L466), `hl.meta.lua:835-839`.

- (+) Independent of Hyprland's drag internals; the layout fully owns the gesture.
- (−) Polling at a fixed rate; each tick runs a layout message and a recalculate.
- (−) No resize cursor, no focus/raise, no client resizing state; the client under the cursor keeps getting pointer motion.
- (−) Floating windows must be special-cased in the wrapper (dispatch `hl.dsp.window.resize()` for them).

### 4c. Arm on a separate `non_consuming` click and detect the end by polling or events (the drag-and-drop pattern)

- A tiled resize produces **no** `window.update_state` event (it doesn't float the window, section 2), so the drag-and-drop
  trick has nothing to listen to.
- `hl.is_key_down` reads keyboard keys only (`inputState().isKeycodeDown/isKeysymDown`,
  [`LuaBindingsToplevel.cpp#L384-L410`](https://github.com/hyprwm/Hyprland/blob/v0.57.0/src/config/lua/bindings/LuaBindingsToplevel.cpp#L381-L410)), so a timer can't poll the
  mouse button either (matches the handoff note).
- A separate `release = true` bind doesn't fire while the drag bind is active (handoff observation). The `request_release`
  return of 4a is the supported way to get the release on the same bind.
- **Not recommended:** 4a gives the same thing with a clean start and end.

## 5. Keyboard resize

- `hl.dsp.window.resize({ x = 40, y = 0, relative = true })` on a threecol tile → `Actions::resize` → `resizeTarget(delta)` →
  `recalculate()`, delta dropped ([`ConfigActions.cpp#L693`](https://github.com/hyprwm/Hyprland/blob/v0.57.0/src/config/shared/actions/ConfigActions.cpp#L693),
  [`LuaLayoutProvider.cpp#L47-L49`](https://github.com/hyprwm/Hyprland/blob/v0.57.0/src/config/lua/layout/LuaLayoutProvider.cpp#L47-L49)). It is a visible no-op, and the layout can't
  catch it because nothing in `recalculate`'s `ctx` says a resize happened or by how much.
- **So keyboard resize has to be a `layout_msg`**, e.g. `hl.dsp.layout("resize w +0.02")` / `"resize h -40"`. The layout gets
  `ctx` and can find the focused window with `hl.get_active_window()`, as `move`/`focus` already do.
- `layout_msg` return values: `false` → error "lua layout … rejected layoutmsg", a string → that error, anything else is OK.
  Hyprland **always calls `recalculate()` after `layout_msg`**, so the handler only has to update state; calling `place()`
  inside it (as threecol does today) places everything twice. [`LuaLayoutProvider.cpp#L91-L130`](https://github.com/hyprwm/Hyprland/blob/v0.57.0/src/config/lua/layout/LuaLayoutProvider.cpp#L91-L130)
- Binds can use `repeating = true` for hold-to-resize (allowed for function and dispatcher binds, not with `release`/`mouse`).
  `hl.meta.lua:501-517`, [`LuaBindingsToplevel.cpp#L176-L181`](https://github.com/hyprwm/Hyprland/blob/v0.57.0/src/config/lua/bindings/LuaBindingsToplevel.cpp#L186-L190)
- For floating windows or other layouts, the bind falls back to `hl.dsp.window.resize({ …, relative = true })`, the same
  pattern the HJKL binds use (`core/keybinds.lua:91-107`).

## 6. Consequences for the resize model

- **Sizes live entirely in Lua.** Hyprland never reports a size change for a tiled window, so the zone widths and Stack heights
  are threecol's own state, applied in `recalculate`.
- `ctx` has no workspace field. Per-workspace sizes need the workspace from a target:
  `ctx.targets[1].window.workspace.id` (`hl.meta.lua:872`, `883-902`). `recalculate` is never called with zero targets
  ([`LuaLayoutProvider.cpp#L51-L58`](https://github.com/hyprwm/Hyprland/blob/v0.57.0/src/config/lua/layout/LuaLayoutProvider.cpp#L51-L58)).
- Like the zone assignments, the sizes are lost on `hyprctl reload` unless threecol saves them somewhere (handoff: state lives in
  Lua memory).
- The 50 ms budget per layout callback is ample for a few arithmetic steps per drag tick.

## Sources

- `/usr/share/hypr/stubs/hl.meta.lua` (installed Lua API stubs, 0.57.0)
- `/usr/include/hyprland/src/config/lua/layout/{LuaLayoutProvider,LuaLayoutContext,LuaLayoutTarget}.hpp`
- Hyprland source at tag v0.57.0 (= running build `0aa6745f`): <https://github.com/hyprwm/Hyprland/tree/v0.57.0>
- Hyprland `main` at `15b8b8e8` (2026-11-02) for changes since the release
- Release notes v0.57.0: <https://github.com/hyprwm/Hyprland/releases/tag/v0.57.0>
- Closed PR hyprwm/Hyprland#16485: <https://github.com/hyprwm/Hyprland/pull/16485>
- Live session (read-only): `hyprctl version`, `hyprctl getoption general:resize_on_border`
- `~/.config/hypr/custom/threecol.lua`, `~/.config/hypr/core/keybinds.lua:85-123`, `/tmp/threecol-handoff/HANDOFF.md`
