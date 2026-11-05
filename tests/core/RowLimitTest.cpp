#include "core/Workspace.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <optional>
#include <vector>

using threecol::Corner;
using threecol::Direction;
using threecol::LayoutOpts;
using threecol::Rect;
using threecol::RowLimit;
using threecol::Settings;
using threecol::WindowId;
using threecol::Workspace;

namespace {

    // One 5120x1440 ultrawide, no gaps, default widths 0.25 / 0.5 / 0.25, so
    // Left is x 0 and the Stack x 3840, both 1280 px wide. A step of 0.05 is
    // 72 px of height and min_row_height 0.10 is 144 px.
    constexpr Rect AREA{0, 0, 5120, 1440};

    constexpr Rect MASTER_ZONE{1280, 0, 2560, 1440};
    constexpr Rect LEFT_ZONE{0, 0, 1280, 1440};
    constexpr Rect RIGHT_ZONE{3840, 0, 1280, 1440};

    // A workspace with Row limit `leftRows` and windows 1..count added in
    // Fill order.
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
            ws.move(*id, Direction::Right);
        }
        ws.addWindow(master);
        for (const WindowId id : left)
            ws.addWindow(id);
        return ws;
    }

}

TEST(RowLimit, FillOrderFillsTheBottomOfLeftUpToTheRowLimitThenTheStack) {
    Workspace ws = filled(2, 4);

    EXPECT_TRUE(near(ws.rectOf(1), MASTER_ZONE));
    EXPECT_TRUE(near(ws.rectOf(2), leftRow(0, 720)));
    EXPECT_TRUE(near(ws.rectOf(3), leftRow(720, 720)));
    EXPECT_TRUE(near(ws.rectOf(4), RIGHT_ZONE));
}

TEST(RowLimit, LayoutOptsSetTheRowLimitForOneWorkspace) {
    Workspace ws{AREA};
    ws.setSettings(Settings{.leftRows = 1}, LayoutOpts{{"left_rows", "3"}}, "3");
    for (WindowId id = 1; id <= 5; ++id)
        ws.addWindow(id);

    EXPECT_TRUE(near(ws.rectOf(2), leftRow(0, 480)));
    EXPECT_TRUE(near(ws.rectOf(3), leftRow(480, 480)));
    EXPECT_TRUE(near(ws.rectOf(4), leftRow(960, 480)));
    EXPECT_TRUE(near(ws.rectOf(5), RIGHT_ZONE));
}

TEST(RowLimit, ClosingMasterLeavesAHoleAndTheLeftRowsStay) {
    Workspace ws = filled(2, 3); // 1 Master, 2 and 3 Left

    ws.removeWindow(1);

    EXPECT_TRUE(near(ws.rectOf(2), leftRow(0, 720)));
    EXPECT_TRUE(near(ws.rectOf(3), leftRow(720, 720)));
    EXPECT_TRUE(near(ws.nextWindowRect(), MASTER_ZONE));
}

TEST(RowLimit, ClosingALeftRowClosesTheGapAndTheOthersShareTheHeight) {
    Workspace ws = filled(3, 5); // 2, 3 and 4 Left, 5 Stack

    ws.removeWindow(3);

    EXPECT_TRUE(near(ws.rectOf(2), leftRow(0, 720)));
    EXPECT_TRUE(near(ws.rectOf(4), leftRow(720, 720)));
    EXPECT_TRUE(near(ws.rectOf(5), RIGHT_ZONE));
}

TEST(RowLimit, ClosingTheLastLeftWindowLeavesALeftHole) {
    Workspace ws = filled(2, 4);

    ws.removeWindow(2);
    ws.removeWindow(3);

    EXPECT_TRUE(near(ws.rectOf(1), MASTER_ZONE));
    EXPECT_TRUE(near(ws.rectOf(4), RIGHT_ZONE));
    EXPECT_TRUE(near(ws.nextWindowRect(), LEFT_ZONE));
}

