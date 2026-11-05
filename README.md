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
- **Resizing.** SUPER+right-drag moves the **Dividers** between zones and the edges between Rows. SUPER+ALT+HJKL does the same in steps, and SUPER+R resets. Each workspace keeps its own Dividers, saved across reboots.
- **Drag-and-drop.** SUPER+left-drag a window onto another to swap them, onto the bottom quarter of a zone to add it there, or into an empty zone to move it. A highlight shows where it will land.
- **Chain focus.** SUPER+H/L walk the windows left to right on screen: Left top → bottom, Master, Right top → bottom, whichever side is the Stack. SUPER+J/K move up and down inside Left or Right.

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

threecol binds no keys. Copy [`examples/binds.lua`](examples/binds.lua) into your config, after your other binds, and change what you like. It sets these defaults:

| Keys | Action | Repeats |
|---|---|---|
| SUPER+H / L | Focus along the **Chain** (Left → Master → Right) | no |
| SUPER+J / K | Focus down / up inside Left or Right | no |
| SUPER+SHIFT+H / L / J / K | Move the window the same way; it moves into free room and swaps only when there is none | no |
| SUPER+M | Focus the Master window | no |
| SUPER+SHIFT+M | Swap the focused window with Master (or move it into a Master Hole) | no |
| SUPER+left-drag | Drag-and-drop | – |
| SUPER+right-drag | Resize: move the nearest Divider or Row edge | – |
| SUPER+ALT+L / H | Make the focused zone wider / narrower by one `step` | yes |
| SUPER+ALT+K / J | Make the focused Row taller / shorter by one `step` | yes |
| SUPER+R | Reset the workspace's Dividers and Row heights to the settings, even with a floating, fullscreen or maximised window | no |

**The snippet replaces your binds on these keys.** Hyprland runs every bind on a key, not just the last one, so the snippet clears each key with `hl.unbind` before binding it. In Hyprland's stock config that removes SUPER+M (exit), SUPER+R (launcher), SUPER+J (dwindle `togglesplit`) and the SUPER+drag binds (re-added unchanged). SUPER+ALT+L is a common lock-screen bind. Move anything you want to keep to other keys, or change the keys in the snippet.

**The same keys work everywhere.** The HJKL binds check whether the focused window is a tiled threecol window. On a floating window, an empty workspace, a workspace with a fullscreen window or another layout, they fall back to Hyprland's normal `focus` and `window.move`. SUPER+M and SUPER+SHIFT+M also work on the master layout, which uses the same message names; the resize and reset keys do nothing on other layouts.

Drag-and-drop needs no extra bind: Hyprland's own `window.drag` is enough.

Every bind sends a layout message. You can send the same ones from a script:

```sh
hyprctl dispatch "hl.dsp.layout('focus l')"   # focus l|r|u|d
hyprctl dispatch "hl.dsp.layout('move r')"    # move l|r|u|d
hyprctl dispatch "hl.dsp.layout('focusmaster')"
hyprctl dispatch "hl.dsp.layout('swapwithmaster')"
hyprctl dispatch "hl.dsp.layout('resize wider')"   # wider|narrower|taller|shorter
hyprctl dispatch "hl.dsp.layout('reset')"
```

A message that doesn't apply (e.g. `resize taller` on Master, or anything but `focusmaster` and `reset` on a floating window) does nothing and reports no error. `focusmaster` from a floating window jumps to Master.

## Will my existing binds still work?

Mostly yes. Hyprland's own dispatchers fall into three groups on a threecol workspace.

**They follow threecol's rules**, the same as the threecol binds:

| Dispatcher | What it does in threecol |
|---|---|
| `window.move({ direction = … })` | The same move as `move l\|r\|u\|d`: along the Chain, swapping only when there is no room |
| `window.resize()` (mouse bind) | Moves the nearest Divider or Row edge |
| `window.resize({ x, y, relative = true })` | Moves the focused window's Divider or Row edge by that amount |
| `window.drag()` (mouse bind) | Native drag-and-drop |
| `window.swap({ with = … })`, `window.swap({ next = true })` | Swaps the two windows; each takes the other's Zone and Row |
| `layout("focusmaster")`, `layout("swapwithmaster")` | Same names as the master layout, so existing binds keep working |
| `window.float`, `window.fullscreen`, `window.close`, moving to another workspace | Hyprland's own. A window that comes back tiled (or arrives from another workspace) is placed in Fill order, or floats again if there's no room |

