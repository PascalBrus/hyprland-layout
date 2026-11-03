#include "core/Workspace.hpp"

#include <gtest/gtest.h>

using threecol::Rect;
using threecol::WindowId;
using threecol::Workspace;

namespace {

    // One 5120x1440 ultrawide, no gaps, default widths 0.25 / 0.5 / 0.25.
    constexpr Rect AREA{0, 0, 5120, 1440};

    constexpr Rect LEFT_ZONE{0, 0, 1280, 1440};
    constexpr Rect MASTER_ZONE{1280, 0, 2560, 1440};
    constexpr Rect STACK_ZONE{3840, 0, 1280, 1440};

    // A workspace with windows 1..count added in Fill order.
    Workspace filled(WindowId count) {
        Workspace ws{AREA};
        for (WindowId id = 1; id <= count; ++id)
            ws.addWindow(id);
        return ws;
    }

}

TEST(CloseRules, ClosingMasterLeavesAHoleNoWindowMovesInto) {
    Workspace ws = filled(4); // 1 Master, 2 Left, 3 and 4 Stack

    ws.removeWindow(1);

    EXPECT_EQ(ws.rectOf(2), LEFT_ZONE);
    EXPECT_EQ(ws.rectOf(3), (Rect{3840, 0, 1280, 720}));
    EXPECT_EQ(ws.rectOf(4), (Rect{3840, 720, 1280, 720}));
}

TEST(CloseRules, ClosingTheLastLeftWindowLeavesALeftHole) {
    Workspace ws = filled(3); // 1 Master, 2 Left, 3 Stack

    ws.removeWindow(2);

    EXPECT_EQ(ws.rectOf(1), MASTER_ZONE);
    EXPECT_EQ(ws.rectOf(3), STACK_ZONE);
}

TEST(CloseRules, NextWindowFillsAMasterHole) {
    Workspace ws = filled(4);
    ws.removeWindow(1);

    EXPECT_EQ(ws.nextWindowRect(), MASTER_ZONE);
    ws.addWindow(5);

    EXPECT_EQ(ws.rectOf(5), MASTER_ZONE);
}

TEST(CloseRules, NextWindowFillsALeftHole) {
    Workspace ws = filled(4);
    ws.removeWindow(2);

    EXPECT_EQ(ws.nextWindowRect(), LEFT_ZONE);
    ws.addWindow(5);

    EXPECT_EQ(ws.rectOf(5), LEFT_ZONE);
}

TEST(CloseRules, MasterHoleIsFilledBeforeALeftHole) {
    Workspace ws = filled(3);
    ws.removeWindow(2);
    ws.removeWindow(1);

    ws.addWindow(4);
    ws.addWindow(5);
    ws.addWindow(6);

    EXPECT_EQ(ws.rectOf(4), MASTER_ZONE);
    EXPECT_EQ(ws.rectOf(5), LEFT_ZONE);
    EXPECT_EQ(ws.rectOf(6), (Rect{3840, 720, 1280, 720}));
}

TEST(CloseRules, HolesKeepTheirWidthSoTheOtherZonesDontMove) {
    Workspace ws = filled(3);

    // Both Holes at once: the Stack stays a quarter wide on the right.
    ws.removeWindow(1);
    ws.removeWindow(2);

    EXPECT_EQ(ws.rectOf(3), STACK_ZONE);
}

TEST(CloseRules, ClosingAStackWindowClosesTheGapAndRowsShareTheHeight) {
    Workspace ws = filled(5); // 3, 4, 5 are Stack Rows

    ws.removeWindow(4);

    EXPECT_EQ(ws.rectOf(3), (Rect{3840, 0, 1280, 720}));
    EXPECT_EQ(ws.rectOf(5), (Rect{3840, 720, 1280, 720}));
    EXPECT_EQ(ws.rectOf(1), MASTER_ZONE);
    EXPECT_EQ(ws.rectOf(2), LEFT_ZONE);
}

