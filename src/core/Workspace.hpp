#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
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

    // Zone widths as fractions of the workspace area. Master gets the rest.
    struct Widths {
        double left  = 0.25;
        double stack = 0.25;
    };

    // The threecol layout of one workspace: which Zone and Row each window is
    // in, and the rectangle that gives it.
    class Workspace {
      public:
        explicit Workspace(Rect area, Widths widths = {});

        void                setArea(Rect area);

        // A window joins the workspace and takes its place in Fill order.
        void                addWindow(WindowId id);
        void                removeWindow(WindowId id);

        // The window's rectangle, or nothing if it isn't on this workspace.
        std::optional<Rect> rectOf(WindowId id) const;

        // The rectangle the next added window would get.
        Rect                nextWindowRect() const;

      private:
        enum class Zone { Left, Master, Stack };

        Rect                    m_area;
        Widths                  m_widths;
        std::optional<WindowId> m_master;
        std::optional<WindowId> m_left;
        std::vector<WindowId>   m_stack; // Rows, top to bottom

        // The Zone the next added window takes under Fill order.
        Zone                    nextZone() const;

        Rect                    leftZone() const;
        Rect                    masterZone() const;
        Rect                    stackZone() const;
        Rect                    stackRow(std::size_t row, std::size_t rows) const;
    };

}
