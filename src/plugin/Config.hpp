#pragma once

#include "core/Settings.hpp"

#include <hyprland/src/plugins/PluginAPI.hpp>

#include <array>

namespace threecol {

    // Registers every plugin:threecol:* value. Call once, in pluginInit.
    // Returns false if Hyprland refused one.
    bool     registerConfigValues(HANDLE handle);

    // The global values as currently set.
    Settings globalSettings();

    // The switches for Hyprland's dispatchers threecol changes, and wrap,
    // as currently set. Global only, read each time a dispatcher runs.
    struct SBindSwitches {
        // Indexed by Direction: Left, Right, Up, Down.
        std::array<bool, 4> focus     = {true, true, true, true};
        std::array<bool, 4> swap      = {true, true, true, true};
        bool                swapNext  = true;
        bool                cycleNext = true;
        // The next / previous dispatchers go from the Chain's far end back
        // to the start.
        bool                wrap      = false;
    };
    SBindSwitches bindSwitches();

}
