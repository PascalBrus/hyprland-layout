-- threecol binds: the v1 defaults.
--
-- threecol binds no keys itself. Paste this into your Hyprland Lua config
-- (or save it next to it and `require` it), then change any key you like.
--
-- Hyprland runs every bind on a key, not just the last one. So each key here
-- is cleared with hl.unbind first: this snippet REPLACES whatever was bound
-- to these keys before it (in the stock config: SUPER+M exits Hyprland,
-- SUPER+R opens the launcher, SUPER+J toggles a dwindle split). Paste it after
-- your other binds, and move anything you want to keep to other keys.
--
-- Every threecol action is a layout message, sent with hl.dsp.layout(…).

local mainMod = "SUPER"

-- Bind `key`, replacing any bind already on it.
local function bind(key, action, opts)
    hl.unbind(key)
    hl.bind(key, action, opts)
end

-- True when the focused window is a tiled window on a threecol workspace
-- without a fullscreen window. Otherwise the HJKL binds below use Hyprland's
-- normal focus and move, so the same keys work on floating windows, empty
-- workspaces, fullscreen and other layouts.
local function threecol_active()
    local win = hl.get_active_window()
    if not win or win.floating or not win.workspace then
        return false
    end
    return win.workspace.tiled_layout == "threecol" and not win.workspace.has_fullscreen
end

-- Focus (SUPER+HJKL) and move (SUPER+SHIFT+HJKL) along the Chain.
-- H/L walk Left → Master → Right; J/K go down/up inside Left or Right.
-- Not repeating: one press is one step.
local directions = {
    H = { msg = "l", dir = "left" },
    L = { msg = "r", dir = "right" },
    K = { msg = "u", dir = "up" },
    J = { msg = "d", dir = "down" },
}

for key, d in pairs(directions) do
    bind(mainMod .. " + " .. key, function()
        if threecol_active() then
            hl.dispatch(hl.dsp.layout("focus " .. d.msg))
        else
            hl.dispatch(hl.dsp.focus({ direction = d.dir }))
        end
    end)

    bind(mainMod .. " + SHIFT + " .. key, function()
        if threecol_active() then
            hl.dispatch(hl.dsp.layout("move " .. d.msg))
        else
            hl.dispatch(hl.dsp.window.move({ direction = d.dir }))
        end
    end)
end

-- Focus the Master window; swap the focused window with Master.
-- The master layout uses the same message names, so these work there too.
bind(mainMod .. " + M",         hl.dsp.layout("focusmaster"))
bind(mainMod .. " + SHIFT + M", hl.dsp.layout("swapwithmaster"))

-- Mouse: drag-and-drop (left button) and resize (right button).
-- Hyprland's own binds; threecol handles them natively, no extra bind needed.
bind(mainMod .. " + mouse:272", hl.dsp.window.drag(),   { mouse = true })
bind(mainMod .. " + mouse:273", hl.dsp.window.resize(), { mouse = true })

-- Keyboard resize of the focused zone (L/H) or Row (K/J), one `step` per
-- press. These repeat when held. Other layouts ignore these messages.
-- Note: SUPER+ALT+L is a common lock-screen bind; pick other keys if yours is.
bind(mainMod .. " + ALT + L", hl.dsp.layout("resize wider"),    { repeating = true })
bind(mainMod .. " + ALT + H", hl.dsp.layout("resize narrower"), { repeating = true })
bind(mainMod .. " + ALT + K", hl.dsp.layout("resize taller"),   { repeating = true })
bind(mainMod .. " + ALT + J", hl.dsp.layout("resize shorter"),  { repeating = true })

-- Reset the focused workspace's Dividers and Row heights to the settings.
-- Not repeating: one press is one reset.
bind(mainMod .. " + R", hl.dsp.layout("reset"))
