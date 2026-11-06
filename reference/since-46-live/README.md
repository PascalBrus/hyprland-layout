# Live checks since #46

The harness for the checklist in #66 (built in #67). One function in [`checks.py`](checks.py) per checklist item, named by its id (`g1`, `r1` … `l3`). Each one sets up and cleans up its own windows, settings and state file, and prints one line:

```
PASS r1
FAIL c9: want 'centre_master needs equal side widths, …', notice lines […]
```

Copy that line into #66 as the note after the item.

Everything except G1 runs in a **nested Hyprland** with its own config and `XDG_STATE_HOME`, modelled on [`../side-zones-live/`](../side-zones-live/) (#52). `nest.py` refuses to drive the outer session's instance signature.

## Files

| File | What it is |
|---|---|
| `checks.py` | The checks, and the runner |
| `nest.py` | Drives the nested instance through `hyprctl` (the #52 one, extended) |
| `mouse.py` | Mouse input for the (mouse) checks: extends [`../uinput.py`](../uinput.py) with the safety rules below |
| `launch.sh` | Starts the nested Hyprland |
| `nest/hyprland.lua` | Its config: output 2560x720, gaps 5, borders 2, animations off, `examples/binds.lua` recorded in `BINDS`, ALT + mouse for drag and Drag resize, and `nest/extra.lua` (written by each check) |

## Set up

1. Copy this folder to a working directory and fill in the placeholders. `<HARNESS>` is that directory, `<REPO>` this checkout:

   ```sh
   H=~/tmp/since-46; R=~/Projects/work/hyprland-layout
   cp -r "$R/reference/since-46-live" "$H"
   sed -i "s|<HARNESS>|$H|g; s|<REPO>|$R|g" "$H/launch.sh" "$H/nest/hyprland.lua" "$H/nest.py"
   cd "$H"
   ```

2. Run G1 first. It builds the commit under test (`BUILD_REF` in `checks.py`, `7041cc8`) from a clean `git archive` into `build/`, runs the unit suite, and leaves `build/threecol.so` for the nested instance to load:

   ```sh
   python3 -I checks.py g1
   ```

3. Launch the nested Hyprland silently on the outer workspace 9, so nothing on the author's workspaces moves:

   ```sh
   hyprctl dispatch "hl.dsp.exec_cmd('$H/launch.sh', { workspace = '9 silent' })"
   ```

   `checks.py` targets the instance whose signature is in `sig.txt`, or else the newest live one in `hyprctl instances` that isn't the outer session. It refuses to run without `HYPRLAND_INSTANCE_SIGNATURE`, as it then can't tell the outer session apart. Its log is `nest/log.txt`.

The nested output may not be exactly 2560x720: it follows the window it runs in (and is the outer monitor's size when fullscreen). The checks measure in fractions of the output, so this doesn't matter.

## Run

```sh
python3 -I checks.py r1 f3 c9    # some checks
python3 -I checks.py all         # every check that needs no mouse, except G1 (it rebuilds the loaded .so)
```

Test windows are `ghostty`, opened inside the nested instance. Layout messages are sent with `hl.dsp.layout(…)`, and the binds from `examples/binds.lua` are run by calling the recorded action (`BINDS["SUPER + R"]`), so no key is ever pressed.

`SINCE46_BASE` puts plugin settings in front of every check's own, e.g. `SINCE46_BASE='right_rows = "unlimited"'`. It is empty by default, as #66 asks for the default settings. It exists to get past a default that is broken, so the other checks still say something; note it in #66 if you use it.

## (mouse) checks

**D1–D3, C1–C3, C6, C7 and L1–L3** move the author's **real** pointer through a uinput device. Before the first one, stop and ask the author; they keep their hands off until you say it's done. The runner refuses them without `--mouse`:

```sh
python3 -I checks.py --mouse d1 d2 d3 c1 c2 c3 c6 c7 l1 l2 l3
python3 -I checks.py --mouse all  # everything
```

Each mouse check first moves the nested window to the outer workspace 9 (which must hold nothing else), shows that workspace and makes the window fullscreen (`mouse.fullscreen()`). Then `mouse.Mouse` enforces the rules from #66:

- It presses **ALT** only. The uinput device has just ALT and the two mouse buttons, so it cannot send SUPER, let alone SUPER+ALT+L (which locks the author's screen). The nested config binds window drag to ALT + left button and Drag resize to ALT + right button.
- Before every key or button press, every pointer warp and every drag step, it checks that the outer workspace 9 is active, the nested window is on it, and the pointer is inside that window. If not, it releases every button and key and raises `Unsafe`. The runner then prints `FAIL <id>: stopped for safety: …` and stops the whole run.
- On start, it puts the pointer on the nested output's centre and checks the nested instance sees it there, so the coordinate mapping (`mouse.to_outer`) is right before any button goes down.

The only things sent to the outer session are the launch above, the move/focus/fullscreen in `mouse.fullscreen()`, and pointer warps.

**The drop indicator** (D1–D3, L3) is checked from screenshots of the nested output (`grim` through its own Wayland socket, saved in `shots/`). The nested config makes the active border solid magenta, and the indicator fills its box with that colour at 0.2 alpha, so a leftover indicator shows as a large area that turned magenta compared with a screenshot from before the drag. D1–D3 also check that a screenshot taken mid-drag does show it, so a broken detector fails rather than passes. `grim` only works while the nested window is on screen, i.e. during the mouse checks.

## Notes from building it

A shake-out run of the non-mouse checks (2026-11-06, Hyprland 0.57.1, `7041cc8`) found that with no `right_rows` set, `hyprctl getoption plugin:threecol:right_rows` reads `int: 0` and the Right zone is missing: T3 and T4 float. With `right_rows = "unlimited"` set, it reads `-9223372036854775808` and the Stack is back. So the registered default of the "unlimited" sentinel (`Config.cpp`, #43) doesn't seem to take. On the default settings almost every check fails for this one reason. With `SINCE46_BASE='right_rows = "unlimited"'` every non-mouse check passed. Nothing was ticked in #66.
