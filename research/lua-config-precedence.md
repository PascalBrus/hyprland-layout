# Lua config precedence: duplicate binds, repeated `hl.config`, reading the current setup (#90)

Context: the Settings app (#87) writes **one generated Lua file** (threecol settings, workspace rules with
`layout_opts`, the threecol-relevant binds), `require`d from `hyprland.lua`. It must win over the user's own
settings and binds, and import them on first run. This note says what Hyprland 0.57's Lua config does with repeats,
and what an outside program can read back.

Sources:
- `HL/…` = `https://github.com/hyprwm/Hyprland/blob/acc1aa0bfd4bd575dcb0f76c35ccf12b660a1ab9/…` (tag v0.57.1, the
  running session). The installed binary is now **0.57.2** (`11cbc6c5`); `gh api …/compare/v0.57.1...v0.57.2`
  touches only `src/keybinds/Bind.cpp` (modifier matching), `Manager.cpp` (switch events), `ConfigValues.cpp` (two
  ranges), `LuaWindow.cpp`, `LuaEventHandler.cpp`, `ConfigActions.cpp`: nothing below changes.
- `WIKI/…` = `https://github.com/hyprwm/hyprland-wiki/blob/46d1454c94ba05fd8516b1dfcfd94d0e2d830f58/content/…`
  ("latest git"; the wiki has no 0.57 branch).
- `STUB` = `/usr/share/hypr/stubs/hl.meta.lua` (installed 0.57.2).
- `TC/…` = this repo at `7add775`.
- `LIVE` = [`reference/bind-dispatchers-live/live90.py`](../reference/bind-dispatchers-live/live90.py), run in the
  nested Hyprland 0.57.2 with `build/threecol.so` (a build that has the `binds.*` / `wrap.*` switches):
  **7 of 7 pass** (2026-11-10). No key is pressed, so "both fire" is from source only.

## Answer in short

1. **Binds stack; they never replace.** A second `hl.bind` on the same keys adds a second bind, and on a key press
   **both fire**, in registration order. `hl.unbind("KEYS")` removes every bind registered *so far* whose key
   string matches (case and spaces ignored, but `MOD4` ≠ `SUPER`, and modifier order counts). `hl.unbind("all")`
   clears every bind. The handle `hl.bind` returns has `:remove()` / `:unbind()` for exactly that one bind. So
   **`require` order matters**: the generated file must be required **after** the user's binds, and unbind each key
   before it binds it.
2. **`hl.config` merges per leaf.** Each call walks its table down to the registered keys
   (`plugin.threecol.wrap.focus` is one) and sets only those; the last call to set a leaf wins, untouched leaves
   keep their value. Nested `wrap` / `binds` tables are not replaced as a whole. **`hl.workspace_rule` merges per
   field too**: a second rule with the *same* `workspace` string is merged into the first (each field it sets
   wins, `layout_opts` key by key). Rules with *different* strings that match the same workspace are all merged at
   lookup, in order, later wins.
3. **Reading back:** `hyprctl getoption plugin:threecol:<name>` works for every setting, nested switches too
   (`plugin:threecol:wrap:focus`), and says `set: true/false` (set by the config or not). `hyprctl binds -j` lists
   every bind with modmask, key and flags, but the action only as `HL.Dispatcher(<name>)` (e.g.
   `HL.Dispatcher(layout)`, no message, no direction) or `function: 0x…`. `hyprctl workspacerules -j` lists the
   rules but **not `layout` or `layout_opts`**. **Nothing says which file a value or bind came from.** So importing
   binds and workspace overrides needs the Lua source (or a Lua-side shim), not hyprctl alone.

## 1. Duplicate binds, unbind, `require` order

**`hl.bind` always appends.** `hlBind` parses the keys and options, builds a `CBind` and calls
`Keybinds::mgr()->addBind(…)` (HL/src/config/lua/bindings/LuaBindingsToplevel.cpp:64-232, `addBind` at :229).
`CKeybindManager::addBind` → `CRegistry::add`, which is `m_binds.emplace_back(…)` with no lookup for an existing key
(HL/src/keybinds/Manager.cpp:144-147, HL/src/keybinds/Registry.cpp:8-12). LIVE `dup_binds_kept`: two
`SUPER + F9` binds both show in `hyprctl binds -j`, in the order written.

