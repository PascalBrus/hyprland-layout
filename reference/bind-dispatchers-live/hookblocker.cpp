// Test plugin for live83.py hook_failure: hooks Config::Actions::moveFocus
// first, so threecol's hook on it fails. Its own hook calls the original.
// Build: g++ -std=c++26 -shared -fPIC --no-gnu-unique $(pkg-config --cflags hyprland pixman-1 libdrm) hookblocker.cpp -o hookblocker.so
#include <hyprland/src/config/shared/actions/ConfigActions.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/plugins/HookSystem.hpp>

static CFunctionHook* g_hook = nullptr;
using Fn = Config::Actions::ActionResult (*)(Math::eDirection);

static Config::Actions::ActionResult hk(Math::eDirection dir) {
    return reinterpret_cast<Fn>(g_hook->m_original)(dir);
}

APICALL EXPORT std::string PLUGIN_API_VERSION() { return HYPRLAND_API_VERSION; }

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    for (const auto& m : HyprlandAPI::findFunctionsByName(handle, "moveFocus")) {
        if (m.demangled != "Config::Actions::moveFocus(Math::eDirection)")
            continue;
        g_hook = HyprlandAPI::createFunctionHook(handle, m.address, reinterpret_cast<void*>(&hk));
        if (g_hook)
            g_hook->hook();
    }
    return {"hookblocker", "test: holds the moveFocus hook", "test", "0"};
}

APICALL EXPORT void PLUGIN_EXIT() {}
