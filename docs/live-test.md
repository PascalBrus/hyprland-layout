# Live test checklist

A checklist for testing threecol live, in a real Hyprland 0.57 session on the 5120x1440 ultrawide. The unit tests cover the core module. This checklist covers what they can't see: Hyprland callbacks, rendering, config, binds, files on disk and install.

Run it once every feature ticket is done, and again before each release (see the README's release checklist). Each item is one action and the result to expect. File each failure as a bug. Words in **bold** come from [`GLOSSARY.md`](../GLOSSARY.md).

## Setup

Start each section on an empty workspace using threecol, with the [`examples/binds.lua`](../examples/binds.lua) binds and default settings, unless the item says otherwise. Use plain terminals (e.g. `kitty`) so windows open fast.

The saved Dividers live in `$XDG_STATE_HOME/threecol/dividers`, or `~/.local/state/threecol/dividers` if `XDG_STATE_HOME` isn't set. Several items below say "check the state file"; this is the file.

## Install and load (#29)

- [ ] `hyprpm add <repo>` then `hyprpm enable threecol` builds without GoogleTest and loads the plugin
- [ ] `hyprpm.toml` has a `commit_pins` entry for the running Hyprland (`hyprctl version`), and `hyprpm update` builds the pinned threecol commit
- [ ] Manual install: clone, `cmake -B build -DCMAKE_BUILD_TYPE=Release -DTHREECOL_TESTS=OFF && cmake --build build -j`, then `hl.plugin.load("/abs/path/threecol.so")` loads it
- [ ] Loading the repo's own `build/threecol.so` with `hl.plugin.load` works on the live session
- [ ] `cmake -B build && cmake --build build -j && ctest --test-dir build` passes
- [ ] A build for a different Hyprland commit doesn't register the layout and shows a persistent notification ("built for X, running Y; run `hyprpm update` …")
- [ ] With a mismatched build, threecol workspaces fall back to dwindle and stay usable
- [ ] At start-up with hyprpm, the short dwindle fallback turns into threecol once the plugin loads; windows already open are placed in **Fill order**
- [ ] `hyprctl plugin unload` puts workspaces back on dwindle without a crash
- [ ] With the plugin loaded and no `examples/binds.lua`, no key does anything new: threecol binds no keys itself

## Selecting the layout

- [ ] `general.layout = "threecol"` applies to every workspace
- [ ] `hl.workspace_rule({ workspace = …, layout = "threecol" })` applies it to that workspace only
- [ ] Workspaces on other layouts behave exactly as before (open, close, resize, move)
- [ ] Rollback: `layout = "lua:threecol"` switches back to the Lua layout after a reload (only while `reference/threecol.lua` is still in the live config)

## Zones and Fill order (#18)

- [ ] The first window lands in the **Master zone**, with empty space on both sides
- [ ] The second window lands in the **Left zone**
- [ ] The third window takes the whole **Stack zone**; further windows join the bottom of the Stack and the **Rows** share the height
- [ ] A new window opens straight at its final size, without a visible jump (`predictSizeForNewTarget`)

## Holes and close rules (#19)

- [ ] Closing Master leaves a **Hole**; no other window moves
- [ ] Closing the last Left window leaves a Left Hole
- [ ] The next new window fills the Hole (Master before Left)
- [ ] A Hole keeps its width; the other zones don't move
- [ ] Closing a Stack Row closes the gap and the remaining Rows share the height
- [ ] After a close, focus goes to the next window along the **Chain** (or the one before, if it was last)
- [ ] A window moved in from another workspace lands by Fill order
- [ ] A floating window that's un-floated lands by Fill order
- [ ] `hyprctl reload` keeps every window in its Zone and Row, and keeps Holes

## Settings (#20)

- [ ] Each `plugin:threecol:*` value set with `hl.config({ plugin = { threecol = { … } } })` takes effect on the next layout pass after a reload: `left_width`, `stack_width`, `min_width`, `min_master_width`, `min_row_height`, `step`, `left_rows`, `persist`
- [ ] `layout_opts` with `left_width`, `stack_width` and `left_rows` overrides the global value on that workspace only
- [ ] A bad `layout_opts` key or value is ignored and that workspace uses the global value
- [ ] An impossible combination is made to fit: Master keeps its minimum, Left and Stack shrink in proportion
- [ ] One notification per config load lists what was ignored or adjusted; a clean config shows none
- [ ] A saved **Divider** position wins over the settings until reset
- [ ] Tightening a limit clamps the sizes in use and leaves the state file unchanged; loosening it brings the old saved sizes back

## Keyboard resize and reset (#21)

- [ ] SUPER+ALT+L / H on Left makes Left wider / narrower by one `step`; Master gives way, the Stack doesn't move
- [ ] Same on the Stack, mirrored
- [ ] On Master, both Dividers move half a step each and Master stays centred
- [ ] With one side at its limit, Master moves only its free side by half a step
- [ ] L/H on any Stack Row resizes the whole Stack; on any Left Row, the whole Left zone
- [ ] SUPER+ALT+K / J makes the focused Row taller / shorter by one step; the other Rows give way in proportion
- [ ] A height key stops when the other Rows are all at `min_row_height`
- [ ] K/J does nothing on Master or on a lone Row
- [ ] A key that would cross a limit stops at the limit (Left and Stack at `min_width`, Master at `min_master_width`)
- [ ] The resize keys repeat when held
- [ ] Each key press is saved (check the state file after one press)
- [ ] A Hole resizes like any zone when its neighbour moves the shared Divider
- [ ] A lone Master window can grow into the empty side zones
- [ ] Resize keys do nothing on floating, fullscreen or maximised windows, and on other layouts
- [ ] SUPER+R puts the workspace's Dividers back to the settings (`layout_opts`, else global) and Row heights back to equal
- [ ] SUPER+R doesn't repeat when held

## Saved Dividers (#22)

- [ ] After a resize, the state file holds the Divider positions, keyed by workspace name, readable by a human
- [ ] An unnamed workspace is keyed by its number
- [ ] After a reboot, each workspace gets its own Dividers back; Row heights are equal again
- [ ] Each workspace keeps its own Dividers, independent of the others
- [ ] Reset removes that workspace's saved entry
- [ ] Renaming a workspace carries its saved sizes over
- [ ] `persist = false`: no reads or writes, the file stays in place, and every reload resets every workspace to its settings
- [ ] Turning `persist` back on restores the old positions
- [ ] A missing or corrupt state file counts as "nothing saved"; threecol still starts
- [ ] Sizes still fit after a change of resolution or reserved space (bar on/off)

## Mouse resize (#23)

- [ ] SUPER+right-drag on Left moves the Left|Master Divider, live on every frame
- [ ] SUPER+right-drag on the Stack moves the Master|Stack Divider
- [ ] SUPER+right-drag on Master moves whichever Divider is nearer the cursor at press
- [ ] The edge picked at press stays the same for the whole drag, even across zones
- [ ] No jump on grab: the edge moves by the cursor's travel since press
- [ ] The edge stops at its limit and follows again when the cursor comes back
- [ ] Vertical drag on a Row moves its nearer top or bottom edge; only the two Rows either side change
- [ ] The top Row has only a bottom edge, the bottom Row only a top edge, a lone Row resizes sideways only
- [ ] One drag can change width and height together
- [ ] Release saves the Dividers (check the state file)
- [ ] Mid-drag, if the grabbed window closes, floats or leaves, or a window joins the zone, the resize ends, keeps its sizes and saves them
- [ ] Floating windows and other layouts keep Hyprland's normal resize
- [ ] Resize on a fullscreen or maximised window does nothing
- [ ] `hyprctl dispatch` `window.resize` on a threecol window moves the same Divider or Row edge by the given amount

## Row heights (#24)

- [ ] A height belongs to the Row: a window moved into a Row takes that Row's height
- [ ] A new Row gets 1/n of the zone; the others shrink keeping their ratios
- [ ] When a Row closes, the others grow in proportion
- [ ] A crowded Stack (Rows can't all meet `min_row_height`) still takes the window and shares height equally; the old proportions come back once the count fits
- [ ] `min_row_height` stays a hard limit for resizing even in a crowded Stack

## Left zone with several Rows (#25)

- [ ] With `left_rows = 2`, the second and third windows fill Left top to bottom, then the Stack
- [ ] Left Rows close gaps, resize and keep limits exactly like Stack Rows
- [ ] Closing a Left Row that isn't the last closes the gap; closing the last Left Row leaves a Hole
- [ ] Lowering `left_rows` moves the extra bottom Left windows to the top of the Stack, in Chain order
- [ ] Raising `left_rows` moves nothing
- [ ] An impossible `left_rows` uses only as many Rows as fit `min_row_height`

## Chain focus and move (#26)

- [ ] SUPER+H/L walks the Chain: Left top → Left bottom → Master → Stack top → Stack bottom
- [ ] The Chain stops at both ends; no wrap
- [ ] Focus skips empty zones (SUPER+L from Left reaches the Stack when Master is a Hole)
- [ ] SUPER+J/K moves inside Left or the Stack, never leaves the zone, and does nothing on Master
- [ ] SUPER+SHIFT+H/L/J/K moves the focused window along the same Chain and zones
- [ ] A move into a Hole or an empty Stack moves the window there; no swap
- [ ] A move into a Left below its Row limit joins as the bottom Row
- [ ] SHIFT+L from Master joins the top of the Stack and leaves a Master Hole; SHIFT+H moves it back
- [ ] Moves swap only with an occupied Master, the bottom Row of a full Left, or a neighbouring Row
- [ ] A moved window takes its new Row's height and its new zone's width, keeps focus, and no Divider moves
- [ ] SUPER+M focuses Master; does nothing on a Hole
- [ ] SUPER+SHIFT+M swaps the focused window with Master (the old Master takes its Row); on a Hole it moves in; does nothing on Master, floating windows or with no focus
- [ ] Focus and move binds don't repeat when held
- [ ] On floating windows, fullscreen workspaces, empty workspaces and other layouts, the same keys fall back to Hyprland's normal focus and move
- [ ] Native `window.move` (bind, `hyprctl`) runs the same move code

## Drag-and-drop (#27)

- [ ] SUPER+left-drag (`window.drag`) drags a tiled window with no arm bind
- [ ] During the drag the window's place stays open and nothing else moves
- [ ] Dropping on another window swaps the two; each takes the other's Row height and zone width
- [ ] Dropping on the bottom quarter of a zone's bottom Row adds a new bottom Row
- [ ] The Stack always accepts an "add" drop; Left only below its Row limit (a full Left swaps instead)
- [ ] Dropping on the "add" area of its own zone moves the window to the bottom
- [ ] Dropping into a Hole or an empty Stack moves the window there; its old place follows the close rules
- [ ] Dropping on its own place, outside the workspace or on a non-tiled window changes nothing
- [ ] A drop in a gap counts as the zone it's in and the nearest Row
- [ ] Focus stays on the dragged window after the drop
- [ ] A plain float toggle (`window.float`) never swaps anything

## Drop indicator (#28)

- [ ] While dragging, one rectangle shows where the window will land: active-border colour (gradient as Hyprland draws it), ~20% fill, the layout's gaps and rounding
- [ ] It sits above the tiles and below the dragged window
- [ ] It follows the cursor live and always matches where the drop lands (swap, add, Hole)
- [ ] No indicator when the drop would change nothing
- [ ] It disappears on release and when the drag ends any other way

## Binds and README (#30)

- [ ] `examples/binds.lua` pasted into a fresh config gives every bind in the spec and no drag-arm bind
- [ ] Every bind listed under "Will my existing binds still work?" behaves as the README says
- [ ] The README's settings table matches the registered `plugin:threecol:*` values and defaults (`hyprctl getoption plugin:threecol:<name>`)
- [ ] The README opens with the GIF from the Showcase below, and it plays on GitHub
- [ ] The README explains the short dwindle fallback at start-up, and it matches what you saw under Install and load
- [ ] The README's release checklist includes adding a `commit_pins` entry and running this checklist

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
