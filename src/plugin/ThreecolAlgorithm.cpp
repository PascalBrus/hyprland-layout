#include "plugin/ThreecolAlgorithm.hpp"

#include "plugin/Config.hpp"
#include "plugin/SavedDividers.hpp"
#include "plugin/SettingsReporting.hpp"

#include <hyprland/src/config/shared/actions/ConfigActions.hpp>
#include <hyprland/src/config/shared/workspace/WorkspaceRuleManager.hpp>

#include <hyprland/src/desktop/state/FocusState.hpp>
#include <hyprland/src/desktop/state/ViewState.hpp>
#include <hyprland/src/desktop/view/window/Window.hpp>
#include <hyprland/src/layout/LayoutManager.hpp>
#include <hyprland/src/layout/algorithm/Algorithm.hpp>
#include <hyprland/src/layout/space/Space.hpp>
#include <hyprland/src/layout/target/Target.hpp>
#include <hyprland/src/managers/fullscreen/FullscreenController.hpp>
#include <hyprland/src/managers/fullscreen/handler/FullscreenHandler.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/output/Monitor.hpp>

#include <hyprland/src/config/ConfigValue.hpp>
#include <hyprland/src/config/shared/complex/ComplexDataTypes.hpp>

#include <algorithm>
#include <cmath>
#include <iterator>

namespace threecol {

    namespace {

        Rect toRect(const CBox& box) {
            return {box.x, box.y, box.width, box.height};
        }

        CBox toBox(const Rect& rect) {
            return {rect.x, rect.y, rect.w, rect.h};
        }

        // Whether `target` is being resized with a mouse bind. Hyprland's
        // trackpad resize gesture passes a corner too, but never releases.
        bool mouseResizing(const SP<Layout::ITarget>& target) {
            const auto& drag = g_layoutManager->dragController();
            const auto  mode = drag->mode();
            return drag->target() == target && (mode == MBIND_RESIZE || mode == MBIND_RESIZE_BLOCK_RATIO || mode == MBIND_RESIZE_FORCE_RATIO);
        }

        // Whether Hyprland is floating `target` to drag it with window.drag.
        // Before the drag threshold the window isn't floated yet, so it is
        // leaving for another reason, e.g. it closed. Hyprland sets
        // draggingTiled just after floating it.
        bool startingDrag(const SP<Layout::ITarget>& target) {
            const auto& drag = g_layoutManager->dragController();
            return drag->target() == target && drag->mode() == MBIND_MOVE && drag->dragThresholdReached() && !drag->draggingTiled();
        }

        // Where on `workspace`, laid out in `area`, the cursor drops
        // `dragged`. Nothing on a floating window, which isn't a tile to drop
        // on. In the outer gaps or over a bar, the nearest point of `area`;
        // off the workspace's monitor, the cursor, which is outside it.
        std::optional<Vector2D> dropPoint(const PHLWORKSPACE& workspace, const CBox& area, const PHLWINDOW& dragged) {
            const Vector2D cursor = g_pInputManager->getMouseCoordsInternal();
            constexpr auto under  = Desktop::View::RESERVED_EXTENTS | Desktop::View::INPUT_EXTENTS | Desktop::View::ALLOW_FLOATING;
            if (const auto window = Desktop::viewState()->hitTest().windowAt(cursor, under, dragged); window && window->isFloating())
                return std::nullopt;

            const auto monitor = workspace->m_monitor.lock();
            if (!monitor || !monitor->logicalBox().containsPoint(cursor) || area.width < 1 || area.height < 1)
                return cursor;
            // The area's right and bottom edges are outside it.
            return Vector2D{std::clamp(cursor.x, area.x, area.x + area.width - 1), std::clamp(cursor.y, area.y, area.y + area.height - 1)};
        }

