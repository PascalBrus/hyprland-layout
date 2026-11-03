#include "core/Workspace.hpp"

#include <algorithm>

namespace threecol {

    Workspace::Workspace(Rect area, Widths widths) : m_area(area), m_widths(widths) {}

    void Workspace::setArea(Rect area) {
        m_area = area;
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

    Rect Workspace::leftZone() const {
        return {m_area.x, m_area.y, m_area.w * m_widths.left, m_area.h};
    }

    Rect Workspace::masterZone() const {
        const double leftW  = m_area.w * m_widths.left;
        const double stackW = m_area.w * m_widths.stack;
        return {m_area.x + leftW, m_area.y, m_area.w - leftW - stackW, m_area.h};
    }

    Rect Workspace::stackZone() const {
        const double stackW = m_area.w * m_widths.stack;
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