**They are geometric.** Hyprland picks the target by screen position, not by the Chain. They work, but can land somewhere other than the threecol bind would, e.g. around Holes or between Rows:

| Dispatcher | Picks |
|---|---|
| `focus({ direction = … })` | The nearest window in that direction on screen |
| `window.swap({ direction = … })` | The nearest window in that direction, then swaps with it |
| `window.cycle_next()` | The next window in Hyprland's cycle order |

**They do nothing.** Layout messages written for other layouts, such as dwindle's `togglesplit`, `swapsplit` and `preselect`, or master's `addmaster`, `removemaster`, `orientation…` and `mfact`. threecol ignores any message it doesn't know.

## Settings

Set them like any plugin setting:

```lua
hl.config({
    plugin = {
        threecol = {
            left_width  = 0.25,
            right_width = 0.25,
            left_rows   = 2,
        },
    },
})
```

Sizes are fractions of the workspace area (0 to 1). Master gets whatever Left and Right leave.

> **`stack_width` is now `right_width`.** If your config or a `layout_opts` sets `stack_width`, rename it to `right_width` once. The old name is no longer read: globally Hyprland reports it as an unknown setting, and in `layout_opts` threecol warns about an unknown key. Saved Divider positions carry over unchanged.

| Setting | Type | Default | Per workspace | Meaning |
|---|---|---|---|---|
| `left_width` | float | 0.25 | yes | Left zone width |
| `right_width` | float | 0.25 | yes | Right zone width (was `stack_width`) |
| `left_rows` | int or `"unlimited"` | 1 | yes | **Row limit**: most windows the Left zone holds, `0` for no Left zone, or `"unlimited"` for a Stack |
| `right_rows` | int or `"unlimited"` | `"unlimited"` | yes | **Row limit** of the Right zone, the same way |
| `fill_first` | `"left"` or `"right"` | `"left"` | yes | The side new windows go to after Master, before the other side |
| `min_width` | float | 0.10 | no | Smallest Left / Right width |
| `min_master_width` | float | 0.20 | no | Smallest Master width |
| `min_row_height` | float | 0.10 | no | Smallest Row height (Left and Right); also caps both Row limits |
| `step` | float | 0.05 | no | Keyboard resize step |
| `persist` | bool | true | no | Save Divider positions across reloads and reboots |

### Centred Master

`centre_master` (bool, default `true`, per workspace) keeps Master in the middle of the screen, with Left and Right equally wide: every sideways resize moves both Dividers by the same amount in opposite directions. Turned off, a resize moves one Divider. How each setting behaves in each mode:

| Setting | Centred Master on | Centred Master off |
|---|---|---|
| Drag and One-off resizes | The grabbed Divider follows the cursor and the other mirrors it, whichever window or half of Master is grabbed | The grabbed Divider moves (on Master, the one nearer the grabbed half) |
| `left_width`, `right_width` | Must be equal. If not, the settings notice warns, Master keeps its width (1 − `left_width` − `right_width`), and each side gets half of the rest: `0.20` / `0.30` gives 0.25 \| 0.50 \| 0.25 | Used as set |
| `left_rows`, `right_rows` | A Row limit of `0` (missing side) is a conflict: the settings notice warns and that workspace uses `centre_master = false`. Otherwise unaffected | Unaffected |
| `fill_first` | Unaffected | Unaffected |
| `min_width`, `min_master_width` | Both Dividers stop together when either side reaches `min_width` or Master reaches `min_master_width`. Settings that can't all fit still shrink both sides in proportion, so they stay equal | Each Divider stops on its own. Widening Master with one side at `min_width` still moves the other side |
| `min_row_height` | Unaffected | Unaffected |
| `step` | The focused window's width changes by `step`. On Left or Right, both sides change by `step`, so Master changes by twice that. On Master, each Divider moves half a step | The focused window's width changes by `step`. On Left or Right, only that side's Divider moves. On Master, each Divider moves half a step, as Master has no side to grow towards |
| `persist` | Saves both Dividers as usual; they are always equal | Saves both Dividers as usual |
| Reset (SUPER+R) | Back to the settings widths, centred by the `left_width` / `right_width` rule above (no new warning) | Back to the settings widths |

**Turning Centred Master on** centres what you had: unequal widths kept from resizing, or saved by `persist`, become equal, with Master keeping its width. The centred widths replace the kept and saved ones straight away, so turning it off again doesn't bring the old ones back. This happens on a config reload and at runtime with `hl.config`. A workspace that isn't open is centred, and its saved entry rewritten, the first time threecol lays it out.

