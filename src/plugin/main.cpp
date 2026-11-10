#include "plugin/Config.hpp"
#include "plugin/DispatcherHooks.hpp"
#include "plugin/DropIndicator.hpp"
#include "plugin/SavedDividers.hpp"
#include "plugin/SettingsReporting.hpp"
#include "plugin/ThreecolAlgorithm.hpp"
#include "plugin/VersionGuard.hpp"

#include <hyprland/src/debug/log/Logger.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>

#include <stdexcept>
#include <typeinfo>

namespace {

    HANDLE           g_handle = nullptr;

    const CHyprColor ERROR_COLOR{1.0, 0.2, 0.2, 1.0};

    // Stays up until dismissed (about three years).
    constexpr float  PERSISTENT_MS = 1e11F;

}

APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    g_handle = handle;

    // Throwing makes Hyprland unload the plugin before anything is registered,
    // so workspaces that ask for threecol fall back to dwindle.
    if (const auto mismatch = threecol::versionMismatch()) {
        LOG(Log::ERR, "{}", *mismatch);
        HyprlandAPI::addNotification(g_handle, *mismatch, ERROR_COLOR, PERSISTENT_MS);
        throw std::runtime_error(*mismatch);
    }

    if (!threecol::registerConfigValues(g_handle))
        HyprlandAPI::addNotification(g_handle, "[threecol] could not register its plugin:threecol:* settings; using the defaults", ERROR_COLOR, 10000);

    const bool registered = HyprlandAPI::addTiledAlgo(g_handle, "threecol", &typeid(threecol::CThreecolAlgorithm),
                                                      [] { return makeUnique<threecol::CThreecolAlgorithm>(); });
    if (!registered)
        HyprlandAPI::addNotification(g_handle, "[threecol] could not register the threecol layout; is it loaded twice?", ERROR_COLOR, 10000);

    threecol::installDispatcherHooks(g_handle);
    threecol::startSavingDividers();
    threecol::startSettingsReporting(g_handle);
    threecol::startDropIndicator();

    return {"threecol", "A three-zone tiling layout for ultrawide monitors", "Pascal B.", "0.1.0"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    threecol::stopDropIndicator();
    threecol::stopSettingsReporting();
    threecol::stopSavingDividers();
    HyprlandAPI::removeAlgo(g_handle, "threecol");
    // Hyprland removes the dispatcher hooks itself when a plugin unloads.
}
