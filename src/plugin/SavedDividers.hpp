#pragma once

#include "core/Workspace.hpp"

#include <hyprland/src/desktop/DesktopTypes.hpp>

#include <optional>

namespace threecol {

    // Reads the saved Dividers, and follows config reloads and workspace
    // renames from then on. Call once, in pluginInit.
    void                    startSavingDividers();
    void                    stopSavingDividers();

    // The Dividers kept for `workspace`, or nothing if it follows its settings.
    std::optional<Dividers> savedDividers(const PHLWORKSPACE& workspace);

    // Keeps `workspace`'s Dividers, or forgets them with nothing (reset), and
    // writes the state file if persist is on.
    void                    saveDividers(const PHLWORKSPACE& workspace, std::optional<Dividers> dividers);

}
