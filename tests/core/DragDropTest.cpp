#include "core/Workspace.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <optional>
#include <vector>

using threecol::Direction;
using threecol::Rect;
using threecol::RowLimit;
using threecol::Settings;
using threecol::WindowId;
using threecol::Workspace;

namespace {

    // One 5120x1440 ultrawide, no gaps, default widths 0.25 / 0.5 / 0.25.
    constexpr Rect AREA{0, 0, 5120, 1440};

    constexpr Rect MASTER_ZONE{1280, 0, 2560, 1440};
    constexpr Rect LEFT_ZONE{0, 0, 1280, 1440};
    constexpr Rect RIGHT_ZONE{3840, 0, 1280, 1440};

    // A workspace with Row limit `leftRows` and windows 1..count added in
    // Fill order: 1 Master, then Left up to the limit, then the Stack.
    Workspace filled(int leftRows, WindowId count) {
        Workspace ws{AREA};
        ws.setSettings(Settings{.leftRows = leftRows});
        for (WindowId id = 1; id <= count; ++id)
            ws.addWindow(id);
        return ws;
    }

    Rect leftRow(double y, double h) {
        return {0, y, 1280, h};
    }

    Rect rightRow(double y, double h) {
        return {3840, y, 1280, h};
    }

    // Heights come from fractions, so compare to well under a pixel.
    ::testing::AssertionResult near(const std::optional<Rect>& actual, const Rect& expected) {
        if (!actual)
            return ::testing::AssertionFailure() << "window has no rectangle";
        constexpr double TOLERANCE = 1e-6;
        const auto       close     = [](double a, double b) { return std::abs(a - b) < TOLERANCE; };
        if (close(actual->x, expected.x) && close(actual->y, expected.y) && close(actual->w, expected.w) && close(actual->h, expected.h))
            return ::testing::AssertionSuccess();
        return ::testing::AssertionFailure() << "got {" << actual->x << ", " << actual->y << ", " << actual->w << ", " << actual->h << "}, expected {" << expected.x
                                             << ", " << expected.y << ", " << expected.w << ", " << expected.h << "}";
    }

    // A mirrored layout: Left a Stack, Right bounded to `rightRows` (or a
    // second Stack). Windows land through Fill order and moves: `right` top
    // to bottom, then `master`, then `left` top to bottom.
    Workspace mirrored(RowLimit rightRows, const std::vector<WindowId>& left, WindowId master, const std::vector<WindowId>& right) {
        Workspace ws{AREA};
        ws.setSettings(Settings{.leftRows = RowLimit::unlimited(), .rightRows = rightRows});
        // Each joins Right at its top, so the bottom one goes first.
        for (auto id = right.rbegin(); id != right.rend(); ++id) {
            ws.addWindow(*id);
            ws.move(*id, Direction::Right, false);
        }
        ws.addWindow(master);
        for (const WindowId id : left)
            ws.addWindow(id);
        return ws;
    }

}

// Swap

TEST(DropOnAWindow, SwapsTheTwo) {
    Workspace ws = filled(1, 3); // 1 Master, 2 Left, 3 Stack

    EXPECT_TRUE(ws.drop(2, 2000, 700)); // Left window onto Master

    EXPECT_TRUE(near(ws.rectOf(2), MASTER_ZONE));
    EXPECT_TRUE(near(ws.rectOf(1), LEFT_ZONE));
    EXPECT_TRUE(near(ws.rectOf(3), RIGHT_ZONE));
}

TEST(DropOnAWindow, EachTakesTheOthersRowHeight) {
    Workspace ws = filled(1, 4); // 1 Master, 2 Left, 3 and 4 Stack
    ws.layoutMessage("resize taller", 3); // Stack Rows 0.55 / 0.45

    EXPECT_TRUE(ws.drop(4, 4000, 100)); // the bottom Stack Row onto the top one

    EXPECT_TRUE(near(ws.rectOf(4), rightRow(0, 792)));
    EXPECT_TRUE(near(ws.rectOf(3), rightRow(792, 648)));
}

