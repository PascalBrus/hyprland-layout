#include "plugin/VersionGuard.hpp"

#include <hyprland/src/plugins/PluginAPI.hpp>

#include <format>
#include <string_view>

namespace threecol {

    namespace {

        // A hash reads "<commit>_aq_<ver>_hu_<ver>_..."; the commit comes first.
        std::string_view commitOf(std::string_view hash) {
            return hash.substr(0, hash.find('_'));
        }

        // A short commit when the commits differ. Otherwise only a library
        // version differs, so the whole hash is the only useful name.
        std::string describe(std::string_view hash, bool commitsDiffer) {
            if (commitsDiffer)
                return std::format("Hyprland {}", commitOf(hash).substr(0, 7));
            return std::string{hash};
        }

    }

    std::optional<std::string> versionMismatch() {
        const std::string_view running = __hyprland_api_get_hash();
        const std::string_view built   = __hyprland_api_get_client_hash();
        if (running == built)
            return std::nullopt;

        const bool commitsDiffer = commitOf(running) != commitOf(built);
        return std::format("[threecol] built for {}, running {}; run `hyprpm update` (or rebuild threecol), then reload. Using dwindle until then.",
                           describe(built, commitsDiffer), describe(running, commitsDiffer));
    }

}
