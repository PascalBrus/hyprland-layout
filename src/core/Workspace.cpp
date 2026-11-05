#include "core/Workspace.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

namespace threecol {

    namespace {

        // The window after `id` in `windows`, or before it; nothing at the
        // ends or if it isn't there.
        std::optional<WindowId> neighbour(const std::vector<WindowId>& windows, WindowId id, bool after) {
            const auto it = std::ranges::find(windows, id);
            if (it == windows.end())
                return std::nullopt;
            if (after)
                return std::next(it) == windows.end() ? std::nullopt : std::optional{*std::next(it)};
            return it == windows.begin() ? std::nullopt : std::optional{*std::prev(it)};
        }

    }

    Workspace::Workspace(Rect area) : m_area(area) {}

    void Workspace::setArea(Rect area) {
        m_area = area;
    }

    SettingsChange Workspace::setSettings(const Settings& global, const LayoutOpts& opts, std::string_view workspaceName) {
        auto resolved = resolveSettings(global, opts, workspaceName);
        m_settings    = resolved.settings;
        SettingsChange change{.warnings = std::move(resolved.warnings), .noRoom = {}};
        const auto grabbedZone = m_gesture ? zoneOf(m_gesture->id) : std::nullopt;
        const bool moved       = fitToRowLimits(change.noRoom);
        if (!m_gesture)
            return change;
        // No more ticks come for a window that left.
        if (std::ranges::contains(change.noRoom, m_gesture->id)) {
            m_gesture.reset();
            return change;
        }
        // Moved windows change the Rows under a Row edge drag; a window
        // pushed to the other side, or a Divider whose zone went missing,
        // no longer matches what was grabbed.
        const bool rowsMoved      = moved && m_gesture->edge;
        const bool sideChanged    = zoneOf(m_gesture->id) != grabbedZone;
        const bool dividerMissing = m_gesture->divider && !exists(*m_gesture->divider == Divider::LeftMaster ? Zone::Left : Zone::Right);
        if (rowsMoved || sideChanged || dividerMissing)
            m_gesture->over = true;
        return change;
    }

    void Workspace::Rows::insert(std::size_t row, WindowId id) {
        windows.insert(windows.begin() + static_cast<std::ptrdiff_t>(row), id);
        heights.insert(row);
    }

    Workspace::End Workspace::endNearestMaster(Zone side) {
        return side == Zone::Left ? End::Bottom : End::Top;
    }

    bool Workspace::masterIsForwardOf(Zone side) {
        // Along the Chain a side's Rows run top to bottom.
        return endNearestMaster(side) == End::Bottom;
    }

    Workspace::Zone Workspace::otherSide(Zone side) {
        return side == Zone::Left ? Zone::Right : Zone::Left;
    }

    RowLimit Workspace::rowLimit(Zone side) const {
        return side == Zone::Left ? m_settings.leftRows : m_settings.rightRows;
    }

    bool Workspace::exists(Zone side) const {
        return rowLimit(side).role() != Role::Missing;
    }

    bool Workspace::hasRoom(Zone side) const {
        const auto limit = rowLimit(side).rows();
        return !limit || std::cmp_less(rowsOf(side)->windows.size(), *limit);
    }

    bool Workspace::fitToRowLimits(std::vector<WindowId>& noRoom) {
        bool moved = false;
        for (const Zone side : {Zone::Left, Zone::Right}) {
            const auto limit = rowLimit(side).rows();
            auto&      rows  = *rowsOf(side);
            if (!limit || std::cmp_less_equal(rows.windows.size(), *limit))
                continue;

            const auto           keep = static_cast<std::size_t>(std::max(0, *limit));
            std::vector<WindowId> out(rows.windows.begin() + static_cast<std::ptrdiff_t>(keep), rows.windows.end());
            while (rows.windows.size() > keep) {
                rows.heights.remove(rows.windows.size() - 1);
                rows.windows.pop_back();
            }

            // In Chain order, each after the one before, while there's room.
            const Zone  to    = otherSide(side);
            auto&       other = *rowsOf(to);
            std::size_t row   = endNearestMaster(to) == End::Top ? 0 : other.windows.size();
            for (const WindowId id : out) {
                if (hasRoom(to))
                    other.insert(row++, id);
                else
                    noRoom.push_back(id);
            }
            moved = true;
        }
        return moved;
    }

