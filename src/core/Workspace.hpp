#pragma once

#include "core/RowHeights.hpp"
#include "core/Settings.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace threecol {

    using WindowId = std::uint64_t;

    // A rectangle in the same coordinates as the workspace area.
    struct Rect {
        double x = 0;
        double y = 0;
        double w = 0;
        double h = 0;

        friend bool operator==(const Rect&, const Rect&) = default;
    };

    // Where a workspace's two Dividers sit, kept as the widths of the zones
    // outside them: Left|Master after leftWidth, Master|Stack before the last
    // stackWidth. Fractions of the workspace width; Master gets the rest.
    struct Dividers {
        double leftWidth  = 0;
        double stackWidth = 0;

        friend bool operator==(const Dividers&, const Dividers&) = default;
    };

    // The corner of a window being resized, from which part of the window the
    // cursor is in. None when no cursor is involved, e.g. a bind or hyprctl.
    enum class Corner { None, TopLeft, TopRight, BottomLeft, BottomRight };

    // The threecol layout of one workspace: which Zone and Row each window is
    // in, and the rectangle that gives it.
    class Workspace {
      public:
        explicit Workspace(Rect area);

        void                setArea(Rect area);

        // The settings changed: the global values and this workspace's
        // layout_opts, which win over them. Returns what was ignored or
        // adjusted; `workspaceName` names the workspace in those warnings.
        std::vector<std::string> setSettings(const Settings& global, const LayoutOpts& opts = {}, std::string_view workspaceName = {});

        // The settings in use, after layout_opts and making them fit.
        const Settings&     settings() const;

        // A window joins the workspace and takes its place in Fill order.
        void                addWindow(WindowId id);

        // A window leaves the workspace. Closing Master or the last Left window
        // leaves a Hole; closing a Stack Row closes the gap. Returns the window
        // to focus next: the one after it along the Chain, or the one before if
        // it was last. Nothing if no window is left or it wasn't here.
        std::optional<WindowId> removeWindow(WindowId id);

        // The window's rectangle, or nothing if it isn't on this workspace.
        std::optional<Rect> rectOf(WindowId id) const;

        // The rectangle the next added window would get.
        Rect                nextWindowRect() const;

        // A layout message from the user; `focused` is the focused window.
        // Unknown messages, or ones that don't apply, do nothing. Returns
        // whether the layout changed.
        //
        // `resize wider` / `resize narrower` move the focused zone's Dividers;
        // `resize taller` / `resize shorter` change the focused Stack Row's
        // height, the other Rows giving way in proportion; `reset` puts the
        // Dividers back to the settings and makes the Rows equal.
        bool                layoutMessage(std::string_view message, std::optional<WindowId> focused);

        // A resize of `id`'s tile by `dx`, `dy` pixels.
        //
        // Sideways, a Left window moves Left|Master and a Stack window
        // Master|Stack. Master moves the Divider on the grabbed half, or
        // Master|Stack with no corner.
        //
        // Up and down, a Stack Row moves the edge on the grabbed half, or its
        // bottom edge with no corner, changing only the two Rows either side.
        // The top Row has only a bottom edge, the bottom Row only a top edge,
        // and a lone Row has none.
        //
        // Each stops at its limits. Returns whether the layout changed.
        //
        // dragResize is one tick of a mouse drag: the Divider and edge are
        // chosen at press, and sit where the cursor's travel since press puts
        // them. resizeWindow is a one-off, e.g. from a bind, hyprctl or a
        // trackpad gesture, which never releases.
        bool                dragResize(WindowId id, double dx, double dy, Corner corner);
        bool                resizeWindow(WindowId id, double dx, double dy = 0, Corner corner = Corner::None);

        // The mouse was released: the drag ends, keeping its sizes. The next
        // drag chooses its Divider again.
        void                endResize();

        // Whether a mouse drag is in progress. A drag ends early, keeping its
        // sizes, if the grabbed window leaves, a window joins or leaves its
        // zone, or a layout message changes the layout.
        bool                resizing() const;

        // The Dividers set by resizing, to keep for this workspace; nothing if
        // they follow the settings. A new instance takes them back with
        // setDividers. They win over the settings until reset, clamped to
        // the current limits.
        std::optional<Dividers> dividers() const;
        void                setDividers(std::optional<Dividers> dividers);

      private:
        enum class Zone { Left, Master, Stack };

        Rect                    m_area;
        Settings                m_settings;
        std::optional<WindowId> m_master;
        std::optional<WindowId> m_left;
        std::vector<WindowId>   m_stack; // Rows, top to bottom
        RowHeights              m_stackHeights;

        // Set by resizing; until then the Dividers follow the settings.
        std::optional<Dividers> m_dividers;

        // The Dividers in use.
        Dividers                currentDividers() const;

        // The Zone `id` is in, or nothing if it isn't on this workspace.
        std::optional<Zone>     zoneOf(WindowId id) const;

        bool                    applyLayoutMessage(std::string_view message, std::optional<WindowId> focused);
        bool                    resize(Zone zone, double by);
        bool                    resizeRow(WindowId id, double by);

        enum class Divider { LeftMaster, MasterStack };

        // A Row edge being moved: the one above `row`, and the zone's Row
        // heights at press.
        struct Edge {
            std::size_t         row;
            std::vector<double> atPress;
        };

        // A mouse resize in progress: the window grabbed, the Divider and Row
        // edge it moves, the Dividers at press and the cursor's travel since,
        // in pixels. A drag that ended early is kept, over, until release, so
        // its later ticks don't start a new one.
        struct Gesture {
            WindowId            id;
            Divider             divider;
            Dividers            atPress;
            std::optional<Edge> edge    = std::nullopt;
            double              travelX = 0;
            double              travelY = 0;
            bool                over    = false;
        };
        std::optional<Gesture>  m_gesture;

        static Divider          dividerFor(Zone zone, Corner corner);

        // The Stack Row whose top edge `id` moves when grabbed at `corner`;
        // nothing outside the Stack or for a lone Row.
        std::optional<std::size_t> edgeFor(WindowId id, Corner corner) const;

        // Keeps `heights` for the Stack if they differ from the ones in use.
        bool                    setStackHeights(std::vector<double> heights);

        // Keeps `dividers` if they differ from the ones in use.
        bool                    setResized(const Dividers& dividers);

        // `from` with `divider` moved `px` pixels right, within its limits.
        Dividers                moveDivider(Divider divider, Dividers from, double px) const;

        // The Zone the next added window takes under Fill order.
        Zone                    nextZone() const;

        // The windows in Chain order: Left, Master, then Stack top to bottom.
        // Unlike the Chain itself, Holes are skipped.
        std::vector<WindowId>   windowsInChainOrder() const;

        Rect                    leftZone() const;
        Rect                    masterZone() const;
        Rect                    stackZone() const;
        Rect                    stackRow(std::size_t row) const;
    };

}
