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

// The Chain runs Left, Master, Right: 2, 1, 3.
TEST(NextPrev, NextFollowsTheChainFromLeftThroughMasterToRight) {
    const Workspace ws = filled();
    EXPECT_EQ(ws.next(2, /*forward=*/true, /*wrap=*/false), 1u);
    EXPECT_EQ(ws.next(1, /*forward=*/true, /*wrap=*/false), 3u);
}