        // The box Hyprland gives a tile placed at `node` in `area`, as
        // CWindowTarget::setPositionGlobal works it out: gaps_in on every edge
        // not at the area's edge, then the window's border and other
        // decorations.
        CBox tileBox(const CBox& node, const CBox& area, const PHLWORKSPACE& workspace, const PHLWINDOW& window) {
            static auto PGAPSIN = CConfigValue<Config::IComplexConfigValue>("general:gaps_in");
            const auto  rule    = Config::workspaceRuleMgr()->getWorkspaceRuleFor(workspace);
            const auto  gaps    = rule && rule->m_gapsIn ? *rule->m_gapsIn : *sc<Config::CCssGapData*>(PGAPSIN.ptr());
            const auto  sticks  = [](double a, double b) { return std::abs(a - b) < 2; };

            const double left   = sticks(node.x, area.x) ? 0 : gaps.m_left;
            const double top    = sticks(node.y, area.y) ? 0 : gaps.m_top;
            const double right  = sticks(node.x + node.w, area.x + area.w) ? 0 : gaps.m_right;
            const double bottom = sticks(node.y + node.h, area.y + area.h) ? 0 : gaps.m_bottom;
            const auto   decor  = window->getFullWindowReservedArea();

            CBox box = node.copy().round();
            box      = {box.x + left + decor.topLeft.x, box.y + top + decor.topLeft.y, box.w - left - right - decor.topLeft.x - decor.bottomRight.x,
                        box.h - top - bottom - decor.topLeft.y - decor.bottomRight.y};
            return box.round();
        }

        Corner toCorner(Layout::eRectCorner corner) {
            if (corner == Layout::CORNER_NONE)
                return Corner::None;
            if (Layout::edgeLeft(corner))
                return Layout::edgeTop(corner) ? Corner::TopLeft : Corner::BottomLeft;
            return Layout::edgeTop(corner) ? Corner::TopRight : Corner::BottomRight;
        }

        std::optional<Direction> toDirection(Math::eDirection dir) {
            switch (dir) {
                case Math::DIRECTION_LEFT: return Direction::Left;
                case Math::DIRECTION_RIGHT: return Direction::Right;
                case Math::DIRECTION_UP: return Direction::Up;
                case Math::DIRECTION_DOWN: return Direction::Down;
                default: return std::nullopt;
            }
        }

        // Every live instance, so a config reload can reach them all.
        std::vector<CThreecolAlgorithm*> g_instances;

    }

    CThreecolAlgorithm::CThreecolAlgorithm() : m_model(Rect{}) {
        g_instances.push_back(this);
        m_onDragEnded = g_layoutManager->dragController()->m_events.ended.listen([this] { dragEnded(); });
    }

    CThreecolAlgorithm::~CThreecolAlgorithm() {
        std::erase(g_instances, this);
        // Gone mid-drag, e.g. the layout changed: save the sizes so far.
        if (const auto workspace = m_dragWorkspace.lock(); workspace && m_model.resizing())
            saveDividers(workspace, m_model.dividers());
    }

    PHLWORKSPACE CThreecolAlgorithm::workspace() const {
        return m_parent && m_parent->space() ? m_parent->space()->workspace() : nullptr;
    }

    std::vector<std::string> CThreecolAlgorithm::applySettings() {
        // A lower Row limit moves Left windows, which ends a drag.
        const bool wasResizing = m_model.resizing();
        const auto workspace   = this->workspace();
        std::vector<std::string> warnings;
        if (workspace) {
            const auto rule = Config::workspaceRuleMgr()->getWorkspaceRuleFor(workspace);
            warnings        = m_model.setSettings(globalSettings(), rule ? rule->m_layoutopts : LayoutOpts{}, workspace->displayName());
        } else
            warnings = m_model.setSettings(globalSettings());
        keepDividersIfDragEnded(wasResizing);
        return warnings;
    }

    std::vector<std::string> applySettingsToAllWorkspaces() {
        std::vector<std::string> warnings;
        for (auto* instance : g_instances)
            std::ranges::move(instance->applySettings(), std::back_inserter(warnings));
        return warnings;
    }