TEST(RowLimit, AClosedLeftRowIsRefilledBeforeTheStack) {
    Workspace ws = filled(2, 4);
    ws.removeWindow(2);

    ws.addWindow(5);

    EXPECT_TRUE(near(ws.rectOf(3), leftRow(0, 720)));
    EXPECT_TRUE(near(ws.rectOf(5), leftRow(720, 720)));
    EXPECT_TRUE(near(ws.rectOf(4), RIGHT_ZONE));
}

TEST(RowLimit, FocusAfterCloseWalksTheChainThroughLeftRows) {
    Workspace ws = filled(2, 4); // Chain: 2, 3, 1, 4

    EXPECT_EQ(ws.removeWindow(2), 3u);
    EXPECT_EQ(ws.removeWindow(3), 1u);
}

// Left Rows follow the Stack's height and resize rules.

TEST(RowLimit, TallerGrowsTheFocusedLeftRowAndTheOthersGiveWayInProportion) {
    Workspace ws = filled(3, 4); // 2, 3 and 4 share Left, 480 px each

    EXPECT_TRUE(ws.layoutMessage("resize taller", 3));

    EXPECT_TRUE(near(ws.rectOf(2), leftRow(0, 444)));
    EXPECT_TRUE(near(ws.rectOf(3), leftRow(444, 552)));
    EXPECT_TRUE(near(ws.rectOf(4), leftRow(996, 444)));
}

TEST(RowLimit, ALeftRowsHeightLeavesTheStackAlone) {
    Workspace ws = filled(2, 5); // 4 and 5 share the Stack

    ws.layoutMessage("resize taller", 2);

    EXPECT_TRUE(near(ws.rectOf(4), rightRow(0, 720)));
    EXPECT_TRUE(near(ws.rectOf(5), rightRow(720, 720)));
}

TEST(RowLimit, HeightKeysDoNothingOnALoneLeftRow) {
    Workspace ws = filled(2, 2);

    EXPECT_FALSE(ws.layoutMessage("resize taller", 2));
    EXPECT_FALSE(ws.layoutMessage("resize shorter", 2));
    EXPECT_TRUE(near(ws.rectOf(2), LEFT_ZONE));
}

TEST(RowLimit, ShorterStopsAtTheMinimumRowHeight) {
    Workspace ws = filled(2, 3);
    for (int i = 0; i < 20; ++i)
        ws.layoutMessage("resize shorter", 2);

    EXPECT_FALSE(ws.layoutMessage("resize shorter", 2));
    EXPECT_TRUE(near(ws.rectOf(2), leftRow(0, 144)));
    EXPECT_TRUE(near(ws.rectOf(3), leftRow(144, 1296)));
}

TEST(RowLimit, WiderOnAnyLeftRowWidensTheWholeLeftZone) {
    Workspace ws = filled(2, 3);

    EXPECT_TRUE(ws.layoutMessage("resize wider", 3));

    EXPECT_TRUE(near(ws.rectOf(2), {0, 0, 1536, 720}));
    EXPECT_TRUE(near(ws.rectOf(3), {0, 720, 1536, 720}));
}

TEST(RowLimit, ResetMakesTheLeftRowsEqualAgain) {
    Workspace ws = filled(2, 3);
    ws.layoutMessage("resize taller", 2);

    EXPECT_TRUE(ws.layoutMessage("reset", 2));

    EXPECT_TRUE(near(ws.rectOf(2), leftRow(0, 720)));
    EXPECT_TRUE(near(ws.rectOf(3), leftRow(720, 720)));
}

TEST(RowLimit, DraggingALeftRowDownMovesItsBottomEdgeAndTheDivider) {
    Workspace ws = filled(3, 4);

    // The middle Row grabbed at its bottom half: its bottom edge moves.
    EXPECT_TRUE(ws.dragResize(3, 100, 60, Corner::BottomRight));
    EXPECT_TRUE(ws.dragResize(3, 0, 40, Corner::BottomRight));

    EXPECT_TRUE(near(ws.rectOf(2), {0, 0, 1380, 480}));
    EXPECT_TRUE(near(ws.rectOf(3), {0, 480, 1380, 580}));
    EXPECT_TRUE(near(ws.rectOf(4), {0, 1060, 1380, 380}));
}