    Workspace::Rows* Workspace::rowsOf(Zone zone) {
        return const_cast<Rows*>(std::as_const(*this).rowsOf(zone));
    }

    const Workspace::Rows* Workspace::rowsOf(Zone zone) const {
        switch (zone) {
            case Zone::Left: return &m_left;
            case Zone::Right: return &m_right;
            case Zone::Master: return nullptr;
        }
        return nullptr;
    }

    const Settings& Workspace::settings() const {
        return m_settings;
    }

    std::optional<Workspace::Zone> Workspace::nextZone() const {
        if (!m_master)
            return Zone::Master;
        // Then the fill_first side up to its Row limit, then the other.
        const auto order = m_settings.fillFirst == Side::Left ? std::array{Zone::Left, Zone::Right} : std::array{Zone::Right, Zone::Left};
        for (const Zone side : order) {
            if (hasRoom(side))
                return side;
        }
        return std::nullopt;
    }

    bool Workspace::addWindow(WindowId id) {
        if (rectOf(id))
            return true;

        const auto zone = nextZone();
        if (!zone)
            return false;
        if (m_gesture && zoneOf(m_gesture->id) == zone)
            m_gesture->over = true;

        // New windows join a side zone at its bottom.
        if (auto* rows = rowsOf(*zone))
            rows->insert(rows->windows.size(), id);
        else
            m_master = id;
        return true;
    }

    std::vector<WindowId> Workspace::windowsInChainOrder() const {
        std::vector<WindowId> windows = m_left.windows;
        if (m_master)
            windows.push_back(*m_master);
        windows.insert(windows.end(), m_right.windows.begin(), m_right.windows.end());
        return windows;
    }

    std::optional<WindowId> Workspace::removeWindow(WindowId id) {
        const auto windows = windowsInChainOrder();
        if (!std::ranges::contains(windows, id))
            return std::nullopt;

        // No more ticks come for a window that left. One leaving its zone
        // moves the Rows under the drag.
        if (m_gesture && m_gesture->id == id)
            m_gesture.reset();
        else if (m_gesture && zoneOf(m_gesture->id) == zoneOf(id))
            m_gesture->over = true;

        takeOut(id);

        if (const auto next = neighbour(windows, id, true))
            return next;
        return neighbour(windows, id, false);
    }

