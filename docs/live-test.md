# Live test checklist

A checklist for testing threecol live, in a real Hyprland 0.57 session on the 5120x1440 ultrawide. The unit tests cover the core module. This checklist covers what they can't see: Hyprland callbacks, rendering, config, binds, files on disk and install.

Run it once every feature ticket is done, and again before each release (see the README's release checklist). Each item is one action and the result to expect. File each failure as a bug. Words in **bold** come from [`GLOSSARY.md`](../GLOSSARY.md).

**Last run:** 2026-11-04, by an agent on Hyprland 0.57.0 (`0aa6745`), threecol at `5bbd1a1`, then a second agent pass in a nested Hyprland 0.57.1 (`acc1aa0`) at `9fd58e0` for the items that need a config edit, a restart or a plugin unload. Ticks below are from both; notes in *italics* ("nested" marks the second pass). Failures, steps and the items left for the author are in [`reference/LIVE-TEST-RESULTS.md`](../reference/LIVE-TEST-RESULTS.md).

## Setup

Start each section on an empty workspace using threecol, with the [`examples/binds.lua`](../examples/binds.lua) binds and default settings, unless the item says otherwise. Use plain terminals (e.g. `kitty`) so windows open fast.

The saved Dividers live in `$XDG_STATE_HOME/threecol/dividers`, or `~/.local/state/threecol/dividers` if `XDG_STATE_HOME` isn't set. Several items below say "check the state file"; this is the file.

## Install and load (#29)

- [ ] `hyprpm add <repo>` then `hyprpm enable threecol` builds without GoogleTest and loads the plugin — *agent: `hyprpm` isn't installed on this machine: author*
- [ ] `hyprpm.toml` has a `commit_pins` entry for the running Hyprland (`hyprctl version`), and `hyprpm update` builds the pinned threecol commit — *agent: pin for the running 0.57.1 (`acc1aa0…` → `9fd58e0`) is present, as is 0.57.0 (`0aa6745…`); `hyprpm update`: author*
- [x] Manual install: clone, `cmake -B build -DCMAKE_BUILD_TYPE=Release -DTHREECOL_TESTS=OFF && cmake --build build -j`, then `hl.plugin.load("/abs/path/threecol.so")` loads it — *nested: fresh clone built against the 0.57.1 headers, no GoogleTest in the build log; loaded by `hl.plugin.load` at start-up, no config errors*
- [x] Loading the repo's own `build/threecol.so` with `hl.plugin.load` works on the live session
- [x] `cmake -B build && cmake --build build -j && ctest --test-dir build` passes — *215/215*
- [x] A build for a different Hyprland commit doesn't register the layout and shows a persistent notification ("built for X, running Y; run `hyprpm update` …") — *nested: the 0.57.0 `build/threecol.so` in 0.57.1: `built for Hyprland 0aa6745, running Hyprland acc1aa0; …`, timeout ~1e11 ms, layout not registered*
- [x] With a mismatched build, threecol workspaces fall back to dwindle and stay usable — *nested: `tiledLayout` dwindle; open, close and `window.resize` work*
- [ ] At start-up with hyprpm, the short dwindle fallback turns into threecol once the plugin loads; windows already open are placed in **Fill order** — *nested, simulated: `hyprctl plugin load` with 4 windows open on dwindle switched the workspace to threecol, placed in Fill order (1st Master, 2nd Left, 3rd and 4th Stack). A real hyprpm login: author*
- [x] `hyprctl plugin unload` puts workspaces back on dwindle without a crash — *nested: dwindle, still usable; loading again brings threecol back*
- [x] With the plugin loaded and no `examples/binds.lua`, no key does anything new: threecol binds no keys itself — *the plugin registers no binds or dispatchers; `hyprctl binds` shows only the author's own*

## Selecting the layout

- [x] `general.layout = "threecol"` applies to every workspace
- [x] `hl.workspace_rule({ workspace = …, layout = "threecol" })` applies it to that workspace only — *tested with runtime rules on workspaces 8 and 10*
- [x] Workspaces on other layouts behave exactly as before (open, close, resize, move) — *dwindle on workspace 8 via a runtime rule: open, close, mouse resize, SUPER+H/L and SHIFT+L*

## Zones and Fill order (#18)

- [x] The first window lands in the **Master zone**, with empty space on both sides
- [x] The second window lands in the **Left zone**
- [x] The third window takes the whole **Stack zone**; further windows join the bottom of the Stack and the **Rows** share the height
- [x] A new window opens straight at its final size, without a visible jump (`predictSizeForNewTarget`) — *first reported size = final size; the visual check is the author's*

## Holes and close rules (#19)

- [x] Closing Master leaves a **Hole**; no other window moves
- [x] Closing the last Left window leaves a Left Hole
- [x] The next new window fills the Hole (Master before Left)
- [x] A Hole keeps its width; the other zones don't move
- [x] Closing a Stack Row closes the gap and the remaining Rows share the height
- [x] After a close, focus goes to the next window along the **Chain** (or the one before, if it was last)
- [x] A window moved in from another workspace lands by Fill order
- [x] A floating window that's un-floated lands by Fill order
- [ ] `hyprctl reload` keeps every window in its Zone and Row, and keeps Holes — ***FAIL:** reload re-places every window by Fill order: Holes fill, windows change zones, Row heights go back to equal (bug 1)*

## Settings (#20)

- [x] Each `plugin:threecol:*` value set with `hl.config({ plugin = { threecol = { … } } })` takes effect on the next layout pass after a reload: `left_width`, `stack_width`, `min_width`, `min_master_width`, `min_row_height`, `step`, `left_rows`, `persist` — *agent: all but `persist` take effect at runtime (straight away, no reload needed). `persist` is only read on a config load; nested: a config edit + reload applies it*
- [x] `layout_opts` with `left_width`, `stack_width` and `left_rows` overrides the global value on that workspace only
- [x] A bad `layout_opts` key or value is ignored and that workspace uses the global value
- [x] An impossible combination is made to fit: Master keeps its minimum, Left and Stack shrink in proportion
- [x] One notification per config load lists what was ignored or adjusted; a clean config shows none — *agent: a clean config gave no notice across 6 reloads. Nested: a bad `left_rows` and bad `layout_opts` key and value gave one notification per reload. Out-of-range globals (`left_width = 2.0`, `stack_width = -1`, `min_width = 'abc'`) are dropped silently by Hyprland, with no notice (see results)*
- [x] A saved **Divider** position wins over the settings until reset
- [x] Tightening a limit clamps the sizes in use and leaves the state file unchanged; loosening it brings the old saved sizes back

## Keyboard resize and reset (#21)

- [x] SUPER+ALT+L / H on Left makes Left wider / narrower by one `step`; Master gives way, the Stack doesn't move — *agent: run as `resize wider/narrower` messages, not keys*
- [x] Same on the Stack, mirrored
- [x] On Master, both Dividers move half a step each and Master stays centred
- [x] With one side at its limit, Master moves only its free side by half a step
- [x] L/H on any Stack Row resizes the whole Stack; on any Left Row, the whole Left zone
- [x] SUPER+ALT+K / J makes the focused Row taller / shorter by one step; the other Rows give way in proportion
- [x] A height key stops when the other Rows are all at `min_row_height`
- [x] K/J does nothing on Master or on a lone Row
- [x] A key that would cross a limit stops at the limit (Left and Stack at `min_width`, Master at `min_master_width`)
- [ ] The resize keys repeat when held — *nested: `hyprctl binds -j` shows `repeat: true` on the four SUPER+ALT binds from `examples/binds.lua`; holding a key: author*
- [x] Each key press is saved (check the state file after one press)
- [x] A Hole resizes like any zone when its neighbour moves the shared Divider
- [x] A lone Master window can grow into the empty side zones
- [x] Resize keys do nothing on floating, fullscreen or maximised windows, and on other layouts — *on dwindle the message returns dwindle's own error `Unknown dwindle layoutmsg: resize wider`; nothing changes*
- [x] SUPER+R puts the workspace's Dividers back to the settings (`layout_opts`, else global) and Row heights back to equal — *agent: reset to the global settings ✓; nested: the SUPER+R bind's action resets a workspace with `layout_opts` (0.30 / 0.20) to that shape, Rows equal, saved entry removed*
- [ ] SUPER+R doesn't repeat when held — *nested: `repeat: false` on the bind; holding a key: author*

## Saved Dividers (#22)

- [x] After a resize, the state file holds the Divider positions, keyed by workspace name, readable by a human
- [x] An unnamed workspace is keyed by its number
- [x] After a reboot, each workspace gets its own Dividers back; Row heights are equal again — *nested: Hyprland restarted (new process, same state dir); workspaces 1 and 3 got their own Dividers back (3 kept its saved Stack over its `layout_opts`), Rows equal*
- [x] Each workspace keeps its own Dividers, independent of the others
- [x] Reset removes that workspace's saved entry
- [x] Renaming a workspace carries its saved sizes over
- [x] `persist = false`: no reads or writes, the file stays in place, and every reload resets every workspace to its settings — *nested*
- [x] Turning `persist` back on restores the old positions — *nested*
- [x] A missing or corrupt state file counts as "nothing saved"; threecol still starts — *nested: missing on the first start; binary garbage at start-up gave the default widths, no error, and the next resize wrote a clean file. The file is read once per start, so a reload doesn't re-read it*
- [x] Sizes still fit after a change of resolution or reserved space (bar on/off) — *nested: 2560→1920 wide, reserved top 60 + left 100, scale 1.25: windows stay inside the usable area and keep the saved fraction*

## Mouse resize (#23)

- [x] SUPER+right-drag on Left moves the Left|Master Divider, live on every frame
- [x] SUPER+right-drag on the Stack moves the Master|Stack Divider
- [x] SUPER+right-drag on Master moves whichever Divider is nearer the cursor at press
- [x] The edge picked at press stays the same for the whole drag, even across zones
- [x] No jump on grab: the edge moves by the cursor's travel since press
- [x] The edge stops at its limit and follows again when the cursor comes back
- [x] Vertical drag on a Row moves its nearer top or bottom edge; only the two Rows either side change
- [x] The top Row has only a bottom edge, the bottom Row only a top edge, a lone Row resizes sideways only
- [x] One drag can change width and height together
- [x] Release saves the Dividers (check the state file)
- [x] Mid-drag, if the grabbed window closes, floats or leaves, or a window joins the zone, the resize ends, keeps its sizes and saves them
- [x] Floating windows and other layouts keep Hyprland's normal resize
- [ ] Resize on a fullscreen or maximised window does nothing — ***FAIL:** Hyprland drops fullscreen/maximised at the press, then the drag moves the Divider and saves it (bug 2)*
- [x] `hyprctl dispatch` `window.resize` on a threecol window moves the same Divider or Row edge by the given amount — *note: `x = -120` on a Stack window moves the Divider left, so the Stack gets wider (question 1)*

## Row heights (#24)

- [x] A height belongs to the Row: a window moved into a Row takes that Row's height
- [x] A new Row gets 1/n of the zone; the others shrink keeping their ratios
- [x] When a Row closes, the others grow in proportion
- [x] A crowded Stack (Rows can't all meet `min_row_height`) still takes the window and shares height equally; the old proportions come back once the count fits
- [x] `min_row_height` stays a hard limit for resizing even in a crowded Stack

## Left zone with several Rows (#25)

- [x] With `left_rows = 2`, the second and third windows fill Left top to bottom, then the Stack
- [x] Left Rows close gaps, resize and keep limits exactly like Stack Rows
- [x] Closing a Left Row that isn't the last closes the gap; closing the last Left Row leaves a Hole
- [x] Lowering `left_rows` moves the extra bottom Left windows to the top of the Stack, in Chain order
- [x] Raising `left_rows` moves nothing
- [x] An impossible `left_rows` uses only as many Rows as fit `min_row_height`

## Chain focus and move (#26)

- [x] SUPER+H/L walks the Chain: Left top → Left bottom → Master → Stack top → Stack bottom
- [x] The Chain stops at both ends; no wrap
- [x] Focus skips empty zones (SUPER+L from Left reaches the Stack when Master is a Hole)
- [x] SUPER+J/K moves inside Left or the Stack, never leaves the zone, and does nothing on Master
- [x] SUPER+SHIFT+H/L/J/K moves the focused window along the same Chain and zones
- [x] A move into a Hole or an empty Stack moves the window there; no swap
- [x] A move into a Left below its Row limit joins as the bottom Row
- [x] SHIFT+L from Master joins the top of the Stack and leaves a Master Hole; SHIFT+H moves it back
- [x] Moves swap only with an occupied Master, the bottom Row of a full Left, or a neighbouring Row
- [x] A moved window takes its new Row's height and its new zone's width, keeps focus, and no Divider moves
- [x] SUPER+M focuses Master; does nothing on a Hole — *note: it also works from a floating window; the README says layout messages do nothing there (README/code mismatch, see results)*
- [x] SUPER+SHIFT+M swaps the focused window with Master (the old Master takes its Row); on a Hole it moves in; does nothing on Master, floating windows or with no focus — *"no focus" case not checked*
- [x] Focus and move binds don't repeat when held — *with the author's own binds (SUPER+L, SUPER+SHIFT+H held 1.5 s)*
- [x] On floating windows, fullscreen workspaces, empty workspaces and other layouts, the same keys fall back to Hyprland's normal focus and move — *nested, with `examples/binds.lua`: the HJKL binds send `focus`/`move` in all four cases and the layout message only on a tiled threecol window. The author's own `keybinds.lua` still lacks the `has_fullscreen` check*
- [x] Native `window.move` (bind, `hyprctl`) runs the same move code

## Drag-and-drop (#27)

- [x] SUPER+left-drag (`window.drag`) drags a tiled window with no arm bind
- [x] During the drag the window's place stays open and nothing else moves
- [x] Dropping on another window swaps the two; each takes the other's Row height and zone width
- [x] Dropping on the bottom quarter of a zone's bottom Row adds a new bottom Row
- [x] The Stack always accepts an "add" drop; Left only below its Row limit (a full Left swaps instead)
- [x] Dropping on the "add" area of its own zone moves the window to the bottom
- [x] Dropping into a Hole or an empty Stack moves the window there; its old place follows the close rules — *Hole ✓; empty Stack not checked by drag (checked by `move r`)*
- [x] Dropping on its own place, outside the workspace or on a non-tiled window changes nothing
- [x] A drop in a gap counts as the zone it's in and the nearest Row
- [x] Focus stays on the dragged window after the drop
- [x] A plain float toggle (`window.float`) never swaps anything

## Drop indicator (#28)

- [ ] While dragging, one rectangle shows where the window will land: active-border colour (gradient as Hyprland draws it), ~20% fill, the layout's gaps and rounding — *agent: screenshots taken (tint, rounded border, gaps); whether colour and gradient look right is the author's call*
- [x] It sits above the tiles and below the dragged window
- [x] It follows the cursor live and always matches where the drop lands (swap, add, Hole)
- [x] No indicator when the drop would change nothing
- [x] It disappears on release and when the drag ends any other way — *but closing the dragged window mid-drag moves the window under the cursor into its place (bug 3)*

## Binds and README (#30)

- [x] `examples/binds.lua` pasted into a fresh config gives every bind in the spec and no drag-arm bind — *nested: 17 binds, resize keys repeat, the rest don't*
- [x] Every bind listed under "Will my existing binds still work?" behaves as the README says — *all three tables checked by `hyprctl`. See results for the `focusmaster` mismatch and the `window.resize` sign*
- [x] The README's settings table matches the registered `plugin:threecol:*` values and defaults (`hyprctl getoption plugin:threecol:<name>`)
- [ ] The README opens with the GIF from the Showcase below, and it plays on GitHub
- [x] The README explains the short dwindle fallback at start-up, and it matches what you saw under Install and load — *matches the simulated late load*
- [x] The README's release checklist includes adding a `commit_pins` entry and running this checklist

## Showcase

The shots for the README GIF (#30), in order. Each is a few seconds and shows one feature; together they should stay under about 45 seconds. Record on the ultrawide with a few plain terminals.

1. **Fill order:** open four windows one by one: Master, Left, then two Stack Rows.
2. **Holes:** close Master; nothing slides in. Open a new window; it fills the Hole.
3. **Mouse resize:** SUPER+right-drag the Left|Master Divider, then a Stack Row edge, re-tiling live.
4. **Keyboard resize and reset:** SUPER+ALT+L a few times on Master, then SUPER+R.
5. **Drag-and-drop:** drag a Stack window over Master with the drop indicator showing, drop to swap; then drag one onto the Stack's "add" area.
6. **Chain focus and move:** SUPER+H/L walks the Chain; SUPER+SHIFT+L moves Master into the Stack, leaving a Hole.
7. **Left with several Rows:** a workspace with `left_rows = 2` holding two Left windows.
8. **Per-workspace shape:** switch to a workspace with a `layout_opts` override and a different shape.
9. **Survives reload:** `hyprctl reload`; nothing moves.
