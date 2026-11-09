-- threecol: an example set of binds for the threecol dispatchers. Optional:
-- your Hyprland focus, move and swap binds already work the threecol way on
-- threecol windows. The README's Binds section lists every dispatcher.
--
-- Pick your keys in `keys` below. Set an entry to false to leave it unbound.
-- Paste this after your other binds.
--
-- Each key is cleared with hl.unbind first, because Hyprland runs every bind
-- on a key, not just the last one. So this REPLACES what was on these keys
-- (with the keys below and Hyprland's stock config, SUPER+R opens the
-- launcher).

local keys = {
    resize = "SUPER + ALT", -- + H/J/K/L: narrower/shorter/taller/wider
                            -- (SUPER+ALT+L is a common lock-screen bind)
    reset  = "SUPER + R",

    -- Chain focus without hooks, e.g. on a machine that isn't x86_64. Your
    -- hl.dsp.focus binds already do it where the hooks work.
    -- focus = "SUPER",     -- + H/J/K/L
}

-- Bind `key`, replacing any bind already on it. Skips keys set to false.
local function bind(key, action, opts)
    if not key then
        return
    end
    hl.unbind(key)
    hl.bind(key, action, opts)
end

-- With a key prefix: "SUPER" + "H" → "SUPER + H".
local function with(prefix, key)
    return prefix and (prefix .. " + " .. key)
end

local directions = {
    H = { msg = "l", resize = "narrower" },
    L = { msg = "r", resize = "wider" },
    K = { msg = "u", resize = "taller" },
    J = { msg = "d", resize = "shorter" },
}

for key, d in pairs(directions) do
    -- Step resize; repeats while held.
    bind(with(keys.resize, key), hl.dsp.layout("resize " .. d.resize), { repeating = true })

    -- Chain focus, if `keys.focus` is set. On floating and fullscreen
    -- windows it runs Hyprland's focus.
    bind(with(keys.focus, key), hl.dsp.layout("focus " .. d.msg))
end

bind(keys.reset, hl.dsp.layout("reset"))
