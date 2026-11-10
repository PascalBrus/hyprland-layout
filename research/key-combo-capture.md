# How can the Settings app record a key combo that Hyprland already binds?

Research for [#89](https://github.com/PascalBrus/hyprland-layout/issues/89) (map
[#87](https://github.com/PascalBrus/hyprland-layout/issues/87)). The user clicks a bind field in the **Settings app** and presses,
say, SUPER+ALT+L. Hyprland's own bind on that combo fires and the app never sees the keys. The question is how to get the whole
combo to the app, named the way `hl.bind` expects it. Target: Hyprland **0.57.1** (running here: `acc1aa0b`, Arch package).

Source links:
- `HL/…` = `https://github.com/hyprwm/Hyprland/blob/acc1aa0bfd4bd575dcb0f76c35ccf12b660a1ab9/…` (tag v0.57.1).
- `WIKI/…` = `https://github.com/hyprwm/hyprland-wiki/blob/46d1454c94ba05fd8516b1dfcfd94d0e2d830f58/content/configuring/core/binds/…`.
- `PROTO` = `/usr/share/wayland-protocols/unstable/keyboard-shortcuts-inhibit/keyboard-shortcuts-inhibit-unstable-v1.xml`
  (wayland-protocols 1.50).
- `STUBS` = `/usr/share/hypr/stubs/hl.meta.lua`; `GDK` = `/usr/include/gtk-4.0/gdk/` (gtk4 4.24.1); `XKB` =
  `/usr/include/xkbcommon/xkbcommon.h`.

Everything is read from source and headers. Nothing was built, loaded or run live.

## Answer in short

- **Use the keyboard-shortcuts-inhibit protocol from the app, and only while a bind field is recording.** Hyprland 0.57 supports
  `zwp_keyboard_shortcuts_inhibit_manager_v1`. No permission or config is needed to use it. While the app's surface has keyboard
  focus and holds an inhibitor, Hyprland runs **no** key binds, mouse binds or touchpad gestures, and passes every key to the
  app. The one exception is a bind with the `dont_inhibit` flag. In GTK 4 this is one call:
  `gdk_toplevel_inhibit_system_shortcuts(toplevel, event)`. When the field is done, `gdk_toplevel_restore_system_shortcuts`
  undoes it. If the app dies, the inhibitor goes with it.
- **What can't be captured this way:** binds flagged `dont_inhibit`, everything when the user set
  `binds.disable_keybind_grabbing = true` or a `no_shortcuts_inhibit` window rule matches the app, VT switching
  (`XF86Switch_VT_n`), and hotkeys registered by other clients through Hyprland's hotkey protocols (`vicinae_hotkey_manager_v1`,
  `xx_hotkey_manager_v1`), which ignore the inhibitor. The app can tell the user about all of these. The field can show "this
  combo is reserved" for a `dont_inhibit` bind, because `hyprctl binds -j` lists each bind's flags.
- **A temporary submap works as a fallback, but it's worse.** In a submap that has no catch-all, the global binds don't run and
  keys go to the focused app. The submap must be registered in the config (`hl.define_submap`), though, and Hyprland's own
  warning applies: if the app crashes, the user is left in the submap. `submap_universal` binds still fire.
- **A plugin can catch every key** by listening to `Event::bus()->m_events.input.keyboard.key`. That event fires before binds
  are matched and can be cancelled. The cost is a second channel to hand the combo to the app, and it's tied to the plugin's
  ABI. The old `"keyPress"` callback is gone: `registerCallbackDynamic` is deprecated and does nothing. This only makes sense if
  the Settings app runs inside the plugin, which it doesn't.
- **Naming:** a combo is `"MODS + key"`, joined by `+`, with spaces trimmed. The modifiers are `SUPER` (or `WIN`, `LOGO`,
  `MOD4`, `META`), `SHIFT`, `CTRL` (or `CONTROL`), `ALT` (or `MOD1`), `CAPS`, `MOD2`, `MOD3` and `MOD5`. The key is an xkb
  keysym name, matched case-insensitively, or `code:<n>`, `mouse:<n>` (n ≥ 272), `mouse_up`, `mouse_down`, `mouse_left` or
  `mouse_right`. By default Hyprland matches a key by its keysym **on the first layout with no modifiers applied**. So
  SUPER+SHIFT+1 is `"SUPER + SHIFT + 1"`, not `exclam`. The app has to name keys the same way (see [Naming](#naming-the-combo)).

## 1. keyboard-shortcuts-inhibit (from a separate app)

**The protocol.** A client calls `inhibit_shortcuts(surface, seat)`. The compositor then "ignore[s] its own keyboard shortcuts
when the associated surface has keyboard focus". The compositor "is however under no obligation to disable all of its
shortcuts" and may keep an escape combo. The inhibitor stops mattering when the surface loses focus, unmaps or is destroyed
(`PROTO`, interface `zwp_keyboard_shortcuts_inhibitor_v1`). Calling it a second time on the same surface is a protocol error
(`already_inhibited`).

**Hyprland 0.57.1:**
- The global is always advertised, at version 1 (`HL/src/managers/ProtocolManager.cpp:184`).
- Hyprland sends `active` as soon as the inhibitor is created, whether or not the surface has focus ("let's make the app believe
  it's always active", `HL/src/protocols/ShortcutsInhibit.cpp:14-16`). It never sends `inactive`. It also has no escape combo of
  its own.
- `isInhibited()` is true only when the **focused surface** has an inhibitor. A matching `no_shortcuts_inhibit` window rule
  turns it off (`HL/src/protocols/ShortcutsInhibit.cpp:70-86`). The rule is listed in
  `HL/src/desktop/rule/windowRule/WindowRuleEffectContainer.cpp:58`.
- Every bind goes through `canInvokeNow`, which ends with
  `return *PDISABLEINHIBIT || !PROTO::shortcutsInhibit->isInhibited() || bind->hasFlag(BIND_FLAG_DONT_INHIBIT);`
  (`HL/src/keybinds/Manager.cpp:900-909`). `processEvent` calls it for keys, mouse buttons, scrolling and switches alike
  (`HL/src/keybinds/Manager.cpp:447-470`). So mouse binds such as `SUPER + mouse:272` and `submap_universal` binds are
  inhibited too. Touchpad gestures are inhibited unless they set `disable_inhibit` (`HL/src/managers/input/trackpad/TrackpadGestures.cpp:157,225`).
- `binds:disable_keybind_grabbing` (default `false`) switches the protocol off entirely: "apps that request keybinds to be
  disabled will not be able to do so" (`HL/src/config/values/ConfigValues.cpp:638`). In Lua it's
  `binds.disable_keybind_grabbing` (`STUBS:49`).
- The `dont_inhibit` bind option "Bypasses the app's requests to inhibit keybinds" (`WIKI/flags.md:36`, `STUBS:507`,
  `HL/src/config/lua/bindings/LuaBindingsToplevel.cpp:137,149`). `hyprctl binds` reports the flag as `dont_inhibit`
  (`HL/src/ipc/s1/Commands.cpp:1176`).
- Nothing in the permission system gates it. The dynamic permission types are only screencopy, plugin, keyboard (a new input
  device), cursor position and input capture (`HL/src/managers/permissions/DynamicPermissionManager.hpp:17-22`).

**These always run, inhibitor or not:**
- The VT switch keys, plus Escape while in kill mode: `handleInternalKeybinds` runs before any bind is matched
  (`HL/src/keybinds/Manager.cpp:214,886-898`).
- Hotkeys that other clients registered through `vicinae_hotkey_manager_v1` / `xx_hotkey_manager_v1`
  (`HL/src/managers/ProtocolManager.cpp:217-218`). `CHotkeyManager::onKey` never checks the inhibitor
  (`HL/src/hotkeys/Manager.cpp:106-128`), and a consumed hotkey is not passed to the app (`HL/src/managers/input/InputManager.cpp:1721-1722`).

**In the app (GTK 4):**
- `gdk_toplevel_inhibit_system_shortcuts(GdkToplevel*, GdkEvent*)` and `gdk_toplevel_restore_system_shortcuts`
  (`GDK/gdktoplevel.h:202-206`). There is also a `shortcuts-inhibited` property to watch (`GDK/gdktoplevel.h:248`). Pass the
  click event that started the recording.
- Inhibit on the field's click. Restore on the first complete combo, on Escape-to-cancel or when focus leaves the field. That
  way the user's binds work again as soon as recording ends, even though the window keeps focus.
- The inhibitor belongs to one surface. The field must sit in the toplevel that holds it. A dialog is its own toplevel, and a
  popover may get its own keyboard focus (not checked).
- GTK's own keyboard handling (Tab focus moves, Alt mnemonics) still runs. The recording field needs a capture-phase
  `GtkEventControllerKey` that claims every key while recording.
- Qt: no Qt 6.12 library on this machine contains `keyboard_shortcuts_inhibit` (`grep` over `/usr/lib/libQt6*.so.6`). A Qt app
  would have to bind the protocol itself (not checked further).

## 2. A temporary submap (from the app, needs config)

- Binds match only in their own submap. In any other submap, only `submap_universal` binds apply (`WIKI/submaps.md`). With no
  catch-all in the submap, a key that matches nothing goes to the focused window (`aggregate.passEvent = !claimed`,
  `HL/src/keybinds/Manager.cpp:577`).
- The app could run `hyprctl dispatch 'hl.dsp.submap("threecol_record")'`, record, then send `"reset"`. But `setSubmap` refuses
  a submap that has no binds: "Cannot set submap …, submap doesn't exist (wasn't registered!)"
  (`HL/src/config/shared/actions/ConfigActions.cpp:1740-1756`). The generated file would have to define it with at least one bind,
  for example `escape` → reset.
- Downsides compared with the inhibitor:
  - The submap is global, not tied to the app's focus, so it's still on after a click elsewhere.
  - A crashed app leaves the user stuck. The wiki warns about this, and the only way out is `hyprctl dispatch 'hl.dsp.submap("reset")'`
    (`WIKI/submaps.md`).
  - `submap_universal` binds still fire.
  - The combo that would leave the submap (e.g. Escape) can't be recorded.
  - It needs the generated config to be loaded first.

## 3. Plugin-side key hook (plugin only)

- `CInputManager::onKeyboardKey` emits `Event::bus()->m_events.input.keyboard.key` (a `Cancellable<IKeyboard::SKeyEvent>`,
  `HL/src/event/EventBus.hpp:135`) **before** hotkeys, binds and the client. If a listener cancels it, the key goes no further
  (`HL/src/managers/input/InputManager.cpp:1711-1714`). threecol already listens on the bus the same way
  (`src/plugin/DropIndicator.cpp`). Unlike the inhibitor, this catches everything, even `dont_inhibit` binds and the hotkey
  protocols.
- The plugin gets the evdev keycode and has to work out the rest itself. `g_pInputManager->getModsFromAllKBs()` gives the
  modifiers (`HL/src/managers/input/InputManager.hpp:213`), and xkb turns the keycode into a name.
- The old `"keyPress"` callback no longer exists. `HyprlandAPI::registerCallbackDynamic` is marked "Deprecated: doesn't do
  anything anymore, use Event::bus()" (`HL/src/plugins/PluginAPI.hpp:168-177`).
- Lua sees the same event: `hl.on("input.keyboard.key", fn)` gets `(keycode + 8, timeMs, state)` but can't cancel it, and it
  carries no modifier state (`HL/src/config/lua/LuaEventHandler.cpp:174-180`). So Lua alone can't stop the bind.
- For the app, this is the wrong shape. The keys arrive in the compositor and not in the app, so threecol would need:
  - a "start recording" entry point (`hl.plugin.threecol.*`, a `hl.dsp.layout` message or an IPC event);
  - a way to hand the combo back;
  - a guard so a crashed app can't leave the keyboard swallowed.

  It also breaks when the plugin isn't loaded or has an ABI mismatch, which is exactly when the user may open the Settings app.
  Keep it only as a fallback, for capturing `dont_inhibit` combos.

## 4. Other routes, briefly

- **The hotkey protocols** (`vicinae_hotkey_manager_v1`, `xx_hotkey_manager_v1`) register one known keysym+mods. They can't
  record an unknown combo.
- **Turn the bind off and on again:** `HL.Keybind:set_enabled` exists (`STUBS:728-731`), but only from Lua, for one bind at a
  time. That doesn't scale to "whatever the user presses".
- **Typed mode** (already in the map): the user types `SUPER + ALT + L`. This always works, and it's the answer for combos none
  of the above can capture.

## Naming the combo

How `hl.bind` reads a key string:
- `parseKeyString` splits the string on `+`, trims each part, and rejects an empty or repeated part
  (`HL/src/config/lua/bindings/LuaBindingsToplevel.cpp:34-60`).
- The modifier names come from a fixed table (`HL/src/keybinds/Resolver.cpp:17-21`). Left and right modifier keysyms (`SUPER_L`,
  `CTRL_R`, …) are separate keys whose press order matters (`HL/src/keybinds/Bind.cpp:20-31`, `WIKI/_index.md` "Binding modkeys
  only": `hl.bind("Super_L", …, { release = true })`).
- Any other part is `catchall`, `mouse:…`, `switch:…`, `mouse_up|down|left|right` (`HL/src/keybinds/Key.cpp:8-10`), `code:<n>`,
  or a keysym looked up with `xkb_keysym_from_name(…, XKB_KEYSYM_CASE_INSENSITIVE)` (`HL/src/keybinds/Key.cpp:25-42`). When two
  names differ only by case, that lookup picks the lower-case keysym (`XKB:644-645`), so `L` and `l` are the same bind.
- Mouse binds use Linux button codes (`mouse:272` left, `mouse:273` right) with `{ mouse = true }`
  (`/usr/share/hypr/hyprland.lua:301-302`).
- `hl.unbind` compares the string **case-sensitively** against what `hl.bind` was given (`WIKI/_index.md:105-113`). The app
  should therefore always write one spelling, e.g. `SUPER + ALT + L` as in `examples/binds.lua`.

How a pressed key is matched:
- The key's keysym comes from `m_xkbTranslationState`, unless the device sets `resolve_binds_by_sym`
  (`HL/src/keybinds/Manager.cpp:208`). That state is a fresh `xkb_state_new` for the configured `kb_layout`. It has no modifiers
  and uses the first layout (`HL/src/keybinds/Manager.cpp:827-867`, `WIKI/keyboard-layouts.md:9-14`).
- The app gets the shifted, current-layout keyval in its own key event (SUPER+SHIFT+1 arrives as `exclam`). Name it from the
  hardware keycode instead: `gdk_display_translate_key(display, keycode, 0, 0, &keyval, …)`, i.e. state 0 and group 0
  (`GDK/gdkdisplay.h:124-131`), then `gdk_keyval_name`. GDK keyval names on Wayland are xkb keysym names. On a multi-layout
  setup, group 0 is the first layout only if the app's keymap is the one Hyprland built from `kb_layout` (not checked).
- When only modifiers were pressed and released, the combo is a modifier key on its own (e.g. `Super_L`). The field should
  record it only if the app wants to support release binds. Otherwise it waits for a non-modifier key.

## Recommendation for the spec

1. Recording mode = the inhibitor on the Settings window, turned on by clicking the field and off when the combo is done or
   cancelled.
2. Record on the first key press that is not a modifier. Take the modifiers held at that moment and name the key as described
   above. Mouse buttons and scrolling recorded the same way become `mouse:<n>` or `mouse_up` / `mouse_down`.
3. Before recording, read `hyprctl binds -j`. Warn when `binds.disable_keybind_grabbing` is on, and mark `dont_inhibit` combos
   as reserved. Point the user to typed mode for anything that can't be captured.
4. Skip the submap and plugin routes. A live check on workspace 9 should confirm that the inhibitor really suppresses
   SUPER+ALT+L and `SUPER + mouse:272` while the field is recording.
