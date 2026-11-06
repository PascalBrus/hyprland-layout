hl.monitor({ output = "", mode = "2560x720", position = "0x0", scale = 1 })
hl.plugin.load("/tmp/claude-1000/-home-pascal-brus-Projects-work-hyprland-layout/0b2b2af0-79db-47d3-b379-76e8b8284135/scratchpad/clone/build/threecol.so")
hl.config({ general = { layout = "threecol", gaps_in = 5, gaps_out = 5, border_size = 2 },
            misc = { disable_hyprland_logo = true, disable_splash_rendering = true },
            animations = { enabled = false } })
-- Record each bind's action so the harness can run it without pressing keys.
BINDS = {}
local real_bind = hl.bind
hl.bind = function(k, a, o) BINDS[k] = { a = a, o = o }; return real_bind(k, a, o) end
dofile("/home/pascal.brus/Projects/work/hyprland-layout/examples/binds.lua")
hl.bind = real_bind
dofile("/tmp/claude-1000/-home-pascal-brus-Projects-work-hyprland-layout/0b2b2af0-79db-47d3-b379-76e8b8284135/scratchpad/nest/extra.lua")
