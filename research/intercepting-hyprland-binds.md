# Can threecol make Hyprland's stock binds act "the threecol way"?

Goal: a user switching to threecol keeps as many of their existing binds as possible. Plain
`hl.dsp.focus({ direction = "left" })` should do threecol's Chain focus (today: `hl.dsp.layout("focus l")`),
`hl.dsp.window.swap({ next = true })` should follow threecol's order, and so on, but only on threecol workspaces.
Target: Hyprland **0.57.0** and **0.57.1** (pins in `hyprpm.toml`). Running here: 0.57.1, `acc1aa0b`, `/usr/bin/Hyprland`
from the Arch package.

Source links:
- `HL/…` = `https://github.com/hyprwm/Hyprland/blob/acc1aa0bfd4bd575dcb0f76c35ccf12b660a1ab9/…` (0.57.1).
  `git diff 0aa6745f acc1aa0b` touches none of `src/config/shared/`, `src/config/lua/`, `src/layout/`, `src/plugins/`,
  `src/ipc/`, `src/desktop/state/`, so every line number below is also right for 0.57.0.
- `HY3/…` = `https://github.com/outfoxxed/hy3/blob/4c0199b42967b3511e3520b75cfa1f6411fb7e87/…` (pinned to 0.57).
- `SPLIT/…` = `https://github.com/shezdy/hyprsplit/blob/6b00b677d8905fb38779c91e12d6294e0e586a44/…` (last pin 0.54.3).
- `SCROLL/…` = `https://github.com/cpiber/hyprscroller/blob/c4979f723d27356c5faca216712cc55176764530/…` (last pin 0.53.3).
- `PLUG/…` = `https://github.com/hyprwm/hyprland-plugins/blob/d2427d4d15b36df40468f5a58bbd67cdba19835d/…`.
- `WIKI/…` = `https://github.com/hyprwm/hyprland-wiki/blob/88020541790ac876a5228ba031a6036955a6a061/content/…`.
- `BIN` = what `nm -D` / `objdump -d` show for the installed `/usr/bin/Hyprland` 0.57.1.

Source was read, and the installed binary was inspected with `nm`/`objdump`. No hook was built or loaded.

## Answer in short

- **Yes, a direction focus can be caught and turned into Chain focus.** The way to do it is a **function hook**
  (`HyprlandAPI::createFunctionHook`) on `Config::Actions::moveFocus(Math::eDirection)`. Every `hl.dsp.focus({direction})`
  bind, and every `hyprctl dispatch 'hl.dsp.focus({direction="left"})'`, calls that one function. The hook checks "focused window
  is tiled on a threecol workspace and not fullscreen". If so, it runs threecol's Chain focus. Otherwise it calls the original
  through the trampoline, so other workspaces, floating windows and fullscreen keep Hyprland's behaviour.
- **No layout virtual is involved in focus.** `moveFocus` picks the target from window geometry only
  (`CWindowQuery::inDirection`). The layout interface is never asked. The same is true for the neighbour lookup in direction
  swap and for the order used by `swap {next}` / `cycle_next`.
- **Many actions already reach threecol through virtuals**, and threecol overrides most of them: `window.move {direction}` →
  `moveTargetInDirection`, `window.resize` → `resizeTarget(Δ)`, any swap → `swapTargets(a, b)` (but Hyprland has already
  chosen `b`), float toggle → `removeTarget`/`movedTarget`, fullscreen → `getFSHandler()`, window close → `getNextCandidate`, all
  `hl.dsp.layout("…")` → `layoutMsg`. The master/dwindle layoutmsgs (`focusmaster`, `swapwithmaster`, `cyclenext`, `addmaster`,
  `mfact`, `togglesplit`, …) are plain strings that already arrive in `layoutMsg` unchanged.
- **The other options don't work in 0.57.** `addDispatcher`/`addDispatcherV2` are stubs that `return false`, and
  `registerCallbackDynamic` is a stub that returns `nullptr`. The event bus has no "dispatch" or "bind" event to cancel, only raw
  key events. Patching `hl.dsp.*` from C++ is undone on every reload, because the Lua state is rebuilt.