    void Workspace::keepOnly(const std::vector<WindowId>& ids) {
        m_gesture.reset();
        for (const WindowId id : windowsInChainOrder()) {
            if (!std::ranges::contains(ids, id))
                takeOut(id);
        }
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
            return std::abs(a.leftWidth - b.leftWidth) < EPSILON && std::abs(a.rightWidth - b.rightWidth) < EPSILON;
        }

    }

    std::optional<Dividers> Workspace::dividers() const {
        return m_dividers;
    }

    void Workspace::setDividers(std::optional<Dividers> dividers) {
        m_dividers = dividers;
    }

    Dividers Workspace::savedDividers() const {
        return m_dividers.value_or(Dividers{m_settings.leftWidth, m_settings.rightWidth});
    }

    Dividers Workspace::currentDividers() const {
        // Limits may have changed since the resize: clamp the way settings
        // are made to fit, keeping m_dividers so a looser limit brings them
        // back. A missing zone has no width, Master taking it.
        const Dividers saved = savedDividers();
        Dividers       fitted{exists(Zone::Left) ? std::max(saved.leftWidth, m_settings.minWidth) : 0,
                        exists(Zone::Right) ? std::max(saved.rightWidth, m_settings.minWidth) : 0};
        leaveMasterItsMinimum(fitted.leftWidth, fitted.rightWidth, m_settings.minMasterWidth);
        return fitted;
    }

    void Workspace::keepDividers(const Dividers& dividers) {
        // A missing zone's saved width stays, so it comes back where it was.
        const Dividers saved = savedDividers();
        m_dividers           = Dividers{exists(Zone::Left) ? dividers.leftWidth : saved.leftWidth, exists(Zone::Right) ? dividers.rightWidth : saved.rightWidth};
    }

    std::optional<Workspace::Zone> Workspace::zoneOf(WindowId id) const {
        if (m_master == id)
            return Zone::Master;
        if (std::ranges::contains(m_left.windows, id))
            return Zone::Left;
        if (std::ranges::contains(m_right.windows, id))
            return Zone::Right;
        return std::nullopt;
    }

    namespace {

        Direction toDirection(char c) {
            switch (c) {
                case 'l': return Direction::Left;
                case 'r': return Direction::Right;
                case 'u': return Direction::Up;
                default: return Direction::Down;
            }
        }

    }

    bool needsFocusedTile(std::string_view message) {
        return message != "focusmaster" && message != "reset";
    }

    MessageResult Workspace::layoutMessage(std::string_view message, std::optional<WindowId> focused) {
        const auto result = applyLayoutMessage(message, focused);
        // A drag's later ticks would undo the change.
        if (result.changed && m_gesture)
            m_gesture->over = true;
        return result;
    }

    MessageResult Workspace::applyLayoutMessage(std::string_view message, std::optional<WindowId> focused) {
        if (message == "reset") {
            const bool left    = m_left.heights.equalise(m_settings.minRowHeight);
            const bool heights = m_right.heights.equalise(m_settings.minRowHeight) || left;
            return {.changed = std::exchange(m_dividers, std::nullopt).has_value() || heights};
        }
        if (message == "focusmaster")
            return {.focus = m_master};

        const auto zone = focused ? zoneOf(*focused) : std::nullopt;
        if (!zone)
            return {};
        if (message == "resize wider")
            return {.changed = resize(*zone, m_settings.step)};
        if (message == "resize narrower")
            return {.changed = resize(*zone, -m_settings.step)};
        if (message == "resize taller")
            return {.changed = resizeRow(*focused, m_settings.step)};
        if (message == "resize shorter")
            return {.changed = resizeRow(*focused, -m_settings.step)};
        if (message == "focus l")
            return {.focus = chainNeighbour(*focused, false)};
        if (message == "focus r")
            return {.focus = chainNeighbour(*focused, true)};
        if (message == "focus u")
            return {.focus = rowNeighbour(*focused, false)};
        if (message == "focus d")
            return {.focus = rowNeighbour(*focused, true)};
        if (message == "move l" || message == "move r" || message == "move u" || message == "move d") {
            const bool moved = applyMove(*focused, toDirection(message.back()));
            return {.changed = moved, .focus = moved ? focused : std::nullopt};
        }
        if (message == "swapwithmaster") {
            if (*zone == Zone::Master)
                return {};
            toMaster(*focused);
            return {.changed = true, .focus = focused};
        }
        return {};
    }

    std::optional<WindowId> Workspace::chainNeighbour(WindowId id, bool right) const {
        return neighbour(windowsInChainOrder(), id, right);
    }

    bool Workspace::move(WindowId id, Direction direction) {
        const bool moved = applyMove(id, direction);
        if (moved && m_gesture)
            m_gesture->over = true;
        return moved;
    }

    bool Workspace::swapWindows(WindowId a, WindowId b) {
        if (a == b || !slotOf(a) || !slotOf(b))
            return false;
        swapPlaces(a, b);
        if (m_gesture)
            m_gesture->over = true;
        return true;
    }

    WindowId* Workspace::slotOf(WindowId id) {
        if (m_master == id)
            return &*m_master;
        for (Rows* rows : {&m_left, &m_right}) {
            if (const auto it = std::ranges::find(rows->windows, id); it != rows->windows.end())
                return &*it;
        }
        return nullptr;
    }

    void Workspace::swapPlaces(WindowId a, WindowId b) {
        std::swap(*slotOf(a), *slotOf(b));
    }

    void Workspace::takeOut(WindowId id) {
        if (m_master == id)
            m_master.reset();
        for (Rows* rows : {&m_left, &m_right}) {
            if (const auto row = std::ranges::find(rows->windows, id); row != rows->windows.end()) {
                rows->heights.remove(row - rows->windows.begin());
                rows->windows.erase(row);
            }
        }
    }

    void Workspace::toMaster(WindowId id) {
        if (m_master)
            swapPlaces(id, *m_master);
        else {
            takeOut(id);
            m_master = id;
        }
    }

    bool Workspace::applyMove(WindowId id, Direction direction) {
        const auto zone = zoneOf(id);
        if (!zone)
            return false;

        // Along the Chain, a side zone's Rows run top to bottom, so right is
        // down inside them.
        const bool forward = direction == Direction::Right || direction == Direction::Down;
        if (const auto other = rowNeighbour(id, forward)) {
            swapPlaces(id, *other);
            return true;
        }
        if (direction == Direction::Up || direction == Direction::Down)
            return false;

        if (*zone != Zone::Master) {
            // Off the end nearest Master is Master; the other ways are the
            // Chain's ends.
            if (masterIsForwardOf(*zone) != forward)
                return false;
            toMaster(id);
            return true;
        }

        const Zone side = forward ? Zone::Right : Zone::Left;
        if (hasRoom(side)) {
            joinNearMaster(side, id);
            return true;
        }
        const auto& windows = rowsOf(side)->windows;
        if (windows.empty())
            return false;
        swapPlaces(id, endNearestMaster(side) == End::Top ? windows.front() : windows.back());
        return true;
    }

    std::optional<WindowId> Workspace::rowNeighbour(WindowId id, bool down) const {
        const auto  zone = zoneOf(id);
        const auto* rows = zone ? rowsOf(*zone) : nullptr;
        if (!rows)
            return std::nullopt;
        return neighbour(rows->windows, id, down);
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
        auto& [left, right]    = dividers;
        switch (zone) {
            case Zone::Left: left = moved(left, by, minWidth, 1 - right - minMaster); break;
            case Zone::Right: right = moved(right, by, minWidth, 1 - left - minMaster); break;
            // Each Divider moves an equal share of the step, so Master stays
            // centred: half each, or all of it for the only one. A missing
            // zone has no Divider; with neither, nothing moves.
            case Zone::Master: {
                const bool hasLeft  = exists(Zone::Left);
                const bool hasRight = exists(Zone::Right);
                const int  count    = int{hasLeft} + int{hasRight};
                if (count == 0)
                    return false;
                if (by > 0) {
                    // A side at min_width stays; the free side still moves.
                    if (hasLeft)
                        left = moved(left, -by / count, minWidth, left);
                    if (hasRight)
                        right = moved(right, -by / count, minWidth, right);
                } else {
                    // Narrowing stops when Master reaches min_master_width.
                    const double room  = std::max(0.0, 1 - left - right - minMaster);
                    const double share = std::min(-by, room) / count;
                    if (hasLeft)
                        left += share;
                    if (hasRight)
                        right += share;
                }
                break;
            }
        }
        if (same(dividers, currentDividers()))
            return false;
        keepDividers(dividers);
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

    std::optional<Workspace::Divider> Workspace::dividerFor(Zone zone, Corner corner) const {
        switch (zone) {
            case Zone::Left: return Divider::LeftMaster;
            case Zone::Right: return Divider::MasterRight;
            case Zone::Master:
                // A missing zone has no Divider: Master moves the other one.
                if (!exists(Zone::Left))
                    return exists(Zone::Right) ? std::optional{Divider::MasterRight} : std::nullopt;
                if (!exists(Zone::Right))
                    return Divider::LeftMaster;
                // The grabbed half is the side of the nearer Divider. With no
                // corner, the right one, as Hyprland grows a window to the right.
                return isLeft(corner) ? Divider::LeftMaster : Divider::MasterRight;
        }
        return std::nullopt;
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

    Dividers Workspace::moveDivider(std::optional<Divider> divider, Dividers from, double px) const {
        if (!divider)
            return from;
        auto& [left, right]   = from;
        const double by       = px / m_area.w;
        const double minWidth = m_settings.minWidth;
        const double room     = 1 - m_settings.minMasterWidth;
        switch (*divider) {
            case Divider::LeftMaster: left = within(left + by, minWidth, room - right); break;
            case Divider::MasterRight: right = within(right - by, minWidth, room - left); break;
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
        keepDividers(dividers);
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
        const double rightW = m_area.w * currentDividers().rightWidth;
        return {m_area.x + leftW, m_area.y, m_area.w - leftW - rightW, m_area.h};
    }

    Rect Workspace::rightZone() const {
        const double rightW = m_area.w * currentDividers().rightWidth;
        return {m_area.x + m_area.w - rightW, m_area.y, rightW, m_area.h};
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
            case Zone::Right: return rightZone();
        }
        return {};
    }

    Rect Workspace::rowRect(Zone zone, std::size_t row) const {
        return rowOf(zoneRect(zone), rowsOf(zone)->heights.fitted(m_settings.minRowHeight), row);
    }

    std::optional<Rect> Workspace::nextWindowRect() const {
        const auto zone = nextZone();
        if (!zone)
            return std::nullopt;
        const auto* rows = rowsOf(*zone);
        if (!rows)
            return masterZone();
        RowHeights next = rows->heights;
        next.add();
        return rowOf(zoneRect(*zone), next.fitted(m_settings.minRowHeight), rows->windows.size());
    }

    namespace {

        // The "add" area: this much of a zone's bottom Row, at its bottom.
        constexpr double ADD_AREA = 0.25;

        bool inAddArea(const Rect& bottomRow, double y) {
            return y >= bottomRow.y + bottomRow.h * (1 - ADD_AREA);
        }

    }

    std::optional<Workspace::Zone> Workspace::zoneAt(double x, double y) const {
        if (x < m_area.x || x >= m_area.x + m_area.w || y < m_area.y || y >= m_area.y + m_area.h)
            return std::nullopt;
        if (x < masterZone().x)
            return Zone::Left;
        if (x < rightZone().x)
            return Zone::Master;
        return Zone::Right;
    }

    std::size_t Workspace::rowAt(Zone zone, double y) const {
        const auto& windows = rowsOf(zone)->windows;
        for (std::size_t row = 0; row + 1 < windows.size(); ++row) {
            const Rect rect = rowRect(zone, row);
            if (y < rect.y + rect.h)
                return row;
        }
        return windows.size() - 1;
    }

    bool Workspace::drop(WindowId id, double x, double y) {
        const bool dropped = applyDrop(id, x, y);
        if (dropped && m_gesture)
            m_gesture->over = true;
        return dropped;
    }

    std::optional<Rect> Workspace::dropIndicator(WindowId id, double x, double y) const {
        // The drop itself, tried on a copy, so the two never disagree.
        Workspace after = *this;
        if (!after.applyDrop(id, x, y))
            return std::nullopt;
        return after.rectOf(id);
    }

    bool Workspace::applyDrop(WindowId id, double x, double y) {
        const auto zone = zoneAt(x, y);
        if (!zone || !zoneOf(id))
            return false;

        Rows* rows = rowsOf(*zone);
        if (!rows) {
            if (m_master == id)
                return false;
            toMaster(id);
            return true;
        }

        auto& windows = rows->windows;
        if (windows.empty()) {
            joinBottom(*rows, id);
            return true;
        }
        const std::size_t row  = rowAt(*zone, y);
        const WindowId    onto = windows[row];
        if (onto == id)
            return false;
        if (row + 1 == windows.size() && inAddArea(rowRect(*zone, row), y)) {
            // In its own zone it moves to the bottom; the Rows keep their heights.
            if (const auto from = std::ranges::find(windows, id); from != windows.end()) {
                std::rotate(from, std::next(from), windows.end());
                return true;
            }
            if (hasRoom(*zone)) {
                joinBottom(*rows, id);
                return true;
            }
        }
        swapPlaces(id, onto);
        return true;
    }

    void Workspace::joinBottom(Rows& rows, WindowId id) {
        takeOut(id);
        rows.insert(rows.windows.size(), id);
    }

    void Workspace::joinNearMaster(Zone side, WindowId id) {
        takeOut(id);
        auto& rows = *rowsOf(side);
        rows.insert(endNearestMaster(side) == End::Top ? 0 : rows.windows.size(), id);
    }

}
