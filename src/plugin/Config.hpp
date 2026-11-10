#pragma once

#include "core/Settings.hpp"

#include <hyprland/src/plugins/PluginAPI.hpp>

namespace threecol {

    // Registers every plugin:threecol:* value. Call once, in pluginInit.
    // Returns false if Hyprland refused one.
    bool     registerConfigValues(HANDLE handle);

    // The global values as currently set.
    Settings globalSettings();

    // The switches for Hyprland's dispatchers threecol changes, and wrap,
    // as currently set. Global only, read each time a dispatcher runs.
    struct SBindSwitches {
        // Each covers every direction or both ways of its dispatcher.
        bool focus     = true;
        bool swap      = true;
        bool swapNext  = true;
        bool cycleNext = true;
        // Which dispatchers loop. Only threecol's behaviour reads them.
        Wrap wrap      = {};
    };
    SBindSwitches bindSwitches();

}
