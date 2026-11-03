#pragma once

#include "core/Workspace.hpp"

#include <hyprland/src/layout/algorithm/TiledAlgorithm.hpp>

#include <string>
#include <utility>
#include <vector>

namespace threecol {

    // One instance per workspace. Translates Hyprland's layout callbacks into
    // calls on the core Workspace model and places targets where it says.
    class CThreecolAlgorithm : public Layout::ITiledAlgorithm {
      public:
        CThreecolAlgorithm();
        ~CThreecolAlgorithm() override;

        CThreecolAlgorithm(const CThreecolAlgorithm&)            = delete;
        CThreecolAlgorithm& operator=(const CThreecolAlgorithm&) = delete;

        void                            newTarget(SP<Layout::ITarget> target) override;
        void                            movedTarget(SP<Layout::ITarget> target, std::optional<Vector2D> focalPoint = std::nullopt) override;
        void                            removeTarget(SP<Layout::ITarget> target) override;

        void                            resizeTarget(const Vector2D& Δ, SP<Layout::ITarget> target, Layout::eRectCorner corner = Layout::CORNER_NONE) override;
        void                            recalculate(Layout::eRecalculateReason reason = Layout::RECALCULATE_REASON_UNKNOWN) override;

        void                            swapTargets(SP<Layout::ITarget> a, SP<Layout::ITarget> b) override;
        void                            moveTargetInDirection(SP<Layout::ITarget> t, Math::eDirection dir, bool silent) override;

        SP<Layout::ITarget>             getNextCandidate(SP<Layout::ITarget> old) override;

        Config::ErrorResult             layoutMsg(const std::string_view& sv) override;
        std::optional<Vector2D>         predictSizeForNewTarget() override;

      private:
        Workspace                                          m_model;
        std::vector<std::pair<WindowId, WP<Layout::ITarget>>> m_targets;
        WindowId                                           m_nextId = 1;

        // The window the model named to focus after `closed` left. Hyprland
        // asks getNextCandidate on unmap, after removeTarget; used up there.
        struct SFocusAfterClose {
            WP<Layout::ITarget> closed;
            WindowId            next;
        };
        std::optional<SFocusAfterClose>                    m_focusAfterClose;

        // Whether this workspace has asked for its settings to be checked.
        // After that, config reloads check them.
        bool                                               m_settingsChecked = false;

        void                                               syncArea();

        // The model's id for `target`, if it is one of this workspace's tiles.
        std::optional<WindowId>                            idOf(const SP<Layout::ITarget>& target) const;

        // The tile the model calls `id`, if it is still alive.
        SP<Layout::ITarget>                                targetOf(WindowId id) const;

        // The workspace this instance lays out, if it has one yet.
        PHLWORKSPACE                                       workspace() const;

        // Hands the model the current area, settings and the Dividers kept
        // for this workspace. Returns whether that moved any tile, e.g. a
        // lower Row limit moving Left windows to the Stack.
        bool                                               refreshModel();

        // Every tile's rectangle in the model, in m_targets order.
        std::vector<std::optional<Rect>>                   tileRects() const;

        // Keeps the model's Dividers after a change; they outlive the instance
        // and, with persist on, are saved to the state file.
        void                                               keepDividers();

        // Keeps the model's Dividers if they differ from `before`.
        void                                               keepDividersIfChanged(const std::optional<Dividers>& before);

        // Saves the Dividers if the model's mouse drag ended since
        // `wasResizing` was read: a mid-drag change counts as a release.
        void                                               keepDividersIfDragEnded(bool wasResizing);

        // The mouse was released after a drag: keep and save its sizes.
        void                                               dragEnded();
        CHyprSignalListener                                m_onDragEnded;
        // The workspace of the last mouse drag, to save it if this instance
        // goes mid-drag.
        PHLWORKSPACEREF                                    m_dragWorkspace;

        // Hands the model the global settings and this workspace's
        // layout_opts. Returns what was ignored or adjusted.
        std::vector<std::string>                           applySettings();

        friend std::vector<std::string>                    applySettingsToAllWorkspaces();
    };

    // Re-reads the settings on every threecol workspace and returns all their
    // warnings.
    std::vector<std::string> applySettingsToAllWorkspaces();

    // Lays out every threecol workspace again, e.g. after its Dividers changed.
    void                     recalculateAllWorkspaces();

}
