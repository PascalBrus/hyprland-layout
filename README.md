# threecol

A three-zone tiling layout for Hyprland, built for ultrawide monitors. Three columns aren't forced: drop a zone, per workspace, for two columns or just one.

![threecol: windows filling the Left, Master and Stack zones, Holes, mouse and keyboard resize, drag-and-drop, then workspaces with two Left Rows, a mirrored shape, two columns and one column](docs/threecol.gif)

## What it is

threecol splits each workspace into three **Zones**, side by side. By default Left holds one window and Right is the **Stack**:

```
┌──────────┬────────────────────┬──────────┐
│          │                    │ Right 1  │
│   Left   │       Master       ├──────────┤
│          │                    │ Right 2  │
│          │                    ├──────────┤
│          │                    │ Right 3  │
└──────────┴────────────────────┴──────────┘
   ¼ wide          ½ wide          ¼ wide
```

Either side can be the Stack, or both: `left_rows = "unlimited"` with `right_rows = 1` mirrors the layout. A Row limit of `0` drops a side altogether, and Master takes its width: `left_rows = 0` gives two columns, and both sides at `0` gives one, with further windows floating (see [Settings](#settings)).

- **Fill order.** The first window goes to Master, then the `fill_first` side (Left by default) up to its **Row limit**, then the other side up to its own. New windows join a side at its bottom. By default that is Master, one window in Left, and the rest at the bottom of the Stack on the right. A lone window sits in the middle, not stretched across the whole screen.
- **No room.** With both sides limited, a window that finds no room (Master taken and both sides full) floats at its own size, centred. It stays floating when room frees up.
- **Holes.** Close the Master window and its place stays empty; nothing slides in. The next new window fills it.
- **Rows.** Each side holds one window per **Row**. A Stack holds any number; a side with a Row limit (`left_rows`, `right_rows`) holds up to that many. By default Left holds one and Right is the Stack; mirrored, it's the other way round.
- **Resizing.** Dragging with `window.resize` moves the **Dividers** between zones and the edges between Rows. The keyboard `resize` actions do the same in steps, and `reset` puts them back. By default Master stays in the middle (**Centred Master**): every sideways resize moves both Dividers, equal and opposite. Each workspace keeps its own Dividers, saved across reboots.
- **Drag-and-drop.** Drag a window (`window.drag`) onto another to swap them, onto the bottom quarter of a zone to add it there, or into an empty zone to move it. A highlight shows where it will land.
- **Per workspace.** Zone widths, Row limits, `fill_first` and Centred Master can each be set for one workspace with a workspace rule: three columns on the ultrawide, two on a laptop screen, one on a vertical monitor. See [Workspace overrides](#workspace-overrides).
- **Chain focus.** `focus l` / `focus r` walk the windows left to right on screen: Left top → bottom, Master, Right top → bottom, whichever side is the Stack. `focus u` / `focus d` move up and down inside Left or Right. See [Binds](#binds).

The words in **bold** are defined in [`GLOSSARY.md`](GLOSSARY.md).

threecol needs **Hyprland 0.57** with the Lua config.

> [!IMPORTANT]
> In your config, set `general.layout = "threecol"` **before** any file that calls `hl.layout.register`. Otherwise every `hyprctl reload` briefly switches your threecol workspaces to dwindle. See [Selecting the layout](#selecting-the-layout).

## Install with hyprpm

```sh
hyprpm update
hyprpm add https://github.com/PascalBrus/hyprland-layout
hyprpm enable threecol
```

hyprpm builds the plugin (CMake, a C++23 compiler and `pkg-config`; no GoogleTest needed) and pins the threecol commit that matches your Hyprland.

`hyprpm update` also builds Hyprland's headers from source, so it needs Hyprland's own build dependencies at the versions your Hyprland release asks for. An outdated one stops it with a CMake error. For example, 0.57.1 needs `hyprland-protocols` 0.8.0 or newer. Update the package it names and run `hyprpm update` again.

**Load it at start-up.** hyprpm doesn't load plugins on its own. Add this to your `hyprland.lua`:

```lua
hl.on("hyprland.start", function()
    hl.exec_cmd("hyprpm reload")
end)
```

If you use `hl.permission`, also allow hyprpm to load plugins:

```lua
hl.permission("/usr/(bin|local/bin)/hyprpm", "plugin", "allow")
```

**The short dwindle fallback.** Hyprland reads your config before `hyprpm reload` has loaded threecol. Until it is loaded, workspaces that ask for `threecol` use dwindle. This lasts about a second at start-up. Once threecol loads, those workspaces switch to it, and any windows already open are placed in Fill order.

## Manual install

```sh
git clone https://github.com/PascalBrus/hyprland-layout ~/src/threecol
cd ~/src/threecol
cmake -B build -DCMAKE_BUILD_TYPE=Release -DTHREECOL_TESTS=OFF
cmake --build build -j
```

Then load it from your `hyprland.lua` with an **absolute path**:

```lua
hl.plugin.load("/home/you/src/threecol/build/threecol.so")
```

`hl.plugin.load` does **not** expand `~` or `$HOME`. Hyprland passes the path straight to `dlopen`, so `"~/src/…"` fails to load. Build the path in Lua if you don't want to write out your home directory:

```lua
hl.plugin.load(os.getenv("HOME") .. "/src/threecol/build/threecol.so")
```

## Selecting the layout

For every workspace:

```lua
hl.config({ general = { layout = "threecol" } })
```

For single workspaces only, with a workspace rule:

```lua
hl.workspace_rule({ workspace = "3", layout = "threecol" })
```

The name is plain `threecol`, without the `lua:` prefix that Lua layouts use. threecol acts only on workspaces that use it; all others behave as before.

**Set the layout before registering Lua layouts.** On `hyprctl reload`, Hyprland resets every setting, so `general.layout` is back to `dwindle` until your config sets it again. Each `hl.layout.register` call makes Hyprland re-check every workspace's layout on the spot. If one runs before `general.layout = "threecol"`, every threecol workspace switches to dwindle for a moment and then back to a new threecol. threecol carries its Zones, Rows, Holes and Row heights across that switch, but the windows may briefly jump while it happens. Put the `hl.config({ general = { layout = "threecol" } })` line (or the file that holds it) above any file that calls `hl.layout.register`:

```lua
require("core.general")   -- sets general.layout = "threecol"
require("custom.mylayout") -- calls hl.layout.register(...)
```

## Binds

threecol binds no keys. You bind its actions to whatever keys you like, the same way as any Hyprland action:

```lua
hl.bind("<your keys>", hl.dsp.layout("reset"))
```

**Binds you already have keep working.** Rows marked *Hyprland* are Hyprland's own actions, which threecol follows. Rows marked *threecol* are new: add the ones you want.

| Action | What to bind | From |
|---|---|---|
| Move a window | `hl.dsp.window.move({ direction = "left" })`<br>`left` / `right` / `up` / `down` | Hyprland |
| Drag-and-drop a window | `hl.dsp.window.drag()`, as a mouse bind | Hyprland |
| Resize with the mouse | `hl.dsp.window.resize()`, as a mouse bind | Hyprland |
| Swap two windows | `hl.dsp.window.swap({ next = true })` | Hyprland |
| Focus the nearest window on screen | `hl.dsp.focus({ direction = "left" })`<br>`left` / `right` / `up` / `down` | Hyprland |
| Focus along the **Chain**<br>(Left → Master → Right) | `hl.dsp.layout("focus l")`<br>`l` / `r` / `u` / `d` | threecol |
| Focus Master | `hl.dsp.layout("focusmaster")` | threecol |
| Swap with Master | `hl.dsp.layout("swapwithmaster")` | threecol |
| Make the focused window wider / narrower | `hl.dsp.layout("resize wider")`<br>`wider` / `narrower` | threecol |
| Make the focused Row taller / shorter | `hl.dsp.layout("resize taller")`<br>`taller` / `shorter` | threecol |
| Reset Dividers and Row heights | `hl.dsp.layout("reset")` | threecol |

- **Resize keys** feel best with `{ repeating = true }`, so holding the key keeps resizing.
- **Focus: Chain or screen position?** Hyprland's focus picks the nearest window on screen, so around Holes and between Rows it can land somewhere unexpected. Chain focus walks the windows in order. It only acts on tiled threecol windows; [`examples/binds.lua`](examples/binds.lua) shows how to fall back to Hyprland's focus everywhere else.
- **Master actions** use the same names as Hyprland's master layout, so existing binds for them work on both.
- **From a script:** anything in *What to bind* also works with `hyprctl dispatch`, e.g. `hyprctl dispatch "hl.dsp.layout('reset')"`.
- **No effect:** a threecol action that doesn't apply (e.g. `resize taller` on Master) does nothing. So do actions for other layouts, such as dwindle's `togglesplit` or master's `addmaster` and `mfact`.

**A ready-made set.** [`examples/binds.lua`](examples/binds.lua) binds every action above except the mouse ones, if you'd rather start from that. Pick your keys in its `keys` list at the top, or set one to `false` to skip it. It replaces whatever is on those keys, so read its header first.

## Settings

Set them like any plugin setting. Sizes are fractions of the workspace area (0 to 1); Master gets whatever Left and Right leave.

### Full example

Every global setting, at its default, plus workspace rules that override the per-workspace ones (see [Workspace overrides](#workspace-overrides)). Copy it and delete what you don't change:

```lua
hl.config({
    general = { layout = "threecol" },
    plugin = {
        threecol = {
            -- Zone widths (Master gets the rest)
            left_width       = 0.25,
            right_width      = 0.25,

            -- Row limits: a number, "unlimited" (a Stack) or 0 (no zone)
            left_rows        = 1,
            right_rows       = "unlimited",

            -- Side new windows go to after Master
            fill_first       = "left",

            -- Keep Master in the middle; resizing moves both Dividers
            centre_master    = true,

            -- Limits (global only)
            min_width        = 0.10,
            min_master_width = 0.20,
            min_row_height   = 0.10,

            -- Keyboard resize step (global only)
            step             = 0.05,

            -- Save Divider positions across reloads and reboots (global only)
            persist          = true,
        },
    },
})

-- Workspace 2: mirrored, the Stack on the left
hl.workspace_rule({
    workspace   = "2",
    layout      = "threecol",
    layout_opts = { left_rows = "unlimited", right_rows = 1, fill_first = "right" },
})

-- Workspace 3: every per-workspace setting at once, two columns
hl.workspace_rule({
    workspace   = "3",
    layout      = "threecol",
    layout_opts = {
        left_width    = 0.30,
        right_width   = 0.20, -- ignored: right_rows = 0 removes the Right zone
        left_rows     = 2,
        right_rows    = 0,
        fill_first    = "left",
        centre_master = false, -- needed: Centred Master needs both sides
    },
})
```

> **`stack_width` is now `right_width`.** If your config or a `layout_opts` sets `stack_width`, rename it to `right_width` once. The old name is no longer read: globally Hyprland reports it as an unknown setting, and in `layout_opts` threecol warns about an unknown key. Saved Divider positions carry over unchanged.

### All settings

| Setting | Type | Default | Per workspace | With Centred Master | Meaning |
|---|---|---|---|---|---|
| `left_width` | float | 0.25 | yes | yes, must equal `right_width` ([how](#centred-master)) | Left zone width |
| `right_width` | float | 0.25 | yes | yes, must equal `left_width` ([how](#centred-master)) | Right zone width (was `stack_width`) |
| `left_rows` | int or `"unlimited"` | 1 | yes | yes, but not `0` | **Row limit**: most windows the Left zone holds, `0` for no Left zone, or `"unlimited"` for a Stack |
| `right_rows` | int or `"unlimited"` | `"unlimited"` | yes | yes, but not `0` | **Row limit** of the Right zone, the same way |
| `fill_first` | `"left"` or `"right"` | `"left"` | yes | yes | The side new windows go to after Master, before the other side |
| `centre_master` | bool | true | yes | – | **Centred Master**: keep Master in the middle, resizing both sides together (see [below](#centred-master)) |
| `min_width` | float | 0.10 | no | yes, acts differently ([how](#centred-master)) | Smallest Left / Right width |
| `min_master_width` | float | 0.20 | no | yes, acts differently ([how](#centred-master)) | Smallest Master width |
| `min_row_height` | float | 0.10 | no | yes | Smallest Row height (Left and Right); also caps both Row limits |
| `step` | float | 0.05 | no | yes, acts differently ([how](#centred-master)) | Keyboard resize step |
| `persist` | bool | true | no | yes | Save Divider positions across reloads and reboots |

### Centred Master

`centre_master` (bool, default `true`, per workspace) keeps Master in the middle of the screen, with Left and Right equally wide: every sideways resize moves both Dividers by the same amount in opposite directions. Turned off, a resize moves one Divider. Up and down resizes (Row edges) are the same either way.

**Not supported: a missing side.** Centred Master needs both sides. With `left_rows = 0` or `right_rows = 0` and `centre_master` on (the default), the settings notice warns, for example `workspace 3: centre_master needs both side zones, but right_rows is 0; using centre_master = false`, and that workspace runs with it off. Set `centre_master = false` for two- or one-column workspaces to silence it.

What changes when Centred Master is on. Settings not listed (`fill_first`, `min_row_height`, Row limits other than `0`, Row edge resizes) work the same either way. The examples use a 1000 px screen at 250 | 500 | 250:

| Setting or action | Centred Master on | Centred Master off |
|---|---|---|
| Mouse resize (`window.resize`) | The grabbed Divider follows the cursor and the other mirrors it, whichever window or half of Master is grabbed. Master's left half dragged 100 px left: 150 \| 700 \| 150. A Left window dragged 100 px right: 350 \| 300 \| 350 | Only the grabbed Divider moves (on Master, the one nearer the grabbed half). Master's left half dragged 100 px left: 150 \| 600 \| 250 |
| Keyboard resize and `step` (`resize wider` / `narrower`) | On Left or Right, both sides change by `step`, so Master changes by twice that: `resize wider` on Left gives 300 \| 400 \| 300. On Master, each Divider moves half a step: 225 \| 550 \| 225 | On Left or Right, only that side's Divider moves: 300 \| 450 \| 250. On Master, each Divider moves half a step too, as Master has no side to grow towards |
| `left_width`, `right_width` | Must be equal. If not, the settings notice warns (`centre_master needs equal side widths, but left_width is 0.2 and right_width is 0.3; using 0.25 each`), Master keeps its width, and each side gets half of the rest: `0.20` / `0.30` gives 0.25 \| 0.50 \| 0.25 | Used as set: 0.20 \| 0.50 \| 0.30 |
| `min_width`, `min_master_width` | Both Dividers stop together when either side reaches `min_width` or Master reaches `min_master_width`. Settings that can't all fit still shrink both sides in proportion, so they stay equal | Each Divider stops on its own. Widening Master with one side at `min_width` still moves the other side |
| `persist` | Both Dividers are saved; they are always equal | Both Dividers are saved as they are |
| Reset (`reset`) | Back to the settings widths, centred by the `left_width` / `right_width` rule above (no new warning) | Back to the settings widths |

**Turning Centred Master on** centres what you had: unequal widths kept from resizing, or saved by `persist`, become equal, with Master keeping its width. The centred widths replace the kept and saved ones straight away, so turning it off again doesn't bring the old ones back. This happens on a config reload and at runtime with `hl.config`. A workspace that isn't open is centred, and its saved entry rewritten, the first time threecol lays it out.

## Workspace overrides

Every workspace can have its own shape. The six per-workspace settings, `left_width`, `right_width`, `left_rows`, `right_rows`, `fill_first` and `centre_master`, can be overridden with a workspace rule's `layout_opts`, using the same names and values as the global settings (`centre_master` as `true` or `false`, or `1` / `0`). The rest (`min_width`, `min_master_width`, `min_row_height`, `step`, `persist`) are global only.

```lua
-- Workspace 1: the defaults, three columns on the ultrawide (no rule needed)

-- Workspace 2: mirrored, the Stack on the left
hl.workspace_rule({
    workspace   = "2",
    layout      = "threecol",
    layout_opts = { left_rows = "unlimited", right_rows = 1, fill_first = "right" },
})

-- Workspace 3: wider Left with two Rows, narrower Right
hl.workspace_rule({
    workspace   = "3",
    layout      = "threecol",
    layout_opts = { left_width = 0.30, right_width = 0.20, left_rows = 2, centre_master = false },
})

-- Workspace 4: two columns, Master and a Right Stack
hl.workspace_rule({
    workspace   = "4",
    layout      = "threecol",
    layout_opts = { left_rows = 0, centre_master = false },
})

-- Workspace 5: one column, every further window floats
hl.workspace_rule({
    workspace   = "5",
    layout      = "threecol",
    layout_opts = { left_rows = 0, right_rows = 0, centre_master = false },
})
```

- **Unequal widths and missing sides need `centre_master = false`.** Otherwise threecol warns and centres the widths, or turns Centred Master off for that workspace (see [Centred Master](#centred-master)).
- **Which value wins.** A Divider you moved by hand wins until you `reset`. Then the `layout_opts` value, then the global setting.
- **Mistakes.** An unknown key or a bad value is ignored with a warning naming the workspace, and that workspace uses the global value.
- **Changes apply on reload**, the same as global settings: a lower Row limit pushes extra windows to the other side (see [Changing settings](#setting-details)).
- **Each workspace keeps its own Dividers**, and saves them under its name with `persist`.

## Settings in pictures

### Settings at a glance

Every setting the GIF at the top doesn't show, one at a time:

![threecol settings: zone widths, the min_width, min_master_width and min_row_height limits, step, persist and reset, per-workspace widths, fill_first, a window floating when both sides are full, Two Stacks and no Right zone](docs/threecol-settings.gif)

Every setting, drawn on the screen it shapes:

```
  ◄── left_width ──► ◄────── Master: the rest ──────► ◄─ right_width ──►
 ┌──────────────────┬────────────────────────────────┬──────────────────┐
 │                  │                                │ Right 1          │
 │  Left 1          │                                ├──────────────────┤ ▲
 │                  │                                │ Right 2          │ │ each Row is at least
 ├──────────────────┤             Master             ├──────────────────┤ ▼ min_row_height tall
 │                  │                                │ Right 3          │
 │  Left 2          │                                ├──────────────────┤
 │                  │                                │ Right 4 …        │
 └──────────────────┴────────────────────────────────┴──────────────────┘
   left_rows = 2      at least                         right_rows = "unlimited"
   at least           min_master_width wide            at least
   min_width wide                                      min_width wide
                    ▲                                ▲
                    └── Dividers: mouse resize ──────┘
                       or keyboard resize by `step`; saved if `persist`;
                       with `centre_master`, they move together
```

`fill_first` doesn't change the shape, only which side new windows go to after Master. The numbers show the order windows arrive in:

```
 fill_first = "left" (default)          fill_first = "right"
 left_rows = 2, right_rows = 2          left_rows = 2, right_rows = 2
┌──────┬────────────┬──────┐           ┌──────┬────────────┬──────┐
│  2   │            │  4   │           │  4   │            │  2   │
├──────┤     1      ├──────┤           ├──────┤     1      ├──────┤
│  3   │            │  5   │           │  5   │            │  3   │
└──────┴────────────┴──────┘           └──────┴────────────┴──────┘
```

### Layout gallery

`left_rows` and `right_rows` decide what each side is: a number is a side with a Row limit, `"unlimited"` is a Stack, and `0` removes the side. Together with the widths, they give all of these:

**Default** · `left_rows = 1`, `right_rows = "unlimited"`

```
┌──────────┬────────────────────┬──────────┐
│          │                    │ Right 1  │
│   Left   │       Master       ├──────────┤
│          │                    │ Right 2  │
│          │                    ├──────────┤
│          │                    │ Right 3  │
└──────────┴────────────────────┴──────────┘
```

**Mirrored** · `left_rows = "unlimited"`, `right_rows = 1`, `fill_first = "right"`

```
┌──────────┬────────────────────┬──────────┐
│ Left 1   │                    │          │
├──────────┤       Master       │  Right   │
│ Left 2   │                    │          │
├──────────┤                    │          │
│ Left 3   │                    │          │
└──────────┴────────────────────┴──────────┘
```

**Two Stacks** · `left_rows = "unlimited"`, `right_rows = "unlimited"`

```
┌──────────┬────────────────────┬──────────┐
│ Left 1   │                    │ Right 1  │
├──────────┤       Master       ├──────────┤
│ Left 2   │                    │ Right 2  │
├──────────┤                    │          │
│ Left 3   │                    │          │
└──────────┴────────────────────┴──────────┘
```

With `fill_first = "left"`, the Left Stack takes every window after Master; move or drag windows to the Right Stack yourself.

**No Left zone** · `left_rows = 0`, `centre_master = false`

```
┌──────────────────────────────┬──────────┐
│                              │ Right 1  │
│            Master            ├──────────┤
│                              │ Right 2  │
│                              ├──────────┤
│                              │ Right 3  │
└──────────────────────────────┴──────────┘
```

**No Right zone** · `left_rows = "unlimited"`, `right_rows = 0`, `centre_master = false`

```
┌──────────┬──────────────────────────────┐
│ Left 1   │                              │
├──────────┤            Master            │
│ Left 2   │                              │
├──────────┤                              │
│ Left 3   │                              │
└──────────┴──────────────────────────────┘
```

**Master only** · `left_rows = 0`, `right_rows = 0`, `centre_master = false`

```
┌─────────────────────────────────────────┐
│                                         │
│                 Master                  │
│              ┌──────────┐               │
│              │ floating │               │
│              └──────────┘               │
│                                         │
└─────────────────────────────────────────┘
```

One tiled window fills the workspace; every other window floats.

**Both sides limited** · `left_rows = 1`, `right_rows = 2`

```
┌──────────┬────────────────────┬──────────┐
│          │                    │ Right 1  │
│   Left   │    ┌──────────┐    │          │
│          │    │ window 5 │    ├──────────┤
│          │    │ floats   │    │ Right 2  │
│          │    └──────────┘    │          │
└──────────┴────────────────────┴──────────┘
```

Four places; the fifth window and any later one float, centred, at their own size.

**Different widths** · `left_width = 0.15`, `right_width = 0.35`, `centre_master = false`

```
┌──────┬─────────────────────┬──────────────┐
│      │                     │ Right 1      │
│ Left │       Master        ├──────────────┤
│      │                     │ Right 2      │
│      │                     ├──────────────┤
│      │                     │ Right 3      │
└──────┴─────────────────────┴──────────────┘
```

Each of these can be set for one workspace only with `layout_opts` (see [Workspace overrides](#workspace-overrides)), so a workspace on a second monitor can have, say, no Left zone while the ultrawide keeps all three.

## Setting details

**Mirrored.** Make Left the Stack, give Right a Row limit, and fill Right first:

```lua
hl.config({
    plugin = {
        threecol = {
            left_rows  = "unlimited",
            right_rows = 1,
            fill_first = "right",
        },
    },
})
```

New windows then go to Master, one to Right, and the rest to the bottom of the Left Stack. Moves, drops, close rules and the Chain are the mirror image of the default. Both sides may be `"unlimited"` (two Stacks).

**`fill_first` and a Stack.** A Stack has no Row limit, so if the `fill_first` side is a Stack it takes every new window after Master. The other side then fills only when you move or drag a window there. That happens with `fill_first = "right"` on the default layout, or with the mirrored layout above if you leave out `fill_first`.

**No Left zone.** A Row limit of `0` makes a side a **Missing zone**: it has no width, no Divider and no place in the Chain, and Master reaches the screen edge:

```lua
hl.config({
    plugin = {
        threecol = {
            left_rows     = 0,
            centre_master = false, -- it needs both sides
        },
    },
})
```

```
┌──────────────────────────────┬──────────┐
│                              │ Right 1  │
│            Master            ├──────────┤
│                              │ Right 2  │
└──────────────────────────────┴──────────┘
              ¾ wide              ¼ wide
```

New windows go to Master, then the Right Stack. Centred Master needs both sides, hence `centre_master = false` (left on, it warns and is turned off for you). Resizing Master moves only the Master|Right Divider, `move l` from Master does nothing, and a window dropped where Left would be lands on Master. The missing side's width setting is ignored: it isn't raised to `min_width`, counted against `min_master_width` or warned about. Its saved Divider is kept, so if the side comes back (by a reload or a `layout_opts` change) it returns where you left it. With both sides at `0`, Master fills the workspace, resizing does nothing, and every other window floats.

**Both sides limited.** Neither side has to be a Stack. Give both a Row limit and windows that find no room float instead:

```lua
hl.config({
    plugin = {
        threecol = {
            left_rows  = 1,
            right_rows = 2,
        },
    },
})
```

Here the fifth window floats at its own size, centred, as does any later one while all four places are taken. A floated window stays floating when a place frees up; toggle it back to tiled with `window.float` to place it.

**Mistakes are forgiven.**

- An unknown or bad `layout_opts` key or value is ignored; that workspace uses the global value.
- A global value out of range (a size outside 0 to 1, or a negative Row limit) is ignored; the default is used.
- A global value of the wrong type, such as text where a number belongs (`min_width = "abc"`), is dropped by Hyprland before threecol sees it. Neither Hyprland nor threecol warns about it; the default is used. `hyprctl getoption plugin:threecol:<name>` shows `set: false` for it.
- Settings that contradict Centred Master (a missing side, or unequal side widths) are resolved as [Centred Master](#centred-master) says, with a warning.
- Settings that can't all fit are made to fit: Master keeps its minimum, and Left and Right shrink in proportion. A side with a Row limit uses only as many Rows as fit `min_row_height`.
- Each config load shows one notification listing what was ignored or adjusted.

**Changing settings.** Changes to `plugin:threecol:*` apply as soon as the config reloads.

- Lowering a Row limit, setting it to `0`, or switching a side from `"unlimited"` to a number, pushes that side's extra bottom windows out to the other side, at its end nearest Master (the top of Right, the bottom of Left), keeping their order. A Stack takes them all; a side with a Row limit takes as many as it has free Rows, first in Chain order; the rest float. Raising a limit moves nothing.
- Tighter limits clamp the saved Dividers in use but don't rewrite the saved file, so loosening them again brings the old sizes back.

**Saved Dividers.** With `persist = true`, each workspace's Dividers are saved after every finished resize to `$XDG_STATE_HOME/threecol/dividers` (or `~/.local/state/threecol/dividers`), keyed by workspace name, and come back after a reboot. Row heights are not saved; they go back to equal after a reboot. `reset` always goes back to the workspace's `layout_opts`, or the global settings, and removes its saved entry.

With `persist = false` the file is hidden, renamed to `dividers.off` (replacing an older one), so nothing is lost; it is neither read nor written while off. Turning `persist` back on renames it to `dividers` again, unless a `dividers` file is already there, which then wins and `dividers.off` is left alone. To bring it back by hand, rename `dividers.off` to `dividers`. How the change shows depends on where it's made:

- In the config file, applied on reload: turning it off resets every workspace to its settings straight away; turning it on loads the saved Dividers.
- At runtime with `hl.config`: nothing moves. Turned off, Dividers you resize still last for the session (closing and reopening windows keeps them), but after a reboot or reload every workspace starts from its settings. Turned on, what is on screen is saved, along with the saved entries for workspaces that aren't open.

A state file threecol can't fully read (from a newer threecol, or hand-edited into something it doesn't understand) counts as nothing saved, or loses just the lines it can't read. The next save moves it aside to `dividers.bak` first (replacing an older `dividers.bak`), so nothing in it is lost; Windows (CRLF) line endings are fine. The file is replaced in one step, so a crash never leaves half a file, but it isn't forced to disk: after a power cut some filesystems may lose it, and threecol then starts with nothing saved. Two Hyprland sessions with the same `XDG_STATE_HOME` (or `HOME`), such as a nested one, share the file: the last one to save wins, and one starting with `persist = false` hides it from the other.

## After a Hyprland update

A plugin must be built for the exact Hyprland it runs in. After a Hyprland update, threecol refuses to start and shows a notification that stays until dismissed:

> [threecol] built for Hyprland abc1234, running def5678; run \`hyprpm update\` (or rebuild threecol), then reload. Using dwindle until then.

Your threecol workspaces use dwindle until you rebuild. Nothing crashes, and your saved Dividers are kept.

**With hyprpm:**

```sh
hyprpm update
hyprpm reload
```

`hyprpm update` picks the threecol commit pinned for your Hyprland. If there is no pin for it yet, it builds the latest commit, which may not compile; see the next section.

**Manual install:** pull, rebuild (`cmake --build build -j`), then restart Hyprland. A config reload doesn't retry a plugin path that is already in the config, so log out and back in, or load it once by hand:

```sh
hyprctl plugin load /home/you/src/threecol/build/threecol.so
```

## Building from source

```sh
cmake -B build
cmake --build build -j
ctest --test-dir build
```

The unit tests need GoogleTest. Turn them off with `-DTHREECOL_TESTS=OFF`.

To try a build on your live session, point `hl.plugin.load` at `build/threecol.so` (see [Manual install](#manual-install)).

The code has two parts:

- `src/core/`: all layout behaviour, in plain C++ with no Hyprland headers. The unit tests in `tests/core/` drive it. A test fails if a Hyprland header sneaks in.
- `src/plugin/`: a thin layer that turns Hyprland callbacks into core calls and core output into Hyprland actions.

### Release checklist

For a new Hyprland release:

1. Update Hyprland and restart it.
2. Fix any build errors against the new headers, then run `cmake --build build -j && ctest --test-dir build`.
3. Load the build on the live session and check that it works.
4. If the plugin's version changed, bump it in both `CMakeLists.txt` (`project(… VERSION …)`) and `src/plugin/main.cpp` (`PLUGIN_INIT`'s return value).
5. Commit and push.
6. Add a `commit_pins` entry to `hyprpm.toml`: `["<Hyprland commit>", "<threecol commit>"]`, with a comment naming the Hyprland version. The Hyprland commit is the full hash from `hyprctl version`. The threecol commit is the one from step 5. A commit can't pin itself, so the pin is a commit of its own.
7. Push again, then check that `hyprpm update` on a clean machine builds and loads it.

## License

BSD-3-Clause. See [`LICENSE`](LICENSE).