### Settings at a glance

Every setting the GIF at the top doesn't show, one at a time:

![threecol settings: zone widths, the min_width, min_master_width and min_row_height limits, step, persist and SUPER+R, per-workspace widths, fill_first, a window floating when both sides are full, Two Stacks and no Right zone](docs/threecol-settings.gif)

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
                    └── Dividers: SUPER+right-drag ──┘
                       or SUPER+ALT+HJKL by `step`; saved if `persist`
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

**No Left zone** · `left_rows = 0`

```
┌──────────────────────────────┬──────────┐
│                              │ Right 1  │
│            Master            ├──────────┤
│                              │ Right 2  │
│                              ├──────────┤
│                              │ Right 3  │
└──────────────────────────────┴──────────┘
```

**No Right zone** · `left_rows = "unlimited"`, `right_rows = 0`

```
┌──────────┬──────────────────────────────┐
│ Left 1   │                              │
├──────────┤            Master            │
│ Left 2   │                              │
├──────────┤                              │
│ Left 3   │                              │
└──────────┴──────────────────────────────┘
```

**Master only** · `left_rows = 0`, `right_rows = 0`

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

**Different widths** · `left_width = 0.15`, `right_width = 0.35`

```
┌──────┬─────────────────────┬──────────────┐
│      │                     │ Right 1      │
│ Left │       Master        ├──────────────┤
│      │                     │ Right 2      │
│      │                     ├──────────────┤
│      │                     │ Right 3      │
└──────┴─────────────────────┴──────────────┘
```

Each of these can be set for one workspace only with `layout_opts` (see below), so a workspace on a second monitor can have, say, no Left zone while the ultrawide keeps all three.

### Details

**Per workspace.** `left_width`, `right_width`, `left_rows`, `right_rows` and `fill_first` can be overridden with a workspace rule's `layout_opts`, using the same names:

```lua
hl.workspace_rule({
    workspace   = "3",
    layout      = "threecol",
    layout_opts = { left_width = 0.30, right_width = 0.20, left_rows = 2 },
})
```

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
            left_rows = 0,
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

New windows go to Master, then the Right Stack. Resizing Master moves only the Master|Right Divider, `move l` from Master does nothing, and a window dropped where Left would be lands on Master. The missing side's width setting is ignored: it isn't raised to `min_width`, counted against `min_master_width` or warned about. Its saved Divider is kept, so if the side comes back (by a reload or a `layout_opts` change) it returns where you left it. With both sides at `0`, Master fills the workspace, resizing does nothing, and every other window floats.

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

**Which value wins.** A Divider you moved by hand wins until you reset (SUPER+R). Then the `layout_opts` value, then the global setting.

**Mistakes are forgiven.**

- An unknown or bad `layout_opts` key or value is ignored; that workspace uses the global value.
- A global value out of range (a size outside 0 to 1, or a negative Row limit) is ignored; the default is used.
- A global value of the wrong type, such as text where a number belongs (`min_width = "abc"`), is dropped by Hyprland before threecol sees it. Neither Hyprland nor threecol warns about it; the default is used. `hyprctl getoption plugin:threecol:<name>` shows `set: false` for it.
- Settings that can't all fit are made to fit: Master keeps its minimum, and Left and Right shrink in proportion. A side with a Row limit uses only as many Rows as fit `min_row_height`.
- Each config load shows one notification listing what was ignored or adjusted.

**Changing settings.** Changes to `plugin:threecol:*` apply as soon as the config reloads.

- Lowering a Row limit, setting it to `0`, or switching a side from `"unlimited"` to a number, pushes that side's extra bottom windows out to the other side, at its end nearest Master (the top of Right, the bottom of Left), keeping their order. A Stack takes them all; a side with a Row limit takes as many as it has free Rows, first in Chain order; the rest float. Raising a limit moves nothing.
- Tighter limits clamp the saved Dividers in use but don't rewrite the saved file, so loosening them again brings the old sizes back.

**Saved Dividers.** With `persist = true`, each workspace's Dividers are saved after every finished resize to `$XDG_STATE_HOME/threecol/dividers` (or `~/.local/state/threecol/dividers`), keyed by workspace name, and come back after a reboot. Row heights are not saved; they go back to equal after a reboot. Reset (SUPER+R) always goes back to the workspace's `layout_opts`, or the global settings, and removes its saved entry.

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
