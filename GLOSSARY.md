# threecol

A custom Hyprland tiling layout that splits a workspace into up to three side-by-side zones, built for an ultrawide monitor.

## Language

**Zone**:
One of the fixed areas of a workspace: Left, Master and Right, from left to right. Left and Right are the side zones; either can be missing.
_Avoid_: column, area, region

**Master zone**:
The middle zone, holding exactly one window. The first window on a workspace lands here. It is half the screen wide by default and also takes the width of a missing side zone.
_Avoid_: middle column, main, center

**Left zone**:
The side zone on the left, a quarter of the screen by default. Bounded, the Stack or missing.
_Avoid_: left column, side

**Right zone**:
The side zone on the right, a quarter of the screen by default. Bounded, the Stack or missing.
_Avoid_: right column, stack zone, slave area

**Stack**:
The role of a side zone with no Row limit: it holds any number of windows stacked top to bottom, and closing one closes the gap. Either side zone, both or neither may be a Stack.
_Avoid_: stack zone, infinite zone, overflow zone

**Bounded zone**:
A side zone with a Row limit. Closing a window here closes the gap, and the others share the height. When its last window closes it leaves a Hole.
_Avoid_: limited zone, fixed zone

**Missing zone**:
A side zone with a Row limit of zero. It has no width, no Divider and no place in the Chain.
_Avoid_: disabled zone, hidden zone, empty zone

**Hole**:
An empty Master zone, or a Bounded zone whose last window closed. Other windows never slide into a hole; the next new window fills it.
_Avoid_: gap, empty slot

**Centred Master**:
A workspace whose Master zone stays in the middle of the screen, with Left and Right equally wide. Resizing sideways moves both Dividers by the same amount in opposite directions, whichever window or half of Master is grabbed, and both stop together at a limit. It needs both side zones; with one missing it is a settings conflict, not a fallback. On by default; turned off, a resize moves one Divider.
_Avoid_: forced centre, mirrored resize, symmetric resize, center mode

**Fill order**:
The order in which new windows take zones: Master, then the first side zone up to its Row limit, then the other side zone up to its own. Which side comes first is a setting. A missing zone is skipped. A window that finds no room floats.
_Avoid_: opening order, spawn order

**Divider**:
A vertical boundary between Master and a side zone that isn't missing: Left|Master and Master|Right. Resizing any zone means moving one or both dividers; each workspace keeps its own.
_Avoid_: edge, border, split

**Chain**:
The order H/L focus and moves walk through a workspace's windows: Left top to bottom, then Master, then Right top to bottom, as they sit on screen. It has two ends, and focus, swaps and moves go from one end back to the other, as Hyprland's own dispatchers loop. Up and down loop the same way inside a side zone. Looping can be turned off, and a bind can ask not to loop. Nothing leaves the Chain for another monitor. For a move, an empty zone counts as one step; focus skips it. A missing zone isn't in it.
_Avoid_: cycle, focus order, walk

**Cycle**:
The order `cycle_next` walks through a workspace's windows: the Chain, then the floating windows in Hyprland's order, then back to the start of the Chain. Only `cycle_next` walks it; every other next and previous dispatcher stays on the Chain.
_Avoid_: loop, window order, focus order

**Row**:
One window's slot in a side zone, counted from the top. A height belongs to the Row, not the window: a window moved into a Row takes that Row's height.
_Avoid_: stack position, slot, cell

**Row limit**:
The most Rows a side zone may hold: a whole number, zero for a missing zone, or none for the Stack. Lowering it pushes the zone's bottom windows out: into a Stack on the other side, else into the other side's free Rows, else they float. They join that side at its end nearest Master.
_Avoid_: cap, max rows, left count

**Kept sizes**:
The Divider positions and Row heights set by resizing. They win over the workspace's settings until reset, and a setting change that squeezes them makes the squeezed sizes the new kept sizes. With persist on, the Divider positions also survive a reboot; Row heights don't.
_Avoid_: saved sizes, state sizes, overrides

**Drag resize**:
A resize with the mouse. The Divider or Row edge nearest the grabbed point follows the cursor, and the new sizes are kept when the button is released.
_Avoid_: mouse resize

**One-off resize**:
A resize by a given amount in pixels, from a bind or command that names the amount, or from a trackpad gesture. The amount moves the window's Divider and Row edge as a drag by that much would: a positive x to the right, a positive y down. So a positive x makes a Left window wider and a Right window narrower, as in Hyprland's master layout. The new sizes are kept straight away.
_Avoid_: resizeactive, gesture resize

**Step resize**:
A resize by one step, a fixed share of the screen set in the settings, from the resize keys or the resize layout message. It makes the focused zone wider or narrower, or the focused Row taller or shorter. The new sizes are kept straight away. A floating window grows or shrinks by one step on both sides, so its centre stays put.
_Avoid_: keyboard resize, resize wider

**Hyprland dispatcher**:
One of Hyprland's own bind targets, such as focus, move or swap by direction. On a window threecol tiles, threecol gives some of them its own meaning. Where Hyprland has a behaviour of its own for one, a switch can give it back; where Hyprland leaves it to the layout, there is nothing to give back and it is always threecol's.
_Avoid_: stock bind, Hyprland action, default bind

**threecol dispatcher**:
A bind target threecol adds through the layout message, such as Chain focus, Step resize and reset. It always behaves the threecol way and has no switch.
_Avoid_: threecol action, custom bind, layout command

**Settings app**:
The graphical program, `threecol-settings`, for setting every threecol setting, workspace override and bind without reading the README. Each entry shows what it does, as a picture and in words, and changes take effect when saved.
_Avoid_: GUI, visual manager, configurator, overlay

**Settings file**:
The one config file the Settings app writes and owns, loaded after the user's own config so its values win. The user's hand-written files are never changed by the Settings app.
_Avoid_: generated file, generated config, managed file
