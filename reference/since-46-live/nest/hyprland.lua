hl.monitor({ output = "", mode = "2560x720", position = "0x0", scale = 1 })
hl.plugin.load("<HARNESS>/build/threecol.so")
-- A solid magenta active border: the drop indicator is drawn in it, so the
-- checks can find a leftover indicator in a screenshot.
hl.config({ general = { layout = "threecol", gaps_in = 5, gaps_out = 5, border_size = 2,
                        col = { active_border = "rgb(ff00ff)", inactive_border = "rgb(404040)" } },
            misc = { disable_hyprland_logo = true, disable_splash_rendering = true },
            animations = { enabled = false } })
BINDS = {}
local real_bind = hl.bind
hl.bind = function(k, a, o) BINDS[k] = { a = a, o = o }; return real_bind(k, a, o) end
dofile("<REPO>/examples/binds.lua")
hl.bind = real_bind
-- The outer session owns SUPER + mouse, so the nested one uses ALT + mouse.
hl.unbind("SUPER + mouse:272")
hl.unbind("SUPER + mouse:273")
hl.bind("ALT + mouse:272", hl.dsp.window.drag(),   { mouse = true })
hl.bind("ALT + mouse:273", hl.dsp.window.resize(), { mouse = true })
dofile("<HARNESS>/nest/extra.lua")
