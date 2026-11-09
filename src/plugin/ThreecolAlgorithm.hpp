#pragma once

#include "core/Workspace.hpp"
#include "plugin/DropIndicator.hpp"

#include <hyprland/src/layout/algorithm/TiledAlgorithm.hpp>
#include <hyprland/src/managers/eventLoop/EventLoopManager.hpp>

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace threecol {

    // A workspace's layout as an instance had it: the model, which tile each
    // window id is, and the next id to give.
    struct SCarriedLayout {
        Workspace                                             model;
        std::vector<std::pair<WindowId, WP<Layout::ITarget>>> targets;
        WindowId                                              nextId;
    };

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

        // For the hooks on Hyprland's dispatchers (DispatcherHooks.cpp).
        // The instance that tiles `window`, with nothing fullscreen on its
        // workspace and no tile being dragged; nullptr otherwise.
        static CThreecolAlgorithm*      tiling(const PHLWINDOW& window);

        // For a tile, tiling(window). For a floating window, the instance
        // laying out its workspace, with nothing fullscreen on it and no tile
        // being dragged; nullptr otherwise.
        static CThreecolAlgorithm*      onWorkspace(const PHLWINDOW& window);

        // The window focus in `direction` goes to from `window`, one of this
        // instance's tiles, as Workspace::inDirection says; nullptr if none.
        PHLWINDOW                       inDirection(const PHLWINDOW& window, Direction direction) const;

        // The window after `window` on the Chain, or before it, as
        // Workspace::next says; nullptr if none.
        PHLWINDOW                       next(const PHLWINDOW& window, bool forward, bool wrap) const;

        // The window cycle_next goes to from `window`, a tile or a floating
        // window of this workspace: along the Cycle, as cycleStep says, or
        // with `tiledOnly` along the Chain alone. nullptr if none.
        PHLWINDOW                       cycleNext(const PHLWINDOW& window, bool forward, bool wrap, bool tiledOnly) const;

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

        // The tile being dragged with window.drag. Hyprland floats it for the
        // drag; the model keeps its place open until the drop.
        struct SDragged {
            WP<Layout::ITarget> target;
            WindowId            id;
            // The window is closing. Hyprland ends its drag as if released
            // before it removes the window: that is no drop.
            bool                closing = false;
        };
        std::optional<SDragged>                            m_dragged;
        CHyprSignalListener                                m_onWindowClose;

        // The dragged tile was dropped back on this workspace.
        void                                               dropped(WindowId id);

        // A drag ended without the tile coming back, e.g. it went to another
        // workspace or closed: its place follows the close rules.
        void                                               dragLeft();

        // Hyprland replaces a workspace's instance by removing every tile
        // here and handing it to the new one, e.g. when a config reload
        // briefly switches the workspace to another layout. Nothing tells
        // that from closes, so the layout from before a run of removals is
        // kept, and if this instance goes right after them it is carried over
        // to the next threecol instance of the workspace.
        std::optional<SCarriedLayout>                      m_beforeRemovals;

        // Takes the layout carried over for this workspace, if any, on the
        // first tile handed to this instance. Its windows that come back keep
        // their places.
        bool                                               m_carryChecked = false;
        std::vector<std::pair<WindowId, WP<Layout::ITarget>>> m_awaited; // carried, not back yet

        // The model's id for `target` if it comes back from a carried layout.
        std::optional<WindowId>                            cameBack(const SP<Layout::ITarget>& target);

        // Ends the carry: the carried windows that didn't come back leave
        // under the close rules. Not while Hyprland is still handing tiles
        // over to this instance.
        void                                               finishCarry();
        UP<SEventLoopDoLaterLock>                          m_pendingCarry; // set while the carry waits to end

        // Whether Hyprland is handing tiles over to this instance, which
        // replaces the workspace's current one when it's done.
        bool                                               handingOver() const;

        // Whether this workspace has asked for its settings to be checked.
        // After that, config reloads check them.
        bool                                               m_settingsChecked = false;

        void                                               syncArea();

        // The model's id for `target`, if it is one of this workspace's tiles.
        // A tile being dragged floats, so it isn't one until it is dropped.
        std::optional<WindowId>                            idOf(const SP<Layout::ITarget>& target) const;

        // The tile the model calls `id`, if it is still alive.
        SP<Layout::ITarget>                                targetOf(WindowId id) const;

        // The workspace this instance lays out, if it has one yet.
        PHLWORKSPACE                                       workspace() const;

        // Hands the model the current area, settings and the Dividers kept
        // for this workspace. Returns whether that moved any tile, e.g. a
        // lower Row limit pushing windows out of a side zone.
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
        // layout_opts, floating the windows pushed out with no room.
        // Returns what was ignored or adjusted.
        std::vector<std::string>                           applySettings();

        // Floats a window that finds no room: a new one, one toggled back to
        // tiled or one moved in. Not one of m_targets.
        void                                               floatNoRoom(const SP<Layout::ITarget>& target);

        // Floats `target` once Hyprland is done with what it is doing, if it
        // is still a tile of this workspace that isn't tiled here again.
        void                                               floatLater(const WP<Layout::ITarget>& target);
        // Floats the queued targets that still need it, as floatLater says.
        void                                               floatQueued();
        std::vector<WP<Layout::ITarget>>                   m_toFloat;
        UP<SEventLoopDoLaterLock>                          m_pendingFloat; // set while floating is queued

        // Where the dragged tile would land if dropped now: the box drop
        // would give it, with Hyprland's gaps and borders. Nothing when no
        // tile of this workspace is being dragged or the drop changes nothing.
        std::optional<SDropIndicator>                      dropIndicator() const;

        friend std::vector<std::string>                    applySettingsToAllWorkspaces();
        friend std::optional<SDropIndicator>               currentDropIndicator();
    };

    // Re-reads the settings on every threecol workspace and returns all their
    // warnings.
    std::vector<std::string> applySettingsToAllWorkspaces();

    // The drop indicator of the threecol tile being dragged, if any.
    std::optional<SDropIndicator> currentDropIndicator();

    // Lays out every threecol workspace again, e.g. after its Dividers changed.
    void                     recalculateAllWorkspaces();

}
