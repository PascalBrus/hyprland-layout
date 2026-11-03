#include "plugin/ThreecolAlgorithm.hpp"

#include "plugin/Config.hpp"
#include "plugin/SettingsReporting.hpp"

#include <hyprland/src/config/shared/workspace/WorkspaceRuleManager.hpp>

#include <hyprland/src/layout/algorithm/Algorithm.hpp>
#include <hyprland/src/layout/space/Space.hpp>
#include <hyprland/src/layout/target/Target.hpp>
#include <hyprland/src/managers/fullscreen/FullscreenController.hpp>
#include <hyprland/src/managers/fullscreen/handler/FullscreenHandler.hpp>

#include <algorithm>
#include <iterator>

namespace threecol {

    namespace {

        Rect toRect(const CBox& box) {
            return {box.x, box.y, box.width, box.height};
        }

        CBox toBox(const Rect& rect) {
            return {rect.x, rect.y, rect.w, rect.h};
        }

        // Every live instance, so a config reload can reach them all.
        std::vector<CThreecolAlgorithm*> g_instances;

    }

    CThreecolAlgorithm::CThreecolAlgorithm() : m_model(Rect{}) {
        g_instances.push_back(this);
    }

    CThreecolAlgorithm::~CThreecolAlgorithm() {
        std::erase(g_instances, this);
    }

    std::vector<std::string> CThreecolAlgorithm::applySettings() {
        const auto workspace = m_parent && m_parent->space() ? m_parent->space()->workspace() : nullptr;
        if (!workspace)
            return m_model.setSettings(globalSettings());

        const auto rule = Config::workspaceRuleMgr()->getWorkspaceRuleFor(workspace);
        return m_model.setSettings(globalSettings(), rule ? rule->m_layoutopts : LayoutOpts{}, workspace->displayName());
    }

    std::vector<std::string> applySettingsToAllWorkspaces() {
        std::vector<std::string> warnings;
        for (auto* instance : g_instances)
            std::ranges::move(instance->applySettings(), std::back_inserter(warnings));
        return warnings;
    }

    void CThreecolAlgorithm::syncArea() {
        if (m_parent && m_parent->space())
            m_model.setArea(toRect(m_parent->space()->workArea()));
    }

    void CThreecolAlgorithm::newTarget(SP<Layout::ITarget> target) {
        if (std::ranges::any_of(m_targets, [&](const auto& entry) { return entry.second.lock() == target; }))
            return;

        const WindowId id = m_nextId++;
        m_targets.emplace_back(id, target);
        m_model.addWindow(id);
        recalculate();
    }

    void CThreecolAlgorithm::movedTarget(SP<Layout::ITarget> target, std::optional<Vector2D>) {
        newTarget(target);
    }

    void CThreecolAlgorithm::removeTarget(SP<Layout::ITarget> target) {
        const auto it = std::ranges::find_if(m_targets, [&](const auto& entry) { return entry.second.lock() == target; });
        if (it == m_targets.end())
            return;

        m_focusAfterClose.reset();
        if (const auto next = m_model.removeWindow(it->first))
            m_focusAfterClose = SFocusAfterClose{target, *next};
        m_targets.erase(it);
        recalculate();
    }

    void CThreecolAlgorithm::resizeTarget(const Vector2D&, SP<Layout::ITarget>, Layout::eRectCorner) {
        // Resizing comes in a later ticket.
    }

    void CThreecolAlgorithm::recalculate(Layout::eRecalculateReason) {
        if (!m_parent || !m_parent->space())
            return;

        if (Fullscreen::controller()->hasFullscreen(m_parent->space()->workspace(), true)) {
            m_defaultFullscreenHandler->syncTargetSizeAndPosition();
            return;
        }

        // Forget targets that died without a removeTarget, so they don't hold a zone.
        std::erase_if(m_targets, [this](const auto& entry) {
            if (!entry.second.expired())
                return false;
            m_model.removeWindow(entry.first);
            return true;
        });

        syncArea();
        applySettings();
        if (!std::exchange(m_settingsChecked, true))
            reportSettingsLater();

        for (const auto& [id, weak] : m_targets) {
            if (const auto rect = m_model.rectOf(id))
                weak.lock()->setPositionGlobal(toBox(*rect));
        }
    }

    void CThreecolAlgorithm::swapTargets(SP<Layout::ITarget>, SP<Layout::ITarget>) {
        // Swaps come in a later ticket.
    }

    void CThreecolAlgorithm::moveTargetInDirection(SP<Layout::ITarget>, Math::eDirection, bool) {
        // Moves come in a later ticket.
    }

    SP<Layout::ITarget> CThreecolAlgorithm::getNextCandidate(SP<Layout::ITarget> old) {
        const auto hint = std::exchange(m_focusAfterClose, std::nullopt);
        if (!old || !hint || hint->closed.lock() != old)
            return nullptr; // Hyprland falls back to another tiled window.

        const auto it = std::ranges::find_if(m_targets, [&](const auto& entry) { return entry.first == hint->next; });
        return it == m_targets.end() ? nullptr : it->second.lock();
    }

    Config::ErrorResult CThreecolAlgorithm::layoutMsg(const std::string_view&) {
        // Unknown messages do nothing and report no error.
        return {};
    }

    std::optional<Vector2D> CThreecolAlgorithm::predictSizeForNewTarget() {
        syncArea();
        applySettings();
        const Rect next = m_model.nextWindowRect();
        return Vector2D{next.w, next.h};
    }

}
