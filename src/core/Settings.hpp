#pragma once

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace threecol {

    // threecol's settings, as set with plugin:threecol:* (the defaults are the
    // spec's). Sizes are fractions of the workspace area; Master gets what
    // Left and Stack leave.
    struct Settings {
        double leftWidth      = 0.25;
        double stackWidth     = 0.25;
        double minWidth       = 0.10;
        double minMasterWidth = 0.20;
        double minRowHeight   = 0.10;
        double step           = 0.05;
        int    leftRows       = 1;
        bool   persist        = true;

        friend bool operator==(const Settings&, const Settings&) = default;
    };

    // A workspace rule's layout_opts: key → value, as Hyprland stores them.
    using LayoutOpts = std::map<std::string, std::string>;

    struct ResolvedSettings {
        Settings                 settings;
        std::vector<std::string> warnings; // what was ignored or adjusted
    };

    // The settings one workspace uses: its layout_opts win over the global
    // values. `workspaceName` names the workspace in warnings.
    ResolvedSettings resolveSettings(const Settings& global, const LayoutOpts& opts, std::string_view workspaceName);

    // The one notification for a config load: every warning gathered from the
    // workspaces, each listed once. Nothing if there were none.
    std::optional<std::string> settingsNotice(const std::vector<std::string>& warnings);

}
