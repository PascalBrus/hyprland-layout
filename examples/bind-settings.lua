-- threecol: every bind switch, at its default. Optional: you need none of
-- these, since the defaults live in the plugin.
--
-- On a window threecol tiles, with nothing fullscreen, these Hyprland
-- dispatchers behave the threecol way. Set a switch to false to give that
-- dispatcher Hyprland's behaviour back. The README's Binds section says
-- what each one does.
--
-- Paste the switches you want into plugin.threecol in your hl.config, as
-- below. They're global only and take effect straight away.

hl.config({
    plugin = {
        threecol = {
            binds = {
                focus      = true, -- hl.dsp.focus({ direction }): the Chain, a side zone's Rows
                swap       = true, -- hl.dsp.window.swap({ direction }): with the window focus goes to
                swap_next  = true, -- hl.dsp.window.swap({ next = true }) and { prev = true }
                cycle_next = true, -- hl.dsp.window.cycle_next(): walks the Cycle
            },

            -- Looping: from one end of the Chain to the other, and inside a
            -- side zone. false stops them at the end; only threecol's
            -- behaviour reads these.
            wrap = {
                focus      = true, -- focus({ direction }), focus l / r / u / d
                swap       = true, -- swap({ direction })
                move       = true, -- move({ direction }), move l / r / u / d
                swap_next  = true, -- swap({ next | prev }), swapnext / swapprev
                cycle_next = true, -- cycle_next(), cyclenext / cycleprev
            },
        },
    },
})
