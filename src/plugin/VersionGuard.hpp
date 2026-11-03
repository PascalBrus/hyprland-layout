#pragma once

#include <optional>
#include <string>

namespace threecol {

    // Why this build can't run on the running Hyprland, naming both versions
    // and the fix, or nothing when the build matches.
    std::optional<std::string> versionMismatch();

}
