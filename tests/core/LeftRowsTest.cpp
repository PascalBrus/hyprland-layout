#include "core/Workspace.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <optional>

using threecol::Corner;
using threecol::LayoutOpts;
using threecol::Rect;
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
    constexpr Rect STACK_ZONE{3840, 0, 1280, 1440};

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

    Rect stackRow(double y, double h) {
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

}

TEST(LeftRows, FillOrderFillsTheBottomOfLeftUpToTheRowLimitThenTheStack) {
    Workspace ws = filled(2, 4);

    EXPECT_TRUE(near(ws.rectOf(1), MASTER_ZONE));
    EXPECT_TRUE(near(ws.rectOf(2), leftRow(0, 720)));
    EXPECT_TRUE(near(ws.rectOf(3), leftRow(720, 720)));
    EXPECT_TRUE(near(ws.rectOf(4), STACK_ZONE));
}

TEST(LeftRows, LayoutOptsSetTheRowLimitForOneWorkspace) {
    Workspace ws{AREA};
    ws.setSettings(Settings{.leftRows = 1}, LayoutOpts{{"left_rows", "3"}}, "3");
    for (WindowId id = 1; id <= 5; ++id)
        ws.addWindow(id);

    EXPECT_TRUE(near(ws.rectOf(2), leftRow(0, 480)));
    EXPECT_TRUE(near(ws.rectOf(3), leftRow(480, 480)));
    EXPECT_TRUE(near(ws.rectOf(4), leftRow(960, 480)));
    EXPECT_TRUE(near(ws.rectOf(5), STACK_ZONE));
}

TEST(LeftRows, ClosingMasterLeavesAHoleAndTheLeftRowsStay) {
    Workspace ws = filled(2, 3); // 1 Master, 2 and 3 Left

    ws.removeWindow(1);

    EXPECT_TRUE(near(ws.rectOf(2), leftRow(0, 720)));
    EXPECT_TRUE(near(ws.rectOf(3), leftRow(720, 720)));
    EXPECT_TRUE(near(ws.nextWindowRect(), MASTER_ZONE));
}

TEST(LeftRows, ClosingALeftRowClosesTheGapAndTheOthersShareTheHeight) {
    Workspace ws = filled(3, 5); // 2, 3 and 4 Left, 5 Stack

    ws.removeWindow(3);

    EXPECT_TRUE(near(ws.rectOf(2), leftRow(0, 720)));
    EXPECT_TRUE(near(ws.rectOf(4), leftRow(720, 720)));
    EXPECT_TRUE(near(ws.rectOf(5), STACK_ZONE));
}

TEST(LeftRows, ClosingTheLastLeftWindowLeavesALeftHole) {
    Workspace ws = filled(2, 4);

    ws.removeWindow(2);
    ws.removeWindow(3);

    EXPECT_TRUE(near(ws.rectOf(1), MASTER_ZONE));
    EXPECT_TRUE(near(ws.rectOf(4), STACK_ZONE));
    EXPECT_TRUE(near(ws.nextWindowRect(), LEFT_ZONE));
}

TEST(LeftRows, AClosedLeftRowIsRefilledBeforeTheStack) {
    Workspace ws = filled(2, 4);
    ws.removeWindow(2);

    ws.addWindow(5);

    EXPECT_TRUE(near(ws.rectOf(3), leftRow(0, 720)));
    EXPECT_TRUE(near(ws.rectOf(5), leftRow(720, 720)));
    EXPECT_TRUE(near(ws.rectOf(4), STACK_ZONE));
}

TEST(LeftRows, FocusAfterCloseWalksTheChainThroughLeftRows) {
    Workspace ws = filled(2, 4); // Chain: 2, 3, 1, 4

    EXPECT_EQ(ws.removeWindow(2), 3u);
    EXPECT_EQ(ws.removeWindow(3), 1u);
}

// Left Rows follow the Stack's height and resize rules.

TEST(LeftRows, TallerGrowsTheFocusedLeftRowAndTheOthersGiveWayInProportion) {
    Workspace ws = filled(3, 4); // 2, 3 and 4 share Left, 480 px each

    EXPECT_TRUE(ws.layoutMessage("resize taller", 3));

    EXPECT_TRUE(near(ws.rectOf(2), leftRow(0, 444)));
    EXPECT_TRUE(near(ws.rectOf(3), leftRow(444, 552)));
    EXPECT_TRUE(near(ws.rectOf(4), leftRow(996, 444)));
}

TEST(LeftRows, ALeftRowsHeightLeavesTheStackAlone) {
    Workspace ws = filled(2, 5); // 4 and 5 share the Stack

    ws.layoutMessage("resize taller", 2);

    EXPECT_TRUE(near(ws.rectOf(4), stackRow(0, 720)));
    EXPECT_TRUE(near(ws.rectOf(5), stackRow(720, 720)));
}

