#include "core/Settings.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <format>
#include <limits>
#include <optional>
#include <ostream>
#include <tuple>

namespace threecol {

    namespace {

        // The whole of `text` as a finite number, or nothing.
        std::optional<double> parseNumber(const std::string& text) {
            double     value = 0;
            const auto end   = text.data() + text.size();
            const auto [ptr, ec] = std::from_chars(text.data(), end, value);
            if (ec != std::errc{} || ptr != end || !std::isfinite(value))
                return std::nullopt;
            return value;
        }

        // What a fraction setting must be, and a Row limit; warnings quote these.
        constexpr std::string_view FRACTION  = "a number from 0 to 1";
        constexpr std::string_view ROW_LIMIT = "a whole number of at least 0 or \"unlimited\"";

        // False for NaN too.
        bool isFraction(double value) {
            return value >= 0 && value <= 1;
        }

        std::optional<double> parseWidth(const std::string& text) {
            const auto value = parseNumber(text);
            if (!value || !isFraction(*value))
                return std::nullopt;
            return value;
        }

        // A bool as Hyprland hands over a Lua one ("true", "false"), or 1 / 0.
        std::optional<bool> parseBool(const std::string& text) {
            if (text == "true" || text == "1")
                return true;
            if (text == "false" || text == "0")
                return false;
            return std::nullopt;
        }

        std::string boolText(bool value) {
            return value ? "true" : "false";
        }

        std::optional<RowLimit> parseRowLimit(const std::string& text) {
            if (text == "unlimited")
                return RowLimit::unlimited();
            const auto value = parseNumber(text);
            if (!value || *value < 0 || *value != std::floor(*value) || *value > std::numeric_limits<int>::max())
                return std::nullopt;
            return RowLimit{static_cast<int>(*value)};
        }

        // A setting's value as a warning shows it. Hyprland stores settings as
        // floats, so this prints the shortest text that reads back as the same
        // float: 0.1, not the 0.10000000149011612 it widens to.
        std::string settingText(double value) {
            return std::format("{}", static_cast<float>(value));
        }

        std::string workspaceLabel(std::string_view name) {
            return name.empty() ? std::string{"a workspace"} : std::format("workspace {}", name);
        }

        // What starts a warning about a setting: the workspace's name only
        // when its own layout_opts set it, so a global problem reads the same
        // on every workspace.
        std::string warningPrefix(bool fromLayoutOpts, std::string_view workspaceName) {
            return fromLayoutOpts ? workspaceLabel(workspaceName) + ": " : std::string{};
        }

        // Hyprland hands over global values unchecked, so one out of range is
        // replaced by its default here, where it can be warned about.
        void ignoreBadGlobals(Settings& settings, std::vector<std::string>& warnings) {
            const Settings defaults{};
            for (auto [key, value, def] : {std::tuple{"left_width", &settings.leftWidth, defaults.leftWidth}, std::tuple{"right_width", &settings.rightWidth, defaults.rightWidth},
                                           std::tuple{"min_width", &settings.minWidth, defaults.minWidth},
                                           std::tuple{"min_master_width", &settings.minMasterWidth, defaults.minMasterWidth},
                                           std::tuple{"min_row_height", &settings.minRowHeight, defaults.minRowHeight}, std::tuple{"step", &settings.step, defaults.step}}) {
                if (isFraction(*value))
                    continue;
                warnings.push_back(std::format("{} {} is not {}; using {}", key, settingText(*value), FRACTION, settingText(def)));
                *value = def;
            }
            for (auto [key, limit, def] : {std::tuple{"left_rows", &settings.leftRows, defaults.leftRows}, std::tuple{"right_rows", &settings.rightRows, defaults.rightRows}}) {
                if (limit->rows().value_or(0) >= 0)
                    continue;
                warnings.push_back(std::format("{} {} is not {}; using {}", key, limit->text(), ROW_LIMIT, def.text()));
                *limit = def;
            }
        }

        // Which settings came from the workspace's layout_opts.
        struct SOverridden {
            bool left      = false;
            bool right     = false;
            bool leftRows     = false;
            bool rightRows    = false;
            bool centreMaster = false;
        };

        // Centred Master needs both side zones: with one missing it is a
        // conflict, warned about, and the workspace runs with it off.
        void resolveCentreMaster(Settings& settings, SOverridden overridden, std::string_view workspaceName, std::vector<std::string>& warnings) {
            if (!settings.centreMaster)
                return;
            const bool noLeft  = settings.leftRows.role() == Role::Missing;
            const bool noRight = settings.rightRows.role() == Role::Missing;
            if (!noLeft && !noRight)
                return;
            const auto missing = noLeft && noRight ? std::string{"left_rows and right_rows are 0"} : noLeft ? std::string{"left_rows is 0"} : std::string{"right_rows is 0"};
            const bool fromLayoutOpts = overridden.centreMaster || (noLeft && overridden.leftRows) || (noRight && overridden.rightRows);
            warnings.push_back(std::format("{}centre_master needs both side zones, but {}; using centre_master = false", warningPrefix(fromLayoutOpts, workspaceName), missing));
            settings.centreMaster = false;
        }