TEST(RowLimit, DraggingTheTopLeftRowUpMovesItsBottomEdgeOnly) {
    Workspace ws = filled(2, 3);

    // The top Row has only a bottom edge, whichever half is grabbed.
    ws.dragResize(2, 0, -120, Corner::TopLeft);

    EXPECT_TRUE(near(ws.rectOf(2), leftRow(0, 600)));
    EXPECT_TRUE(near(ws.rectOf(3), leftRow(600, 840)));
}

TEST(RowLimit, ALoneLeftRowDragsSidewaysOnly) {
    Workspace ws = filled(2, 2);

    ws.dragResize(2, 100, 200, Corner::BottomRight);

    EXPECT_TRUE(near(ws.rectOf(2), {0, 0, 1380, 1440}));
}

TEST(RowLimit, AWindowJoiningLeftEndsTheDrag) {
    Workspace ws = filled(2, 2);
    ws.dragResize(2, 100, 0, Corner::BottomRight);

    ws.addWindow(3);

    EXPECT_FALSE(ws.resizing());
    EXPECT_FALSE(ws.dragResize(2, 100, 0, Corner::BottomRight));
}

// Left uses only as many Rows as fit min_row_height.

TEST(RowLimit, ALimitTooHighForTheMinimumRowHeightIsMadeToFit) {
    Workspace  ws{AREA};
    const auto warnings = ws.setSettings(Settings{.minRowHeight = 0.3, .leftRows = 5}).warnings;

    EXPECT_EQ(ws.settings().leftRows, 3);
    ASSERT_EQ(warnings.size(), 1u);
    EXPECT_NE(warnings[0].find("left_rows"), std::string::npos);

    for (WindowId id = 1; id <= 5; ++id)
        ws.addWindow(id);
    EXPECT_TRUE(near(ws.rectOf(4), leftRow(960, 480)));
    EXPECT_TRUE(near(ws.rectOf(5), RIGHT_ZONE));
}

TEST(RowLimit, ALayoutOptsLimitTooHighNamesTheWorkspace) {
    Workspace  ws{AREA};
    const auto warnings = ws.setSettings(Settings{.minRowHeight = 0.25}, LayoutOpts{{"left_rows", "9"}}, "3").warnings;

    EXPECT_EQ(ws.settings().leftRows, 4);
    ASSERT_EQ(warnings.size(), 1u);
    EXPECT_NE(warnings[0].find("workspace 3"), std::string::npos);
}

TEST(RowLimit, ALimitThatFitsExactlyIsKept) {
    Workspace  ws{AREA};
    const auto warnings = ws.setSettings(Settings{.minRowHeight = 0.1, .leftRows = 10}).warnings;

    EXPECT_EQ(ws.settings().leftRows, 10);
    EXPECT_TRUE(warnings.empty());
}

// Lowering the limit moves the extra bottom Left windows to the top of the
// Stack in Chain order; raising it moves nothing.

TEST(RowLimit, LoweringTheLimitMovesTheExtraBottomLeftWindowsToTheTopOfTheStack) {
    Workspace ws = filled(4, 6); // 2, 3, 4, 5 Left; 6 Stack

    ws.setSettings(Settings{.leftRows = 2});

    EXPECT_TRUE(near(ws.rectOf(2), leftRow(0, 720)));
    EXPECT_TRUE(near(ws.rectOf(3), leftRow(720, 720)));
    EXPECT_TRUE(near(ws.rectOf(4), rightRow(0, 480)));
    EXPECT_TRUE(near(ws.rectOf(5), rightRow(480, 480)));
    EXPECT_TRUE(near(ws.rectOf(6), rightRow(960, 480)));
    EXPECT_TRUE(near(ws.rectOf(1), MASTER_ZONE));
}

