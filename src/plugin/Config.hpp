#pragma once

#include "core/Settings.hpp"

#include <hyprland/src/plugins/PluginAPI.hpp>

namespace threecol {

    // Registers every plugin:threecol:* value. Call once, in pluginInit.
    // Returns false if Hyprland refused one.
    bool     registerConfigValues(HANDLE handle);

    // The global values as currently set.
    Settings globalSettings();

}