- **Fragility is low within a pinned release.** The `Config::Actions::*` and `CWindowQuery::*` symbols are exported (`T` in
  `nm -D`), because Hyprland links with `ENABLE_EXPORTS`. The binary has real out-of-line calls to every one of them, so none
  is inlined away. Names and signatures can change between minor releases, but hyprpm already rebuilds threecol per commit.
  The real limits: function hooks work on x86_64 only, and each function can be hooked by one plugin at a time.
- **Precedent:** none of hy3, hyprscroller or hyprsplit hook the stock focus. hy3 overrides the layout virtuals (so stock
  `movewindow` works hy3-style) and ships its own `hl.plugin.hy3.move_focus`. hyprsplit and the official csgo-vulkan-fix
  use `findFunctionsByName` + `createFunctionHook` on internal functions, so the mechanism itself is established.

## 1. What already routes through the layout algorithm

The interface is `Layout::IModeAlgorithm` (`HL/src/layout/algorithm/ModeAlgorithm.hpp:19-69`) plus
`ITiledAlgorithm::getNextCandidate` (`HL/src/layout/algorithm/TiledAlgorithm.hpp:90`). The Lua dispatcher closures in
`HL/src/config/lua/bindings/LuaBindingsDispatchers.cpp` call the free functions in `Config::Actions`
(`HL/src/config/shared/actions/ConfigActions.cpp`). The table below follows each action down to the layout.

| Lua bind | `Config::Actions::` (file:line) | Reaches layout? | Virtual and context threecol receives |
|---|---|---|---|
| `hl.dsp.focus({direction})` | `moveFocus(dir)` `ConfigActions.cpp:448-535`. Closure `dsp_moveFocus` `LuaBindingsDispatchers.cpp:1082-1084`, built in `hlFocus` `:1116-1128` | **No** | none. Target is `windowState()->query().inDirection(window, dir)` (`:465`), pure geometry (`HL/src/desktop/state/WindowQuery.cpp:23-47`), or `query().cycle(...)` when `binds:movefocus_cycles_fullscreen` applies (`:461-463`). Then group cycling, monitor fallback, edge wrap (`:467-534`). |
| `hl.dsp.focus({window/urgent_or_last/last})` | `focus`, `focusUrgentOrLast`, `focusCurrentOrLast` `:537`, `:632`, `:616` | No | none (history/selector) |
| `hl.dsp.window.move({direction})` | `moveInDirection` `:559-573` | **Yes** | `moveTargetInDirection(target, dir, silent)` via `CLayoutManager::moveInDirection` (`HL/src/layout/LayoutManager.cpp:136-147`) → `CSpace` (`HL/src/layout/space/Space.cpp:182-185`) → `CAlgorithm` sends it to the floating or tiled algo by `t->floating()` (`HL/src/layout/algorithm/Algorithm.cpp:163-168`). Refused before that if fullscreen (`:564-565`). **threecol already overrides this** (`src/plugin/ThreecolAlgorithm.cpp:589`). |
| `hl.dsp.window.move({direction, group_aware=true})` | `moveWindowOrGroup` `:1452-1498` | Partly | uses geometric `query().inDirection` (`:1467`) to decide on group entry/exit, then `moveTargetInDirection` for the plain-move branches (`:1463`, `:1478`, `:1484`, `:1491`) |
| `hl.dsp.window.swap({direction})` | `swapInDirection` `:575-593` | Partly | the **partner is chosen by geometry** (`query().inDirection`, `:583`). Then `switchTargets(a, b, true)` → `ITarget::swap` → `CSpace::swap` → `CAlgorithm::swapTargets` → `swapTargets(a, b)` (`LayoutManager.cpp:117-134`, `HL/src/layout/target/Target.cpp:103-127`, `Space.cpp:170-180`, `Algorithm.cpp:136-161`). threecol gets the two targets, **not the direction**. |
| `hl.dsp.window.swap({next/prev})` | `swapNext` `:742-763` | Partly | partner from `query().cycle(...)` (window order, `:750-753`), then `switchTargets(…, false)` → `swapTargets(a, b)` |
| `hl.dsp.window.swap({window=sel})` | `swapWith` `:595-614` | Partly | partner from the selector, then `swapTargets(a, b)` |
| `hl.dsp.window.cycle_next({…})` | `cycleNext` `:1758-1803` | **Only for built-ins** | if `tiled=true` and the tiled algo's `typeid` is `CMonocleAlgorithm` or `CMasterAlgorithm` (hard-coded list, `:1771-1782`), it sends `layoutMessage("cyclenext"/"cycleprev")`. Any other layout, threecol included, gets geometric `query().cycle` (`:1792-1798`). |
| `hl.dsp.window.resize({x,y,relative})` | `resize` `:680-699` | Yes | `resizeTarget(Δ, target, CORNER_NONE)` (`:693`, `LayoutManager.cpp:63-82`, `Algorithm.cpp:124-129`). Refused if fullscreen (`:685-686`). threecol overrides it. |
| `hl.dsp.window.float({…})` | `floatWindow` `:237-268` | Yes | `changeFloatingMode` → `CSpace::toggleTargetFloating` → `CAlgorithm::setFloating` = `removeTarget` on the old mode algo, then `moveTarget` into the new one (`LayoutManager.cpp:32-53`, `Space.cpp:117-125`, `Algorithm.cpp:84-94`). Tiled algo sees `removeTarget` / `movedTarget`. |
| `hl.dsp.window.fullscreen({…})` | `fullscreenWindow` `:340-380` | Yes (opt-in) | `Fullscreen::controller()->setFullscreenMode`, which uses the algorithm's `getFSHandler()` (`ModeAlgorithm.hpp:50-55`, `HL/src/managers/fullscreen/FullscreenController.cpp:559-562`). A layout can return its own `IFullscreenHandler` (virtuals in `HL/src/managers/fullscreen/handler/FullscreenHandler.hpp:23-73`). |
| window close (focus after) | `CWindow` unmap | Yes | `getNextCandidate(old)` when `input:focus_on_close` is 0 and the closed window was tiled (`HL/src/desktop/view/window/Window.cpp:1690-1697`, `Algorithm.cpp:231-260`) |
| `hl.dsp.layout("…")` | `layoutMessage(msg)` `:1228-1233` | Yes | `layoutMsg(sv)` on the **focused monitor's active (or special) workspace** (`LayoutManager.cpp:91-104`). The floating algo gets it first, the tiled algo next (`Algorithm.cpp:114-118`). |
| move to another monitor/workspace in a direction | built-in algos | Yes | `focalPointForDir(t, dir)` (`ModeAlgorithm.hpp:58`, default `ModeAlgorithm.cpp:26-55`) is used by master/monocle/scrolling/dwindle when a move leaves the workspace (`MasterAlgorithm.cpp:497`, `DwindleAlgorithm.cpp:567`). It is a helper threecol may call. Nothing outside the algorithms calls it. |

