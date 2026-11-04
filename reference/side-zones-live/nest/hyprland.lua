hl.monitor({ output = "", mode = "2560x720", position = "0x0", scale = 1 })
hl.plugin.load("<HARNESS>/build/threecol.so")
hl.config({ general = { layout = "threecol", gaps_in = 5, gaps_out = 5, border_size = 2 },
            misc = { disable_hyprland_logo = true, disable_splash_rendering = true },
            animations = { enabled = false } })
BINDS = {}
local real_bind = hl.bind
hl.bind = function(k, a, o) BINDS[k] = { a = a, o = o }; return real_bind(k, a, o) end
dofile("<REPO>/examples/binds.lua")
hl.bind = real_bind
dofile("<HARNESS>/nest/extra.lua")
