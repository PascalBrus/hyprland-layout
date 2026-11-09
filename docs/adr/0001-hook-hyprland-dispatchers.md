# Hook Hyprland's dispatchers so stock binds act the threecol way

People switching to threecol should keep their binds. Hyprland 0.57 sends move, resize and swap to the layout, but it never asks the layout about direction focus, about which window a swap pairs with, or about `cycle_next`. So threecol hooks those Hyprland functions (`HyprlandAPI::createFunctionHook` on `Config::Actions::moveFocus`, `swapInDirection`, `swapNext` and `cycleNext`). On a window threecol tiles with nothing fullscreen, the hook does the threecol behaviour. Everywhere else it calls Hyprland's original. Each hooked dispatcher has a switch under `plugin:threecol:binds`, `true` by default, and setting it to `false` gives Hyprland's behaviour back: see [Hyprland dispatchers threecol changes](../../README.md#hyprland-dispatchers-threecol-changes).

## Considered Options

- **threecol dispatchers only** (what hy3 does): no hooks, but every user rebinds focus, swap and cycle. Kept as the fallback (`hl.dsp.layout("focus l")` and so on), not the main path.
- **A Lua shim users `require` before their binds**: no hooks, but it costs a config line and is easy to get wrong. A C++ patch to `hl.dsp` doesn't survive a reload.
- `addDispatcherV2` and `registerCallbackDynamic` do nothing in 0.57, and Hyprland has no dispatch or bind event that can be cancelled.

## Consequences

- The hooks work on x86_64 only, and only one plugin can hook a given function. If a hook can't be installed, threecol shows one notification at load, and that dispatcher keeps Hyprland's behaviour.
- The hooked functions are Hyprland internals. Each Hyprland release can rename or change them, so every hyprpm commit pin needs a rebuild and a check.
- Move, resize and drop have no switch: Hyprland leaves them to the layout, so there is no Hyprland behaviour to give back.

Research: [research/intercepting-hyprland-binds.md](../../research/intercepting-hyprland-binds.md).