**Both fire.** On an event, `processEvent` collects every bind in registry order that fully matches
(Manager.cpp:469-501), `resolveChordMatches` drops a full match only when it is a *shorter* sub-chord of another full
match (HL/src/keybinds/MatchResolver.cpp:19-23; `isSubChordOf` returns false for equal chord sizes,
HL/src/keybinds/Bind.cpp:306-308), and every remaining hit is invoked in turn (Manager.cpp:550-569). There is no
"first match wins": consuming only decides whether the key reaches the app (`CONSUMES`, :565). The wiki says the
same: "The keybinds will be executed top to bottom, in the order they were written" (WIKI/configuring/core/binds/_index.md:85-88).

**Unbind.** `hl.unbind(key)` (Toplevel.cpp:370-381):
- `hl.unbind("all")` → `clearBinds()`, every bind.
- otherwise `removeBinds(key)` → `CRegistry::removeByDisplayKey`, which erases every bind whose *display string*
  equals the argument after `normalizeDisplayKey`: spaces dropped, lower-cased (Registry.cpp:18-21, :88-100). So
  `"super+f9"` removes `"SUPER + F9"` (LIVE `unbind_all_before`), but `"MOD4 + F9"` does not, though it is the same
  modmask (LIVE `unbind_other_spelling`), and `"SHIFT + SUPER + H"` would not remove `"SUPER + SHIFT + H"`.
- It only sees binds already registered: a bind after the unbind stays (LIVE `unbind_all_before`, and the wiki:
  "Unbind will remove all occurrences of the key that were registered before it", binds/_index.md:106).
- The wiki says the key is case-sensitive (binds/_index.md:105, :112-113). **That is wrong for 0.57**: the code
  lower-cases both sides, and LIVE removes `SUPER + F9` with `super+f9`.

The handle: `hl.bind` returns an `HL.Keybind` (Toplevel.cpp:229-231); its `remove` / `unbind` remove just that bind, and
`set_enabled` turns it off without removing it (HL/src/config/lua/objects/LuaKeybind.cpp:107-116, STUB:727-731).
LIVE `bind_handle_remove`.

**`require` order.** A `require` runs the module there and then, in the one Lua state, so binds and settings land in
the order the files are executed. Each `require` is its own error scope (an error in one file doesn't stop the
others) but a missing module kills the calling file (WIKI/configuring/core/_index.md:67-88). `reload()` clears every
bind, workspace rule and window rule, resets every config value to its default and re-runs `hyprland.lua` from the
top, re-executing each required module (`package.loaded` is cleared) (HL/src/config/lua/ConfigManager.cpp:609-728:
`workspaceRuleMgr()->clear()` :676, `clearBinds()` :690-691, `reset()` + `resetSetByUser()` :706-708). So the
result after a reload is the same as at startup: whatever the last call in execution order said.