The master/dwindle-style messages (`focusmaster`, `swapwithmaster`, `cyclenext`, `cycleprev`, `addmaster`, `removemaster`, `mfact`,
`orientation*`, `togglesplit`, `swapsplit`, `preselect`, …) are all `hl.dsp.layout("<string>")` binds. The string reaches threecol's
`layoutMsg` untouched, so threecol can implement any of them by name. It already does `focusmaster` and `swapwithmaster`
(`src/core/Workspace.cpp:300`, `:326`). This needs no hook. Unknown strings should return an error, or be ignored on purpose.

`hyprctl dispatch` takes the same route: with a Lua config it evaluates `return hl.dispatch(<args>)`
(`HL/src/ipc/s1/Commands.cpp:1332-1349`), which calls the same C closures and the same `Config::Actions::*`. **A hook on
`Config::Actions::moveFocus` catches binds, Lua function binds and `hyprctl dispatch` alike.**

## 2. Ways to intercept the rest

### 2a. `addDispatcher` / `addDispatcherV2`: no

| Fact | Source |
|---|---|
| `addDispatcher`, `addDispatcherV2` and `removeDispatcher` all just `return false;` in 0.57. There is no dispatcher name table left to override; Lua binds hold C closures. | `HL/src/plugins/PluginAPI.cpp:198-208` |
| hy3 still calls `addDispatcherV2("hy3:…")` for the legacy path, but that does nothing. Its working binds are `addLuaFunction` ones. | `HY3/src/dispatchers.cpp:953-984` |

### 2b. Event callbacks that cancel: no

