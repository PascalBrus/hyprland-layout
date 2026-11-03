#pragma once

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
        // whether the Dividers changed.
        bool                layoutMessage(std::string_view message, std::optional<WindowId> focused);

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

        // Set by resizing; until then the Dividers follow the settings.
        std::optional<Dividers> m_dividers;

        // The Dividers in use.
        Dividers                currentDividers() const;

        // The Zone `id` is in, or nothing if it isn't on this workspace.
        std::optional<Zone>     zoneOf(WindowId id) const;

        bool                    resize(Zone zone, double by);

        // The Zone the next added window takes under Fill order.
        Zone                    nextZone() const;

        // The windows in Chain order: Left, Master, then Stack top to bottom.
        // Unlike the Chain itself, Holes are skipped.
        std::vector<WindowId>   windowsInChainOrder() const;

        Rect                    leftZone() const;
        Rect                    masterZone() const;
        Rect                    stackZone() const;
        Rect                    stackRow(std::size_t row, std::size_t rows) const;
    };

}