TEST(RowLimit, LoweringTheLimitKeepsTheChainOrder) {
    Workspace ws = filled(3, 5); // Chain: 2, 3, 4, 1, 5

    ws.setSettings(Settings{.leftRows = 1});

    // Chain now: 2, 1, 3, 4, 5. Closing 1 focuses the moved 3.
    EXPECT_EQ(ws.removeWindow(1), 3u);
    EXPECT_EQ(ws.removeWindow(4), 5u);
}

TEST(RowLimit, LoweringTheLimitThroughLayoutOptsMovesWindowsToo) {
    Workspace ws{AREA};
    ws.setSettings(Settings{}, LayoutOpts{{"left_rows", "3"}}, "3");
    for (WindowId id = 1; id <= 4; ++id)
        ws.addWindow(id);

    ws.setSettings(Settings{}, LayoutOpts{}, "3");

    EXPECT_TRUE(near(ws.rectOf(2), LEFT_ZONE));
    EXPECT_TRUE(near(ws.rectOf(3), rightRow(0, 720)));
    EXPECT_TRUE(near(ws.rectOf(4), rightRow(720, 720)));
}

TEST(RowLimit, RaisingTheMinimumRowHeightLowersTheLimitAndMovesWindows) {
    Workspace ws = filled(4, 5);

    ws.setSettings(Settings{.minRowHeight = 0.4, .leftRows = 4});

    EXPECT_TRUE(near(ws.rectOf(2), leftRow(0, 720)));
    EXPECT_TRUE(near(ws.rectOf(3), leftRow(720, 720)));
    EXPECT_TRUE(near(ws.rectOf(4), rightRow(0, 720)));
    EXPECT_TRUE(near(ws.rectOf(5), rightRow(720, 720)));
}

TEST(RowLimit, LoweringTheLimitEndsARowEdgeDrag) {
    Workspace ws = filled(2, 5); // 4 and 5 share the Stack
    ws.dragResize(4, 0, 50, Corner::None);

    ws.setSettings(Settings{.leftRows = 1});

    EXPECT_FALSE(ws.resizing());
}

TEST(RowLimit, LoweringTheLimitKeepsADividerOnlyDrag) {
    Workspace ws = filled(2, 4);
    ws.dragResize(1, 100, 0, Corner::BottomRight); // Master|Right

    ws.setSettings(Settings{.leftRows = 1});

    EXPECT_TRUE(ws.resizing());
    EXPECT_TRUE(ws.dragResize(1, 100, 0, Corner::BottomRight));
    // Centred Master: 200 px of travel widens Master by 200 px on each side.
    EXPECT_TRUE(near(ws.rectOf(1), {1080, 0, 2960, 1440}));
}

TEST(RowLimit, APushedOutWindowWithNoRoomEndsItsOwnDividerOnlyDrag) {
    Workspace ws{AREA};
    ws.setSettings(Settings{.leftRows = 1, .rightRows = 1});
    for (WindowId id = 1; id <= 3; ++id)
        ws.addWindow(id); // 1 Master, 2 Left, 3 Right
    ws.dragResize(2, 100, 0, Corner::BottomRight); // a lone Row: Left|Master only
    ASSERT_TRUE(ws.resizing());

    const auto noRoom = ws.setSettings(Settings{.leftRows = 0, .rightRows = 1}).noRoom;

    EXPECT_EQ(noRoom, std::vector<WindowId>{2});
    EXPECT_FALSE(ws.resizing());
}

TEST(RowLimit, AWindowPushedToTheOtherSideEndsItsDividerOnlyDrag) {
    Workspace ws = filled(1, 2); // 1 Master, 2 Left
    ws.dragResize(2, 100, 0, Corner::BottomRight); // a lone Row: Left|Master only
    ASSERT_TRUE(ws.resizing());

    ws.setSettings(Settings{.leftRows = 0}); // 2 goes to the Right Stack

    EXPECT_FALSE(ws.resizing());
    EXPECT_FALSE(ws.dragResize(2, 100, 0, Corner::BottomRight));
}