        // Centred Master needs equal side widths: unequal ones are a
        // conflict, warned about, where Master keeps its width and each side
        // gets half of the rest.
        void centreWidths(Settings& settings, SOverridden overridden, std::string_view workspaceName, std::vector<std::string>& warnings) {
            if (!settings.centreMaster || settings.leftWidth == settings.rightWidth)
                return;
            const double side           = (settings.leftWidth + settings.rightWidth) / 2;
            const bool   fromLayoutOpts = overridden.centreMaster || overridden.left || overridden.right;
            warnings.push_back(std::format("{}centre_master needs equal side widths, but left_width is {} and right_width is {}; using {} each",
                                           warningPrefix(fromLayoutOpts, workspaceName), settingText(settings.leftWidth), settingText(settings.rightWidth), settingText(side)));
            settings.leftWidth = settings.rightWidth = side;
        }

        // Makes the widths fit the limits: Left and Right at least min_width,
        // and Master at least min_master_width. When both can't hold, Master
        // keeps its minimum and Left and Right shrink in proportion. Only side
        // zones that exist count: a missing zone's width is left as it is,
        // unused, so it is neither raised, counted nor warned about.
        // A warning names the workspace only when its own layout_opts caused
        // it, so a global problem reads the same on every workspace.
        void makeWidthsFit(Settings& settings, SOverridden overridden, std::string_view workspaceName, std::vector<std::string>& warnings) {
            const auto prefix   = [&](bool fromLayoutOpts) { return warningPrefix(fromLayoutOpts, workspaceName); };
            const bool hasLeft  = settings.leftRows.role() != Role::Missing;
            const bool hasRight = settings.rightRows.role() != Role::Missing;

            for (auto [key, width, fromLayoutOpts, exists] : {std::tuple{"left_width", &settings.leftWidth, overridden.left, hasLeft},
                                                                std::tuple{"right_width", &settings.rightWidth, overridden.right, hasRight}}) {
                if (!exists || *width >= settings.minWidth)
                    continue;
                warnings.push_back(std::format("{}{} {} is below min_width {}; using {}", prefix(fromLayoutOpts), key, settingText(*width), settingText(settings.minWidth),
                                               settingText(settings.minWidth)));
                *width = settings.minWidth;
            }

            double left  = hasLeft ? settings.leftWidth : 0;
            double right = hasRight ? settings.rightWidth : 0;
            if (!leaveMasterItsMinimum(left, right, settings.minMasterWidth))
                return;
            // Only the zones that exist are named, as only they were counted.
            if (hasLeft && hasRight)
                warnings.push_back(std::format("{}left_width {} and right_width {} leave Master less than min_master_width {}; using {:.3g} and {:.3g}",
                                               prefix(overridden.left || overridden.right), settingText(settings.leftWidth), settingText(settings.rightWidth),
                                               settingText(settings.minMasterWidth), left, right));
            else {
                const auto [key, width, fitted, fromLayoutOpts] =
                    hasLeft ? std::tuple{"left_width", settings.leftWidth, left, overridden.left} : std::tuple{"right_width", settings.rightWidth, right, overridden.right};
                warnings.push_back(std::format("{}{} {} leaves Master less than min_master_width {}; using {:.3g}", prefix(fromLayoutOpts), key, settingText(width),
                                               settingText(settings.minMasterWidth), fitted));
            }
            if (hasLeft)
                settings.leftWidth = left;
            if (hasRight)
                settings.rightWidth = right;
        }

        // Makes the Row limits fit: a Bounded zone uses only as many Rows as
        // can each be at least min_row_height tall, and always at least one.
        void makeRowLimitsFit(Settings& settings, SOverridden overridden, std::string_view workspaceName, std::vector<std::string>& warnings) {
            if (settings.minRowHeight <= 0)
                return;
            // The most Rows n with n * min_row_height <= 1, allowing the
            // rounding error RowHeights allows, so 1 / 0.1 isn't 9.
            const int fits = std::max(1, static_cast<int>(std::min<double>(std::floor((1 + 1e-9) / settings.minRowHeight), std::numeric_limits<int>::max())));

            for (auto [key, limit, fromLayoutOpts] :
                 {std::tuple{"left_rows", &settings.leftRows, overridden.leftRows}, std::tuple{"right_rows", &settings.rightRows, overridden.rightRows}}) {
                if (limit->role() != Role::Bounded || *limit->rows() <= fits)
                    continue;
                warnings.push_back(std::format("{}{} {} doesn't fit min_row_height {}; using {}", warningPrefix(fromLayoutOpts, workspaceName), key,
                                               limit->text(), settingText(settings.minRowHeight), fits));
                *limit = fits;
            }
        }

    }

    std::string_view sideName(Side side) {
        return side == Side::Left ? "left" : "right";
    }