TEST(CloseRules, ClosingTheLastStackWindowLeavesTheOtherZonesInPlace) {
    Workspace ws = filled(3);

    ws.removeWindow(3);

    EXPECT_EQ(ws.rectOf(1), MASTER_ZONE);
    EXPECT_EQ(ws.rectOf(2), LEFT_ZONE);
    EXPECT_EQ(ws.nextWindowRect(), STACK_ZONE);
}

// Focus after a close goes to the next window along the Chain (Left top to
// bottom, Master, Stack top to bottom), or the one before if it was the last.

TEST(FocusAfterClose, ClosingAStackRowFocusesTheRowThatTakesItsPlace) {
    Workspace ws = filled(5); // Chain: 2, 1, 3, 4, 5

    EXPECT_EQ(ws.removeWindow(4), 5u);
}

TEST(FocusAfterClose, ClosingTheBottomStackRowFocusesTheRowAbove) {
    Workspace ws = filled(5);

    EXPECT_EQ(ws.removeWindow(5), 4u);
}

TEST(FocusAfterClose, ClosingTheOnlyStackRowFocusesMaster) {
    Workspace ws = filled(3);

    EXPECT_EQ(ws.removeWindow(3), 1u);
}

TEST(FocusAfterClose, ClosingMasterFocusesTheTopOfTheStack) {
    Workspace ws = filled(4);

    EXPECT_EQ(ws.removeWindow(1), 3u);
}

TEST(FocusAfterClose, ClosingMasterWithAnEmptyStackFocusesLeft) {
    Workspace ws = filled(2);

    EXPECT_EQ(ws.removeWindow(1), 2u);
}

TEST(FocusAfterClose, ClosingLeftFocusesMaster) {
    Workspace ws = filled(3);

    EXPECT_EQ(ws.removeWindow(2), 1u);
}

TEST(FocusAfterClose, ClosingLeftNextToAMasterHoleFocusesTheTopOfTheStack) {
    Workspace ws = filled(4);
    ws.removeWindow(1);

    EXPECT_EQ(ws.removeWindow(2), 3u);
}

TEST(FocusAfterClose, ClosingTheOnlyWindowNamesNoNeighbour) {
    Workspace ws = filled(1);

    EXPECT_EQ(ws.removeWindow(1), std::nullopt);
}

TEST(FocusAfterClose, RemovingAnUnknownWindowNamesNoNeighbour) {
    Workspace ws = filled(3);

    EXPECT_EQ(ws.removeWindow(42), std::nullopt);
    EXPECT_EQ(ws.rectOf(1), MASTER_ZONE);
}

// A window moved in from another workspace, or un-floated, joins through
// addWindow like a new one.

TEST(MovedIn, AWindowMovedInFillsAHoleFirst) {
    Workspace ws = filled(4);
    ws.removeWindow(1);

    ws.addWindow(10);

    EXPECT_EQ(ws.rectOf(10), MASTER_ZONE);
}

TEST(MovedIn, AWindowThatLeavesAndComesBackFollowsFillOrderNotItsOldPlace) {
    Workspace ws = filled(4); // 4 is the bottom Stack Row

    // 4 is floated, then Master closes; 4 is un-floated into the Master Hole.
    ws.removeWindow(4);
    ws.removeWindow(1);
    ws.addWindow(4);

    EXPECT_EQ(ws.rectOf(4), MASTER_ZONE);
    EXPECT_EQ(ws.rectOf(3), STACK_ZONE);
}

// hyprctl reload keeps the same layout instance and only re-places windows,
// which reaches the model as setArea.

TEST(Reload, ZoneAssignmentsAndHolesSurviveReplacing) {
    Workspace ws = filled(5);
    ws.removeWindow(1);

    ws.setArea(AREA);

    EXPECT_EQ(ws.rectOf(2), LEFT_ZONE);
    EXPECT_EQ(ws.rectOf(3), (Rect{3840, 0, 1280, 480}));
    EXPECT_EQ(ws.rectOf(4), (Rect{3840, 480, 1280, 480}));
    EXPECT_EQ(ws.rectOf(5), (Rect{3840, 960, 1280, 480}));
    EXPECT_EQ(ws.nextWindowRect(), MASTER_ZONE);
}