TEST(DropOnAWindow, WithAWindowFromAnotherZoneTakesItsZoneWidth) {
    Workspace ws = filled(1, 4); // 1 Master, 2 Left, 3 and 4 Stack

    EXPECT_TRUE(ws.drop(1, 4000, 1000)); // Master onto the bottom Stack Row, above its add area

    EXPECT_TRUE(near(ws.rectOf(1), rightRow(720, 720)));
    EXPECT_TRUE(near(ws.rectOf(4), MASTER_ZONE));
}

// The add area

TEST(DropOnTheAddArea, OfTheStackAddsANewBottomRow) {
    Workspace ws = filled(1, 4); // 1 Master, 2 Left, 3 and 4 Stack

    EXPECT_TRUE(ws.drop(1, 4000, 1400)); // Master onto the bottom quarter of 4

    EXPECT_TRUE(near(ws.rectOf(3), rightRow(0, 480)));
    EXPECT_TRUE(near(ws.rectOf(4), rightRow(480, 480)));
    EXPECT_TRUE(near(ws.rectOf(1), rightRow(960, 480)));
    EXPECT_EQ(ws.nextWindowRect(), MASTER_ZONE); // Master left a Hole
}

TEST(DropOnTheAddArea, OfAFullLeftSwapsWithItsBottomRow) {
    Workspace ws = filled(1, 3); // 1 Master, 2 Left (full), 3 Stack

    EXPECT_TRUE(ws.drop(3, 600, 1400));

    EXPECT_TRUE(near(ws.rectOf(3), LEFT_ZONE));
    EXPECT_TRUE(near(ws.rectOf(2), RIGHT_ZONE));
}

TEST(DropOnTheAddArea, OfALeftBelowItsRowLimitAddsANewBottomRow) {
    Workspace ws = filled(2, 4); // 1 Master, 2 and 3 Left (full), 4 Stack
    ws.removeWindow(3);          // Left below its limit again

    EXPECT_TRUE(ws.drop(4, 600, 1400));

    EXPECT_TRUE(near(ws.rectOf(2), leftRow(0, 720)));
    EXPECT_TRUE(near(ws.rectOf(4), leftRow(720, 720)));
}

TEST(DropOnTheAddArea, OfItsOwnZoneMovesItToTheBottom) {
    Workspace ws = filled(1, 5);          // 1 Master, 2 Left, 3, 4 and 5 Stack
    ws.layoutMessage("resize taller", 3); // Stack Rows 552 / 444 / 444 px

    EXPECT_TRUE(ws.drop(3, 4000, 1400));

    EXPECT_TRUE(near(ws.rectOf(4), rightRow(0, 552)));
    EXPECT_TRUE(near(ws.rectOf(5), rightRow(552, 444)));
    EXPECT_TRUE(near(ws.rectOf(3), rightRow(996, 444)));
}

TEST(DropOnTheAddArea, OfItsOwnFullLeftMovesItToTheBottom) {
    Workspace ws = filled(3, 4); // 1 Master, 2, 3 and 4 Left (full)

    EXPECT_TRUE(ws.drop(2, 600, 1400));

    EXPECT_TRUE(near(ws.rectOf(3), leftRow(0, 480)));
    EXPECT_TRUE(near(ws.rectOf(4), leftRow(480, 480)));
    EXPECT_TRUE(near(ws.rectOf(2), leftRow(960, 480)));
}

// Empty zones

TEST(DropIntoAnEmptyZone, FillsAMasterHole) {
    Workspace ws = filled(1, 4); // 1 Master, 2 Left, 3 and 4 Stack
    ws.removeWindow(1);          // Master Hole

    EXPECT_TRUE(ws.drop(3, 2000, 700));

    EXPECT_TRUE(near(ws.rectOf(3), MASTER_ZONE));
    EXPECT_TRUE(near(ws.rectOf(4), RIGHT_ZONE)); // the Stack closed the gap
}

TEST(DropIntoAnEmptyZone, FillsALeftHole) {
    Workspace ws = filled(1, 3); // 1 Master, 2 Left, 3 Stack
    ws.removeWindow(2);          // Left Hole

    EXPECT_TRUE(ws.drop(1, 600, 700));

    EXPECT_TRUE(near(ws.rectOf(1), LEFT_ZONE));
    EXPECT_EQ(ws.nextWindowRect(), MASTER_ZONE); // Master left a Hole
}