What this means for the generated file:
- Require it **last** in `hyprland.lua` (after the user's bind files), and for each key it owns emit
  `hl.unbind("<key>")` then `hl.bind("<key>", …)`. Its settings, also last, win leaf by leaf.
- `hl.unbind` only matches the same spelling. If the user wrote `MOD4 + H` or another modifier order, the user's
  bind survives and both fire. The app either learns the user's exact strings on import (from the Lua source), or
  emits the unbind in each spelling it finds.
- Untested option without string matching: run code after the whole config has loaded. `hl.on("config.reloaded",
  fn)` fires at the end of every load, the first one included (`config.reloaded.emit()` in `postConfigReload`,
  ConfigManager.cpp:824; event name at HL/src/config/lua/LuaEventHandler.cpp:142). But Lua has no API that lists
  binds (STUB:910-953), so a shim would have to wrap `hl.bind` before the user's files run to record their handles,
  and then `:remove()` those whose `modmask` + `key` (both readable on the handle, LuaKeybind.cpp:133-140) clash.
  That puts a `require` at the *top* too. Not tried here.
- `hl.config({ plugin = { threecol = … } })` needs the plugin's values registered, i.e. after `hl.plugin.load` (or
  hyprpm's load). Before that, each key is an "unknown config key" error (`hlConfig`, below), at least at first
  launch. Another reason to require the file late.

## 2. Repeated `hl.config` and `hl.workspace_rule`

**`hl.config` sets leaves, never whole tables.** `hlConfig` walks the table recursively, building a dotted key
(`plugin.threecol.wrap.focus`). At each step, if that key is a registered value it is parsed and set; only if it is
*not* registered and is a table does it recurse (HL/src/config/lua/bindings/LuaBindingsConfigRules.cpp:943-990,
the test at :972). Nothing is reset first, so every key the call doesn't name keeps its value, and the last set wins.
Each threecol switch is its own flat value (`plugin:threecol:wrap:focus` etc., TC/src/plugin/Config.cpp:107-124),
registered as `plugin.threecol.wrap.focus` (`registerPluginValue` → `luaConfigValueName`, which turns `:` into `.`
and `-` into `_`, ConfigManager.cpp:1114-1137). So `wrap = { focus = false }` in one call and
`wrap = { swap = false }` in another leave both false and `wrap.move` at its default. LIVE `config_merges_leaves`
(three calls; `left_width` 0.3 then 0.35 reads 0.35; unnamed switches read their default with `set: false`).

A runtime `hl.config` (from `hyprctl dispatch` / `eval`) changes the live value and schedules a refresh
(`isDynamicParse`, :981-982), but the next reload resets every value and re-runs the files (above), so only what
is in the files survives a reload.

**`hl.workspace_rule` merges rules with the same string.** `hlWorkspaceRule` builds a `CWorkspaceRule`, then
`workspaceRuleMgr()->replaceOrAdd(…)` (LuaBindingsConfigRules.cpp:634-741, call at :735). `replaceOrAdd` finds the
first *enabled* rule with the identical `m_workspaceString` and `mergeLeft`s the new one into it; only if none
exists is it added (HL/src/config/shared/workspace/WorkspaceRuleManager.cpp:26-33). `mergeLeft` copies each field the
new rule set, and `layout_opts` key by key (HL/src/config/shared/workspace/WorkspaceRule.cpp:5-43, `m_layoutopts`
at :36-40). So `{ workspace = "2", layout_opts = { left_width = 0.4 } }` then
`{ workspace = "2", layout_opts = { left_width = 0.35, right_width = 0.15 } }` gives one rule with 0.35 / 0.15.
LIVE `ws_rule_merges`: same tile positions as one rule with both, and `hyprctl workspacerules -j` lists one rule
for `"2"`.

The string must match exactly: `"2"` and `"name:2"` or `"r[2]"` are separate rules. They still all apply:
`getWorkspaceRuleFor` merges every enabled rule whose selector matches, in order, later wins
(WorkspaceRuleManager.cpp:35-53), and that merged rule is what threecol reads `layout_opts` from
(TC/src/plugin/ThreecolAlgorithm.cpp:98, :190-191). Either way the generated file's rule wins if it comes last.
One catch: because of the merge, the handle a second `hl.workspace_rule` returns is the *first* rule, so its
`set_enabled(false)` turns off the user's fields too.

## 3. Reading the current setup from outside

**Settings: `hyprctl getoption`.** `dispatchGetOption` → `getConfigValue(name)`, which tries the name as given, then
`luaConfigValueName(name)` (HL/src/ipc/s1/Commands.cpp:1717-1800, ConfigManager.cpp:1004-1025). So both
`plugin:threecol:wrap:focus` and `plugin.threecol.wrap.focus` answer, for every nested switch; `plugin:threecol:wrap`
itself is "no such option" (LIVE `getoption_names`). `-j` gives e.g.
`{"option": "plugin:threecol:left_width", "float": 0.250000, "set": false }`. `set` is whether the config set it
(`setByUser`), not where. Notes: a bool is `"bool": true`, `left_rows` / `right_rows` are `int` (`"unlimited"` maps to
an int, TC/src/plugin/Config.cpp:68-71), `fill_first` is `str`; a `Config::STRING` is not JSON-escaped (Commands.cpp:1785-1786).
In the author's live session (threecol loaded by hyprpm from an older build), `wrap:focus` and `binds:focus` say
"no such option": the app must cope with values the loaded build doesn't have.

How the plugin reads them: each setting is an `SP<C…Value>` registered with `HyprlandAPI::addConfigValueV2` and read
with `->value()` when needed; the switches when a dispatcher runs, so a change needs no relayout
(TC/src/plugin/Config.cpp:21-54, :89-179). There is no nested `wrap` table on the plugin side, only nine flat bools.

**Binds: `hyprctl binds -j`.** One entry per registered bind, registry order (duplicates included): flags,
`modmask`, `submap`, `key`, `keycode`, `catch_all`, `description`, `dispatcher`, `arg` (Commands.cpp:1199-1257).
But in Lua, `dispatcher` is `luaL_tolstring` of the action (Toplevel.cpp:76, :94): for an `hl.dsp.*` object that is
`HL.Dispatcher(<name>)` (HL/src/config/lua/bindings/LuaBindingsDispatcherUtils.cpp:33-43), for a function
`function: 0x…`, and `arg` is the Lua registry id of the callback. The live session shows
`64 M HL.Dispatcher(layout) 66` and `68 L HL.Dispatcher(focus) 98`: no layout message, no direction, and
`focus({ direction })` and `focus({ workspace })` look the same. Good for **key clashes** (modmask + key + submap
are exact), not enough to **import** what a bind does.

**Workspace rules: `hyprctl workspacerules -j`.** Lists `workspaceString`, `enabled`, `monitor`, `default`,
`persistent`, gaps, border, rounding, decorate, shadow, `defaultName`, `onCreatedEmpty`; **no `layout`, no
`layout_opts`** (`getWorkspaceRuleData`, Commands.cpp:640-690). The Lua `HL.WorkspaceRule` has only
`is_enabled` / `set_enabled` (STUB:905-908). No IPC returns a rule's `layout_opts`; LIVE `ws_rule_merges` checks
it isn't in the JSON.

**`hyprctl eval` / `repl`.** Runs Lua in the live config state (Commands.cpp:1316-1330, `eval` at
ConfigManager.cpp:852-915); `repl` returns what the chunk returns, so `hyprctl repl 'return hl.get_config(…)'`
reads a value. It can't list binds or rules either, and it clears the config error list (`m_errors.clear()`,
:856). Not run here: the agent's sandbox refused a command containing `eval`.