    std::optional<SDropIndicator> currentDropIndicator() {
        for (const auto* instance : g_instances) {
            if (auto indicator = instance->dropIndicator())
                return indicator;
        }
        return std::nullopt;
    }

    std::optional<SDropIndicator> CThreecolAlgorithm::dropIndicator() const {
        const auto workspace = this->workspace();
        const auto target    = m_dragged ? m_dragged->target.lock() : nullptr;
        const auto window    = target ? target->window() : nullptr;
        if (!workspace || !window || !workspace->visible() || Fullscreen::controller()->hasFullscreen(workspace, std::nullopt))
            return std::nullopt;

        // The same point and core drop that dropped() uses.
        const CBox area  = m_parent->space()->workArea();
        const auto point = dropPoint(workspace, area, window);
        const auto rect  = point ? m_model.dropIndicator(m_dragged->id, point->x, point->y) : std::nullopt;
        if (!rect)
            return std::nullopt;
        return SDropIndicator{window, workspace->m_monitor, tileBox(toBox(*rect), area, workspace, window)};
    }

    void recalculateAllWorkspaces() {
        for (auto* instance : g_instances)
            instance->recalculate();
    }

    void CThreecolAlgorithm::syncArea() {
        if (m_parent && m_parent->space())
            m_model.setArea(toRect(m_parent->space()->workArea()));
    }

    std::vector<std::optional<Rect>> CThreecolAlgorithm::tileRects() const {
        std::vector<std::optional<Rect>> rects;
        for (const auto& [id, target] : m_targets)
            rects.push_back(m_model.rectOf(id));
        return rects;
    }

    bool CThreecolAlgorithm::refreshModel() {
        const auto before = tileRects();
        syncArea();
        applySettings();

        // Mid-drag, the model's Dividers are newer than the saved ones.
        if (const auto workspace = this->workspace(); workspace && !m_model.resizing())
            m_model.setDividers(savedDividers(workspace));
        return tileRects() != before;
    }

    void CThreecolAlgorithm::keepDividers() {
        if (const auto workspace = this->workspace())
            saveDividers(workspace, m_model.dividers());
    }

    void CThreecolAlgorithm::keepDividersIfChanged(const std::optional<Dividers>& before) {
        // A height change alone has nothing to save.
        if (m_model.dividers() != before)
            keepDividers();
    }

    void CThreecolAlgorithm::keepDividersIfDragEnded(bool wasResizing) {
        if (wasResizing && !m_model.resizing())
            keepDividers();
    }

    void CThreecolAlgorithm::dragEnded() {
        const bool wasResizing = m_model.resizing();
        m_model.endResize();
        keepDividersIfDragEnded(wasResizing);
        // A drop back on this workspace came first, through movedTarget.
        if (m_dragged)
            dragLeft();
    }

    void CThreecolAlgorithm::dropped(WindowId id) {
        const auto workspace   = this->workspace();
        const auto target      = targetOf(id);
        refreshModel();
        const bool wasResizing = m_model.resizing();
        if (workspace && target && !Fullscreen::controller()->hasFullscreen(workspace, std::nullopt)) {
            if (const auto point = dropPoint(workspace, m_parent->space()->workArea(), target->window()))
                m_model.drop(id, point->x, point->y);
        }
        keepDividersIfDragEnded(wasResizing);
        // Hyprland focuses the dragged window after the drop.
        recalculate();
    }

    void CThreecolAlgorithm::dragLeft() {
        const WindowId id          = std::exchange(m_dragged, std::nullopt)->id;
        const bool     wasResizing = m_model.resizing();
        m_model.removeWindow(id);
        keepDividersIfDragEnded(wasResizing);
        std::erase_if(m_targets, [id](const auto& entry) { return entry.first == id; });
        recalculate();
    }

    void CThreecolAlgorithm::newTarget(SP<Layout::ITarget> target) {
        if (idOf(target))
            return;

        const WindowId id          = m_nextId++;
        const bool     wasResizing = m_model.resizing();
        m_targets.emplace_back(id, target);
        m_model.addWindow(id);
        keepDividersIfDragEnded(wasResizing);
        recalculate();
    }

