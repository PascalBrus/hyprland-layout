#include "plugin/SettingsReporting.hpp"

#include "core/Settings.hpp"
#include "plugin/ThreecolAlgorithm.hpp"

#include <hyprland/src/debug/log/Logger.hpp>
#include <hyprland/src/event/EventBus.hpp>
#include <hyprland/src/managers/eventLoop/EventLoopManager.hpp>

namespace threecol {

    namespace {

        const CHyprColor          WARNING_COLOR{1.0, 0.8, 0.2, 1.0};

        HANDLE                    g_handle = nullptr;
        SettingsReport            g_report;
        CHyprSignalListener       g_onConfigReloaded;
        UP<SEventLoopDoLaterLock> g_pendingCheck; // set while a check is queued

        void checkSettings() {
            const auto warnings = applySettingsToAllWorkspaces();
            const auto notice   = g_report.notice(warnings);
            if (!notice)
                return;

            LOG(Log::WARN, "{}", *notice);
            HyprlandAPI::addNotification(g_handle, *notice, WARNING_COLOR, 10000);
        }

    }

    void startSettingsReporting(HANDLE handle) {
        g_handle           = handle;
        g_onConfigReloaded = Event::bus()->m_events.config.reloaded.listen([] {
            g_report.configLoaded();
            reportSettingsLater();
        });
    }

    void stopSettingsReporting() {
        g_onConfigReloaded.reset();
        g_pendingCheck.reset();
    }

    void reportSettingsLater() {
        if (g_pendingCheck || !g_pEventLoopManager)
            return;

        g_pendingCheck = g_pEventLoopManager->doLaterLock([] {
            g_pendingCheck.reset();
            checkSettings();
        });
    }

}
