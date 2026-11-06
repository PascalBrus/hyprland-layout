#!/bin/sh
cd <HARNESS>/nest
[ -e extra.lua ] || : > extra.lua
exec env -u HYPRLAND_INSTANCE_SIGNATURE XDG_STATE_HOME=<HARNESS>/nest/state Hyprland --config <HARNESS>/nest/hyprland.lua > <HARNESS>/nest/log.txt 2>&1
