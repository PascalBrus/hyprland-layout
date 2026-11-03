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
        // Moved windows change the Rows under a Row edge drag.
        const bool rowDrag = m_gesture && m_gesture->edge;
        if (fitLeftToRowLimit() && rowDrag)
            m_gesture->over = true;
        return std::move(resolved.warnings);
    }

    std::size_t Workspace::leftRowLimit() const {
        return static_cast<std::size_t>(std::max(1, m_settings.leftRows));
    }

    bool Workspace::fitLeftToRowLimit() {
        const auto limit = leftRowLimit();
        auto&      left  = m_left.windows;
        if (left.size() <= limit)
            return false;
        // Bottom up, each moved window goes on top of the ones moved before.
        while (left.size() > limit) {
            m_stack.windows.insert(m_stack.windows.begin(), left.back());
            m_stack.heights.insert(0);
            m_left.heights.remove(left.size() - 1);
            left.pop_back();
        }
        return true;
    }

    Workspace::Rows* Workspace::rowsOf(Zone zone) {
        return const_cast<Rows*>(std::as_const(*this).rowsOf(zone));
    }

    const Workspace::Rows* Workspace::rowsOf(Zone zone) const {
        switch (zone) {
            case Zone::Left: return &m_left;
            case Zone::Stack: return &m_stack;
            case Zone::Master: return nullptr;
        }
        return nullptr;
    }

    const Settings& Workspace::settings() const {
        return m_settings;
    }

    Workspace::Zone Workspace::nextZone() const {
        if (!m_master)
            return Zone::Master;
        if (m_left.windows.size() < leftRowLimit())
            return Zone::Left;
        return Zone::Stack;
    }

    void Workspace::addWindow(WindowId id) {
        if (rectOf(id))
            return;

        const Zone zone = nextZone();
        if (m_gesture && zoneOf(m_gesture->id) == zone)
            m_gesture->over = true;

        if (auto* rows = rowsOf(zone)) {
            rows->windows.push_back(id);
            rows->heights.add();
        } else
            m_master = id;
    }

    std::vector<WindowId> Workspace::windowsInChainOrder() const {
        std::vector<WindowId> windows = m_left.windows;
        if (m_master)
            windows.push_back(*m_master);
        windows.insert(windows.end(), m_stack.windows.begin(), m_stack.windows.end());
        return windows;
    }

    std::optional<WindowId> Workspace::removeWindow(WindowId id) {
        const auto windows = windowsInChainOrder();
        const auto it      = std::ranges::find(windows, id);
        if (it == windows.end())
            return std::nullopt;

        // No more ticks come for a window that left. One leaving its zone
        // moves the Rows under the drag.
        if (m_gesture && m_gesture->id == id)
            m_gesture.reset();
        else if (m_gesture && zoneOf(m_gesture->id) == zoneOf(id))
            m_gesture->over = true;

        if (m_master == id)
            m_master.reset();
        for (Rows* rows : {&m_left, &m_stack}) {
            if (const auto row = std::ranges::find(rows->windows, id); row != rows->windows.end()) {
                rows->heights.remove(row - rows->windows.begin());
                rows->windows.erase(row);
            }
        }

        if (std::next(it) != windows.end())
            return *std::next(it);
        if (it != windows.begin())
            return *std::prev(it);
        return std::nullopt;
    }

    std::optional<Rect> Workspace::rectOf(WindowId id) const {
        const auto zone = zoneOf(id);
        if (!zone)
            return std::nullopt;
        if (const auto* rows = rowsOf(*zone))
            return rowRect(*zone, std::ranges::find(rows->windows, id) - rows->windows.begin());
        return masterZone();
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
        if (std::ranges::contains(m_left.windows, id))
            return Zone::Left;
        if (std::ranges::contains(m_stack.windows, id))
            return Zone::Stack;
        return std::nullopt;
    }

    bool Workspace::layoutMessage(std::string_view message, std::optional<WindowId> focused) {
        const bool changed = applyLayoutMessage(message, focused);
        // A drag's later ticks would undo the change.
        if (changed && m_gesture)
            m_gesture->over = true;
        return changed;
    }

    bool Workspace::applyLayoutMessage(std::string_view message, std::optional<WindowId> focused) {
        if (message == "reset") {
            const bool left    = m_left.heights.equalise(m_settings.minRowHeight);
            const bool heights = m_stack.heights.equalise(m_settings.minRowHeight) || left;
            return std::exchange(m_dividers, std::nullopt).has_value() || heights;
        }

        const auto zone = focused ? zoneOf(*focused) : std::nullopt;
        if (!zone)
            return false;
        if (message == "resize wider")
            return resize(*zone, m_settings.step);
        if (message == "resize narrower")
            return resize(*zone, -m_settings.step);
        if (message == "resize taller")
            return resizeRow(*focused, m_settings.step);
        if (message == "resize shorter")
            return resizeRow(*focused, -m_settings.step);
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

    bool Workspace::resizeRow(WindowId id, double by) {
        const auto zone = zoneOf(id);
        auto*      rows = zone ? rowsOf(*zone) : nullptr;
        if (!rows)
            return false;
        const auto row = std::ranges::find(rows->windows, id) - rows->windows.begin();
        return rows->heights.resize(row, by, m_settings.minRowHeight);
    }

    namespace {

        // `width` clamped to [smallest, largest]; smallest wins if they cross.
        double within(double width, double smallest, double largest) {
            return std::max(smallest, std::min(width, largest));
        }

        bool isLeft(Corner corner) {
            return corner == Corner::TopLeft || corner == Corner::BottomLeft;
        }

        bool isTop(Corner corner) {
            return corner == Corner::TopLeft || corner == Corner::TopRight;
        }

    }

    Workspace::Divider Workspace::dividerFor(Zone zone, Corner corner) {
        switch (zone) {
            case Zone::Left: return Divider::LeftMaster;
            case Zone::Stack: return Divider::MasterStack;
            // The grabbed half is the side of the nearer Divider. With no
            // corner, the right one, as Hyprland grows a window to the right.
            case Zone::Master: return isLeft(corner) ? Divider::LeftMaster : Divider::MasterStack;
        }
        return Divider::MasterStack;
    }

    std::optional<Workspace::Edge> Workspace::edgeFor(WindowId id, Corner corner) const {
        const auto  zone = zoneOf(id);
        const auto* rows = zone ? rowsOf(*zone) : nullptr;
        if (!rows || rows->windows.size() < 2)
            return std::nullopt;
        const std::size_t row  = std::ranges::find(rows->windows, id) - rows->windows.begin();
        const std::size_t last = rows->windows.size() - 1;
        // With no corner, the bottom one, as Hyprland grows a window down.
        const std::size_t edge = row == 0 ? 1 : row == last ? row : isTop(corner) ? row : row + 1;
        return Edge{*zone, edge, rows->heights.fitted(m_settings.minRowHeight)};
    }

    bool Workspace::setHeights(Zone zone, std::vector<double> heights) {
        auto* rows = rowsOf(zone);
        return rows && rows->heights.set(std::move(heights), m_settings.minRowHeight);
    }

    Dividers Workspace::moveDivider(Divider divider, Dividers from, double px) const {
        auto& [left, stack]   = from;
        const double by       = px / m_area.w;
        const double minWidth = m_settings.minWidth;
        const double room     = 1 - m_settings.minMasterWidth;
        switch (divider) {
            case Divider::LeftMaster: left = within(left + by, minWidth, room - stack); break;
            case Divider::MasterStack: stack = within(stack - by, minWidth, room - left); break;
        }
        return from;
    }

    bool Workspace::dragResize(WindowId id, double dx, double dy, Corner corner) {
        const auto zone = zoneOf(id);
        if (!zone || m_area.w <= 0)
            return false;
        if (m_gesture && m_gesture->over)
            return false;

        if (!m_gesture || m_gesture->id != id) {
            m_gesture = Gesture{.id = id, .divider = dividerFor(*zone, corner), .atPress = currentDividers()};
            m_gesture->edge = edgeFor(id, corner);
        }
        m_gesture->travelX += dx;
        m_gesture->travelY += dy;
        // Measured from press, so the Divider and edge follow again as soon
        // as the cursor comes back from past a limit.
        const bool width  = setResized(moveDivider(m_gesture->divider, m_gesture->atPress, m_gesture->travelX));
        bool       height = false;
        if (const auto& edge = m_gesture->edge; edge && m_area.h > 0)
            height = setHeights(edge->zone, RowHeights::movedEdge(edge->atPress, edge->row, m_gesture->travelY / m_area.h, m_settings.minRowHeight));
        return width || height;
    }

    bool Workspace::resizeWindow(WindowId id, double dx, double dy, Corner corner) {
        const auto zone = zoneOf(id);
        if (!zone || m_area.w <= 0)
            return false;
        const bool width  = setResized(moveDivider(dividerFor(*zone, corner), currentDividers(), dx));
        bool       height = false;
        if (const auto edge = edgeFor(id, corner); edge && m_area.h > 0)
            height = setHeights(edge->zone, RowHeights::movedEdge(edge->atPress, edge->row, dy / m_area.h, m_settings.minRowHeight));
        return width || height;
    }

    bool Workspace::setResized(const Dividers& dividers) {
        if (same(dividers, currentDividers()))
            return false;
        m_dividers = dividers;
        return true;
    }

    void Workspace::endResize() {
        m_gesture.reset();
    }

    bool Workspace::resizing() const {
        return m_gesture && !m_gesture->over;
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

    namespace {

        // Row `row` of `zone`, given the zone's Row heights in use.
        Rect rowOf(const Rect& zone, const std::vector<double>& heights, std::size_t row) {
            double top = 0;
            for (std::size_t i = 0; i < row; ++i)
                top += heights[i];
            return {zone.x, zone.y + zone.h * top, zone.w, zone.h * heights[row]};
        }

    }

    Rect Workspace::zoneRect(Zone zone) const {
        switch (zone) {
            case Zone::Left: return leftZone();
            case Zone::Master: return masterZone();
            case Zone::Stack: return stackZone();
        }
        return {};
    }

    Rect Workspace::rowRect(Zone zone, std::size_t row) const {
        return rowOf(zoneRect(zone), rowsOf(zone)->heights.fitted(m_settings.minRowHeight), row);
    }

    Rect Workspace::nextWindowRect() const {
        const Zone  zone = nextZone();
        const auto* rows = rowsOf(zone);
        if (!rows)
            return masterZone();
        RowHeights next = rows->heights;
        next.add();
        return rowOf(zoneRect(zone), next.fitted(m_settings.minRowHeight), rows->windows.size());
    }

}
