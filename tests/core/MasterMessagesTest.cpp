#include "core/Workspace.hpp"

#include <gtest/gtest.h>

using threecol::Rect;
using threecol::WindowId;
using threecol::Workspace;

namespace {

    constexpr Rect AREA{0, 0, 1000, 1000};

    // A workspace with windows 1..count added in Fill order: 1 Master,
    // 2 Left, then the Stack.
    Workspace filled(WindowId count = 3) {
        Workspace ws{AREA};
        for (WindowId id = 1; id <= count; ++id)
            ws.addWindow(id);
        return ws;
    }

}

TEST(MasterMessages, FocusmasterFromASideZoneFocusesMaster) {
    Workspace  ws     = filled();
    const auto result = ws.layoutMessage("focusmaster", 2);
    EXPECT_FALSE(result.changed);
    EXPECT_EQ(result.focus, 1u);
}