    void CThreecolAlgorithm::movedTarget(SP<Layout::ITarget> target, std::optional<Vector2D>) {
        // The dragged tile is tiled again: a drop if the mouse let go of it.
        // A float toggle mid-drag puts it back in its place.
        if (m_dragged && m_dragged->target.lock() == target) {
            const WindowId id = std::exchange(m_dragged, std::nullopt)->id;
            if (g_layoutManager->dragController()->wasDraggingWindow())
                dropped(id);
            else
                recalculate();
            return;
        }
        newTarget(target);
    }

    void CThreecolAlgorithm::removeTarget(SP<Layout::ITarget> target) {
        const auto it = std::ranges::find_if(m_targets, [&](const auto& entry) { return entry.second.lock() == target; });
        if (it == m_targets.end())
            return;

        // Floated to be dragged: its place stays open until the drop, and
        // nothing else moves.
        if (startingDrag(target)) {
            m_dragged = SDragged{target, it->first};
            return;
        }

        m_focusAfterClose.reset();
        const bool wasResizing = m_model.resizing();
        if (const auto next = m_model.removeWindow(it->first))
            m_focusAfterClose = SFocusAfterClose{target, *next};
        keepDividersIfDragEnded(wasResizing);
        m_targets.erase(it);
        recalculate();
    }

    void CThreecolAlgorithm::resizeTarget(const Vector2D& Δ, SP<Layout::ITarget> target, Layout::eRectCorner corner) {
        // Hidden tiles aren't resized under a fullscreen or maximised window.
        const auto workspace = this->workspace();
        if (!workspace || Fullscreen::controller()->hasFullscreen(workspace, std::nullopt))
            return;

        const auto id = idOf(target);
        if (!id)
            return;

        const bool refreshed = refreshModel();
        bool       resized   = false;
        // A mouse drag is saved on release; anything else right away.
        if (mouseResizing(target)) {
            m_dragWorkspace = workspace;
            resized         = m_model.dragResize(*id, Δ.x, Δ.y, toCorner(corner));
        } else {
            const auto before = m_model.dividers();
            resized           = m_model.resizeWindow(*id, Δ.x, Δ.y, toCorner(corner));
            if (resized)
                keepDividersIfChanged(before);
        }
        if (resized || refreshed)
            recalculate();
    }

    void CThreecolAlgorithm::recalculate(Layout::eRecalculateReason) {
        if (!m_parent || !m_parent->space())
            return;

        if (Fullscreen::controller()->hasFullscreen(workspace(), true)) {
            m_defaultFullscreenHandler->syncTargetSizeAndPosition();
            return;
        }

        // Forget targets that died without a removeTarget, so they don't hold a zone.
        const bool wasResizing = m_model.resizing();
        std::erase_if(m_targets, [this](const auto& entry) {
            if (!entry.second.expired())
                return false;
            m_model.removeWindow(entry.first);
            return true;
        });
        keepDividersIfDragEnded(wasResizing);

        refreshModel();
        if (!std::exchange(m_settingsChecked, true))
            reportSettingsLater();

        for (const auto& [id, weak] : m_targets) {
            // The dragged tile follows the cursor.
            if (m_dragged && m_dragged->id == id)
                continue;
            if (const auto rect = m_model.rectOf(id))
                weak.lock()->setPositionGlobal(toBox(*rect));
        }
    }

    std::optional<WindowId> CThreecolAlgorithm::idOf(const SP<Layout::ITarget>& target) const {
        if (m_dragged && m_dragged->target.lock() == target)
            return std::nullopt;
        const auto it = std::ranges::find_if(m_targets, [&](const auto& entry) { return entry.second.lock() == target; });
        return it == m_targets.end() ? std::nullopt : std::optional{it->first};
    }

