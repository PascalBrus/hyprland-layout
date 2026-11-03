#include "core/DividerStore.hpp"

#include <charconv>
#include <cmath>
#include <format>
#include <ranges>
#include <utility>

namespace threecol {

    // The state file is plain text, one workspace per line after a header:
    //
    //   threecol dividers 1
    //   # left_width stack_width workspace
    //   0.3 0.2 1
    //   0.15 0.35 web
    //
    // Widths are fractions written in their shortest exact form. The name is
    // the rest of the line, with "\" written "\\" and a newline "\n".
    namespace {

        constexpr std::string_view HEADER = "threecol dividers 1";

        std::string escape(std::string_view name) {
            std::string out;
            for (const char c : name) {
                if (c == '\\')
                    out += "\\\\";
                else if (c == '\n')
                    out += "\\n";
                else
                    out += c;
            }
            return out;
        }

        std::optional<std::string> unescape(std::string_view text) {
            std::string out;
            for (std::size_t i = 0; i < text.size(); ++i) {
                if (text[i] != '\\') {
                    out += text[i];
                    continue;
                }
                if (++i == text.size())
                    return std::nullopt;
                if (text[i] == '\\')
                    out += '\\';
                else if (text[i] == 'n')
                    out += '\n';
                else
                    return std::nullopt;
            }
            return out;
        }

        // A width from the start of `text`, followed by one space; advances
        // `text` past both. Nothing unless it is a fraction from 0 to 1.
        std::optional<double> takeWidth(std::string_view& text) {
            double     value = 0;
            const auto [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
            if (ec != std::errc{} || !std::isfinite(value) || value < 0 || value > 1)
                return std::nullopt;
            text.remove_prefix(ptr - text.data());
            if (!text.starts_with(' '))
                return std::nullopt;
            text.remove_prefix(1);
            return value;
        }

        std::string serialise(const std::map<std::string, Dividers, std::less<>>& all) {
            std::string out = std::format("{}\n# left_width stack_width workspace\n", HEADER);
            for (const auto& [name, dividers] : all)
                out += std::format("{} {} {}\n", dividers.leftWidth, dividers.stackWidth, escape(name));
            return out;
        }

        // The Dividers in a state file. Unknown or corrupt content counts as
        // nothing saved: a file without the header gives nothing, and a line
        // that doesn't parse is skipped.
        std::map<std::string, Dividers, std::less<>> parse(std::string_view text) {
            std::map<std::string, Dividers, std::less<>> all;

            auto lines = text | std::views::split('\n') | std::views::transform([](auto line) { return std::string_view{line}; });
            auto it    = lines.begin();
            if (it == lines.end() || *it != HEADER)
                return {};

            for (++it; it != lines.end(); ++it) {
                std::string_view line = *it;
                if (line.empty() || line.starts_with('#'))
                    continue;
                const auto left  = takeWidth(line);
                const auto stack = left ? takeWidth(line) : std::nullopt;
                const auto name  = stack ? unescape(line) : std::nullopt;
                if (name)
                    all.insert_or_assign(*name, Dividers{*left, *stack});
            }
            return all;
        }

    }

    void DividerStore::configLoaded(bool persist, const ReadFile& readFile) {
        m_persist = persist;
        if (!persist) {
            m_dividers.clear();
            m_read = false;
            return;
        }
        if (std::exchange(m_read, true))
            return;

        const auto file = readFile();
        m_dividers      = file ? parse(*file) : decltype(m_dividers){};
    }

    std::optional<Dividers> DividerStore::find(std::string_view workspace) const {
        const auto it = m_dividers.find(workspace);
        return it == m_dividers.end() ? std::nullopt : std::optional{it->second};
    }

    std::optional<std::string> DividerStore::set(std::string_view workspace, std::optional<Dividers> dividers) {
        const auto it = m_dividers.find(workspace);
        if (!dividers) {
            if (it == m_dividers.end())
                return std::nullopt;
            m_dividers.erase(it);
        } else if (it == m_dividers.end())
            m_dividers.emplace(workspace, *dividers);
        else if (it->second != *dividers)
            it->second = *dividers;
        else
            return std::nullopt;
        return changed();
    }

    std::optional<std::string> DividerStore::rename(std::string_view from, std::string_view to) {
        if (from == to)
            return std::nullopt;

        // Carrying "nothing saved" over clears any old entry under the new name.
        const auto dividers = find(from);
        const auto erased   = set(from, std::nullopt);
        const auto written  = set(to, dividers);
        return written ? written : erased;
    }

    std::optional<std::string> DividerStore::changed() const {
        if (!m_persist)
            return std::nullopt;
        return serialise(m_dividers);
    }

}
