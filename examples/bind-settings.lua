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
                focus = {
                    left  = true, -- hl.dsp.focus({ direction = "left" }): Chain focus
                    right = true, -- hl.dsp.focus({ direction = "right" }): Chain focus
                    up    = true, -- hl.dsp.focus({ direction = "up" }): the Row above
                    down  = true, -- hl.dsp.focus({ direction = "down" }): the Row below
                },
                swap = {
                    left  = true, -- hl.dsp.window.swap({ direction = "left" })
                    right = true, -- hl.dsp.window.swap({ direction = "right" })
                    up    = true, -- hl.dsp.window.swap({ direction = "up" })
                    down  = true, -- hl.dsp.window.swap({ direction = "down" })
                },
                swap_next  = true, -- hl.dsp.window.swap({ next = true }) and { prev = true }
                cycle_next = true, -- hl.dsp.window.cycle_next()
            },

            -- Not a switch: the next / previous dispatchers wrap from the
            -- Chain's end back to its start when true.
            wrap = false,
        },
    },
})