| Fact | Source |
|---|---|
| `registerCallbackDynamic` is `[[deprecated]]`, its body is commented out, and it returns `nullptr`. | `HL/src/plugins/PluginAPI.hpp:177`, `HL/src/plugins/PluginAPI.cpp:38-47` |
| The replacement, `Event::bus()->m_events`, has cancellable **input** events (`input.keyboard.key`, mouse, touch, gestures) but no dispatch, bind or focus-request event. `keybinds` only has `submap`. Cancelling a raw key would mean re-implementing bind matching. | `HL/src/event/EventBus.hpp:41-43`, `:127-157`, `:209-211` |

### 2c. Patching the Lua `hl.dsp.*` table: works, but only from the user's config

| Fact | Source |
|---|---|
| `hl.dsp` is an ordinary table. Each entry is a factory closure that returns a dispatcher userdata. | `HL/src/config/lua/bindings/LuaBindingsDispatcherUtils.cpp:61-91`, `HL/src/config/lua/bindings/LuaBindingsDispatchers.cpp:1379-1390` |
| `hl.bind` accepts a plain Lua function as well as a dispatcher. | `LuaBindingsDispatcherUtils.cpp:109-128` |
| On every reload the Lua state is thrown away and rebuilt (`luaL_newstate`), and plugin functions are re-registered. Anything a plugin wrote into `hl.dsp` from C++ is gone, and the user's binds have already been evaluated by then. | `HL/src/config/lua/ConfigManager.cpp:474-494`, `:604`, `:694`, `:1334-1340` |

So a C++ plugin can't reliably patch `hl.dsp`. A **Lua shim shipped with threecol** could do it, though. The user would add
`require("threecol")` (or similar) **before** their binds, and the shim would wrap `hl.dsp.focus` etc. in a Lua function that checks
the active workspace's tiled layout and either dispatches `hl.dsp.layout("focus l")` or the original dispatcher. That costs
one config line per user and adds a Lua call to every bind. It also only covers binds created after the `require`, and
`hyprctl dispatch` too, since that evaluates in the same Lua state. I did not check which Lua query gives the active
workspace's layout cheaply (`ws.tiled_layout` exists, see `research/cpp-plugin-layout.md`). This is a fallback, not the
recommendation.

### 2d. Function hooks: yes, the mechanism that works

| Fact | Source |
|---|---|
| `createFunctionHook(handle, source, destination)` → `CFunctionHook`. `hook()` overwrites the start of the target with a `jmp rel32` to the plugin's function, and `m_original` points to a trampoline that runs the original. | `HL/src/plugins/PluginAPI.cpp:125-132`, `HL/src/plugins/HookSystem.hpp:12-36`, `HL/src/plugins/HookSystem.cpp:144-247` |
| **x86_64 only**: on other architectures `hook()` returns `false`. | `HookSystem.cpp:147-149`, `WIKI/hyprland-plugins/development/advanced.md:26-27` |
| **One hook per function**: a second hook on the same address fails with "function is already hooked". If another plugin hooks `moveFocus`, the two conflict. | `HookSystem.cpp:151-155` |
| Unloading a plugin removes all its hooks (`removeAllHooksFrom`), so Hyprland's behaviour comes back cleanly. | `HL/src/plugins/PluginSystem.cpp:139-158`, `HookSystem.cpp:289` |
| `findFunctionsByName(handle, name)` runs `nm -D -j` (and `--demangle`) on `/proc/self/exe`, then `dlsym`s each match. It needs `nm` from binutils at runtime (present here: `binutils 2.47-4`). Matches carry the demangled signature for filtering. | `HL/src/plugins/PluginAPI.cpp:258-330` |
| Hyprland is linked with `CMAKE_EXECUTABLE_ENABLE_EXPORTS TRUE`, so its functions are in the dynamic symbol table. | `HL/CMakeLists.txt:120` |
| `BIN`: all the action functions are exported `T` symbols, e.g. `_ZN6Config7Actions9moveFocusEN4Math10eDirectionE`, `…15swapInDirection…`, `…8swapNext…`, `…9cycleNext…`, `…6resize…`, `…16fullscreenWindow…` (both overloads), `…11floatWindow…`, `…13layoutMessage…`. `CWindowQuery::inDirection(PHLWINDOW, eDirection) const` is exported too (`_ZNK7Desktop12CWindowQuery11inDirectionEN9Hyprutils6Memory14CSharedPointerINS_4View7CWindowEEEN4Math10eDirectionE`). | `nm -D /usr/bin/Hyprland` |
| `BIN`: each of these has real `call` sites in the binary: `moveFocus` 1 (the Lua closure), `swapInDirection` 1, `swapNext` 3, `cycleNext` 1, `layoutMessage` 2, `CWindowQuery::inDirection(PHLWINDOW,…)` 6, matching the 6 source callers (`ConfigActions.cpp:464`, `:583`, `:1405`, `:1467`, `:1821`, `MasterAlgorithm.cpp:473`). So the callers do not carry inlined copies that a prologue hook would miss. | `objdump -d /usr/bin/Hyprland` |
| Since `ConfigActions.hpp` is installed (`/usr/include/hyprland/src/config/shared/actions/ConfigActions.hpp`), the plugin can also take `&Config::Actions::moveFocus` directly; it resolves against the executable's exports. The difference: if the symbol is missing, `dlopen` fails outright, while `findFunctionsByName` lets the plugin skip that hook and log it. | local headers |
| Precedent at 0.5x: hyprsplit hooks `CWorkspaceSwipeGesture::begin` with `findFunctionsByName` + signature filter + `createFunctionHook`. The official csgo-vulkan-fix hooks `sendPointerMotion`, `configure` and `computeDamage` the same way. | `SPLIT/src/main.cpp:544-550`, `PLUG/csgo-vulkan-fix/main.cpp:184-207` |

