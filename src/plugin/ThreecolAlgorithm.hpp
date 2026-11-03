#pragma once

#include "core/Workspace.hpp"

#include <hyprland/src/layout/algorithm/TiledAlgorithm.hpp>

#include <utility>
#include <vector>

namespace threecol {

    // One instance per workspace. Translates Hyprland's layout callbacks into
    // calls on the core Workspace model and places targets where it says.
    class CThreecolAlgorithm : public Layout::ITiledAlgorithm {
      public:
        CThreecolAlgorithm();

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

        void                                               syncArea();
    };

}