TEST(RowLimit, AMissingZoneEndsADragOfItsDivider) {
    Workspace ws = filled(1, 1); // Master only
    ws.dragResize(1, 100, 0, Corner::TopLeft); // Left|Master
    ASSERT_TRUE(ws.resizing());

    ws.setSettings(Settings{.leftRows = 0});

    EXPECT_FALSE(ws.resizing());
}

TEST(RowLimit, AMissingZoneKeepsADragOfTheOtherDivider) {
    Workspace ws = filled(1, 1); // Master only
    ws.dragResize(1, 100, 0, Corner::TopRight); // Master|Right
    ASSERT_TRUE(ws.resizing());

    ws.setSettings(Settings{.leftRows = 0});

    EXPECT_TRUE(ws.resizing());
}

TEST(RowLimit, SettingALimitToZeroPushesTheSidesWindowsToTheTopOfTheOtherSidesStack) {
    Workspace ws = filled(2, 4); // 1 Master, Left 2 3, Right 4

    const auto noRoom = ws.setSettings(Settings{.leftRows = 0}).noRoom;

    EXPECT_TRUE(noRoom.empty());
    EXPECT_TRUE(near(ws.rectOf(1), {0, 0, 3840, 1440}));
    EXPECT_TRUE(near(ws.rectOf(2), rightRow(0, 480)));
    EXPECT_TRUE(near(ws.rectOf(3), rightRow(480, 480)));
    EXPECT_TRUE(near(ws.rectOf(4), rightRow(960, 480)));
}

TEST(RowLimit, SettingALimitToZeroFillsTheOtherSidesFreeRowsThenFloats) {
    Workspace ws{AREA};
    ws.setSettings(Settings{.leftRows = 3, .rightRows = 2});
    for (WindowId id = 1; id <= 5; ++id)
        ws.addWindow(id); // 1 Master, Left 2 3 4, Right 5

    const auto noRoom = ws.setSettings(Settings{.leftRows = 0, .rightRows = 2}).noRoom;

    EXPECT_EQ(noRoom, (std::vector<WindowId>{3, 4}));
    EXPECT_TRUE(near(ws.rectOf(2), rightRow(0, 720)));
    EXPECT_TRUE(near(ws.rectOf(5), rightRow(720, 720)));
    EXPECT_FALSE(ws.rectOf(3));
    EXPECT_FALSE(ws.rectOf(4));
}

TEST(RowLimit, ALimitChangeThatMovesNothingKeepsADrag) {
    Workspace ws = filled(2, 3);
    ws.dragResize(2, 100, 0, Corner::None);

    ws.setSettings(Settings{.leftRows = 3});

    EXPECT_TRUE(ws.resizing());
}

TEST(RowLimit, RaisingTheLimitMovesNothing) {
    Workspace ws = filled(1, 4); // 2 Left; 3 and 4 Stack

    ws.setSettings(Settings{.leftRows = 3});

    EXPECT_TRUE(near(ws.rectOf(2), LEFT_ZONE));
    EXPECT_TRUE(near(ws.rectOf(3), rightRow(0, 720)));
    EXPECT_TRUE(near(ws.rectOf(4), rightRow(720, 720)));
}

TEST(RowLimit, RaisingTheLimitLetsTheNextWindowJoinLeft) {
    Workspace ws = filled(1, 3);
    ws.setSettings(Settings{.leftRows = 2});

    ws.addWindow(4);

    EXPECT_TRUE(near(ws.rectOf(2), leftRow(0, 720)));
    EXPECT_TRUE(near(ws.rectOf(4), leftRow(720, 720)));
    EXPECT_TRUE(near(ws.rectOf(3), RIGHT_ZONE));
}

// min_row_height caps the Row limit of a Bounded Right as it does Left's.

