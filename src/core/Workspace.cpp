#include "core/Workspace.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace threecol {

    Workspace::Workspace(Rect area) : m_area(area) {}

    void Workspace::setArea(Rect area) {
        m_area = area;
    }

    std::vector<std::string> Workspace::setSettings(const Settings& global, const LayoutOpts& opts, std::string_view workspaceName) {
        auto resolved = resolveSettings(global, opts, workspaceName);
        m_settings    = resolved.settings;
        return std::move(resolved.warnings);
    }

    const Settings& Workspace::settings() const {
        return m_settings;
    }

    Workspace::Zone Workspace::nextZone() const {
        if (!m_master)
            return Zone::Master;
        if (!m_left)
            return Zone::Left;
        return Zone::Stack;
    }

    void Workspace::addWindow(WindowId id) {
        if (rectOf(id))
            return;

        switch (nextZone()) {
            case Zone::Master: m_master = id; break;
            case Zone::Left: m_left = id; break;
            case Zone::Stack: m_stack.push_back(id); break;
        }
    }

    std::vector<WindowId> Workspace::windowsInChainOrder() const {
        std::vector<WindowId> windows;
        if (m_left)
            windows.push_back(*m_left);
        if (m_master)
            windows.push_back(*m_master);
        windows.insert(windows.end(), m_stack.begin(), m_stack.end());
        return windows;
    }

    std::optional<WindowId> Workspace::removeWindow(WindowId id) {
        const auto windows = windowsInChainOrder();
        const auto it      = std::ranges::find(windows, id);
        if (it == windows.end())
            return std::nullopt;

        if (m_master == id)
            m_master.reset();
        if (m_left == id)
            m_left.reset();
        std::erase(m_stack, id);

        if (std::next(it) != windows.end())
            return *std::next(it);
        if (it != windows.begin())
            return *std::prev(it);
        return std::nullopt;
    }

    std::optional<Rect> Workspace::rectOf(WindowId id) const {
        if (m_master == id)
            return masterZone();
        if (m_left == id)
            return leftZone();
        if (const auto it = std::ranges::find(m_stack, id); it != m_stack.end())
            return stackRow(it - m_stack.begin(), m_stack.size());
        return std::nullopt;
    }

    namespace {

        // Width sums pick up rounding error; differences below this aren't moves.
        constexpr double EPSILON = 1e-9;

        bool same(const Dividers& a, const Dividers& b) {
            return std::abs(a.leftWidth - b.leftWidth) < EPSILON && std::abs(a.stackWidth - b.stackWidth) < EPSILON;
        }

    }

    std::optional<Dividers> Workspace::dividers() const {
        return m_dividers;
    }

    void Workspace::setDividers(std::optional<Dividers> dividers) {
        m_dividers = dividers;
    }

    Dividers Workspace::currentDividers() const {
        if (!m_dividers)
            return {m_settings.leftWidth, m_settings.stackWidth};

        // Limits may have changed since the resize: clamp the way settings
        // are made to fit, keeping m_dividers so a looser limit brings them back.
        Dividers fitted{std::max(m_dividers->leftWidth, m_settings.minWidth), std::max(m_dividers->stackWidth, m_settings.minWidth)};
        const double room = 1 - m_settings.minMasterWidth;
        const double sum  = fitted.leftWidth + fitted.stackWidth;
        if (sum > room + EPSILON)
            fitted = {fitted.leftWidth * room / sum, fitted.stackWidth * room / sum};
        return fitted;
    }

    std::optional<Workspace::Zone> Workspace::zoneOf(WindowId id) const {
        if (m_master == id)
            return Zone::Master;
        if (m_left == id)
            return Zone::Left;
        if (std::ranges::contains(m_stack, id))
            return Zone::Stack;
        return std::nullopt;
    }

    bool Workspace::layoutMessage(std::string_view message, std::optional<WindowId> focused) {
        if (message == "reset")
            return std::exchange(m_dividers, std::nullopt).has_value();

        const auto zone = focused ? zoneOf(*focused) : std::nullopt;
        if (!zone)
            return false;
        if (message == "resize wider")
            return resize(*zone, m_settings.step);
        if (message == "resize narrower")
            return resize(*zone, -m_settings.step);
        return false;
    }

    namespace {

        // `width` moved by `by`, stopping at the limit in that direction.
        // Never moves the other way, even if `width` is already past it.
        double moved(double width, double by, double smallest, double largest) {
            if (by > 0)
                return std::max(width, std::min(width + by, largest));
            return std::min(width, std::max(width + by, smallest));
        }

    }

    bool Workspace::resize(Zone zone, double by) {
        const double minWidth  = m_settings.minWidth;
        const double minMaster = m_settings.minMasterWidth;

        Dividers     dividers  = currentDividers();
        auto& [left, stack]    = dividers;
        switch (zone) {
            case Zone::Left: left = moved(left, by, minWidth, 1 - stack - minMaster); break;
            case Zone::Stack: stack = moved(stack, by, minWidth, 1 - left - minMaster); break;
            // Each Divider moves half the step, so Master stays centred.
            case Zone::Master:
                if (by > 0) {
                    // A side at min_width stays; the free side still moves.
                    left  = moved(left, -by / 2, minWidth, left);
                    stack = moved(stack, -by / 2, minWidth, stack);
                } else {
                    // Narrowing stops when Master reaches min_master_width.
                    const double room = std::max(0.0, 1 - left - stack - minMaster);
                    const double half = std::min(-by, room) / 2;
                    left += half;
                    stack += half;
                }
                break;
        }
        if (same(dividers, currentDividers()))
            return false;
        m_dividers = dividers;
        return true;
    }

    Rect Workspace::leftZone() const {
        return {m_area.x, m_area.y, m_area.w * currentDividers().leftWidth, m_area.h};
    }

    Rect Workspace::masterZone() const {
        const double leftW  = m_area.w * currentDividers().leftWidth;
        const double stackW = m_area.w * currentDividers().stackWidth;
        return {m_area.x + leftW, m_area.y, m_area.w - leftW - stackW, m_area.h};
    }

    Rect Workspace::stackZone() const {
        const double stackW = m_area.w * currentDividers().stackWidth;
        return {m_area.x + m_area.w - stackW, m_area.y, stackW, m_area.h};
    }

    Rect Workspace::stackRow(std::size_t row, std::size_t rows) const {
        const Rect   zone = stackZone();
        const double h    = zone.h / rows;
        return {zone.x, zone.y + h * row, zone.w, h};
    }

    Rect Workspace::nextWindowRect() const {
        switch (nextZone()) {
            case Zone::Master: return masterZone();
            case Zone::Left: return leftZone();
            case Zone::Stack: return stackRow(m_stack.size(), m_stack.size() + 1);
        }
        return {};
    }

}