TEST(LeftRows, HeightKeysDoNothingOnALoneLeftRow) {
    Workspace ws = filled(2, 2);

    EXPECT_FALSE(ws.layoutMessage("resize taller", 2));
    EXPECT_FALSE(ws.layoutMessage("resize shorter", 2));
    EXPECT_TRUE(near(ws.rectOf(2), LEFT_ZONE));
}

TEST(LeftRows, ShorterStopsAtTheMinimumRowHeight) {
    Workspace ws = filled(2, 3);
    for (int i = 0; i < 20; ++i)
        ws.layoutMessage("resize shorter", 2);

    EXPECT_FALSE(ws.layoutMessage("resize shorter", 2));
    EXPECT_TRUE(near(ws.rectOf(2), leftRow(0, 144)));
    EXPECT_TRUE(near(ws.rectOf(3), leftRow(144, 1296)));
}

TEST(LeftRows, WiderOnAnyLeftRowWidensTheWholeLeftZone) {
    Workspace ws = filled(2, 3);

    EXPECT_TRUE(ws.layoutMessage("resize wider", 3));

    EXPECT_TRUE(near(ws.rectOf(2), {0, 0, 1536, 720}));
    EXPECT_TRUE(near(ws.rectOf(3), {0, 720, 1536, 720}));
}

TEST(LeftRows, ResetMakesTheLeftRowsEqualAgain) {
    Workspace ws = filled(2, 3);
    ws.layoutMessage("resize taller", 2);

    EXPECT_TRUE(ws.layoutMessage("reset", 2));

    EXPECT_TRUE(near(ws.rectOf(2), leftRow(0, 720)));
    EXPECT_TRUE(near(ws.rectOf(3), leftRow(720, 720)));
}

TEST(LeftRows, DraggingALeftRowDownMovesItsBottomEdgeAndTheDivider) {
    Workspace ws = filled(3, 4);

    // The middle Row grabbed at its bottom half: its bottom edge moves.
    EXPECT_TRUE(ws.dragResize(3, 100, 60, Corner::BottomRight));
    EXPECT_TRUE(ws.dragResize(3, 0, 40, Corner::BottomRight));

    EXPECT_TRUE(near(ws.rectOf(2), {0, 0, 1380, 480}));
    EXPECT_TRUE(near(ws.rectOf(3), {0, 480, 1380, 580}));
    EXPECT_TRUE(near(ws.rectOf(4), {0, 1060, 1380, 380}));
}

TEST(LeftRows, DraggingTheTopLeftRowUpMovesItsBottomEdgeOnly) {
    Workspace ws = filled(2, 3);

    // The top Row has only a bottom edge, whichever half is grabbed.
    ws.dragResize(2, 0, -120, Corner::TopLeft);

    EXPECT_TRUE(near(ws.rectOf(2), leftRow(0, 600)));
    EXPECT_TRUE(near(ws.rectOf(3), leftRow(600, 840)));
}

TEST(LeftRows, ALoneLeftRowDragsSidewaysOnly) {
    Workspace ws = filled(2, 2);

    ws.dragResize(2, 100, 200, Corner::BottomRight);

    EXPECT_TRUE(near(ws.rectOf(2), {0, 0, 1380, 1440}));
}

TEST(LeftRows, AWindowJoiningLeftEndsTheDrag) {
    Workspace ws = filled(2, 2);
    ws.dragResize(2, 100, 0, Corner::BottomRight);

    ws.addWindow(3);

    EXPECT_FALSE(ws.resizing());
    EXPECT_FALSE(ws.dragResize(2, 100, 0, Corner::BottomRight));
}

// Left uses only as many Rows as fit min_row_height.

TEST(LeftRows, ALimitTooHighForTheMinimumRowHeightIsMadeToFit) {
    Workspace  ws{AREA};
    const auto warnings = ws.setSettings(Settings{.minRowHeight = 0.3, .leftRows = 5});

    EXPECT_EQ(ws.settings().leftRows, 3);
    ASSERT_EQ(warnings.size(), 1u);
    EXPECT_NE(warnings[0].find("left_rows"), std::string::npos);

    for (WindowId id = 1; id <= 5; ++id)
        ws.addWindow(id);
    EXPECT_TRUE(near(ws.rectOf(4), leftRow(960, 480)));
    EXPECT_TRUE(near(ws.rectOf(5), STACK_ZONE));
}

TEST(LeftRows, ALayoutOptsLimitTooHighNamesTheWorkspace) {
    Workspace  ws{AREA};
    const auto warnings = ws.setSettings(Settings{.minRowHeight = 0.25}, LayoutOpts{{"left_rows", "9"}}, "3");

    EXPECT_EQ(ws.settings().leftRows, 4);
    ASSERT_EQ(warnings.size(), 1u);
    EXPECT_NE(warnings[0].find("workspace 3"), std::string::npos);
}

TEST(LeftRows, ALimitThatFitsExactlyIsKept) {
    Workspace  ws{AREA};
    const auto warnings = ws.setSettings(Settings{.minRowHeight = 0.1, .leftRows = 10});

    EXPECT_EQ(ws.settings().leftRows, 10);
    EXPECT_TRUE(warnings.empty());
}

