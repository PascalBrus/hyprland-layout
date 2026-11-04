#include "core/Settings.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <format>
#include <limits>
#include <optional>
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

        std::optional<double> parseWidth(const std::string& text) {
            const auto value = parseNumber(text);
            if (!value || *value < 0 || *value > 1)
                return std::nullopt;
            return value;
        }

        std::optional<int> parseRowLimit(const std::string& text) {
            const auto value = parseNumber(text);
            if (!value || *value < 1 || *value != std::floor(*value) || *value > std::numeric_limits<int>::max())
                return std::nullopt;
            return static_cast<int>(*value);
        }

        std::string workspaceLabel(std::string_view name) {
            return name.empty() ? std::string{"a workspace"} : std::format("workspace {}", name);
        }

        // Which widths came from the workspace's layout_opts.
        struct SOverridden {
            bool left  = false;
            bool stack = false;
        };

        // Makes the widths fit the limits: Left and Stack at least min_width,
        // and Master at least min_master_width. When both can't hold, Master
        // keeps its minimum and Left and Stack shrink in proportion.
        // A warning names the workspace only when its own layout_opts caused
        // it, so a global problem reads the same on every workspace.
        void makeWidthsFit(Settings& settings, SOverridden overridden, std::string_view workspaceName, std::vector<std::string>& warnings) {
            const auto prefix = [&](bool fromLayoutOpts) { return fromLayoutOpts ? workspaceLabel(workspaceName) + ": " : std::string{}; };

            for (auto [key, width, fromLayoutOpts] :
                 {std::tuple{"left_width", &settings.leftWidth, overridden.left}, std::tuple{"stack_width", &settings.stackWidth, overridden.stack}}) {
                if (*width >= settings.minWidth)
                    continue;
                warnings.push_back(std::format("{}{} {} is below min_width {}; using {}", prefix(fromLayoutOpts), key, *width, settings.minWidth, settings.minWidth));
                *width = settings.minWidth;
            }

            const double left  = settings.leftWidth;
            const double stack = settings.stackWidth;
            if (!leaveMasterItsMinimum(settings.leftWidth, settings.stackWidth, settings.minMasterWidth))
                return;
            warnings.push_back(std::format("{}left_width {} and stack_width {} leave Master less than min_master_width {}; using {:.3g} and {:.3g}",
                                           prefix(overridden.left || overridden.stack), left, stack, settings.minMasterWidth, settings.leftWidth, settings.stackWidth));
        }


        // Makes the Row limit fit: Left uses only as many Rows as can each be
        // at least min_row_height tall, and always at least one.
        void makeRowLimitFit(Settings& settings, bool fromLayoutOpts, std::string_view workspaceName, std::vector<std::string>& warnings) {
            if (settings.minRowHeight <= 0)
                return;
            // The most Rows n with n * min_row_height <= 1, allowing the
            // rounding error RowHeights allows, so 1 / 0.1 isn't 9.
            const int fits = std::max(1, static_cast<int>(std::min<double>(std::floor((1 + 1e-9) / settings.minRowHeight), std::numeric_limits<int>::max())));
            if (settings.leftRows <= fits)
                return;
            warnings.push_back(std::format("{}left_rows {} doesn't fit min_row_height {}; using {}", fromLayoutOpts ? workspaceLabel(workspaceName) + ": " : std::string{},
                                           settings.leftRows, settings.minRowHeight, fits));
            settings.leftRows = fits;
        }

    }

    bool leaveMasterItsMinimum(double& leftWidth, double& stackWidth, double minMasterWidth) {
        // Allows for rounding, so widths that just fit aren't shrunk.
        constexpr double EPSILON = 1e-9;
        const double     room    = 1 - minMasterWidth;
        const double     sum     = leftWidth + stackWidth;
        if (sum <= room + EPSILON)
            return false;
        leftWidth  = leftWidth * room / sum;
        stackWidth = stackWidth * room / sum;
        return true;
    }

    ResolvedSettings resolveSettings(const Settings& global, const LayoutOpts& opts, std::string_view workspaceName) {
        ResolvedSettings resolved{.settings = global, .warnings = {}};
        auto&            settings = resolved.settings;

        const auto       warnIgnored = [&](const std::string& key, const std::string& text, std::string_view expected, std::string globalValue) {
            resolved.warnings.push_back(std::format("{}: layout_opts {} = \"{}\" is not {}; using {}", workspaceLabel(workspaceName), key, text, expected, globalValue));
        };

        SOverridden overridden;
        bool        rowsOverridden = false;
        for (const auto& [key, text] : opts) {
            if (key == "left_width" || key == "stack_width") {
                double& width = key == "left_width" ? settings.leftWidth : settings.stackWidth;
                if (const auto value = parseWidth(text)) {
                    width = *value;
                    (key == "left_width" ? overridden.left : overridden.stack) = true;
                } else
                    warnIgnored(key, text, "a number from 0 to 1", std::format("{}", width));
            } else if (key == "left_rows") {
                if (const auto value = parseRowLimit(text)) {
                    settings.leftRows = *value;
                    rowsOverridden    = true;
                } else
                    warnIgnored(key, text, "a whole number of at least 1", std::format("{}", settings.leftRows));
            } else
                resolved.warnings.push_back(std::format("{}: unknown layout_opts key \"{}\", ignored", workspaceLabel(workspaceName), key));
        }

        makeWidthsFit(settings, overridden, workspaceName, resolved.warnings);
        makeRowLimitFit(settings, rowsOverridden, workspaceName, resolved.warnings);

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
