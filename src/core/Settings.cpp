#include "core/Settings.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <format>
#include <limits>
#include <optional>
#include <utility>

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

        // Makes the widths fit the limits: Left and Stack at least min_width,
        // and Master at least min_master_width. When both can't hold, Master
        // keeps its minimum and Left and Stack shrink in proportion.
        // `prefix` starts each warning ("" for global values, so every
        // workspace warns the same).
        void makeWidthsFit(Settings& settings, const std::string& prefix, std::vector<std::string>& warnings) {
            for (auto [key, width] : {std::pair{"left_width", &settings.leftWidth}, std::pair{"stack_width", &settings.stackWidth}}) {
                if (*width >= settings.minWidth)
                    continue;
                warnings.push_back(std::format("{}{} {} is below min_width {}; using {}", prefix, key, *width, settings.minWidth, settings.minWidth));
                *width = settings.minWidth;
            }

            const double room = 1 - settings.minMasterWidth;
            const double sum  = settings.leftWidth + settings.stackWidth;
            if (sum <= room)
                return;

            const double left  = settings.leftWidth * room / sum;
            const double stack = settings.stackWidth * room / sum;
            warnings.push_back(std::format("{}left_width {} and stack_width {} leave Master less than min_master_width {}; using {:.3g} and {:.3g}", prefix,
                                           settings.leftWidth, settings.stackWidth, settings.minMasterWidth, left, stack));
            settings.leftWidth  = left;
            settings.stackWidth = stack;
        }

    }

    ResolvedSettings resolveSettings(const Settings& global, const LayoutOpts& opts, std::string_view workspaceName) {
        ResolvedSettings resolved{.settings = global, .warnings = {}};
        auto&            settings = resolved.settings;

        const auto       ignore = [&](const std::string& key, const std::string& text, std::string_view expected, std::string globalValue) {
            resolved.warnings.push_back(std::format("{}: layout_opts {} = \"{}\" is not {}; using {}", workspaceLabel(workspaceName), key, text, expected, globalValue));
        };

        bool widthOverridden = false;
        for (const auto& [key, text] : opts) {
            if (key == "left_width" || key == "stack_width") {
                double& width = key == "left_width" ? settings.leftWidth : settings.stackWidth;
                if (const auto value = parseWidth(text)) {
                    width           = *value;
                    widthOverridden = true;
                } else
                    ignore(key, text, "a number from 0 to 1", std::format("{}", width));
            } else if (key == "left_rows") {
                if (const auto value = parseRowLimit(text))
                    settings.leftRows = *value;
                else
                    ignore(key, text, "a whole number of at least 1", std::format("{}", settings.leftRows));
            } else
                resolved.warnings.push_back(std::format("{}: unknown layout_opts key \"{}\", ignored", workspaceLabel(workspaceName), key));
        }

        makeWidthsFit(settings, widthOverridden ? workspaceLabel(workspaceName) + ": " : "", resolved.warnings);

        return resolved;
    }

    std::optional<std::string> settingsNotice(const std::vector<std::string>& warnings) {
        std::vector<std::string_view> listed;
        std::string                   notice = "[threecol] settings ignored or adjusted:";
        for (const auto& warning : warnings) {
            if (std::ranges::find(listed, warning) != listed.end())
                continue;
            listed.push_back(warning);
            notice += "\n- " + warning;
        }
        if (listed.empty())
            return std::nullopt;
        return notice;
    }

}