TEST(RowLimit, ARightLimitTooHighForTheMinimumRowHeightIsMadeToFit) {
    Workspace  ws{AREA};
    const auto warnings = ws.setSettings(Settings{.minRowHeight = 0.3, .leftRows = RowLimit::unlimited(), .rightRows = 5}).warnings;

    EXPECT_EQ(ws.settings().rightRows, 3);
    ASSERT_EQ(warnings.size(), 1u);
    EXPECT_NE(warnings[0].find("right_rows"), std::string::npos) << warnings[0];
}

TEST(RowLimit, AStackIsNeverCappedByTheMinimumRowHeight) {
    Workspace  ws{AREA};
    const auto warnings = ws.setSettings(Settings{.minRowHeight = 0.5, .leftRows = RowLimit::unlimited()}).warnings;

    EXPECT_TRUE(warnings.empty());
    EXPECT_EQ(ws.settings().leftRows, RowLimit::unlimited());
    EXPECT_EQ(ws.settings().rightRows, RowLimit::unlimited());
}

// Lowering a Right limit pushes its extra bottom windows to the bottom of
// the Left Stack, its end nearest Master, keeping their Chain order.

TEST(RowLimit, LoweringTheRightLimitMovesItsExtraBottomWindowsToTheBottomOfTheLeftStack) {
    Workspace ws = mirrored(3, {2}, 1, {3, 4, 5});

    ws.setSettings(Settings{.leftRows = RowLimit::unlimited(), .rightRows = 1});

    EXPECT_TRUE(near(ws.rectOf(3), RIGHT_ZONE));
    EXPECT_TRUE(near(ws.rectOf(2), leftRow(0, 480)));
    EXPECT_TRUE(near(ws.rectOf(4), leftRow(480, 480)));
    EXPECT_TRUE(near(ws.rectOf(5), leftRow(960, 480)));
    EXPECT_TRUE(near(ws.rectOf(1), MASTER_ZONE));
}

TEST(RowLimit, LoweringTheRightLimitKeepsTheChainOrder) {
    Workspace ws = mirrored(3, {2}, 1, {3, 4, 5}); // Chain: 2, 1, 3, 4, 5

    ws.setSettings(Settings{.leftRows = RowLimit::unlimited(), .rightRows = 1});

    // Chain now: 2, 4, 5, 1, 3.
    EXPECT_EQ(ws.removeWindow(2), 4u);
    EXPECT_EQ(ws.removeWindow(5), 1u);
}

TEST(RowLimit, SwitchingRightFromUnlimitedToANumberPushesItsExtrasIntoTheLeftStack) {
    Workspace ws = filled(1, 5); // 2 Left; 3, 4 and 5 Right

    ws.setSettings(Settings{.leftRows = RowLimit::unlimited(), .rightRows = 1});

    EXPECT_TRUE(near(ws.rectOf(3), RIGHT_ZONE));
    EXPECT_TRUE(near(ws.rectOf(2), leftRow(0, 480)));
    EXPECT_TRUE(near(ws.rectOf(4), leftRow(480, 480)));
    EXPECT_TRUE(near(ws.rectOf(5), leftRow(960, 480)));
}

TEST(RowLimit, SwitchingLeftFromUnlimitedToANumberPushesItsExtrasToTheTopOfRight) {
    Workspace ws = mirrored(RowLimit::unlimited(), {2, 3, 4}, 1, {5}); // two Stacks

    ws.setSettings(Settings{});

    EXPECT_TRUE(near(ws.rectOf(2), LEFT_ZONE));
    EXPECT_TRUE(near(ws.rectOf(3), rightRow(0, 480)));
    EXPECT_TRUE(near(ws.rectOf(4), rightRow(480, 480)));
    EXPECT_TRUE(near(ws.rectOf(5), rightRow(960, 480)));
}

