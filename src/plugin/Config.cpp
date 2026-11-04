#include "plugin/Config.hpp"

#include <hyprland/src/config/values/ConfigValues.hpp>

#include <expected>
#include <initializer_list>
#include <string>

namespace threecol {

    namespace {

        using Config::Values::CBoolValue;
        using Config::Values::CFloatValue;
        using Config::Values::CIntValue;
        using Config::Values::CStringValue;
        using Config::Supplementary::REFRESH_LAYOUTS;

        const Settings DEFAULTS{};

        // Kept for the plugin's lifetime; they read the live config value.
        struct SValues {
            SP<CFloatValue> leftWidth;
            SP<CFloatValue> rightWidth;
            SP<CFloatValue> minWidth;
            SP<CFloatValue> minMasterWidth;
            SP<CFloatValue> minRowHeight;
            SP<CFloatValue> step;
            SP<CIntValue>   leftRows;
            SP<CIntValue>    rightRows;
            SP<CStringValue> fillFirst;
            SP<CBoolValue>   persist;
        } g_values;

        // False until every value is registered. An unregistered value never
        // reads the config, so until then the defaults are used.
        bool g_registered = false;

        SP<CFloatValue> fraction(const char* name, const char* description, double def) {
            return makeShared<CFloatValue>(name, description, def, Config::Values::SFloatValueOptions{.min = 0, .max = 1, .refresh = REFRESH_LAYOUTS});
        }

        // How a Row limit of "unlimited" reads back. Hyprland skips the
        // minimum for a map entry, so a negative sentinel works while a typed
        // negative number is still rejected.
        constexpr Config::INTEGER UNLIMITED = -1;

        // A whole number, 0 for a missing zone, or "unlimited" for a Stack.
        SP<CIntValue> rowLimit(const char* name, const char* description, RowLimit def) {
            return makeShared<CIntValue>(name, description, def.rows().value_or(UNLIMITED),
                                         Config::Values::SIntValueOptions{.min = 0, .map = Config::Values::OptionMap{{"unlimited", UNLIMITED}}, .refresh = REFRESH_LAYOUTS});
        }

        RowLimit rowLimitOf(const SP<CIntValue>& value) {
            const auto rows = value->value();
            return rows == UNLIMITED ? RowLimit::unlimited() : RowLimit{static_cast<int>(rows)};
        }

        // "left" or "right" only. A string value, not an int map, so 0 and 1
        // aren't accepted too.
        SP<CStringValue> side(const char* name, const char* description, Side def) {
            const auto validator = [](const Config::STRING& text) -> std::expected<void, std::string> {
                if (parseSide(text))
                    return {};
                return std::unexpected(std::string{"expected \"left\" or \"right\""});
            };
            return makeShared<CStringValue>(name, description, Config::STRING{sideName(def)},
                                            Config::Values::SStringValueOptions{.validator = validator, .refresh = REFRESH_LAYOUTS});
        }

    }

    bool registerConfigValues(HANDLE handle) {
        g_values = {
            .leftWidth      = fraction("plugin:threecol:left_width", "Left zone width, as a fraction of the workspace", DEFAULTS.leftWidth),
            .rightWidth     = fraction("plugin:threecol:right_width", "Right zone width, as a fraction of the workspace", DEFAULTS.rightWidth),
            .minWidth       = fraction("plugin:threecol:min_width", "smallest Left / Right width", DEFAULTS.minWidth),
            .minMasterWidth = fraction("plugin:threecol:min_master_width", "smallest Master width", DEFAULTS.minMasterWidth),
            .minRowHeight   = fraction("plugin:threecol:min_row_height", "smallest Row height (Left and Right)", DEFAULTS.minRowHeight),
            .step           = fraction("plugin:threecol:step", "keyboard resize step", DEFAULTS.step),
            .leftRows       = rowLimit("plugin:threecol:left_rows", "most Rows the Left zone holds, or \"unlimited\"", DEFAULTS.leftRows),
            .rightRows      = rowLimit("plugin:threecol:right_rows", "most Rows the Right zone holds, or \"unlimited\"", DEFAULTS.rightRows),
            .fillFirst      = side("plugin:threecol:fill_first", "side new windows go to after Master: \"left\" or \"right\"", DEFAULTS.fillFirst),
            .persist        = makeShared<CBoolValue>("plugin:threecol:persist", "save Divider positions across reloads and reboots", DEFAULTS.persist,
                                                     Config::Values::SBoolValueOptions{.refresh = REFRESH_LAYOUTS}),
        };

        const std::initializer_list<SP<Config::Values::IValue>> all{
            g_values.leftWidth, g_values.rightWidth, g_values.minWidth, g_values.minMasterWidth, g_values.minRowHeight, g_values.step, g_values.leftRows, g_values.rightRows, g_values.fillFirst, g_values.persist,
        };

        bool ok = true;
        for (const auto& value : all)
            ok = HyprlandAPI::addConfigValueV2(handle, value) && ok;
        g_registered = ok;
        return ok;
    }

    Settings globalSettings() {
        if (!g_registered)
            return DEFAULTS;

        return {
            .leftWidth      = g_values.leftWidth->value(),
            .rightWidth     = g_values.rightWidth->value(),
            .minWidth       = g_values.minWidth->value(),
            .minMasterWidth = g_values.minMasterWidth->value(),
            .minRowHeight   = g_values.minRowHeight->value(),
            .step           = g_values.step->value(),
            .leftRows       = rowLimitOf(g_values.leftRows),
            .rightRows      = rowLimitOf(g_values.rightRows),
            .fillFirst      = parseSide(g_values.fillFirst->value()).value_or(DEFAULTS.fillFirst),
            .persist        = g_values.persist->value(),
        };
    }

}
