#include "plugin/Config.hpp"
#include "plugin/SavedDividers.hpp"
#include "plugin/SettingsReporting.hpp"
#include "plugin/ThreecolAlgorithm.hpp"

#include <hyprland/src/plugins/PluginAPI.hpp>

#include <typeinfo>

namespace {

    HANDLE           g_handle = nullptr;

    const CHyprColor ERROR_COLOR{1.0, 0.2, 0.2, 1.0};

}

APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    g_handle = handle;

    if (!threecol::registerConfigValues(g_handle))
        HyprlandAPI::addNotification(g_handle, "[threecol] could not register its plugin:threecol:* settings; using the defaults", ERROR_COLOR, 10000);

    const bool registered = HyprlandAPI::addTiledAlgo(g_handle, "threecol", &typeid(threecol::CThreecolAlgorithm),
                                                      [] { return makeUnique<threecol::CThreecolAlgorithm>(); });
    if (!registered)
        HyprlandAPI::addNotification(g_handle, "[threecol] could not register the threecol layout; is it loaded twice?", ERROR_COLOR, 10000);

    threecol::startSavingDividers();
    threecol::startSettingsReporting(g_handle);

    return {"threecol", "A three-zone tiling layout for ultrawide monitors", "Pascal B.", "0.1.0"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    threecol::stopSettingsReporting();
    threecol::stopSavingDividers();
    HyprlandAPI::removeAlgo(g_handle, "threecol");
}
