#pragma once

#include <hyprland/src/plugins/PluginAPI.hpp>

namespace threecol {

    // Hooks Hyprland's moveFocus, swapInDirection, swapNext and cycleNext so
    // that, on a window threecol tiles with nothing fullscreen, they act the
    // threecol way (see the README's Binds section). Each one's switch under
    // plugin:threecol:binds gives it back to Hyprland. A hook that can't be
    // installed leaves that dispatcher to Hyprland; one notification lists
    // them. Call once, in pluginInit.
    void installDispatcherHooks(HANDLE handle);

}
