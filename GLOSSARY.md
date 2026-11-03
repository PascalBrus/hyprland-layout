# threecol

A custom Hyprland tiling layout that splits a workspace into three side-by-side zones, built for an ultrawide monitor.

## Language

**Zone**:
One of the three fixed areas of a workspace: Left, Master and Stack, from left to right.
_Avoid_: column, area, region

**Master zone**:
The middle zone, half the screen wide by default, holding exactly one window. The first window on a workspace lands here.
_Avoid_: middle column, main, center

**Left zone**:
The zone on the left, a quarter of the screen by default, holding up to the Row limit of windows stacked top to bottom (one by default). Closing a window here closes the gap; the others share the height, as in the Stack zone.
_Avoid_: left column, side

**Stack zone**:
The zone on the right, a quarter of the screen by default, holding all other windows stacked top to bottom. Closing a window here closes the gap; the others share the height.
_Avoid_: right column, right stack, slave area

**Hole**:
An empty Master zone, or a Left zone whose last window closed. Other windows never slide into a hole; the next new window fills it.
_Avoid_: gap, empty slot

**Fill order**:
The order in which new windows take zones: Master, then the bottom of Left until it reaches its Row limit, then the bottom of the Stack.
_Avoid_: opening order, spawn order

**Divider**:
One of the two vertical boundaries between zones on a workspace: Left|Master and Master|Stack. Resizing any zone means moving one or both dividers; each workspace keeps its own.
_Avoid_: edge, border, split

**Chain**:
The order H/L focus and moves walk through a workspace's windows: Left top to bottom, then Master, then Stack top to bottom. It has two ends and doesn't wrap; empty zones count as one step.
_Avoid_: cycle, focus order, walk

**Row**:
One window's slot in the Left or Stack zone, counted from the top. A height belongs to the Row, not the window: a window moved into a Row takes that Row's height.
_Avoid_: stack position, slot, cell

**Row limit**:
The most Rows the Left zone may hold, one by default. Lowering it moves the extra Left windows to the top of the Stack; raising it moves nothing.
_Avoid_: cap, max rows, left count
