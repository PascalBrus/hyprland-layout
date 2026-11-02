# How does a C++ plugin provide a tiled layout in Hyprland 0.57?

Research for [#12](https://github.com/PascalBrus/hyprland-layout/issues/12). Target: Hyprland **0.57.0** with the Lua config, as running here
(`hyprctl version`: tag v0.57.0, commit `0aa6745f705f2c4b0a5004eb019ef76e5bbdb426`, ABI string
`0aa6745f…_aq_0.15_hu_0.14_hg_0.5_hc_0.1_hlg_0.6`). The installed headers (`/usr/include/hyprland`, package `hyprland 0.57.0-1`)
are from the same commit (`src/version.h:2`).
Vocabulary follows `GLOSSARY.md` (Zone, Master/Left/Stack zone, Hole, Fill order, Divider).

Source links:
- `HL/…` = `https://github.com/hyprwm/Hyprland/blob/v0.57.0/…` (the running commit).
- `INC/…` = `/usr/include/hyprland/…` (same commit, local).
- `WIKI/…` = `https://github.com/hyprwm/hyprland-wiki/blob/88020541790ac876a5228ba031a6036955a6a061/content/…`.
- `PLUG/…` = `https://github.com/hyprwm/hyprland-plugins/blob/d2427d4d15b36df40468f5a58bbd67cdba19835d/…`.
- `HY3/…` = `https://github.com/outfoxxed/hy3/blob/4c0199b42967b3511e3520b75cfa1f6411fb7e87/…` (a third-party layout plugin
  pinned to 0.57.0; the only working 0.57 layout plugin I read).

Everything is read from source. Nothing was built or loaded.

## Answer in short

- **Interface:** a plugin layout is a C++ class deriving from `Layout::ITiledAlgorithm` (the old `IHyprLayout`/`addLayout` API is
  deprecated). It is registered in `pluginInit` with `HyprlandAPI::addTiledAlgo(handle, "threecol", &typeid(CThreecol), factory)` and
  selected with plain `general.layout = "threecol"` (no `lua:` prefix) or per workspace with `hl.workspace_rule({ layout = "threecol" })`.
  Hyprland creates **one instance per workspace**.
- **Resize:** the plugin gets the real thing. `resizeTarget(Δ, target, corner)` is called with the pixel delta of every mouse-drag
  tick (and the grabbed corner), and once with the delta for `hl.dsp.window.resize({x, y, relative})` (corner = `CORNER_NONE`). The
  cursor is readable (`g_pInputManager->getMouseCoordsInternal()`). The Lua-only workaround from #2 is not needed.
- **Binds:** plugin dispatchers are **gone** in 0.57: `addDispatcher`/`addDispatcherV2` are stubs that return `false`. Binds use
  `hl.dsp.layout("…")` (layout messages, as today) and/or Lua functions the plugin publishes under `hl.plugin.threecol.*`
  (`HyprlandAPI::addLuaFunction`), which is how hy3 does it.
- **Config:** `HyprlandAPI::addConfigValueV2` with typed values (`CFloatValue`, `CIntValue`, `CBoolValue`, `CStringValue`, `CColorValue`,
  `CVec2Value`, …, floats with min/max). Name `plugin:threecol:side` is set from Lua as
  `hl.config({ plugin = { threecol = { side = 0.25 } } })`. Plugin values are global; **per-workspace overrides are possible natively**
  through workspace rules: `hl.workspace_rule({ workspace = "3", layout = "threecol", layout_opts = { side = 0.3 } })`, read by the
  plugin from the workspace rule (that is what master's `orientation` and scrolling's `direction` do).
- **Install:** hyprpm (`hyprpm.toml` with `[repository]` + `[threecol]` `output`/`build`, `commit_pins` per Hyprland commit), or
  manual: build the `.so` and put `hl.plugin.load("/abs/path/threecol.so")` in `hyprland.lua`. The `.so` must be rebuilt for every
  Hyprland update (exact commit; Hyprland only checks the API version "0.2", the hash check is the plugin's job).
- **State:** a plugin is ordinary in-process C++, so it can write a state file with `std::ofstream`. It **stays loaded across config
  reloads** and its per-workspace instances survive too, so Zone assignments and Divider positions are no longer lost on `hyprctl
  reload` (the Lua layout loses them). Instances die with their workspace, so Dividers must be kept in a plugin-global map keyed by
  workspace.
- **Drag-and-drop** becomes native: on drop Hyprland calls the layout's `movedTarget` while `dragController()->wasDraggingWindow()` is
  true, so the plugin reads the cursor and swaps there. The 2-second arm-bind hack goes away. `threecol_active()` still works:
  `ws.tiled_layout` returns `"threecol"`.

## 1. The layout API

| Fact | Source |
|---|---|
| Plugin API version is `"0.2"`. Plugins export `pluginAPIVersion`, `pluginInit(HANDLE)` (required), `pluginExit` (optional). | `INC/src/plugins/PluginAPI.hpp:21`, `:95-123` |
| `addLayout`/`removeLayout` (the old `IHyprLayout` API) are `[[deprecated]]` in favour of `addTiledAlgo`/`addFloatingAlgo`/`removeAlgo`. | `INC/src/plugins/PluginAPI.hpp:195-218` |
| `addTiledAlgo(handle, name, typeInfo, std::function<UP<ITiledAlgorithm>()>&& factory)` records the name on the plugin and calls `algoMatcher()->registerTiledAlgo`. | [`HL/src/plugins/PluginAPI.cpp#L76-L85`](https://github.com/hyprwm/Hyprland/blob/v0.57.0/src/plugins/PluginAPI.cpp#L76-L85) |
| Registration fails on a name collision, stores the factory and the `typeid → name` mapping, then calls `updateWorkspaceLayouts()` so existing workspaces switch immediately. | [`HL/src/layout/supplementary/WorkspaceAlgoMatcher.cpp#L50-L60`](https://github.com/hyprwm/Hyprland/blob/v0.57.0/src/layout/supplementary/WorkspaceAlgoMatcher.cpp#L50-L60) |
| The layout for a workspace is the workspace rule's `layout` if set, else `general:layout`. An unknown name silently falls back to `dwindle`. | [`WorkspaceAlgoMatcher.cpp#L92-L109`](https://github.com/hyprwm/Hyprland/blob/v0.57.0/src/layout/supplementary/WorkspaceAlgoMatcher.cpp#L92-L109) |
| The factory is called once **per workspace** (each workspace's `CSpace` gets its own `CAlgorithm` = one tiled + one floating algo). | [`WorkspaceAlgoMatcher.cpp#L111-L113`](https://github.com/hyprwm/Hyprland/blob/v0.57.0/src/layout/supplementary/WorkspaceAlgoMatcher.cpp#L111-L113), [`HL/src/workspace/HLWorkspace.cpp#L90`](https://github.com/hyprwm/Hyprland/blob/v0.57.0/src/workspace/HLWorkspace.cpp#L90) |
| Switching layout (or unloading the plugin) moves every tiled target into the new algo via `removeTarget` + `newTarget`. On unload Hyprland unregisters the plugin's algos and the workspaces fall back to dwindle. | [`HL/src/layout/algorithm/Algorithm.cpp#L196-L221`](https://github.com/hyprwm/Hyprland/blob/v0.57.0/src/layout/algorithm/Algorithm.cpp#L196-L221), [`WorkspaceAlgoMatcher.cpp#L74-L90`](https://github.com/hyprwm/Hyprland/blob/v0.57.0/src/layout/supplementary/WorkspaceAlgoMatcher.cpp#L74-L90), [`HL/src/plugins/PluginSystem.cpp#L154-L155`](https://github.com/hyprwm/Hyprland/blob/v0.57.0/src/plugins/PluginSystem.cpp#L154-L155) |
| `ws.tiled_layout` (Lua) and the layout name come from `getNameForTiledAlgo`: `layoutName()` override if any, else the registered `typeid`. So a plugin registered as `"threecol"` reports `"threecol"`. | [`WorkspaceAlgoMatcher.cpp#L147-L160`](https://github.com/hyprwm/Hyprland/blob/v0.57.0/src/layout/supplementary/WorkspaceAlgoMatcher.cpp#L147-L160), [`HL/src/config/lua/objects/LuaWorkspace.cpp#L148-L155`](https://github.com/hyprwm/Hyprland/blob/v0.57.0/src/config/lua/objects/LuaWorkspace.cpp#L148-L155) |
| Working example: hy3 registers `addTiledAlgo(PHANDLE, "hy3", &typeid(Hy3Layout), [] { return makeUnique<Hy3Layout>(); })`. hyprscrolling, the old example, was dropped from hyprland-plugins on 2026-05-12 (scrolling is now built in). | `HY3/src/main.cpp:85-87`; `PLUG` commit `3aa21f2` "all: drop unmaintained plugins (#663)"; `WorkspaceAlgoMatcher.cpp#L30-L35` |
| The wiki has no page for the algorithm API; its "advanced" page still includes the removed `IHyprLayout.hpp`. The headers are the documentation. | `WIKI/hyprland-plugins/development/advanced.md:19` |

### Callbacks a tiled algorithm gets

`ITiledAlgorithm : IModeAlgorithm` (`INC/src/layout/algorithm/TiledAlgorithm.hpp`, `ModeAlgorithm.hpp`). `m_parent` (a `WP<CAlgorithm>`)
gives `space()` → `workspace()`, `workArea()`, `targets()`.

| Method | Required | When | Source |
|---|---|---|---|
| `newTarget(SP<ITarget>)` | yes | A new tiled window on this workspace (also every window when the layout is switched to this algo). | `ModeAlgorithm.hpp:123-124`; `Algorithm.cpp#L30-L40` |
| `movedTarget(SP<ITarget>, optional<Vector2D> focalPoint)` | yes | A window moved in from elsewhere: another workspace, or **floating → tiled** (incl. the end of a drag-and-drop). | `ModeAlgorithm.hpp:126-127`; `Algorithm.cpp#L62-L94` |
| `removeTarget(SP<ITarget>)` | yes | Window closed, left the workspace, or became floating (incl. the start of a drag). | `ModeAlgorithm.hpp:129-130`; `Algorithm.cpp#L42-L60` |
| `resizeTarget(const Vector2D& Δ, SP<ITarget>, eRectCorner corner)` | yes | See section 2. | `ModeAlgorithm.hpp:132-133` |
| `recalculate(eRecalculateReason)` | yes | Re-place all windows. Reason tells workspace change, special-workspace toggle, fullscreen toggles, monitor geometry change, render-monitor. | `ModeAlgorithm.hpp:135-136`; `INC/src/layout/space/Space.hpp:16-24` |
| `swapTargets(a, b)` | yes | `hl.dsp.window.swap`-style swaps. | `ModeAlgorithm.hpp:138-139`; `Algorithm.cpp#L136-L161` |
| `moveTargetInDirection(t, dir, silent)` | yes | `hl.dsp.window.move({ direction })` (movewindow). | `ModeAlgorithm.hpp:141-142`; [`HL/src/config/shared/actions/ConfigActions.cpp#L1452-L1491`](https://github.com/hyprwm/Hyprland/blob/v0.57.0/src/config/shared/actions/ConfigActions.cpp#L1452-L1491) |
| `getNextCandidate(old)` | yes | Which window to focus after one closes (unless `input:focus_on_close = 2`). | `TiledAlgorithm.hpp:87`; `Algorithm.cpp#L231-L260` |
| `layoutMsg(string_view)` | no | `hl.dsp.layout("…")` / `layoutmsg`. Returns `Config::ErrorResult`. | `ModeAlgorithm.hpp:144-145` |
| `predictSizeForNewTarget()` | no | Size hint for windows about to open. | `ModeAlgorithm.hpp:147-148` |
| `getFSHandler()` | no | Own fullscreen semantics; default handler otherwise. | `ModeAlgorithm.hpp:150-155` |
| `layoutName()` | no | Name override. | `TiledAlgorithm.hpp:89-93` |

A target (`ITarget`) is placed with `setPositionGlobal(CBox)`; it exposes `window()`, `floating()`, `minSize()`/`maxSize()`,
`position()`, `workspace()` (`INC/src/layout/target/Target.hpp:38-86`). Pseudotiled resizes are handled before the layout
(`LayoutManager.cpp#L63-L82`, per #2).

Note: focus in a direction (`hl.dsp.focus({ direction })`) is **not** routed to the layout; it picks the geometrically nearest window
(`Desktop::windowState()->query().inDirection(…)`, [`ConfigActions.cpp#L448-L466`](https://github.com/hyprwm/Hyprland/blob/v0.57.0/src/config/shared/actions/ConfigActions.cpp#L448-L466)).
threecol's own focus walk (skip Holes, Stack order) still needs a layout message or plugin Lua function.

## 2. Native resize

The call chain from #2 is unchanged (`LayoutManager → CSpace → CAlgorithm::resizeTarget`, tiled targets to the tiled algo,
[`Algorithm.cpp#L124-L129`](https://github.com/hyprwm/Hyprland/blob/v0.57.0/src/layout/algorithm/Algorithm.cpp#L124-L129)). The difference
is that a plugin *implements* `resizeTarget` instead of the Lua adapter that drops Δ and corner.

| Input | What the plugin's `resizeTarget` receives | Source |
|---|---|---|
| SUPER+right-drag (`hl.dsp.window.resize()` on a mouse bind) | Once per mouse tick (throttled to ~refresh rate): `Δ` = pixel delta since last tick, `corner` = the grabbed corner, picked from the cursor's quadrant of the window (`CORNER_TOPLEFT` etc., bits top 1, bottom 2, left 4, right 8). The tiled window is **not** floated. | [`HL/src/layout/supplementary/DragController.cpp#L473-L476`](https://github.com/hyprwm/Hyprland/blob/v0.57.0/src/layout/supplementary/DragController.cpp#L473-L476), [`#L195-L231`](https://github.com/hyprwm/Hyprland/blob/v0.57.0/src/layout/supplementary/DragController.cpp#L195-L231), [`#L135-L145`](https://github.com/hyprwm/Hyprland/blob/v0.57.0/src/layout/supplementary/DragController.cpp#L135-L145); `INC/src/layout/LayoutManager.hpp:19-22` |
| `hl.dsp.window.resize({ x, y, relative = true })` | Once, `Δ = {x, y}`, `corner = CORNER_NONE` ("Corner none likely means not interactive"). With `relative = false` Δ = requested size − current size. | [`ConfigActions.cpp#L680-L698`](https://github.com/hyprwm/Hyprland/blob/v0.57.0/src/config/shared/actions/ConfigActions.cpp#L680-L698); `ModeAlgorithm.hpp:132` |
| `resize_on_border`, trackpad resize gesture | Same `resizeTarget` path (per #2). | #2 §2 |

- The built-in master layout is the model: it turns `Δ.x / monitor width` into a split-ratio change and uses the corner to pick the
  edge. [`HL/src/layout/algorithm/tiled/master/MasterAlgorithm.cpp#L318-L360`](https://github.com/hyprwm/Hyprland/blob/v0.57.0/src/layout/algorithm/tiled/master/MasterAlgorithm.cpp#L318-L360). hy3 does the same (`HY3/src/Hy3Layout.cpp:462`).
- **Cursor:** readable at any time with `g_pInputManager->getMouseCoordsInternal()` (master and dwindle use it,
  `MasterAlgorithm.cpp#L64`, `#L94`). The drag controller is reachable as `g_layoutManager->dragController()` with `mode()`,
  `target()`, `wasDraggingWindow()`, `draggingTiled()` and `m_events.motion/ended` signals (`INC/src/layout/supplementary/DragController.hpp:17-45`).
- For threecol, SUPER+ALT+HJKL can be either the native `hl.dsp.window.resize({ x = ±N, y = 0, relative = true })` (plugin maps
  pixel Δ to a Divider move) or a layout message such as `hl.dsp.layout("resize l +0.02")`. The layout message lets threecol define
  which Divider moves without guessing from Δ's sign; both work.

## 3. Binds: layout messages and plugin functions

| Fact | Source |
|---|---|
| **`addDispatcher` and `addDispatcherV2` are stubs that return `false`** in 0.57 (also on `main`, checked 2026-11-02 at `2178c743`). `removeDispatcher` too. A plugin cannot add a named dispatcher. | [`HL/src/plugins/PluginAPI.cpp#L198-L208`](https://github.com/hyprwm/Hyprland/blob/v0.57.0/src/plugins/PluginAPI.cpp#L198-L208) |
| Layout messages still work: `hl.dsp.layout("msg")` → `CLayoutManager::layoutMsg` → active workspace of the **focused monitor** → floating algo, then tiled algo's `layoutMsg`. | [`HL/src/config/lua/bindings/LuaBindingsDispatchers.cpp#L230-L232`](https://github.com/hyprwm/Hyprland/blob/v0.57.0/src/config/lua/bindings/LuaBindingsDispatchers.cpp#L230-L232), [`HL/src/layout/LayoutManager.cpp#L91-L104`](https://github.com/hyprwm/Hyprland/blob/v0.57.0/src/layout/LayoutManager.cpp#L91-L104), [`Algorithm.cpp#L114-L118`](https://github.com/hyprwm/Hyprland/blob/v0.57.0/src/layout/algorithm/Algorithm.cpp#L114-L118) |
| `HyprlandAPI::addLuaFunction(handle, "threecol", "reset", fn)` registers a Lua C function as `hl.plugin.threecol.reset`. Only with the Lua config. Removed on unload; re-registered into the fresh Lua state on every reload. | `INC/src/plugins/PluginAPI.hpp:352-366`; [`PluginAPI.cpp#L394-L410`](https://github.com/hyprwm/Hyprland/blob/v0.57.0/src/plugins/PluginAPI.cpp#L394-L410); [`HL/src/config/lua/ConfigManager.cpp#L1144-L1190`](https://github.com/hyprwm/Hyprland/blob/v0.57.0/src/config/lua/ConfigManager.cpp#L1144-L1190), [`#L1334-L1340`](https://github.com/hyprwm/Hyprland/blob/v0.57.0/src/config/lua/ConfigManager.cpp#L1334-L1340) |
| hy3's pattern: each `hl.plugin.hy3.*` function is a *factory* returning a closure, so it is used as `hl.bind("SUPER + H", hl.plugin.hy3.move_focus("l"))`. | `HY3/src/dispatchers.cpp:337-362`, `:952-969`; `HY3/README.md:399-410` |
| `addEvent` lets a plugin publish its own event for `hl.on(…)`; `registerHyprCtlCommand` adds a `hyprctl` command (e.g. for scripts/status). | `INC/src/plugins/PluginAPI.hpp:329-341`, `:368-380` |

Caveat: `hl.plugin.threecol` exists only once the plugin is loaded. On first start the config runs before the plugin loads; Hyprland
reloads the config after loading config-listed plugins, so the second pass sees it (§5). Binds should look the function up at call
time (`function() hl.plugin.threecol.reset() end`) or guard with `if hl.plugin.threecol then … end` like the wiki example
(`WIKI/hyprland-plugins/using-plugins.md:87-102`).

## 4. Config values

| Fact | Source |
|---|---|
| `addConfigValueV2(handle, SP<Config::Values::IValue>)`; the name must start with `plugin:`. Keep the `SP` and call `value()` to read it. The V1 `addConfigValue`/`getConfigValue`/`addConfigKeyword` are stubs (`false`/`nullptr`). | `INC/src/plugins/PluginAPI.hpp:343-350`; [`PluginAPI.cpp#L375-L392`](https://github.com/hyprwm/Hyprland/blob/v0.57.0/src/plugins/PluginAPI.cpp#L375-L392), [`#L175-L187`](https://github.com/hyprwm/Hyprland/blob/v0.57.0/src/plugins/PluginAPI.cpp#L175-L187) |
| Supported value classes: `CIntValue`, `CFloatValue` (optional `min`/`max`), `CBoolValue`, `CStringValue` (optional validator), `CColorValue`, `CVec2Value`, `CCssGapValue`, `CFontWeightValue`, `CGradientValue`. Anything else → "unsupported value type". Each takes a `refresh` bitmask; `REFRESH_LAYOUTS` makes Hyprland recalc all layouts when the value changes. | [`HL/src/config/lua/types/LuaConfigUtils.cpp#L24-L54`](https://github.com/hyprwm/Hyprland/blob/v0.57.0/src/config/lua/types/LuaConfigUtils.cpp#L24-L54); `INC/src/config/values/types/FloatValue.hpp:8-17`; `INC/src/config/supplementary/propRefresher/PropRefresher.hpp:9-20`; [`PropRefresher.cpp#L138-L145`](https://github.com/hyprwm/Hyprland/blob/v0.57.0/src/config/supplementary/propRefresher/PropRefresher.cpp#L138-L145) |
| Example: `makeShared<Config::Values::CFloatValue>("plugin:hyprfocus:fade_opacity", "fade opacity", 0.8F, SFloatValueOptions{.min = 0.F, .max = 1.F})`. | `PLUG/hyprfocus/main.cpp:180` |
| In Lua the name becomes `plugin.threecol.side` (`:` → `.`, `-` → `_`). `hl.config{}` flattens nested tables, so `hl.config({ plugin = { threecol = { side = 0.25, master = 0.5 } } })` sets them. Unknown keys are a config error. `hl.get_config("plugin:threecol:side")` reads one back. | [`ConfigManager.cpp#L1114-L1141`](https://github.com/hyprwm/Hyprland/blob/v0.57.0/src/config/lua/ConfigManager.cpp#L1114-L1141); [`HL/src/config/lua/bindings/LuaBindingsConfigRules.cpp#L943-L982`](https://github.com/hyprwm/Hyprland/blob/v0.57.0/src/config/lua/bindings/LuaBindingsConfigRules.cpp#L943-L982), [`#L995-L1012`](https://github.com/hyprwm/Hyprland/blob/v0.57.0/src/config/lua/bindings/LuaBindingsConfigRules.cpp#L995-L1012); `WIKI/hyprland-plugins/using-plugins.md:87-100` |
| **Per-workspace:** plugin values themselves are global (no per-workspace config value). But workspace rules carry a free-form `layout_opts` table (`table<string, string\|number\|boolean>`, stored as strings) and a `layout` name. The algo reads them with `Config::workspaceRuleMgr()->getWorkspaceRuleFor(workspace)->m_layoutopts`. Master (`orientation`) and scrolling (`direction`) do exactly this. | `/usr/share/hypr/stubs/hl.meta.lua:681-682`; [`LuaBindingsConfigRules.cpp#L676-L715`](https://github.com/hyprwm/Hyprland/blob/v0.57.0/src/config/lua/bindings/LuaBindingsConfigRules.cpp#L676-L715); `INC/src/config/shared/workspace/WorkspaceRule.hpp:41-42`, `WorkspaceRuleManager.hpp:25`; [`MasterAlgorithm.cpp#L937-L944`](https://github.com/hyprwm/Hyprland/blob/v0.57.0/src/layout/algorithm/tiled/master/MasterAlgorithm.cpp#L937-L944) |

So per-workspace defaults would look like:

```lua
hl.config({ general = { layout = "threecol" }, plugin = { threecol = { left = 0.25, stack = 0.25 } } })
hl.workspace_rule({ workspace = "5", layout_opts = { left = 0.2, stack = 0.3 } })
```

`layout_opts` keys are not validated by Hyprland (any string key is accepted), so the plugin must parse and report bad values itself.
How a `layout_opts` change on reload reaches a live workspace (does the rule refresh trigger `recalculate`?) is **unverified**.

## 5. Install and loading

### Manual `.so` from the Lua config

| Fact | Source |
|---|---|
| `hl.plugin.load(path)` just records the path; after the config is parsed `handlePluginLoads()` loads new paths and unloads ones no longer listed, then **reloads the config** if anything changed (so values and `hl.plugin.*` functions are visible on the second pass). | [`LuaBindingsConfigRules.cpp#L547-L564`](https://github.com/hyprwm/Hyprland/blob/v0.57.0/src/config/lua/bindings/LuaBindingsConfigRules.cpp#L547-L564); [`ConfigManager.cpp#L1093-L1104`](https://github.com/hyprwm/Hyprland/blob/v0.57.0/src/config/lua/ConfigManager.cpp#L1093-L1104); [`HL/src/plugins/PluginSystem.cpp#L197-L236`](https://github.com/hyprwm/Hyprland/blob/v0.57.0/src/plugins/PluginSystem.cpp#L197-L236) |
| The path goes to `dlopen` as is, so it should be absolute (wiki: "Path has to be absolute!" for `hyprctl plugin load`). Whether `~` is expanded by `hl.plugin.load` is **unverified**. | [`PluginSystem.cpp#L70-L78`](https://github.com/hyprwm/Hyprland/blob/v0.57.0/src/plugins/PluginSystem.cpp#L70-L78); `WIKI/hyprland-plugins/using-plugins.md:74-79` |
| Loading goes through the permission manager (`PERMISSION_TYPE_PLUGIN`); `hl.permission(…, "plugin", "allow")` exists for this. Here no permission rules are configured. | [`PluginSystem.cpp#L15-L56`](https://github.com/hyprwm/Hyprland/blob/v0.57.0/src/plugins/PluginSystem.cpp#L15-L56); `WIKI/hyprland-plugins/using-plugins.md:30-37` |
| Hyprland itself only rejects a plugin whose `pluginAPIVersion()` ≠ `"0.2"`. Checking the commit hash (`__hyprland_api_get_hash()` vs `__hyprland_api_get_client_hash()`) is done **by the plugin** in `pluginInit` (hy3, csgo-vulkan-fix throw on mismatch). | [`PluginSystem.cpp#L88-L105`](https://github.com/hyprwm/Hyprland/blob/v0.57.0/src/plugins/PluginSystem.cpp#L88-L105); `INC/src/plugins/PluginAPI.hpp:384-400`; `PLUG/csgo-vulkan-fix/main.cpp:163-174` |
| `hyprctl plugin load/unload /abs/path.so` works at runtime too. Unloading a plugin (incl. via a crash) also queues a config reload. | `WIKI/hyprland-plugins/development/getting-started.md:126-140`; [`PluginSystem.cpp#L189`](https://github.com/hyprwm/Hyprland/blob/v0.57.0/src/plugins/PluginSystem.cpp#L189) |
| Build: `g++ -shared -fPIC -std=c++2b main.cpp -o x.so $(pkg-config --cflags pixman-1 libdrm hyprland …)` (use `--no-gnu-unique` with g++), or CMake (hy3). | `PLUG/hyprfocus/Makefile`; `HY3/hyprpm.toml` |

### hyprpm

| Fact | Source |
|---|---|
| Repo root has `hyprpm.toml`: `[repository]` with `name`, `authors` (required) and optional `commit_pins`; then one `[plugin-name]` table with `output` (path to the `.so` from repo root) and `build` (list of shell commands, each run from repo root), optional `description`, `authors`, `since_hyprland` (minimum Hyprland commit count). | `WIKI/hyprland-plugins/development/plugin-guidelines.md:14-54`; [`HL/hyprpm/src/core/Manifest.cpp#L64-L117`](https://github.com/hyprwm/Hyprland/blob/v0.57.0/hyprpm/src/core/Manifest.cpp#L64-L117); `PLUG/hyprpm.toml:60-95` |
| `commit_pins = [["<hyprland commit>", "<plugin commit>"], …]`: hyprpm checks out the plugin commit pinned to the running Hyprland hash; no match → latest. For 0.57.0 the key is `0aa6745f705f2c4b0a5004eb019ef76e5bbdb426` (hy3 has exactly this pin). | `plugin-guidelines.md:56-66`; [`HL/hyprpm/src/core/PluginManager.cpp#L275-L290`](https://github.com/hyprwm/Hyprland/blob/v0.57.0/hyprpm/src/core/PluginManager.cpp#L275-L290); `HY3/hyprpm.toml:54` |
| hyprpm clones Hyprland at the running commit and builds its own headers into `/var/cache/hyprpm/$USER`; it refuses to build if header hash ≠ running hash, and flags an ABI mismatch separately. It loads plugins by sending `/plugin load <path>` over the socket (`hyprpm reload`, usually from `hyprland.start`). | [`PluginManager.cpp#L416-L489`](https://github.com/hyprwm/Hyprland/blob/v0.57.0/hyprpm/src/core/PluginManager.cpp#L416-L489), [`#L1058-L1066`](https://github.com/hyprwm/Hyprland/blob/v0.57.0/hyprpm/src/core/PluginManager.cpp#L1058-L1066); [`HL/hyprpm/src/core/DataState.cpp#L41`](https://github.com/hyprwm/Hyprland/blob/v0.57.0/hyprpm/src/core/DataState.cpp#L41); `WIKI/hyprland-plugins/using-plugins.md:43-63` |
| hyprpm needs `cpio`, `cmake`, `git`, `meson`, `gcc`. `/var/cache/hyprpm` does not exist here yet (hyprpm never used). | `using-plugins.md:39-41`; `ls /var/cache/hyprpm` |
| Plugins loaded by hyprpm are not "config plugins", so `handlePluginLoads` does not unload them; the layout appears whenever the load arrives (`registerTiledAlgo` → `updateWorkspaceLayouts`), until then `general.layout = "threecol"` falls back to dwindle. | `PluginSystem.cpp#L203-L211`; `WorkspaceAlgoMatcher.cpp#L50-L60` |

Either way the `.so` is bound to one Hyprland commit: every Hyprland update means a rebuild (hyprpm automates this with `hyprpm update`).

## 6. State, reloads, and Lua vs plugin

| Topic | Finding | Source |
|---|---|---|
| State file | No Hyprland API for it, and none is needed: the plugin is native code in the compositor process, so `std::filesystem`/`std::ofstream` to e.g. `$XDG_STATE_HOME/threecol/state.json` works. The guidelines say the event loop is single-threaded and threads are only OK if fully detached, "e.g., saving a file". No plugin I read writes a state file (**pattern unverified in practice**). | `WIKI/hyprland-plugins/development/plugin-guidelines.md:88-91` |
| Config reload | The plugin is **not** unloaded on reload (only if its path leaves the `hl.plugin.load` list). Its algo instances are kept unless the workspace's layout name changes. Plugin config values are re-applied; `Event::bus()->m_events.config.preReload` / `.reloaded` are available. The Lua layout, by contrast, has its Lua state rebuilt on every reload and loses `col`/`rank` (#3). | `PluginSystem.cpp#L197-L211`; `WorkspaceAlgoMatcher.cpp#L115-L145`; `INC/src/event/EventBus.hpp:203-206`; [`ConfigManager.cpp#L676-L697`](https://github.com/hyprwm/Hyprland/blob/v0.57.0/src/config/lua/ConfigManager.cpp#L676-L697) |
| Per-workspace Dividers | Free with one instance per workspace, **but** an instance is destroyed with its workspace (e.g. an empty non-persistent workspace you leave). Keep Divider positions in a plugin-global map keyed by workspace name/id (and persist that map). | `HLWorkspace.cpp#L90` |
| Drag-and-drop swap | Native. Drag start on a tiled window: Hyprland floats it → the algo gets `removeTarget` (check `dragController()->mode() == MBIND_MOVE` to keep the Zone instead of leaving a Hole). Drop: `m_wasDraggingWindow = true`, then `changeFloatingMode` → `movedTarget(target)`; the algo reads the cursor and swaps with the window in that Zone. Master's `drop_at_cursor` uses exactly this. No arm bind, no 2 s timer, no `window.update_state` listener. | [`DragController.cpp#L274-L322`](https://github.com/hyprwm/Hyprland/blob/v0.57.0/src/layout/supplementary/DragController.cpp#L274-L322); [`HL/src/layout/space/Space.cpp#L117-L125`](https://github.com/hyprwm/Hyprland/blob/v0.57.0/src/layout/space/Space.cpp#L117-L125); [`MasterAlgorithm.cpp#L58-L66`](https://github.com/hyprwm/Hyprland/blob/v0.57.0/src/layout/algorithm/tiled/master/MasterAlgorithm.cpp#L58-L66) |
| `threecol_active()` | Keeps working with `ws.tiled_layout == "threecol"` (was `"lua:threecol"`). Alternative: a `hl.plugin.threecol.focus(dir)` that itself falls back to native `movefocus` when the workspace isn't threecol, removing the check from the config. | §1 table; `~/.config/hypr/core/keybinds.lua:85-88` |
| SUPER+SHIFT+HJKL | Could become native `hl.dsp.window.move({ direction })`, which calls the plugin's `moveTargetInDirection`. Focus (SUPER+HJKL) is geometric in Hyprland, so it stays a layout message / plugin function. | `ConfigActions.cpp#L1452-L1491`, `#L448-L466` |

What the plugin can't do (or does worse) than the Lua layout:
- No named dispatchers (§3); binds go through `layoutmsg` or `hl.plugin.threecol.*`.
- Editing the layout means rebuild + `hyprctl plugin unload/load` (which also reloads the config), not a file save with auto-reload.
- It is tied to the exact Hyprland commit and must be rebuilt on every update; a crash takes Hyprland's stability with it
  (Hyprland "employs a few tactics to unload plugins that crash", `using-plugins.md:130-134`).
- Internal APIs it uses (`g_pInputManager`, `workspaceRuleMgr`, `dragController`) carry no stability promise
  (`plugin-guidelines.md:73-79`); expect "chase hyprland" fixes like hy3's (`HY3` commits `c35adfa`, `8daf42e`).

What it gains: real resize Δ and corner, native drag-and-drop, typed `plugin:threecol:*` values with min/max, per-workspace
`layout_opts`, state that survives config reload, no 50 ms Lua budget.

## Unverified

- Behaviour of all of the above at runtime (nothing was built or loaded).
- Whether `hl.plugin.load` expands `~`.
- Whether changing a workspace rule's `layout_opts` on reload triggers a `recalculate` of that workspace.
- Writing a state file from a plugin: no existing plugin example read.
