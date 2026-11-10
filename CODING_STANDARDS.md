# Coding standards

Rules for reviewing a threecol diff. Each one is a judgement call. The mechanical rules are enforced by checks instead: warnings as errors and the unit suite run in `.githooks/pre-commit`, and `cmake/CheckCoreIsPure.cmake` keeps Hyprland headers out of `src/core/`.

The code layout these rules build on is in [README.md § Building from source](README.md#building-from-source): `src/core/` decides, and `src/plugin/` translates.

## One home

Each layout message grammar, Chain or Row lookup, and neighbour rule lives once in `src/core/`. The plugin calls it.

- A message the core already parses, like `focus l noloop`, is parsed by the plugin through the same core function (`parseFocusMessage`), never by a second hand-written parser. Two parsers drift: in #83, `focus l junk` moved on a tile and did nothing on a floating window.
- A new helper is checked against the ones in `src/core/` (`Cycle.hpp`'s `step`, and `Workspace`'s `next`, `inDirection` and `rowNeighbour`). When it matches one, reuse that or give it a parameter. Don't copy its body.

## Hooks stay cheap

Code in the dispatcher hooks (`src/plugin/DispatcherHooks.cpp`) runs on every keypress on every layout. The guards that send a call back to Hyprland come first: no threecol tile, fullscreen, no direction. Config reads and core calls come after them.

## Docs move with behaviour

A change in behaviour updates every place that describes it, in the same diff:

- the README tables and settings sections
- `examples/*.lua`
- the doc comments on the changed functions in `src/core/*.hpp`

A stale sentence is a finding. Search the old wording across the repo to find them.

## Glossary words

Names, comments and docs use the terms in [GLOSSARY.md](GLOSSARY.md) (Zone, Chain, Cycle, Row, Hole, Stack) and avoid the words listed under each term's _Avoid_.

## Settings that shipped

A setting that exists on `master` keeps working, or the diff explains its replacement in the README. A setting that only exists on a feature branch can change freely. Check with `git show master:src/plugin/Config.cpp`.
