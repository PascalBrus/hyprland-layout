#pragma once

#include <hyprland/src/plugins/PluginAPI.hpp>

namespace threecol {

    // Tells the user about settings problems: logs them and shows one
    // notification per config load. Call start in pluginInit, stop in
    // pluginExit.
    void startSettingsReporting(HANDLE handle);
    void stopSettingsReporting();

    // Checks every threecol workspace for settings problems once Hyprland is
    // idle. Calls in the same turn share one check.
    void reportSettingsLater();

}