    std::ostream& operator<<(std::ostream& out, Side side) {
        return out << sideName(side);
    }

    std::optional<Side> parseSide(std::string_view text) {
        if (text == "left")
            return Side::Left;
        if (text == "right")
            return Side::Right;
        return std::nullopt;
    }

    namespace {
        constexpr std::int64_t UNLIMITED_SETTING = std::numeric_limits<std::int64_t>::min();
    }

    std::int64_t rowLimitSetting(RowLimit limit) {
        // Not limit.rows().value_or(…): that would cut the sentinel to an int,
        // and it would read back as 0, a missing zone (#70).
        if (const auto rows = limit.rows())
            return *rows;
        return UNLIMITED_SETTING;
    }

    RowLimit rowLimitFromSetting(std::int64_t setting) {
        if (setting == UNLIMITED_SETTING)
            return RowLimit::unlimited();
        return RowLimit{static_cast<int>(std::clamp<std::int64_t>(setting, std::numeric_limits<int>::min(), std::numeric_limits<int>::max()))};
    }

    std::string RowLimit::text() const {
        return m_rows ? std::to_string(*m_rows) : std::string{"unlimited"};
    }

    std::ostream& operator<<(std::ostream& out, const RowLimit& limit) {
        return out << limit.text();
    }

    bool leaveMasterItsMinimum(double& leftWidth, double& rightWidth, double minMasterWidth) {
        // Allows for rounding, so widths that just fit aren't shrunk.
        constexpr double EPSILON = 1e-9;
        const double     room    = 1 - minMasterWidth;
        const double     sum     = leftWidth + rightWidth;
        if (sum <= room + EPSILON)
            return false;
        leftWidth  = leftWidth * room / sum;
        rightWidth = rightWidth * room / sum;
        return true;
    }

    ResolvedSettings resolveSettings(const Settings& global, const LayoutOpts& opts, std::string_view workspaceName) {
        ResolvedSettings resolved{.settings = global, .warnings = {}};
        auto&            settings = resolved.settings;
        ignoreBadGlobals(settings, resolved.warnings);

        const auto       warnIgnored = [&](const std::string& key, const std::string& text, std::string_view expected, std::string globalValue) {
            resolved.warnings.push_back(std::format("{}: layout_opts {} = \"{}\" is not {}; using {}", workspaceLabel(workspaceName), key, text, expected, globalValue));
        };

        SOverridden overridden;
        for (const auto& [key, text] : opts) {
            if (key == "left_width" || key == "right_width") {
                double& width = key == "left_width" ? settings.leftWidth : settings.rightWidth;
                if (const auto value = parseWidth(text)) {
                    width = *value;
                    (key == "left_width" ? overridden.left : overridden.right) = true;
                } else
                    warnIgnored(key, text, FRACTION, settingText(width));
            } else if (key == "left_rows" || key == "right_rows") {
                RowLimit& limit = key == "left_rows" ? settings.leftRows : settings.rightRows;
                if (const auto value = parseRowLimit(text)) {
                    limit                                                           = *value;
                    (key == "left_rows" ? overridden.leftRows : overridden.rightRows) = true;
                } else
                    warnIgnored(key, text, ROW_LIMIT, limit.text());
            } else if (key == "centre_master") {
                if (const auto value = parseBool(text)) {
                    settings.centreMaster   = *value;
                    overridden.centreMaster = true;
                } else
                    warnIgnored(key, text, "true or false", boolText(settings.centreMaster));
            } else if (key == "fill_first") {
                if (const auto side = parseSide(text))
                    settings.fillFirst = *side;
                else
                    warnIgnored(key, text, "\"left\" or \"right\"", std::string{sideName(settings.fillFirst)});
            } else
                resolved.warnings.push_back(std::format("{}: unknown layout_opts key \"{}\", ignored", workspaceLabel(workspaceName), key));
        }

        resolveCentreMaster(settings, overridden, workspaceName, resolved.warnings);
        centreWidths(settings, overridden, workspaceName, resolved.warnings);
        makeWidthsFit(settings, overridden, workspaceName, resolved.warnings);
        makeRowLimitsFit(settings, overridden, workspaceName, resolved.warnings);

        return resolved;
    }

    void SettingsReport::configLoaded() {
        m_shown.clear();
    }

    std::optional<std::string> SettingsReport::notice(const std::vector<std::string>& warnings) {
        const auto isShown = [this](const std::string& warning) { return std::ranges::find(m_shown, warning) != m_shown.end(); };
        if (std::ranges::all_of(warnings, isShown))
            return std::nullopt;

        std::vector<std::string_view> listed;
        std::string                   notice = "[threecol] settings ignored or adjusted:";
        for (const auto& warning : warnings) {
            if (std::ranges::find(listed, warning) != listed.end())
                continue;
            listed.push_back(warning);
            notice += "\n- " + warning;
            if (!isShown(warning))
                m_shown.push_back(warning);
        }
        return notice;
    }

}