TEST(DropIntoAnEmptyZone, FillsAnEmptyStack) {
    Workspace ws = filled(1, 2); // 1 Master, 2 Left, empty Stack

    EXPECT_TRUE(ws.drop(2, 4000, 100));

    EXPECT_TRUE(near(ws.rectOf(2), RIGHT_ZONE));
    EXPECT_EQ(ws.nextWindowRect(), LEFT_ZONE); // the last Left window left a Hole
}

// Drops that change nothing

TEST(DropChangesNothing, OnItsOwnPlace) {
    Workspace ws = filled(1, 4); // 1 Master, 2 Left, 3 and 4 Stack

    EXPECT_FALSE(ws.drop(1, 2000, 700));
    EXPECT_FALSE(ws.drop(3, 4000, 100));
    EXPECT_FALSE(ws.drop(4, 4000, 1400)); // the add area of its own bottom Row

    EXPECT_TRUE(near(ws.rectOf(1), MASTER_ZONE));
    EXPECT_TRUE(near(ws.rectOf(3), rightRow(0, 720)));
    EXPECT_TRUE(near(ws.rectOf(4), rightRow(720, 720)));
}

TEST(DropChangesNothing, InTheAddAreaOfItsOwnLoneRow) {
    Workspace ws = filled(1, 3); // 1 Master, 2 Left, 3 Stack

    EXPECT_FALSE(ws.drop(2, 600, 1400));
    EXPECT_TRUE(near(ws.rectOf(2), LEFT_ZONE));
}

TEST(DropChangesNothing, OutsideTheWorkspace) {
    Workspace ws = filled(1, 3); // 1 Master, 2 Left, 3 Stack

    EXPECT_FALSE(ws.drop(2, -10, 700));
    EXPECT_FALSE(ws.drop(2, 5120, 700));
    EXPECT_FALSE(ws.drop(2, 2000, -1));
    EXPECT_FALSE(ws.drop(2, 2000, 1440));

    EXPECT_TRUE(near(ws.rectOf(2), LEFT_ZONE));
    EXPECT_TRUE(near(ws.rectOf(1), MASTER_ZONE));
}

TEST(DropChangesNothing, ForAWindowNotOnTheWorkspace) {
    Workspace ws = filled(1, 3);

    EXPECT_FALSE(ws.drop(9, 2000, 700));
    EXPECT_TRUE(near(ws.rectOf(1), MASTER_ZONE));
}

// Gaps

TEST(DropInAGap, CountsAsTheZoneItIsIn) {
    Workspace ws = filled(1, 3); // 1 Master, 2 Left, 3 Stack

    // Just left of the Left|Master Divider, where Hyprland draws a gap.
    EXPECT_TRUE(ws.drop(3, 1275, 700));

    EXPECT_TRUE(near(ws.rectOf(3), LEFT_ZONE));
}

TEST(DropInAGap, BetweenRowsCountsAsTheNearestRow) {
    Workspace ws = filled(1, 5); // 1 Master, 2 Left, 3, 4 and 5 Stack, 480 px each

    EXPECT_TRUE(ws.drop(1, 4000, 475)); // just above the edge between 3 and 4
    EXPECT_TRUE(near(ws.rectOf(1), rightRow(0, 480)));

    EXPECT_TRUE(ws.drop(1, 4000, 485)); // just below it
    EXPECT_TRUE(near(ws.rectOf(1), rightRow(480, 480)));
}

TEST(DropOnAnOffsetWorkspace, UsesScreenCoordinates) {
    // A second monitor, or reserved space at the top and left.
    Workspace ws{Rect{100, 40, 5120, 1440}};
    for (WindowId id = 1; id <= 3; ++id)
        ws.addWindow(id); // 1 Master, 2 Left, 3 Stack

    EXPECT_FALSE(ws.drop(2, 50, 700)); // left of the workspace
    EXPECT_TRUE(ws.drop(2, 3990, 60));  // the Stack's top-left corner

    EXPECT_TRUE(near(ws.rectOf(2), Rect{3940, 40, 1280, 1440}));
}

// Other gestures

TEST(Drop, EndsAMouseResize) {
    Workspace ws = filled(1, 3); // 1 Master, 2 Left, 3 Stack
    ws.dragResize(2, 100, 0, threecol::Corner::BottomRight);

    EXPECT_TRUE(ws.drop(3, 2000, 700));

    EXPECT_FALSE(ws.resizing());
}

