#include "core/Workspace.hpp"

#include <gtest/gtest.h>

using threecol::Rect;
using threecol::WindowId;
using threecol::Workspace;

// Hyprland can replace a workspace's layout instance, e.g. on a config reload.
// The plugin carries the old Workspace over to the new instance; the windows
// that come back keep their places, and the others leave.

namespace {

    // One 5120x1440 ultrawide, no gaps, default widths 0.25 / 0.5 / 0.25.
    constexpr Rect AREA{0, 0, 5120, 1440};

    constexpr Rect LEFT_ZONE{0, 0, 1280, 1440};
    constexpr Rect MASTER_ZONE{1280, 0, 2560, 1440};

    // A workspace with windows 1..count added in Fill order: 1 Master,
    // 2 Left, then the Stack.
    Workspace filled(WindowId count) {
        Workspace ws{AREA};
        for (WindowId id = 1; id <= count; ++id)
            ws.addWindow(id);
        return ws;
    }

}

TEST(CarryAcrossRebuild, WindowsThatDontComeBackLeaveUnderTheCloseRules) {
    Workspace ws = filled(4); // 1 Master, 2 Left, 3 above 4 in the Stack

    ws.keepOnly({2, 4});

    EXPECT_EQ(ws.rectOf(1), std::nullopt);
    EXPECT_EQ(ws.rectOf(3), std::nullopt);
    EXPECT_EQ(ws.rectOf(2), LEFT_ZONE);                    // Master is a Hole
    EXPECT_EQ(ws.rectOf(4), (Rect{3840, 0, 1280, 1440})); // the Stack gap closes
    EXPECT_EQ(ws.nextWindowRect(), MASTER_ZONE);
}

// The issue's cases: 1 Master, 2 Left, 3 above 4 in the Stack; every window
// comes back.

TEST(CarryAcrossRebuild, KeepsRowHeights) {
    Workspace ws = filled(4);
    ws.layoutMessage("resize taller", 3); // 3 is 792 px, 4 is 648

    Workspace carried = ws;
    carried.keepOnly({4, 3, 2, 1});

    ASSERT_TRUE(carried.rectOf(3) && carried.rectOf(4));
    EXPECT_NEAR(carried.rectOf(3)->h, 792, 0.01);
    EXPECT_NEAR(carried.rectOf(4)->y, 792, 0.01);
    EXPECT_NEAR(carried.rectOf(4)->h, 648, 0.01);
}

TEST(CarryAcrossRebuild, KeepsAMasterHole) {
    Workspace ws = filled(4);
    ws.removeWindow(1);

    Workspace carried = ws;
    carried.keepOnly({4, 3, 2});

    EXPECT_EQ(carried.rectOf(2), LEFT_ZONE);
    EXPECT_EQ(carried.rectOf(3), (Rect{3840, 0, 1280, 720}));
    EXPECT_EQ(carried.rectOf(4), (Rect{3840, 720, 1280, 720}));
    EXPECT_EQ(carried.nextWindowRect(), MASTER_ZONE);
}

TEST(CarryAcrossRebuild, KeepsALeftHole) {
    Workspace ws = filled(4);
    ws.removeWindow(2);

    Workspace carried = ws;
    carried.keepOnly({4, 3, 1});

    EXPECT_EQ(carried.rectOf(1), MASTER_ZONE);
    EXPECT_EQ(carried.rectOf(3), (Rect{3840, 0, 1280, 720}));
    EXPECT_EQ(carried.rectOf(4), (Rect{3840, 720, 1280, 720}));
    EXPECT_EQ(carried.nextWindowRect(), LEFT_ZONE);
}

TEST(CarryAcrossRebuild, EndsAMouseDrag) {
    Workspace ws = filled(4);
    ws.dragResize(2, 100, 0, threecol::Corner::BottomRight);
    ASSERT_TRUE(ws.resizing());

    Workspace carried = ws;
    carried.keepOnly({1, 2, 3, 4});

    EXPECT_FALSE(carried.resizing());
}