    SP<Layout::ITarget> CThreecolAlgorithm::targetOf(WindowId id) const {
        const auto it = std::ranges::find_if(m_targets, [&](const auto& entry) { return entry.first == id; });
        return it == m_targets.end() ? nullptr : it->second.lock();
    }

    void CThreecolAlgorithm::swapTargets(SP<Layout::ITarget> a, SP<Layout::ITarget> b) {
        // Hyprland recalculates after a swap. Mid-drag, the dragged
        // window's place stays where it was.
        if (m_dragged)
            return;
        const auto idA = idOf(a);
        const auto idB = idOf(b);
        if (idA && idB) {
            refreshModel();
            const bool wasResizing = m_model.resizing();
            m_model.swapWindows(*idA, *idB);
            keepDividersIfDragEnded(wasResizing);
            return;
        }
        // Only one is ours, e.g. a swap across workspaces or with a floating
        // window: the other takes its place.
        for (auto& [id, target] : m_targets) {
            if (idA && id == *idA)
                target = b;
            else if (idB && id == *idB)
                target = a;
        }
    }

    void CThreecolAlgorithm::moveTargetInDirection(SP<Layout::ITarget> t, Math::eDirection dir, bool) {
        // The same move as `move <dir>`. The window moves, not focus, so
        // `silent` changes nothing: focus stays on it either way.
        const auto workspace = this->workspace();
        const auto id        = idOf(t);
        const auto direction = toDirection(dir);
        if (!workspace || !id || !direction || m_dragged || Fullscreen::controller()->hasFullscreen(workspace, std::nullopt))
            return;

        const bool refreshed   = refreshModel();
        const bool wasResizing = m_model.resizing();
        const bool moved       = m_model.move(*id, *direction);
        keepDividersIfDragEnded(wasResizing);
        if (moved || refreshed)
            recalculate();
    }

    SP<Layout::ITarget> CThreecolAlgorithm::getNextCandidate(SP<Layout::ITarget> old) {
        const auto hint = std::exchange(m_focusAfterClose, std::nullopt);
        if (!old || !hint || hint->closed.lock() != old)
            return nullptr; // Hyprland falls back to another tiled window.

        return targetOf(hint->next);
    }

    Config::ErrorResult CThreecolAlgorithm::layoutMsg(const std::string_view& sv) {
        // Messages that don't apply do nothing and report no error. Mid-drag
        // none do: the dragged window's place stays where it was.
        const auto workspace = this->workspace();
        if (!workspace || m_dragged || Fullscreen::controller()->hasFullscreen(workspace, std::nullopt))
            return {};

        // A focused window must be one of this workspace's tiles: nothing
        // applies to a floating one. With nothing focused, only reset and
        // focusmaster do.
        std::optional<WindowId> focused;
        if (const auto window = Desktop::focusState()->window(); window && window->m_workspace == workspace) {
            if (Fullscreen::controller()->isFullscreen(window))
                return {};
            focused = idOf(window->layoutTarget());
            // focusmaster needs no focused tile, so it works from a floating window too.
            if (!focused && sv != "focusmaster")
                return {};
        }

        const bool refreshed   = refreshModel();
        const auto before      = m_model.dividers();
        const bool wasResizing = m_model.resizing();
        const auto result      = m_model.layoutMessage(sv, focused);
        // A change mid-drag ends it, saving like a release.
        if (wasResizing && !m_model.resizing())
            keepDividers();
        else if (result.changed)
            keepDividersIfChanged(before);
        if (result.changed || refreshed)
            recalculate();

        // After placing, so the cursor warps to where the window now is.
        if (result.focus && result.focus != focused) {
            // Focusing a window of this workspace can't fail in a way to report.
            if (const auto target = targetOf(*result.focus); target && target->window())
                static_cast<void>(Config::Actions::focus(target->window()));
        }
        return {};
    }

    std::optional<Vector2D> CThreecolAlgorithm::predictSizeForNewTarget() {
        refreshModel();
        const Rect next = m_model.nextWindowRect();
        return Vector2D{next.w, next.h};
    }

}