// Lowering the limit moves the extra bottom Left windows to the top of the
// Stack in Chain order; raising it moves nothing.

TEST(LeftRows, LoweringTheLimitMovesTheExtraBottomLeftWindowsToTheTopOfTheStack) {
    Workspace ws = filled(4, 6); // 2, 3, 4, 5 Left; 6 Stack

    ws.setSettings(Settings{.leftRows = 2});

    EXPECT_TRUE(near(ws.rectOf(2), leftRow(0, 720)));
    EXPECT_TRUE(near(ws.rectOf(3), leftRow(720, 720)));
    EXPECT_TRUE(near(ws.rectOf(4), stackRow(0, 480)));
    EXPECT_TRUE(near(ws.rectOf(5), stackRow(480, 480)));
    EXPECT_TRUE(near(ws.rectOf(6), stackRow(960, 480)));
    EXPECT_TRUE(near(ws.rectOf(1), MASTER_ZONE));
}

TEST(LeftRows, LoweringTheLimitKeepsTheChainOrder) {
    Workspace ws = filled(3, 5); // Chain: 2, 3, 4, 1, 5

    ws.setSettings(Settings{.leftRows = 1});

    // Chain now: 2, 1, 3, 4, 5. Closing 1 focuses the moved 3.
    EXPECT_EQ(ws.removeWindow(1), 3u);
    EXPECT_EQ(ws.removeWindow(4), 5u);
}

TEST(LeftRows, LoweringTheLimitThroughLayoutOptsMovesWindowsToo) {
    Workspace ws{AREA};
    ws.setSettings(Settings{}, LayoutOpts{{"left_rows", "3"}}, "3");
    for (WindowId id = 1; id <= 4; ++id)
        ws.addWindow(id);

    ws.setSettings(Settings{}, LayoutOpts{}, "3");

    EXPECT_TRUE(near(ws.rectOf(2), LEFT_ZONE));
    EXPECT_TRUE(near(ws.rectOf(3), stackRow(0, 720)));
    EXPECT_TRUE(near(ws.rectOf(4), stackRow(720, 720)));
}

TEST(LeftRows, RaisingTheMinimumRowHeightLowersTheLimitAndMovesWindows) {
    Workspace ws = filled(4, 5);

    ws.setSettings(Settings{.minRowHeight = 0.4, .leftRows = 4});

    EXPECT_TRUE(near(ws.rectOf(2), leftRow(0, 720)));
    EXPECT_TRUE(near(ws.rectOf(3), leftRow(720, 720)));
    EXPECT_TRUE(near(ws.rectOf(4), stackRow(0, 720)));
    EXPECT_TRUE(near(ws.rectOf(5), stackRow(720, 720)));
}

TEST(LeftRows, LoweringTheLimitEndsARowEdgeDrag) {
    Workspace ws = filled(2, 5); // 4 and 5 share the Stack
    ws.dragResize(4, 0, 50, Corner::None);

    ws.setSettings(Settings{.leftRows = 1});

    EXPECT_FALSE(ws.resizing());
}

TEST(LeftRows, LoweringTheLimitKeepsADividerOnlyDrag) {
    Workspace ws = filled(2, 4);
    ws.dragResize(1, 100, 0, Corner::BottomRight); // Master|Stack

    ws.setSettings(Settings{.leftRows = 1});

    EXPECT_TRUE(ws.resizing());
    EXPECT_TRUE(ws.dragResize(1, 100, 0, Corner::BottomRight));
    EXPECT_TRUE(near(ws.rectOf(1), {1280, 0, 2760, 1440}));
}

TEST(LeftRows, ALimitChangeThatMovesNothingKeepsADrag) {
    Workspace ws = filled(2, 3);
    ws.dragResize(2, 100, 0, Corner::None);

    ws.setSettings(Settings{.leftRows = 3});

    EXPECT_TRUE(ws.resizing());
}

TEST(LeftRows, RaisingTheLimitMovesNothing) {
    Workspace ws = filled(1, 4); // 2 Left; 3 and 4 Stack

    ws.setSettings(Settings{.leftRows = 3});

    EXPECT_TRUE(near(ws.rectOf(2), LEFT_ZONE));
    EXPECT_TRUE(near(ws.rectOf(3), stackRow(0, 720)));
    EXPECT_TRUE(near(ws.rectOf(4), stackRow(720, 720)));
}

TEST(LeftRows, RaisingTheLimitLetsTheNextWindowJoinLeft) {
    Workspace ws = filled(1, 3);
    ws.setSettings(Settings{.leftRows = 2});

    ws.addWindow(4);

    EXPECT_TRUE(near(ws.rectOf(2), leftRow(0, 720)));
    EXPECT_TRUE(near(ws.rectOf(4), leftRow(720, 720)));
    EXPECT_TRUE(near(ws.rectOf(3), STACK_ZONE));
}
