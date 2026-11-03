#include "plugin/Config.hpp"

#include <hyprland/src/config/values/ConfigValues.hpp>

#include <initializer_list>

namespace threecol {

    namespace {

        using Config::Values::CBoolValue;
        using Config::Values::CFloatValue;
        using Config::Values::CIntValue;
        using Config::Supplementary::REFRESH_LAYOUTS;

        const Settings DEFAULTS{};

        // Kept for the plugin's lifetime; they read the live config value.
        struct SValues {
            SP<CFloatValue> leftWidth;
            SP<CFloatValue> stackWidth;
            SP<CFloatValue> minWidth;
            SP<CFloatValue> minMasterWidth;
            SP<CFloatValue> minRowHeight;
            SP<CFloatValue> step;
            SP<CIntValue>   leftRows;
            SP<CBoolValue>  persist;
        } g_values;

        SP<CFloatValue> fraction(const char* name, const char* description, double def) {
            return makeShared<CFloatValue>(name, description, def, Config::Values::SFloatValueOptions{.min = 0, .max = 1, .refresh = REFRESH_LAYOUTS});
        }

    }

    bool registerConfigValues(HANDLE handle) {
        g_values = {
            .leftWidth      = fraction("plugin:threecol:left_width", "Left zone width, as a fraction of the workspace", DEFAULTS.leftWidth),
            .stackWidth     = fraction("plugin:threecol:stack_width", "Stack zone width, as a fraction of the workspace", DEFAULTS.stackWidth),
            .minWidth       = fraction("plugin:threecol:min_width", "smallest Left / Stack width", DEFAULTS.minWidth),
            .minMasterWidth = fraction("plugin:threecol:min_master_width", "smallest Master width", DEFAULTS.minMasterWidth),
            .minRowHeight   = fraction("plugin:threecol:min_row_height", "smallest Row height (Left and Stack)", DEFAULTS.minRowHeight),
            .step           = fraction("plugin:threecol:step", "keyboard resize step", DEFAULTS.step),
            .leftRows       = makeShared<CIntValue>("plugin:threecol:left_rows", "most Rows the Left zone holds", DEFAULTS.leftRows,
                                                    Config::Values::SIntValueOptions{.min = 1, .refresh = REFRESH_LAYOUTS}),
            .persist        = makeShared<CBoolValue>("plugin:threecol:persist", "save Divider positions across reloads and reboots", DEFAULTS.persist,
                                                     Config::Values::SBoolValueOptions{.refresh = REFRESH_LAYOUTS}),
        };

        const std::initializer_list<SP<Config::Values::IValue>> all{
            g_values.leftWidth, g_values.stackWidth, g_values.minWidth, g_values.minMasterWidth, g_values.minRowHeight, g_values.step, g_values.leftRows, g_values.persist,
        };

        bool ok = true;
        for (const auto& value : all)
            ok = HyprlandAPI::addConfigValueV2(handle, value) && ok;
        return ok;
    }

    Settings globalSettings() {
        if (!g_values.leftWidth)
            return DEFAULTS;

        return {
            .leftWidth      = g_values.leftWidth->value(),
            .stackWidth     = g_values.stackWidth->value(),
            .minWidth       = g_values.minWidth->value(),
            .minMasterWidth = g_values.minMasterWidth->value(),
            .minRowHeight   = g_values.minRowHeight->value(),
            .step           = g_values.step->value(),
            .leftRows       = static_cast<int>(g_values.leftRows->value()),
            .persist        = g_values.persist->value(),
        };
    }

}