// Mirrored: the add area works by role, on either side.

TEST(DropOnTheAddAreaMirrored, OfAFullRightSwapsWithItsBottomRow) {
    Workspace ws = mirrored(1, {2, 3}, 4, {1});

    EXPECT_TRUE(ws.drop(2, 4000, 1400));

    EXPECT_TRUE(near(ws.rectOf(2), RIGHT_ZONE));
    EXPECT_TRUE(near(ws.rectOf(1), leftRow(0, 720)));
}

TEST(DropOnTheAddAreaMirrored, OfARightBelowItsRowLimitAddsANewBottomRow) {
    Workspace ws = mirrored(2, {2, 3}, 4, {1});

    EXPECT_TRUE(ws.drop(2, 4000, 1400));

    EXPECT_TRUE(near(ws.rectOf(1), rightRow(0, 720)));
    EXPECT_TRUE(near(ws.rectOf(2), rightRow(720, 720)));
    EXPECT_TRUE(near(ws.rectOf(3), LEFT_ZONE));
}

TEST(DropOnTheAddAreaMirrored, OfTheLeftStackAddsANewBottomRow) {
    Workspace ws = mirrored(1, {2, 3}, 4, {1});

    EXPECT_TRUE(ws.drop(4, 600, 1400));

    EXPECT_TRUE(near(ws.rectOf(2), leftRow(0, 480)));
    EXPECT_TRUE(near(ws.rectOf(3), leftRow(480, 480)));
    EXPECT_TRUE(near(ws.rectOf(4), leftRow(960, 480)));
    EXPECT_EQ(ws.nextWindowRect(), MASTER_ZONE);
}

TEST(DropOnAWindowMirrored, SwapsWithThatWindowInAFullRight) {
    Workspace ws = mirrored(2, {2}, 4, {5, 1});

    EXPECT_TRUE(ws.drop(2, 4000, 300)); // onto 5, the top Right Row

    EXPECT_TRUE(near(ws.rectOf(2), rightRow(0, 720)));
    EXPECT_TRUE(near(ws.rectOf(5), LEFT_ZONE));
}

TEST(DropOnTheAddAreaTwoStacks, OfEitherSideAddsANewBottomRow) {
    Workspace ws = mirrored(RowLimit::unlimited(), {2, 3}, 4, {1});

    EXPECT_TRUE(ws.drop(2, 4000, 1400));

    EXPECT_TRUE(near(ws.rectOf(1), rightRow(0, 720)));
    EXPECT_TRUE(near(ws.rectOf(2), rightRow(720, 720)));
}

TEST(DragDrop, AMissingLeftZoneIsNeverADropTarget) {
    Workspace ws = filled(0, 3); // 1 Master, Right 2 3

    // Where Left would be is Master.
    EXPECT_TRUE(ws.drop(3, 10, 1430));

    EXPECT_EQ(ws.rectOf(3), (Rect{0, 0, 3840, 1440}));
    EXPECT_EQ(ws.rectOf(1), (Rect{3840, 720, 1280, 720}));
}

TEST(DragDrop, AMissingRightZoneIsNeverADropTarget) {
    Workspace ws{AREA};
    ws.setSettings(Settings{.leftRows = RowLimit::unlimited(), .rightRows = 0});
    for (WindowId id = 1; id <= 2; ++id)
        ws.addWindow(id); // 1 Master, Left 2

    // Where Right's "add" area would be is Master.
    EXPECT_TRUE(ws.drop(2, 5110, 1430));

    EXPECT_EQ(ws.rectOf(2), (Rect{1280, 0, 3840, 1440}));
    EXPECT_EQ(ws.rectOf(1), LEFT_ZONE);
}

TEST(DragDrop, WithBothSidesMissingDroppingMasterAnywhereChangesNothing) {
    Workspace ws{AREA};
    ws.setSettings(Settings{.leftRows = 0, .rightRows = 0});
    ws.addWindow(1);

    for (const double x : {10.0, 2560.0, 5110.0})
        EXPECT_FALSE(ws.drop(1, x, 1430)) << x;
    EXPECT_EQ(ws.rectOf(1), AREA);
}
