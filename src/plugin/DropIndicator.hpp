#pragma once

#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/helpers/math/Math.hpp>

namespace threecol {

    // Where a dragged tile will land, as it will sit there: the window's own
    // box, borders outside it, in global layout coordinates.
    struct SDropIndicator {
        PHLWINDOWREF  window;
        PHLMONITORREF monitor;
        CBox          box;

        bool          operator==(const SDropIndicator&) const = default;
    };

    // Draws the drop indicator of the threecol tile being dragged: its
    // landing box in the active-border gradient with a light fill, just below
    // the dragged window. It follows the cursor every frame and goes when the
    // drag ends.
    void startDropIndicator();
    void stopDropIndicator();

}