**Which file?** Nowhere. A bind's metadata has no source (Toplevel.cpp:91-98), a config value only a `setByUser`
flag (Commands.cpp:1753-1780), a workspace rule none. The only file:line Hyprland keeps is in *error* messages
(`getSourceInfo`, HL/src/config/lua/bindings/LuaBindingsInternal.cpp:442-453, used by `hlConfig` and
`hlWorkspaceRule`), shown by `hyprctl configerrors`. Hyprland does keep the list of files it loaded
(`m_configPaths`, filled as `require` runs, ConfigManager.cpp:70-78, :620-622) but only to watch them for
auto-reload (HL/src/config/shared/inotify/ConfigWatcher.cpp:32-35); no hyprctl command prints it.

What this means for import:
- Settings: `hyprctl getoption plugin:threecol:<name> -j` for each name; `set: true` tells "the user set it"
  from "default". Enough.
- Binds and workspace overrides: read the user's Lua files (`hyprland.lua` and what it requires) for the
  `hl.bind(...)` calls with threecol-relevant dispatchers and the `hl.workspace_rule` calls with `layout_opts`.
  `hyprctl binds -j` cross-checks which binds are live and catches clashes.
- Side effect to plan for: Hyprland watches every loaded file, so writing the generated file **reloads the config
  by itself** unless `misc:disable_autoreload` is set (ConfigManager.cpp:419-422, ConfigWatcher.cpp:32-35).
