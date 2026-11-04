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
    // outside them: Left|Master after leftWidth, Master|Right before the last
    // rightWidth. Fractions of the workspace width; Master gets the rest.
    struct Dividers {
        double leftWidth  = 0;
        double rightWidth = 0;

        friend bool operator==(const Dividers&, const Dividers&) = default;
    };

    // The corner of a window being resized, from which part of the window the
    // cursor is in. None when no cursor is involved, e.g. a bind or hyprctl.
    enum class Corner { None, TopLeft, TopRight, BottomLeft, BottomRight };

    // A direction for focus and moves: l, r along the Chain, u, d inside a zone.
    enum class Direction { Left, Right, Up, Down };

    // What a layout message did: whether the layout changed, and the window
    // to focus, if focus should go somewhere.
    struct MessageResult {
        bool                    changed = false;
        std::optional<WindowId> focus   = std::nullopt;

        explicit operator bool() const {
            return changed;
        }
    };

    // What a settings change did.
    struct SettingsChange {
        std::vector<std::string> warnings; // what was ignored or adjusted
        // Windows a lower Row limit pushed out with no room anywhere, in Chain
        // order. They have left the layout.
        std::vector<WindowId>    noRoom;
    };

    // The threecol layout of one workspace: which Zone and Row each window is
    // in, and the rectangle that gives it.
    class Workspace {
      public:
        explicit Workspace(Rect area);

        void                setArea(Rect area);

        // The settings changed: the global values and this workspace's
        // layout_opts, which win over them. `workspaceName` names the
        // workspace in warnings. A lower Row limit pushes the extra bottom
        // windows of a side out to the other side, which they join at its end
        // nearest Master, keeping their Chain order: all of them if it is a
        // Stack, else as many as fit its free Rows, first in Chain order. The
        // rest leave the layout, reported in `noRoom`.
        SettingsChange      setSettings(const Settings& global, const LayoutOpts& opts = {}, std::string_view workspaceName = {});

        // The settings in use, after layout_opts and making them fit.
        const Settings&     settings() const;

        // A window joins the workspace and takes its place in Fill order.
        // Returns whether it is placed: false when it finds no room (Master
        // taken and every side zone full), leaving it out of the layout.
        bool                addWindow(WindowId id);

        // A window leaves the workspace. Closing Master or the last window of
        // a Bounded zone leaves a Hole; closing any other Row closes the gap. Returns the window
        // to focus next: the one after it along the Chain, or the one before if
        // it was last. Nothing if no window is left or it wasn't here.
        std::optional<WindowId> removeWindow(WindowId id);

        // The window's rectangle, or nothing if it isn't on this workspace.
        std::optional<Rect> rectOf(WindowId id) const;

        // The rectangle the next added window would get; an empty one if it
        // would find no room.
        Rect                nextWindowRect() const;

        // A layout message from the user; `focused` is the focused window.
        // Unknown messages, or ones that don't apply, do nothing. Returns
        // whether the layout changed and where focus goes.
        //
        // `resize wider` / `resize narrower` move the focused zone's Dividers;
        // `resize taller` / `resize shorter` change the focused side zone
        // Row's height, the other Rows giving way in proportion; `reset` puts the
        // Dividers back to the settings and makes the Rows equal.
        //
        // `focus l` / `focus r` walk the Chain, stopping at its ends and
        // skipping empty zones; `focus u` / `focus d` move inside a side
        // zone, never leaving it. `focusmaster` focuses Master, with or
        // without a focused window. `move l|r|u|d` moves the focused window as
        // `move` does, focus staying on it. `swapwithmaster` swaps the focused
        // window with Master, the old Master taking its Row, or moves it into
        // a Hole; it does nothing on Master.
        MessageResult       layoutMessage(std::string_view message, std::optional<WindowId> focused);

        // Moves `id` one step along the Chain (`Left`, `Right`) or inside its
        // zone (`Up`, `Down`), stopping at the Chain's ends and the zone's
        // edges. Between Master and a side zone it steps at the side's end
        // nearest Master: the bottom of Left, the top of Right. It swaps only
        // when there is no room: with an occupied Master, the Row nearest
        // Master of a full Bounded zone, or a neighbouring Row in the same
        // zone. Otherwise it moves: into a Hole, or from Master into a Stack
        // or a Bounded zone below its Row limit as a new Row at its end
        // nearest Master, leaving a Hole. A moved window takes its new Row's
        // height. Returns whether the layout changed.
        bool                move(WindowId id, Direction direction);

        // `a` and `b` trade places, each taking the other's Row, as asked
        // for outright, e.g. by Hyprland's swap. Returns whether they did.
        bool                swapWindows(WindowId a, WindowId b);

        // `id` was dragged with the mouse and dropped at (`x`, `y`). Through
        // the drag its place stayed open, so nothing else moved.
        //
        // Dropped on another window, the two swap Rows. Dropped on the "add"
        // area, the bottom quarter of a side zone's bottom Row, it joins as a
        // new bottom Row: a Stack always takes it, a full Bounded zone swaps
        // with its bottom Row instead, and in its own zone it moves to the
        // bottom. Dropped into an empty zone (a Hole or empty Stack) it
        // moves there. Wherever it moves from follows the close rules.
        //
        // A point in a gap counts as the zone it's in and the nearest Row. A
        // drop on its own place or outside the workspace changes nothing.
        // Returns whether the layout changed.
        bool                drop(WindowId id, double x, double y);

        // Where `id` would land if dropped at (`x`, `y`) now: the rectangle
        // drop would give it. Nothing when that drop would change nothing.
        std::optional<Rect> dropIndicator(WindowId id, double x, double y) const;

        // A resize of `id`'s tile by `dx`, `dy` pixels.
        //
        // Sideways, a Left window moves Left|Master and a Right window
        // Master|Right. Master moves the Divider on the grabbed half, or
        // Master|Right with no corner. A missing zone has no Divider: with one
        // side missing Master moves the other Divider, with both nothing.
        //
        // Up and down, a side zone Row moves the edge on the grabbed half, or its
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
        // zone, a layout message changes the layout, or a settings change
        // moves the grabbed window or makes its Divider's zone missing.
        bool                resizing() const;

        // The Dividers set by resizing, to keep for this workspace; nothing if
        // they follow the settings. A new instance takes them back with
        // setDividers. They win over the settings until reset, clamped to
        // the current limits.
        std::optional<Dividers> dividers() const;
        void                setDividers(std::optional<Dividers> dividers);

      private:
        // The zones by position. A side zone's role (Bounded, Stack or
        // Missing) comes from its Row limit, never from which side it is.
        enum class Zone { Left, Master, Right };

        // An end of a side zone's Rows.
        enum class End { Top, Bottom };

        Rect                    m_area;
        Settings                m_settings;
        // A zone of Rows: its windows top to bottom, and their heights.
        struct Rows {
            std::vector<WindowId> windows;
            RowHeights            heights;

            // `id` joins as a new Row at `row`, the Rows from there moving down.
            void                  insert(std::size_t row, WindowId id);
        };

        std::optional<WindowId> m_master;
        Rows                    m_left;
        Rows                    m_right;

        // The end of side zone `side` nearest Master: the bottom of Left, the
        // top of Right. The Chain steps between Master and `side` there.
        static End              endNearestMaster(Zone side);

        // Which way along the Chain Master lies from side zone `side`:
        // forward (right) from Left, back from Right.
        static bool             masterIsForwardOf(Zone side);

        // The other side zone.
        static Zone             otherSide(Zone side);

        // The Row limit of side zone `side`, as resolved.
        RowLimit                rowLimit(Zone side) const;

        // Whether side zone `side` exists: it isn't a missing zone.
        bool                    exists(Zone side) const;

        // Whether side zone `side` can take another Row: a Stack, or a
        // Bounded zone below its Row limit.
        bool                    hasRoom(Zone side) const;

        // Set by resizing; until then the Dividers follow the settings. A
        // missing zone's width is kept here too, unused until it comes back.
        std::optional<Dividers> m_dividers;

        // The Dividers set by resizing, else the settings, unclamped.
        Dividers                savedDividers() const;

        // The Dividers in use: the saved ones made to fit, a missing zone's
        // width 0.
        Dividers                currentDividers() const;

        // Keeps `dividers`, the ones in use after a resize, as resized. A
        // missing zone keeps its saved width.
        void                    keepDividers(const Dividers& dividers);

        // The Zone `id` is in, or nothing if it isn't on this workspace.
        std::optional<Zone>     zoneOf(WindowId id) const;

        MessageResult           applyLayoutMessage(std::string_view message, std::optional<WindowId> focused);

        // The next window from `id` along the Chain, one way or the other.
        std::optional<WindowId> chainNeighbour(WindowId id, bool right) const;

        // The Row above or below `id` in its zone; nothing on Master.
        std::optional<WindowId> rowNeighbour(WindowId id, bool down) const;

        bool                    applyMove(WindowId id, Direction direction);

        // Where `id` is kept: Master or its Row. Nothing if it isn't here.
        WindowId*               slotOf(WindowId id);

        // `a` and `b` trade places, each taking the other's Row.
        void                    swapPlaces(WindowId a, WindowId b);

        // `id` leaves its place under the close rules.
        void                    takeOut(WindowId id);

        bool                    applyDrop(WindowId id, double x, double y);

        // `id` leaves its place under the close rules and joins the bottom of
        // `rows` as a new Row.
        void                    joinBottom(Rows& rows, WindowId id);

        // `id` leaves its place under the close rules and joins side zone
        // `side` as a new Row at its end nearest Master.
        void                    joinNearMaster(Zone side, WindowId id);

        // `id` swaps with Master, or moves into a Master Hole.
        void                    toMaster(WindowId id);
        bool                    resize(Zone zone, double by);
        bool                    resizeRow(WindowId id, double by);

        enum class Divider { LeftMaster, MasterRight };

        // A Row edge being moved: the one above `row` of `zone`, and the
        // zone's Row heights at press.
        struct Edge {
            Zone                zone;
            std::size_t         row;
            std::vector<double> atPress;
        };

        // A mouse resize in progress: the window grabbed, the Divider and Row
        // edge it moves, the Dividers at press and the cursor's travel since,
        // in pixels. A drag that ended early is kept, over, until release, so
        // its later ticks don't start a new one.
        struct Gesture {
            WindowId            id;
            std::optional<Divider> divider;
            Dividers            atPress;
            std::optional<Edge> edge    = std::nullopt;
            double              travelX = 0;
            double              travelY = 0;
            bool                over    = false;
        };
        std::optional<Gesture>  m_gesture;

        // The Divider a resize of `zone` grabbed at `corner` moves; nothing
        // when it has none, i.e. Master with both side zones missing.
        std::optional<Divider>  dividerFor(Zone zone, Corner corner) const;

        // The Rows of `zone`, or nothing for Master.
        Rows*                   rowsOf(Zone zone);
        const Rows*             rowsOf(Zone zone) const;

        // The Row edge `id` moves when grabbed at `corner`, with the heights
        // in use; nothing for Master or a lone Row.
        std::optional<Edge>     edgeFor(WindowId id, Corner corner) const;

        // Keeps `heights` for `zone`'s Rows if they differ from the ones in use.
        bool                    setHeights(Zone zone, std::vector<double> heights);

        // Pushes the bottom windows of each side zone past its Row limit out
        // to the other side, as setSettings says. Returns whether any moved,
        // and adds the ones with no room to `noRoom`.
        bool                    fitToRowLimits(std::vector<WindowId>& noRoom);

        // Keeps `dividers` if they differ from the ones in use.
        bool                    setResized(const Dividers& dividers);

        // `from` with `divider` moved `px` pixels right, within its limits;
        // `from` itself with no Divider.
        Dividers                moveDivider(std::optional<Divider> divider, Dividers from, double px) const;

        // The Zone at (`x`, `y`), or nothing outside the workspace.
        std::optional<Zone>     zoneAt(double x, double y) const;

        // The Row of `zone` whose band of the zone height holds `y`: the top
        // Row above the zone, the bottom one below it. `zone` has Rows.
        std::size_t             rowAt(Zone zone, double y) const;

        // The Zone the next added window takes under Fill order: Master, then
        // Left up to its Row limit, then Right up to its own. Nothing if
        // there's no room anywhere.
        std::optional<Zone>     nextZone() const;

        // The windows in Chain order: Left, Master, then Right, each side top
        // to bottom. Unlike the Chain itself, Holes are skipped.
        std::vector<WindowId>   windowsInChainOrder() const;

        Rect                    leftZone() const;
        Rect                    masterZone() const;
        Rect                    rightZone() const;
        Rect                    zoneRect(Zone zone) const;
        Rect                    rowRect(Zone zone, std::size_t row) const;
    };

}