TEST(RowLimit, LoweringTheRightLimitThroughLayoutOptsMovesWindowsToo) {
    Workspace ws{AREA};
    ws.setSettings(Settings{}, LayoutOpts{{"left_rows", "unlimited"}, {"right_rows", "unlimited"}}, "3");
    for (WindowId id = 1; id <= 2; ++id)
        ws.addWindow(id);
    ws.layoutMessage("move r", 1); // Right: 1
    ws.addWindow(3);               // Master
    ws.layoutMessage("move r", 3); // Right: 3, 1

    ws.setSettings(Settings{}, LayoutOpts{{"left_rows", "unlimited"}, {"right_rows", "1"}}, "3");

    EXPECT_TRUE(near(ws.rectOf(3), RIGHT_ZONE));
    EXPECT_TRUE(near(ws.rectOf(2), leftRow(0, 720)));
    EXPECT_TRUE(near(ws.rectOf(1), leftRow(720, 720)));
}

TEST(RowLimit, RaisingTheRightLimitMovesNothing) {
    Workspace ws = mirrored(1, {2, 3}, 4, {1});

    ws.setSettings(Settings{.leftRows = RowLimit::unlimited(), .rightRows = 3});

    EXPECT_TRUE(near(ws.rectOf(1), RIGHT_ZONE));
    EXPECT_TRUE(near(ws.rectOf(2), leftRow(0, 720)));
    EXPECT_TRUE(near(ws.rectOf(3), leftRow(720, 720)));
    EXPECT_TRUE(near(ws.rectOf(4), MASTER_ZONE));
}

// With both sides bounded, a window that finds no room isn't placed: the
// plugin floats it instead.

namespace {

    // Left bounded to `leftRows`, Right to `rightRows`, windows 1..count
    // added in Fill order.
    Workspace bothBounded(int leftRows, int rightRows, WindowId count) {
        Workspace ws{AREA};
        ws.setSettings(Settings{.leftRows = leftRows, .rightRows = rightRows});
        for (WindowId id = 1; id <= count; ++id)
            ws.addWindow(id);
        return ws;
    }

}

TEST(RowLimit, WithBothSidesBoundedAndFullTheNextWindowIsNotPlaced) {
    Workspace ws = bothBounded(1, 2, 4); // 1 Master; 2 Left; 3 and 4 Right

    EXPECT_FALSE(ws.addWindow(5));

    EXPECT_EQ(ws.rectOf(5), std::nullopt);
    EXPECT_TRUE(near(ws.rectOf(2), LEFT_ZONE));
    EXPECT_TRUE(near(ws.rectOf(3), rightRow(0, 720)));
    EXPECT_TRUE(near(ws.rectOf(4), rightRow(720, 720)));
}

TEST(RowLimit, AWindowThatFindsRoomIsPlaced) {
    Workspace ws = bothBounded(1, 2, 3);

    EXPECT_TRUE(ws.addWindow(4));

    EXPECT_TRUE(near(ws.rectOf(4), rightRow(720, 720)));
}

TEST(RowLimit, AHoleInABoundedZoneCountsAsRoom) {
    Workspace ws = bothBounded(1, 2, 4);
    ws.removeWindow(2); // Left's last window: a Hole

    EXPECT_TRUE(ws.addWindow(5));

    EXPECT_TRUE(near(ws.rectOf(5), LEFT_ZONE));
}

TEST(RowLimit, AMasterHoleCountsAsRoom) {
    Workspace ws = bothBounded(1, 2, 4);
    ws.removeWindow(1);

    EXPECT_TRUE(ws.addWindow(5));

    EXPECT_TRUE(near(ws.rectOf(5), MASTER_ZONE));
}

// Windows pushed out by a lower Row limit go into a Stack on the other side,
// else into its free Rows at its end nearest Master, in Chain order, else
// out of the layout, reported so the plugin floats them.

