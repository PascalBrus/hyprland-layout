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

    // Shrinks Left and Stack in proportion, if needed, so Master keeps
    // min_master_width. Returns whether they shrank. Both settings and resized
    // Dividers are made to fit this way.
    bool leaveMasterItsMinimum(double& leftWidth, double& stackWidth, double minMasterWidth);

    // The settings one workspace uses: its layout_opts win over the global
    // values. `workspaceName` names the workspace in warnings.
    ResolvedSettings resolveSettings(const Settings& global, const LayoutOpts& opts, std::string_view workspaceName);

    // Decides when settings problems reach the user: one notification per
    // config load, listing each problem once. Within a load it stays quiet
    // unless a new problem shows up, e.g. on a workspace created later.
    class SettingsReport {
      public:
        // A config load: its problems are shown again, even if unchanged.
        void                       configLoaded();

        // Every workspace's current warnings. Returns the notification to show,
        // or nothing if there's nothing new to tell.
        std::optional<std::string> notice(const std::vector<std::string>& warnings);

      private:
        std::vector<std::string> m_shown; // since the last config load
    };

}