What a `moveFocus` hook looks like (untested):

```cpp
using FnMoveFocus = Config::Actions::ActionResult (*)(Math::eDirection);
static CFunctionHook* g_moveFocusHook = nullptr;

static Config::Actions::ActionResult hkMoveFocus(Math::eDirection dir) {
    const auto w = Desktop::focusState()->window();
    if (auto* algo = threecolFor(w); algo && !w->isFloating() && !Fullscreen::controller()->isFullscreen(w)) {
        if (algo->chainFocus(dir))          // true = threecol moved focus
            return {};
        // nothing in that direction: let Hyprland do monitor fallback etc.
    }
    return ((FnMoveFocus)g_moveFocusHook->m_original)(dir);
}
// pluginInit: pick the match whose signature is "Config::Actions::moveFocus(Math::eDirection)"
```

`threecolFor(w)` would check `typeid(*w->m_workspace->space()->algorithm()->tiledAlgo().get()) == typeid(CThreecolAlgorithm)`,
the same test Hyprland uses in `cycleNext` (`ConfigActions.cpp:1771-1777`). Because the hook falls through to `m_original`,
non-threecol workspaces, floating windows, fullscreen and "nothing in that direction → next monitor"
(`binds:window_direction_monitor_fallback`, `ConfigActions.cpp:484-485`) all keep Hyprland's behaviour.

**Alternative hook point:** `CWindowQuery::inDirection(PHLWINDOW, dir) const`. The hook would return threecol's Chain
neighbour on threecol workspaces and the original's result elsewhere. One hook would then change `focus {direction}`,
`swap {direction}`, `move {group_aware}` and `move_into_group` together, and keep all of Hyprland's group, monitor-fallback and
fullscreen handling around them. The cost: the signature is a const member returning an `SP` by value (hidden
return pointer + `this` + `SP` by invisible reference), which is easier to get wrong. The query also can't do anything
stateful beyond "which window". Hooking `moveFocus` reuses threecol's existing `focus l` code directly. Use `inDirection`
only if direction-swap and group moves should follow the Chain too.

## 3. Precedent: how other layout plugins handle stock binds