TEST(RowLimit, PushedOutLeftWindowsTakeFreeRightRowsAtTheTop) {
    Workspace ws = bothBounded(3, 3, 5); // 2, 3, 4 Left; 5 Right

    const auto change = ws.setSettings(Settings{.leftRows = 1, .rightRows = 3});

    EXPECT_TRUE(change.noRoom.empty());
    EXPECT_TRUE(near(ws.rectOf(2), LEFT_ZONE));
    EXPECT_TRUE(near(ws.rectOf(3), rightRow(0, 480)));
    EXPECT_TRUE(near(ws.rectOf(4), rightRow(480, 480)));
    EXPECT_TRUE(near(ws.rectOf(5), rightRow(960, 480)));
}

TEST(RowLimit, PushedOutRightWindowsTakeFreeLeftRowsAtTheBottom) {
    Workspace ws = bothBounded(1, 3, 5); // 2 Left; 3, 4, 5 Right

    const auto change = ws.setSettings(Settings{.leftRows = 3, .rightRows = 1});

    EXPECT_TRUE(change.noRoom.empty());
    EXPECT_TRUE(near(ws.rectOf(2), leftRow(0, 480)));
    EXPECT_TRUE(near(ws.rectOf(4), leftRow(480, 480)));
    EXPECT_TRUE(near(ws.rectOf(5), leftRow(960, 480)));
    EXPECT_TRUE(near(ws.rectOf(3), RIGHT_ZONE));
}

TEST(RowLimit, PushedOutWindowsFirstInChainOrderTakeTheFreeRowsAndTheRestLeave) {
    Workspace ws = bothBounded(3, 2, 5); // 2, 3, 4 Left; 5 Right

    const auto change = ws.setSettings(Settings{.leftRows = 1, .rightRows = 2});

    EXPECT_EQ(change.noRoom, std::vector<WindowId>{4});
    EXPECT_EQ(ws.rectOf(4), std::nullopt);
    EXPECT_TRUE(near(ws.rectOf(2), LEFT_ZONE));
    EXPECT_TRUE(near(ws.rectOf(3), rightRow(0, 720)));
    EXPECT_TRUE(near(ws.rectOf(5), rightRow(720, 720)));
}

TEST(RowLimit, PushedOutWindowsWithNoRoomLeaveTheLayoutInChainOrder) {
    Workspace ws = bothBounded(3, 1, 5); // 2, 3, 4 Left; 5 Right

    const auto change = ws.setSettings(Settings{.leftRows = 1, .rightRows = 1});

    EXPECT_EQ(change.noRoom, (std::vector<WindowId>{3, 4}));
    EXPECT_EQ(ws.rectOf(3), std::nullopt);
    EXPECT_EQ(ws.rectOf(4), std::nullopt);
    EXPECT_TRUE(near(ws.rectOf(2), LEFT_ZONE));
    EXPECT_TRUE(near(ws.rectOf(5), RIGHT_ZONE));
    // Chain now: 2, 1, 5.
    EXPECT_EQ(ws.removeWindow(2), 1u);
}

TEST(RowLimit, PushedOutWindowsIntoAStackLeaveNone) {
    Workspace ws = filled(3, 5);

    const auto change = ws.setSettings(Settings{.leftRows = 1});

    EXPECT_TRUE(change.noRoom.empty());
}

TEST(RowLimit, LoweringBothLimitsAtOnceKeepsEachSidesTopRowsAndTheRestLeaveInChainOrder) {
    Workspace ws{AREA};
    ws.setSettings(Settings{.leftRows = 3});
    for (WindowId id = 1; id <= 6; ++id)
        ws.addWindow(id); // 1 Master; 2, 3, 4 Left; 5, 6 Right

    // Two Rows for five side windows: a full side has no room for the
    // other's, so which leave doesn't depend on which side goes first.
    const auto change = ws.setSettings(Settings{.leftRows = 1, .rightRows = 1});

    EXPECT_EQ(change.noRoom, (std::vector<WindowId>{3, 4, 6}));
    EXPECT_TRUE(near(ws.rectOf(2), LEFT_ZONE));
    EXPECT_TRUE(near(ws.rectOf(5), RIGHT_ZONE));
}
