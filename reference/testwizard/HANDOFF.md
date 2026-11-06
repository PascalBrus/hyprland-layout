# Handoff: walk the author through the open live tests (#32)

Repo: `~/Projects/work/hyprland-layout`, branch `dev` (in sync with `origin/dev` at `0d65ed5`). Issue: #32.

**Your job:** guide the author, step by step, through the live-test items only a human can run, record the results in the checklist, and file anything that fails. The author does the keypresses and looks; you prepare, explain, check results with `hyprctl` and write them down.

## Read first

- `docs/live-test.md`: the checklist. Open items are `- [ ]`; notes in *italics* say what the agents already checked.
- `reference/LIVE-TEST-RESULTS.md`: both agent passes, the bugs, and the table **"Left for the author"** (the items you're walking through).
- `reference/LIVE-TEST-HANDOFF.md`: safety rules and tools from the first pass (`reference/livetest.py`, `reference/uinput.py`). The safety rules still apply.
- Issues already filed: #37, #38, #39 (bugs), #40, #41 (README), #42, #43 (second pass). Don't refile.

## The 9 open items

Two are known failures, already filed; only re-check them if the author wants to (#37 reload, #38 fullscreen mouse resize). The 7 for the author:

1. `hyprpm add <repo>` then `hyprpm enable threecol`: **`hyprpm` isn't installed** (`which hyprpm` fails). Ask the author whether to install it (likely the `hyprland` package's tooling or a separate package; check before suggesting).
2. `hyprpm update` builds the pinned commit. Pins in `hyprpm.toml`: 0.57.0 → `fe15550`, 0.57.1 → `9fd58e0` (pushed).
3. Start-up with hyprpm: the short dwindle fallback becomes threecol, windows in Fill order. Needs a login with hyprpm loading the plugin. The late-load behaviour was already simulated (passes, see results).
4. Resize keys (SUPER+ALT+H/J/K/L) repeat when held. Bind flags are right (`repeat: true` in `examples/binds.lua`). **The author's own config doesn't load `examples/binds.lua`**, and in their config **SUPER+ALT+L locks the screen** (`~/.config/hypr/core/keybinds.lua:42`). Check which keys their own binds use before asking them to hold anything.
5. SUPER+R doesn't repeat when held. Same caveat.
6. Drop indicator looks right: active-border colour/gradient, ~20% fill, gaps, rounding. Visual; the author judges while dragging (SUPER+left-drag a tiled window).
7. README opens with the Showcase GIF and it plays on GitHub. Needs recording; the shot list is the "Showcase" section at the end of `docs/live-test.md`.

The "no visible jump" item is ticked but its visual half is also the author's; offer it alongside item 6.

## State of the machine (important)

- The **running session is Hyprland 0.57.0**, but **0.57.1 is installed**. The next login runs 0.57.1.
- `build/threecol.so` is now a **0.57.1 build** (clean rebuild, 215/215 tests). The session has the old one loaded, kept as `build/threecol-0.57.0.so`.
- **Don't run `hyprctl reload` in the 0.57.0 session**, and don't let the author run it without warning them: the config re-loads `build/threecol.so`, which 0.57.0 would refuse, so every workspace drops to dwindle. Items 1–3 naturally fit **after the author logs in on 0.57.1**; propose doing those then.
- The author's state file `~/.local/state/threecol/dividers` holds their real entries. Back it up before any test that resizes.
- The author's own windows: workspaces 1–4 (Teams on 4). Test only on empty workspaces (8–10) and kill only windows you started, by pid.
- Terminal is `ghostty` (start with `--gtk-single-instance=false`); no kitty.

## Nested Hyprland harness (if you need to check something without touching the session)

The second pass ran in a nested Hyprland with its own config and `XDG_STATE_HOME`. Copies of the harness are next to this file: `reference/testwizard/nest.py`, `hyprland.lua`, `launch.sh`. Their paths point into the previous session's scratchpad, so edit `S`/paths before use. Key tricks:

- Launch it on the outer session silently: `hyprctl dispatch "hl.dsp.exec_cmd('<launch.sh>', { workspace = '4 silent', no_initial_focus = true })"`.
- Target it with `HYPRLAND_INSTANCE_SIGNATURE=<its signature>` (`hyprctl instances`).
- `hyprctl dispatch "function() ... end"` runs Lua; write results to a file with `io.open`. `hl.notification.get()` reads notification text.
- The config wraps `hl.bind` to record each action from `examples/binds.lua` into `BINDS`, so a bind can be "pressed" with `BINDS['SUPER + R'].a()` without synthetic input.
- Pitfall: CMake doesn't track system headers; after a Hyprland update use `--clean-first`, and rename a loaded `.so` aside before rebuilding over it.

It can't test held keys or anything visual on the real monitor, so it doesn't replace the author for the 7 items.

## When done

Tick items in `docs/live-test.md` with a short *italic* note, update the counts and the "Left for the author" table in `reference/LIVE-TEST-RESULTS.md`, file failures as issues in the style of #39 (Parent #32, Steps / Seen / Expected / Where to look, acceptance criteria), commit to `dev`, and ask before pushing. GitHub had an incident earlier; a failed push may just need a retry.

## Suggested skills

- `dave-wizard`: generate an interactive bash wizard that walks the author through the hyprpm install/update/login steps and the held-key and visual checks. This is the main fit.
- `dave-diagnosing-bugs`: if an item fails and the author wants it understood before filing.
- `dave-pr`: only if the author asks for a PR from `dev`.
