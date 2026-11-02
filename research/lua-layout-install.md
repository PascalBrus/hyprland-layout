# How are Lua layouts and Lua config modules shared and installed?

Research for [#3](https://github.com/PascalBrus/hyprland-layout/issues/3). Target: Hyprland 0.57.0 (`hyprctl version`: tag v0.57.0, commit `0aa6745f705f2c4b0a5004eb019ef76e5bbdb426`), using the Lua config.

Source links:
- `HL/…` means `https://github.com/hyprwm/Hyprland/blob/v0.57.0/…`, the exact commit installed here.
- `WIKI/…` means `https://github.com/hyprwm/hyprland-wiki/blob/f2022a9ed3d3becaf8d6aadd3a4deb71ea238e83/…`.
- Local files are named by path and line.

## TL;DR

- **No package manager for Lua layouts.** hyprpm only builds and loads compiled `.so` plugins. A Lua layout is just a Lua module, and "installing" it means putting it where `require` finds it.
- **How `require` resolves.** `package.path` is `<dir of hyprland.lua>/?.lua;<dir>/?/init.lua` followed by stock Lua 5.5's path, which includes `/usr/share/lua/5.5/?.lua` and `/usr/share/lua/5.5/?/init.lua`. Hyprland also accepts *explicit paths* such as `require("~/.local/share/threecol")` and `require("/abs/path")`, and tries `X`, `X.lua` and `X/init.lua`. So a clone outside `~/.config/hypr` loads **without a symlink**: use either an explicit-path `require` or one `package.path` line.
- **Binds and options can be bundled.** `hl.bind` works from any required file and returns an `HL.Keybind` handle with `remove`/`set_enabled`. On every reload Hyprland clears all binds and re-runs every module, so `require("threecol").setup({ ... })` with options and an opt-out is a sound pattern. hyprvim already uses it.
- **Order of `register` vs `general.layout` doesn't matter (per the source).** The layout is chosen in a deferred refresh after the whole config has run, and `hl.layout.register` also re-checks workspaces. An unknown or misspelled name falls back to dwindle **silently**. Keeping `require` first is still a sensible convention.
- **Bind order does matter.** Binds run in registration order. The drag-arm bind must be registered before the `SUPER + mouse:272` drag bind (observed). So `setup()` must either run before the user's drag bind, or own both binds.
- **Recommended shape:** a module root with `init.lua`. That one layout supports every option below: clone into `~/.config/hypr/threecol`, an explicit path, `package.path`, a shim, a system package or a rock.

## 1. How `require` resolves in Hyprland's Lua config

| Fact | Source |
|---|---|
| Hyprland links Lua **5.5** (not 5.4). | `HL/CMakeLists.txt#L292` (`pkg_search_module(LUA … lua>=5.5 lua<5.6)`); `ldd /usr/bin/Hyprland` → `liblua.so.5.5` |
| On every Lua state (re)init, `package.path` = `<configDir>/?.lua;<configDir>/?/init.lua;` + the original Lua path. `configDir` = parent dir of the main config file. | `HL/src/config/lua/ConfigManager.cpp#L508-L520` |
| The stock Lua 5.5 path on this machine is `/usr/local/share/lua/5.5/?.lua;/usr/local/share/lua/5.5/?/init.lua;/usr/share/lua/5.5/?.lua;/usr/share/lua/5.5/?/init.lua;/usr/local/lib/lua/5.5/?.lua;…;./?.lua;./?/init.lua`. It can be overridden with the `LUA_PATH_5_5`/`LUA_PATH` env vars (`;;` = default). | `lua5.5 -e 'print(package.path)'` (same liblua); [Lua 5.5 manual, `package.path`](https://www.lua.org/manual/5.5/manual.html#pdf-package.path) |
| `require` is wrapped (`safeLuaRequire`). Runtime errors in a module are recorded as config errors and return `{}`. "Module not found" is a real error that stops the calling file. The original is kept as `__require`. | `HL/src/config/lua/ConfigManager.cpp#L315-L370`, `#L521-L529`; `WIKI/content/configuring/core/_index.md#L67-L94` |
| **Explicit-path requires.** A name starting with `/`, `./`, `../` or `~/` is resolved by Hyprland itself. It tries `X`, `X.lua`, then `X/init.lua`. `~` expands to `$HOME`. Relative paths resolve against the **main config file's dir**, not the requiring file. | `HL/src/config/lua/ConfigManager.cpp#L80-L115`, `#L543-L558`; `HL/src/helpers/MiscFunctions.cpp#L54-L76` |
| Wildcards (`require("./lua/*")`) work only with explicit paths. | `HL/src/config/lua/ConfigManager.cpp#L322-L323`, `#L135-L167`; `WIKI/content/configuring/core/_index.md#L39-L63` |
| Every file loaded through `require` is added to the auto-reload watch list, including files outside `~/.config/hypr`. Symlinks are watched at both the link and its canonical target. `dofile`/`loadfile` are **not** tracked, so edits to them don't trigger auto-reload. | `HL/src/config/lua/ConfigManager.cpp#L535-L571` (hooks `package.searchers[2]` only); `HL/src/config/shared/inotify/ConfigWatcher.cpp#L34`, `#L60-L69` |
| On reload, `package.loaded` is cleared for every non-stdlib module, the Lua state is rebuilt, and the config runs again from the top. Module state does not survive `hyprctl reload`. | `HL/src/config/lua/ConfigManager.cpp#L630-L653`, `#L676-L697` |
| The searcher passes the resolved file path as loader data, so a module can find its own location via `local _, file = ...`. Useful for locating bundled scripts and binaries. | `HL/src/config/lua/ConfigManager.cpp#L557-L558`, `#L561-L568`; [Lua 5.5 manual, `require`](https://www.lua.org/manual/5.5/manual.html#pdf-require) |

**Module identity trap.** `package.loaded` is keyed by the exact string passed to `require`. `require("custom.threecol")` and `require("~/.local/share/threecol")` are two different modules. A second copy would call `hl.layout.register("threecol")` again, which fails with "already registered" (`HL/src/config/lua/layout/LuaLayoutProvider.cpp#L245-L248`), and it would have its own separate state. Today's drag-arm bind lazily calls `require("custom.threecol")` (`~/.config/hypr/core/keybinds.lua:121`). That only works because the name matches `hyprland.lua:10`. A bundled `setup()` should capture `M` in a closure instead of re-requiring by name.

## 2. Does hyprpm (or anything else) handle Lua?

- **hyprpm handles compiled plugins only.** A `hyprpm.toml` plugin needs `build.output` and `build.steps` (`HL/hyprpm/src/core/Manifest.cpp#L40-L58`). hyprpm builds with cmake/make against Hyprland headers and loads `<name>.so` with `/plugin load` (`HL/hyprpm/src/core/PluginManager.cpp#L1017`, `#L1063`). The hyprpm source has no Lua handling at all (`grep -i lua hyprpm/src` finds nothing). The wiki likewise describes plugins as `.so` shared objects (`WIKI/content/hyprland-plugins/using-plugins.md#L19-L65`).
- C++ plugins *can* expose Lua functions under `hl.plugin.*` (`/usr/share/hypr/stubs/hl.meta.lua:1040-1043`; `HL/src/config/lua/ConfigManager.cpp#L269-L278`). That would mean rewriting threecol as a C++ plugin. Not relevant here.
- Distribution channels that do exist for pure-Lua layouts:
  - **LuaRocks**: `hy3` is published as a rock ([luarocks.org/modules/aarobc/hy3](https://luarocks.org/modules/aarobc/hy3)). Its README says Hyprland doesn't search LuaRocks trees, so the user must add `~/.luarocks/share/lua/5.x/?.lua` to `package.path` ([aarobc/hy3-lua README](https://github.com/aarobc/hy3-lua#installation)). Note that it says Lua 5.4, while 0.57 embeds 5.5.
  - **Omarchy's `omarchy plugin add`**: distro-specific ([Sulejman/omarchy-stage](https://github.com/Sulejman/omarchy-stage#install), [kirollosatef/hyprscroll2d](https://github.com/kirollosatef/hyprscroll2d#install-on-omarchy)).
  - **Nix Home Manager modules** ([uhs-robert/hyprvim](https://github.com/uhs-robert/hyprvim#installation)).
  - **A system package** that installs into `/usr/share/lua/5.5/`. That directory is already on Hyprland's path (section 1). I found no example in the wild.

## 3. Install options and trade-offs

These assume the repo exposes the module as `init.lua` at a module root, e.g. `threecol/init.lua` or the repo root.

| # | Method | User's config line(s) | Pros | Cons |
|---|---|---|---|---|
| A | `git clone … ~/.config/hypr/threecol` | `require("threecol")` | Zero path setup (configDir `?/init.lua`). Auto-reload works. `git pull` updates. | Repo inside the config dir. If the config dir is itself a git repo, that means a nested repo or submodule. |
| B | Clone anywhere, then `ln -s` into `~/.config/hypr/` | `require("threecol")` | Repo lives where you like. Auto-reload follows symlinks. Pattern used by [fractal-wm](https://github.com/cgranier/fractal-wm#install-manual-for-now) and [omarchy-plugin-zones](https://github.com/l-kedzielawski/omarchy-plugin-zones#install). | One extra step. The link breaks if the repo moves. |
| C | Clone anywhere, then add a `package.path` line | `package.path = os.getenv("HOME").."/.local/share/hypr-lua/?/init.lua;"..package.path` then `require("threecol")` | No symlink. Standard Lua. Works for many modules in one dir. Pattern used by [hy3](https://github.com/aarobc/hy3-lua#installation). | Must run before the `require`. Every user has to write a path string. |
| D | Clone anywhere, then **explicit path** | `require("~/.local/share/threecol")` | One line, no symlink, no `package.path`. Hyprland-native (`WIKI/…/_index.md#L41`). Auto-reload works. | Hyprland-specific (not plain Lua). Module key is the path string (see the identity trap). Sibling files must be loaded relative to `...` or by explicit path. |
| E | Shim file in the config dir that `loadfile`s the clone | `require("lua/plugins/hyprvim").setup()` ([hyprvim](https://github.com/uhs-robert/hyprvim#manual-install)) | Works on any Lua `require`. | The cloned files are loaded with `loadfile`, so their edits don't trigger auto-reload. More moving parts than D. |
| F | `dofile(path)` ([hyprscroll2d](https://github.com/kirollosatef/hyprscroll2d#install-on-omarchy)) | `dofile(HOME.."/…/init.lua")` | Simple. | Not tracked for auto-reload. Bypasses `package.loaded`. Errors aren't isolated the way `require` isolates them. |
| G | System package (AUR/pacman) into `/usr/share/lua/5.5/threecol/init.lua` | `require("threecol")` | True "just install". Zero config path. | Packaging work. Root install. Tied to the Lua minor version (`5.5`). Not tracked by auto-reload until next require (works, just not watched for edits you shouldn't make anyway). |
| H | LuaRocks rock | `package.path` line + `require("threecol")` | Versioned releases. | The user still needs the `package.path` line (Hyprland doesn't search `~/.luarocks`). A rockspec to maintain. Tiny audience (hy3 shows ~40 downloads). |
| — | hyprpm | n/a | — | Not possible: C++ `.so` only. |

**Is it "just 2 files"?** For a user with no bundled binds, yes: one line `require("threecol")` plus `layout = "lua:threecol"` (or `hl.workspace_rule{ layout = "lua:threecol" }`, the per-workspace style used by hyprscroll2d). Today's setup also needs the HJKL/M/drag binds, and those can live in the module (section 4). So the user-facing surface can shrink to `require("threecol").setup({...})` plus the layout setting.

## 4. Can a module register binds and take options?

Yes:

- `hl.bind(keys, dispatcher|function, opts)` returns an `HL.Keybind` with `remove`, `set_enabled`, `is_enabled` and `unbind` (`/usr/share/hypr/stubs/hl.meta.lua:727-731`, `:912`). `hl.unbind(key)` removes binds by key string (`HL/src/config/lua/bindings/LuaBindingsToplevel.cpp#L370-L382`). `hl.define_submap` exists for submaps (`hl.meta.lua:916`).
- Binds registered inside a required module are ordinary binds. On reload all binds are cleared (`HL/src/config/lua/ConfigManager.cpp#L691-L692`) and every module re-runs (section 1), so `setup()` re-registers cleanly with no duplicates.
- Options are plain Lua: `local M = {}; function M.setup(opts) … end; return M`. Prior art:
  - [hyprvim](https://github.com/uhs-robert/hyprvim#load-hyprvim): `require("hyprvim").setup({ keys = { activate = "ESCAPE" } })`, with `setup()` registering its binds and submaps.
  - [hyprscroll2d](https://github.com/kirollosatef/hyprscroll2d): ships binds in a separate opt-in file (`integration/omarchy.lua`).
  - hy3 and fractal-wm document bind snippets for the user to copy instead.
- **Duplicate keys both fire.** The bind processor collects every matching bind and invokes them in order (`HL/src/keybinds/Manager.cpp#L465-L560`). If the module binds `SUPER+H` and the user's config still binds `SUPER+H` to `focus left`, both run. Bundled binds therefore need an opt-out (`binds = false`), per-group toggles, or overridable keys. The README must tell users to delete their old binds. *Inferred from source, not live-tested.*
- The existing `threecol_active()` fallback (`~/.config/hypr/core/keybinds.lua:84-87`) makes the HJKL binds safe on other layouts. That makes them good candidates for bundling, because they replace rather than fight the user's normal directional binds.

## 5. Ordering constraints

1. **`hl.layout.register` vs `general.layout = "lua:threecol"`: order-independent within one config run, per source.**
   - `hl.config` value changes schedule a *deferred* refresh. `postConfigReload` schedules `REFRESH_ALL` after the whole file has run (`HL/src/config/lua/ConfigManager.cpp#L822`). The refresh runs on the event loop via `doLater` (`HL/src/config/supplementary/propRefresher/PropRefresher.cpp#L30-L43`) and only then calls `updateWorkspaceLayouts()` (`#L138-L139`).
   - `registerTiledAlgo` also calls `updateWorkspaceLayouts()` itself (`HL/src/layout/supplementary/WorkspaceAlgoMatcher.cpp#L50-L58`).
   - Unknown names fall back to `dwindle` without an error (`WorkspaceAlgoMatcher.cpp#L92-L96`, `#L22`).
   - The name is normalised to `lua:<name>`, so the config value **must** include `lua:` (`HL/src/config/lua/layout/LuaLayoutProvider.cpp#L23-L27`). The handoff notes and the hy3 README both say "require before `hl.config`". That is harmless, but the source shows it isn't required. *Not live-tested.* Keep the README advice ("require first") as the safe convention.
2. **Drag-arm bind before the drag bind: required.** Binds are stored in registration order (`HL/src/keybinds/Registry.cpp#L10`) and processed in that order (`HL/src/keybinds/Manager.cpp#L465`). The handoff observed that the `non_consuming` arm bind never fires if it is registered after `SUPER + mouse:272` → `hl.dsp.window.drag()` (`/tmp/threecol-handoff/HANDOFF.md`, `~/.config/hypr/core/keybinds.lua:120-122`). The module can't insert a bind ahead of an existing one, so the options are:
   - call `setup()` before the user's keybinds file (today threecol is required before `core.keybinds`, `hyprland.lua:10,19`), or
   - let `setup({ drag = "SUPER + mouse:272" })` register *both* the arm bind and the `hl.dsp.window.drag()` bind in the right order, and tell the user to remove their own drag bind, or
   - have `setup()` call `hl.unbind("SUPER + mouse:272")` and re-add both. This is plausible from `LuaBindingsToplevel.cpp#L370-L382` but untested, and it would also drop any other bind on that key.
3. `setup()` must run during config load, like any other `hl.bind`. Calling it from a timer or event is possible but would be lost on reload unless it re-runs.

## 6. Conventions seen in existing shared Lua layouts/modules

| Repo | Load method | Binds |
|---|---|---|
| [hyprwm/Hyprland `example/layouts/*.lua`](https://github.com/hyprwm/Hyprland/tree/v0.57.0/example/layouts) | Bare `hl.layout.register` at file top. Not installed by CMake (`HL/CMakeLists.txt#L667-L672` installs only `hyprland.lua` and stubs). | none |
| [aarobc/hy3-lua](https://github.com/aarobc/hy3-lua) | LuaRocks + `package.path`, or path to clone | user copies a snippet |
| [uhs-robert/hyprvim](https://github.com/uhs-robert/hyprvim) | clone to `~/.local/share/…` + shim, Nix HM, AUR planned | bundled, `setup({ keys = … })` |
| [kirollosatef/hyprscroll2d](https://github.com/kirollosatef/hyprscroll2d) | `dofile` from `~/.local/share`, install script, Omarchy plugin | separate opt-in integration file |
| [cgranier/fractal-wm](https://github.com/cgranier/fractal-wm) | symlinks into `~/.config/hypr` | user copies a keys file |
| [l-kedzielawski/omarchy-plugin-zones](https://github.com/l-kedzielawski/omarchy-plugin-zones) | symlink + `require` | user copies binds |
| [Sulejman/omarchy-stage](https://github.com/Sulejman/omarchy-stage) | `omarchy plugin` or `install.sh` that edits `hyprland.lua` | bundled |

Common threads:
- Most repos expose a layout as one `require`-able module.
- Per-workspace trial via `hl.workspace_rule({ workspace = "9", layout = "lua:…" })` is a popular low-risk onboarding step.
- Bundled binds come with a fallback to normal behaviour on other layouts.

## 7. Implications for open decisions

- **#7 (repo structure and install method):**
  - Put the module at `threecol/init.lua` (or the repo root's `init.lua`), and keep internal files loadable relative to `...`. That supports options A, B, C, D and G without code changes.
  - Offer `setup(opts)` with bind groups that can be disabled, plus a `drag` option that registers arm + drag in order.
  - The README needs: the `lua:` prefix, "require/setup before your own SUPER+mouse:272 bind or let setup own it", "remove your old HJKL/M binds", and the silent dwindle fallback as the symptom of a typo.
  - For the author's own live migration, option D (`require("~/Projects/work/hyprland-layout")`) or B lets `~/.config/hypr` keep working with a one-line change.
- **#8 (tile selector):** use the module's loader data (`local _, file = ...`) to compute the module directory and find `scripts/` and the renderer binary. That replaces the hard-coded `$HOME/.config/hypr/custom/scripts`. A separate `require("threecol.tile_selector")`-style optional module works the same way.

## Not verified live

These come from reading the source, not from running them:
- order-independence of `register` vs `general.layout`
- both duplicate-key binds firing
- `hl.unbind` + re-add for the drag key
- explicit-path `require` of a directory resolving to `init.lua`

Each is a quick check on a test workspace.
