-- threecol: an example set of binds. Optional: the README's Binds table lists
-- every action, so you can bind them your own way instead.
--
-- Pick your keys in `keys` below. Set an entry to false to leave it unbound.
-- Paste this after your other binds.
--
-- Each key is cleared with hl.unbind first, because Hyprland runs every bind
-- on a key, not just the last one. So this REPLACES what was on these keys
-- (with the keys below and Hyprland's stock config: SUPER+M exits Hyprland,
-- SUPER+R opens the launcher, SUPER+J toggles a dwindle split).
--
-- Not here: drag-and-drop and mouse resize. Hyprland's stock window.drag()
-- and window.resize() mouse binds already do them.

local keys = {
    -- threecol actions
    focus          = "SUPER",           -- + H/J/K/L: focus along the Chain
    focusmaster    = "SUPER + M",
    swapwithmaster = "SUPER + SHIFT + M",
    resize         = "SUPER + ALT",     -- + H/J/K/L: narrower/shorter/taller/wider
    reset          = "SUPER + R",       -- SUPER+ALT+L is a common lock-screen bind

    -- Hyprland action, which threecol follows. Set to false if you already
    -- bind window.move.
    move           = "SUPER + SHIFT",   -- + H/J/K/L
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

-- True when the focused window is tiled by threecol (and nothing is fullscreen).
local function threecol_active()
    local win = hl.get_active_window()
    if not win or win.floating or not win.workspace then
        return false
    end
    return win.workspace.tiled_layout == "threecol" and not win.workspace.has_fullscreen
end

local directions = {
    H = { msg = "l", dir = "left",  resize = "narrower" },
    L = { msg = "r", dir = "right", resize = "wider" },
    K = { msg = "u", dir = "up",    resize = "taller" },
    J = { msg = "d", dir = "down",  resize = "shorter" },
}

for key, d in pairs(directions) do
    -- Chain focus. Falls back to Hyprland's focus on floating windows,
    -- fullscreen and other layouts, where the threecol action does nothing.
    bind(with(keys.focus, key), function()
        if threecol_active() then
            hl.dispatch(hl.dsp.layout("focus " .. d.msg))
        else
            hl.dispatch(hl.dsp.focus({ direction = d.dir }))
        end
    end)

    -- Resize by one `step`; repeats while held.
    bind(with(keys.resize, key), hl.dsp.layout("resize " .. d.resize), { repeating = true })

    bind(with(keys.move, key), hl.dsp.window.move({ direction = d.dir }))
end

bind(keys.focusmaster,    hl.dsp.layout("focusmaster"))
bind(keys.swapwithmaster, hl.dsp.layout("swapwithmaster"))
bind(keys.reset,          hl.dsp.layout("reset"))