| Plugin | What it does | Source |
|---|---|---|
| hy3 (0.57) | Overrides `moveTargetInDirection` (stock `hl.dsp.window.move({direction})` → hy3 `shiftNode`) and `layoutMsg` (handles `togglesplit` like dwindle). Its `swapTargets` is a `// todo` stub. Focus is **not** intercepted: users bind `hl.plugin.hy3.move_focus(...)` (an `addLuaFunction`). No function hooks. | `HY3/src/Hy3Layout.cpp:524-560`, `HY3/src/dispatchers.cpp:953-968` |
| hyprscroller (≤0.53, old `IHyprLayout` API) | Own `scroller:movefocus`, `scroller:movewindow`, … dispatchers. Users rebind. Uses `registerCallbackDynamic` (dead in 0.57) for events, function hooks only for its overview. | `SCROLL/src/dispatchers.cpp:462-474`, `SCROLL/src/scroller.cpp:696-727`, `SCROLL/src/overview.cpp:221-223` |
| hyprsplit (≤0.54) | Own `split:*` dispatchers, users rebind. Function hook on the workspace swipe gesture to change built-in behaviour. | `SPLIT/src/main.cpp:533-550` |
| Hyprland's own layouts | Even the built-in scrolling/master layouts don't get stock directional focus. Master only gets `cycle_next` through the hard-coded `typeid` list. | `ConfigActions.cpp:1771-1782` |

No plugin I read makes stock `movefocus` layout-aware. threecol would be the first. The mechanism (hooks on exported internals)
is the one hyprsplit and the official plugins already rely on.

## 4. Recommendation

1. **Override virtuals wherever Hyprland already asks the layout.** threecol already does this for move, resize, swap,
   floating and close.
2. **Handle the master/dwindle layoutmsg vocabulary in `layoutMsg`.** It costs nothing, needs no hook, and users of master
   binds keep them as they are.
3. **Hook a small, explicit set of `Config::Actions` functions**, each falling through to `m_original` unless the window is tiled on
   a threecol workspace and not fullscreen. Find them by `findFunctionsByName` + exact demangled signature, skip a missing
   hook and log it. Use `VersionGuard`/hyprpm pins as the safety net, since a pinned build won't meet a changed signature.
   Start with `moveFocus`. Add `swapInDirection`, `swapNext` and `cycleNext` if wanted.
4. Keep `hl.dsp.layout("focus l")` etc. as the stable, hook-free path, and document it for non-x86_64 users and for setups where
   another plugin already hooks the same function.

| Action (Lua bind) | Feasible | How | Caveats |
|---|---|---|---|
| `focus({direction})` | **Yes** | hook `Config::Actions::moveFocus` → Chain focus; fall through otherwise | x86_64 only; one hook per function; decide what happens at the edge (fall through = monitor fallback) |
| `window.move({direction})` | Yes, done | `moveTargetInDirection` override | not called for fullscreen windows (`ConfigActions.cpp:564`) |
| `window.move({direction, group_aware})` | Mostly | plain-move branches already hit `moveTargetInDirection`; group decisions use geometry unless `CWindowQuery::inDirection` is hooked | groups rarely matter in threecol |
| `window.swap({direction})` | Yes | hook `swapInDirection` (or `CWindowQuery::inDirection`) to choose the partner by threecol's rules, then swap | `swapTargets(a,b)` alone can't know the direction |
| `window.swap({next/prev})` | Yes | hook `swapNext` to use threecol's fill order | without a hook the partner is Hyprland's cycle order |
| `window.cycle_next({…})` | Yes | hook `cycleNext`: on threecol tiled, focus the next in fill order (or send `layoutMessage("cyclenext")`); else original | Hyprland's built-in path is a hard-coded `typeid` list, a plugin can't join it |
| `window.resize({x,y,relative})` | Yes, done | `resizeTarget(Δ, t, CORNER_NONE)` | refused for fullscreen before the layout sees it |
| `window.float(...)` | Yes, done | `removeTarget` / `movedTarget` | none |
| `window.fullscreen(...)` | Yes (if wanted) | return a custom handler from `getFSHandler()` | default handler is fine unless threecol wants layout-owned fullscreen |
| focus after close | Yes, done | `getNextCandidate` | only when `input:focus_on_close = 0` and the closed window was tiled |
| `focus({window/urgent_or_last/last})`, `focus({monitor})` | Not needed | stays Hyprland's | none |
| `layout("focusmaster"/"swapwithmaster"/"cyclenext"/"addmaster"/"mfact …"/"togglesplit"/…)` | Yes | `layoutMsg` string match | needs a decision on what each means in threecol; `mfact` maps naturally to the Master Divider |
| `hyprctl dispatch …` | Same as the bind | `hl.dispatch` → the same closures and `Config::Actions` | none |
