# Can a Hyprland plugin show an interactive settings window, or must it be a separate app?

Research for [#88](https://github.com/PascalBrus/hyprland-layout/issues/88) (map [#87](https://github.com/PascalBrus/hyprland-layout/issues/87)).
Target: Hyprland **0.57.2** as installed here (`/usr/include/hyprland/src/version.h`: tag `v0.57.2`, commit `11cbc6c5`).
The GUI is the **Settings app** (`GLOSSARY.md`).

Source links:
- `INC/…` = `/usr/include/hyprland/…` (package `hyprland 0.57.2-1`, local).
- `HL/…` = `https://github.com/hyprwm/Hyprland/blob/v0.57.2/…`.
- `HT/…` = `/usr/include/hyprtoolkit/…` (package `hyprtoolkit 0.6.0-3`, local).
- `PLUG/…` = `https://github.com/hyprwm/hyprland-plugins/blob/d2427d4d15b36df40468f5a58bbd67cdba19835d/…`.
- Package sizes and dependencies are from `pacman -Qi` / `pacman -Si` on this Arch machine (November 2026).

Everything was read from source, headers and docs. Nothing was built or run.

## Answer in short

- **A plugin can draw it, but nothing gives it widgets.** A plugin gets raw input (`Event::bus()` mouse, key, touch, all
  cancellable) and a GL render pass. hyprbars works this way: it draws with cairo into textures and does its own hit-testing.
  Toggles, sliders, text fields with cursor, selection and IME, scrolling and GIF playback would all have to be written by hand.
  That is thousands of lines and stays tied to the compositor's ABI.
- **New middle path, still raw:** hyprtoolkit 0.6.0 has an `IEmbeddedBackend` meant to run its widgets *inside* Hyprland's GLES3
  context. It was added in August 2026 "for some decorations (e.g. hyprland group bars) and some future things (e.g. overview)". Hyprland
  0.57.2 does not use it yet. A plugin would have to write all the host glue (event loop, clipboard, text input, cursor, outputs,
  render pass, input routing). Popups such as combobox dropdowns do not work embedded.
- **Hyprland's own GUI runs as separate processes.** The compositor spawns `hyprland-dialog`, `hyprland-welcome` and
  `hyprland-update-screen` from **hyprland-guiutils**, which is built on **hyprtoolkit** and replaced the archived Qt/QML
  **hyprland-qtutils**. On Arch, `hyprland` depends on `hyprland-guiutils`, which depends on `hyprtoolkit`, so every Arch Hyprland
  install already has the toolkit.
- **Recommendation: a separate Wayland program.** The best fit on cost is **hyprtoolkit (C++)**: no extra packages on Arch, it has every
  widget we need except GIF playback, and it can ask the compositor to inhibit shortcuts while recording a key combo. The main
  alternatives are **Python + GTK4** (nothing to compile) and **Qt 6/QML** (plays GIFs natively, but its Wayland client lacks the
  shortcuts inhibitor).
- **Key recording needs a shortcuts inhibitor whatever the toolkit.** Hyprland eats a combo that is already bound before any client
  sees it, unless the focused surface holds a `zwp_keyboard_shortcuts_inhibitor_v1`.

## 1. What a plugin can do in-compositor

### Input and drawing

- Input: the 0.57 event bus gives plugins `m_events.input.mouse.{move,button,axis}`, `input.keyboard.key` and `input.touch.*` as
  `Cancellable<…>` events, so a plugin can see and swallow them (`INC/src/event/EventBus.hpp:128-152`). The old
  `registerCallbackDynamic` is `[[deprecated]]` (`INC/src/plugins/PluginAPI.hpp:177`).
- Drawing: the bus has `render.stage` (`EventBus.hpp`, `struct render`), and plugins push pass elements and call
  `g_pHyprOpenGL->renderTexture(...)`.
- hyprbars is the reference: `barDeco.cpp` listens to mouse button, move and touch on the bus (`PLUG/hyprbars/barDeco.cpp:46-52`),
  sets `info.cancelled = true` to swallow clicks (`:247`, `:268`), and draws the title and button icons as textures
  (`:444`, `:608`). That is 729 lines for one bar with buttons and a title. It has no text entry, focus, scrolling or animation.
- hyprexpo, the other overlay example the ticket names, is gone: hyprland-plugins dropped it in commit `3aa21f2e0c`
  ("all: drop unmaintained plugins (#663)", 2026-05-12). Only borders-plus-plus, csgo-vulkan-fix, hyprbars and hyprfocus are left
  (`PLUG/README.md`).

### What hand-drawing means for the Settings app

Every widget would need its own code: drawing, hit-testing, keyboard focus, text editing (cursor, selection, clipboard,
`text-input-v3` IME), a scroll view with clipping, and a GIF decoder with a frame timer. The window would also need its own
placement, since an overlay is not a client window and gets no tiling, move or close handling. A bug in any of this crashes the
whole session, because the plugin runs inside the compositor. The plugin is also rebuilt on every Hyprland update (hyprpm already
does this for threecol). **Rough cost: several thousand lines of C++, more than threecol itself, for a worse result than any
toolkit.**

### hyprtoolkit embedded: the new in-compositor option

- `HT/core/EmbeddedBackend.hpp`: `IEmbeddedBackend::create(SCreationData)` takes host-provided `IEventLoop`, `IClipboard`,
  `ITextInput`, `ICursor`, `IOutputProvider` and `ISessionLockProvider` (interfaces in `HT/core/BackendServices.hpp`). "Creation and
  destruction require the GLES3 EGL context that will be used for rendering to be current", and "Embedded surfaces do not
  support native popups yet. Popup-backed controls, including combobox dropdowns and tooltips, remain closed."
- `HT/window/EmbeddedSurface.hpp`: the host must feed `pointerEnter/Motion/Button/Axis/Leave`, `keyboardEnter/Key/Leave`,
  `imCommit/imApply` and touch, and call `render()` with the target framebuffer bound. The surface emits `frameRequested`,
  `damaged` and `cursorChanged`.
- Origin: hyprtoolkit PR [#117](https://github.com/hyprwm/hyprtoolkit/pull/117), merged 2026-08-19: "glue to be able to run hyprtoolkit
  embedded inside another GLES3 app (e.g. hyprland) … a plan to make some decorations (e.g. hyprland group bars) and some future
  things (e.g. overview) be able to reuse elements of hyprtoolkit … a draft until a corresponding Hyprland MR will be made".
- Hyprland 0.57.2 does not link hyprtoolkit: its `CMakeLists.txt` has no `hyprtoolkit` in `pkg_check_modules`, and no installed
  Hyprland header mentions it. A plugin would be the first host. It would write all the services above, a render pass element and
  input routing from the event bus. This is about a few hundred lines of glue against an API that is a month old and has no
  reference host. GIFs still are not covered (see §3).

**Verdict:** possible, but a separate app gets the same widgets from the same toolkit with none of the glue and none of the
crash risk.

## 2. Precedent: Hyprland's own GUI apps are separate processes

- `HL/src/helpers/AsyncDialogBox.cpp:16,103` checks for `hyprland-dialog` in `PATH` and runs it with `CProcess`.
  `HL/src/managers/WelcomeManager.cpp:24,31` does the same for `hyprland-welcome`. The ANR and version-update screens also use these
  (`ANRManager.cpp`, `VersionKeeperManager.cpp`).
- These live in [hyprland-guiutils](https://github.com/hyprwm/hyprland-guiutils) ("successor to hyprland-qtutils"). It ships
  `hyprland-dialog`, `hyprland-donate-screen`, `hyprland-run`, `hyprland-update-screen` and `hyprland-welcome` (`pacman -Ql`), and
  depends on `hyprtoolkit` (`libhyprtoolkit.so=6`).
- [hyprland-qtutils](https://github.com/hyprwm/hyprland-qtutils) (Qt 6/QML) is archived: "This repo has been superseded by
  hyprland-guiutils".
- `xdg-desktop-portal-hyprland` also depends on `hyprtoolkit` and `qt6-base`.

## 3. Toolkits for a separate Settings app

What the app needs: toggles, sliders, text fields, a scroll view, looping GIFs, key-combo capture (including combos Hyprland already
binds), and an install script.

### Key capture has to beat Hyprland's binds

`HL/src/keybinds/Manager.cpp:475` sets `INHIBITED = !binds:disable_keybind_grabbing && PROTO::shortcutsInhibit->isInhibited()`. A
bind fires only when nothing inhibits it or it carries `BIND_FLAG_DONT_INHIBIT` (`:933`, `INC/src/keybinds/Bind.hpp:28`).
`isInhibited()` is true when the focused surface holds an inhibitor (`HL/src/protocols/ShortcutsInhibit.cpp:70-82`), unless a
`no_shortcuts_inhibit` window rule is set (`WindowRuleApplicator.hpp:108`). So the recorder must request
`zwp_keyboard_shortcuts_inhibitor_v1` while it listens. Otherwise pressing `SUPER+H` runs the bind and the app never sees the key.
A fallback that works with any toolkit (not checked): threecol could register a hyprctl command (`registerHyprCtlCommand`) that
swallows the next `input.keyboard.key` on the event bus and reports it.

### Comparison

Sizes are Arch installed sizes. "Already there" means it is pulled in by `hyprland` or `xdg-desktop-portal-hyprland`.

| Toolkit | Widgets | GIF playback | Shortcut inhibit | Extra install on Arch | Elsewhere |
|---|---|---|---|---|---|
| **hyprtoolkit 0.6 (C++)** | Checkbox, Slider, Textbox, ScrollArea, Combobox, Spinbox, Button, Image, Text, Row/ColumnLayout (`HT/element/`) | **No**: hyprgraphics decodes PNG, AVIF, JPEG, JXL, BMP, SVG and WebP into one cairo surface; there is no GIF and no frames (`/usr/include/hyprgraphics/image/Image.hpp`). Frames must be swapped on a timer (`HT/core/Timer.hpp`, `CImageElement::rebuild()`). | **Yes**: `CWindowBuilder::inhibitShortcuts(bool)`, and `IWindow::m_events.keyboardKey` gives the xkb keysym plus a modifier mask (`HT/window/Window.hpp`, `HT/core/Input.hpp`) | **0 MB** (already there: hyprland → hyprland-guiutils → hyprtoolkit, 1.7 MB). Needs a C++ build with cmake, as threecol does. | Packaged in 36 repositories ([Repology](https://repology.org/project/hyprtoolkit/versions)): Debian sid/14, Ubuntu 27.04, openSUSE TW, nixpkgs unstable, Fedora not listed. Pre-1.0: the soname is `.so.6` at 0.6, so expect a rebuild on toolkit updates. |
| **GTK4, Python (PyGObject)** | Full set (GtkSwitch, GtkScale, GtkEntry, GtkScrolledWindow) | `GtkVideo` "supports autoplay, looping" through GTK's GStreamer backend ([docs](https://docs.gtk.org/gtk4/class.Video.html); Arch `gtk4` links gstreamer). Decoding GIF or WebM needs gst-plugins-good, which is not installed here. The alternative is to step a `GdkPixbufAnimation` on a timer. | **Yes**: `gdk_toplevel_inhibit_system_shortcuts()` ([docs](https://docs.gtk.org/gdk4/method.Toplevel.inhibit_system_shortcuts.html)); `libgtk-4.so` has the protocol | `python-gobject` 1.5 MB + `gtk4` 54 MB (gtk4 is often there already, but nothing in Hyprland pulls it in) | Everywhere. No compile step: the install script copies one `.py` file. |
| **GTK4, C/C++ (gtkmm)** | Same | Same | Same | `gtk4` 54 MB (+ `gtkmm-4.0` 10 MB) and a compile | Everywhere |
| **Qt 6 / QML (C++ or PySide6)** | Full set (Switch, Slider, TextField, ScrollView) | **Yes, natively**: `AnimatedImage` plays "animations stored as images containing a series of frames", such as GIF ([docs](https://doc.qt.io/qt-6/qml-qtquick-animatedimage.html)); `qt6-base` ships `libqgif.so` | **No client support found**: no `zwp_keyboard_shortcuts_inhibit` string in `libQt6WaylandClient.so.6.12.0` or `/usr/lib/qt6`. Bound combos would need the plugin fallback above. | `qt6-base` is already there (xdph); add `qt6-declarative` 135 MB + `qt6-wayland` 5 MB (+ `pyside6` 52 MB for Python) | Everywhere; heavy outside KDE |
| **Web view (WebKitGTK 6 / Tauri / Electron)** | HTML | Yes (`<img>`) | Only through host GTK code | `webkitgtk-6.0` 139 MB (38 MB download); Electron is larger | Everywhere; heaviest by far |

### Notes on the hyprtoolkit route

- GIFs: the existing GIFs (`reference/showcase-gif/`, `docs/threecol-settings.gif`) would be converted at install or build time into
  frame strips (PNG or WebP frames, e.g. with ffmpeg). The app then swaps `CImageBuilder::data()` or `path()` on a `CTimer`. This is
  the one feature that must be written by hand.
- Key capture: `inhibitShortcuts(true)` is documented as "request a keyboard shortcuts inhibitor so the compositor does not fire its
  global keybinds while this window is focused" (`HT/window/Window.hpp`). Combined with the `keyboardKey` signal, this is the
  recording field.
- Install script: threecol is already built with cmake against Hyprland headers, so building `threecol-settings` against
  `hyprtoolkit` in the same script adds no new kind of step on Arch. On distros without hyprtoolkit 0.6, the script would build
  it or stop with a message.

## Open points for later tickets

- Whether we accept hyprtoolkit's pre-1.0 API churn, or prefer GTK4/Python's stability and no-compile install. This choice is for
  the toolkit and install tickets.
- GIF format: should the app ship frame strips (hyprtoolkit), keep GIFs (Qt, GdkPixbufAnimation), or use WebM (GtkVideo +
  gst-plugins-good)?
- Whether a plugin-side "record next key" hyprctl command is worth having as a toolkit-independent fallback. It is unverified.
