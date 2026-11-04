#include "core/Workspace.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <optional>

using threecol::Corner;
using threecol::Rect;
using threecol::WindowId;
using threecol::Workspace;

namespace {

    // One 5120x1440 ultrawide, no gaps, default widths 0.25 / 0.5 / 0.25, so
    // the Stack is x 3840, 1280 px wide. A step of 0.05 is 72 px of height and
    // min_row_height 0.10 is 144 px.
    constexpr Rect AREA{0, 0, 5120, 1440};

    // A workspace with windows 1..count added in Fill order: 1 Master,
    // 2 Left, then the Stack.
    Workspace filled(WindowId count) {
        Workspace ws{AREA};
        for (WindowId id = 1; id <= count; ++id)
            ws.addWindow(id);
        return ws;
    }

    // A Stack Row from its top edge and height.
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

}

TEST(RowHeights, TallerGrowsTheFocusedRowAStepAndTheOthersGiveWayInProportion) {
    Workspace ws = filled(5); // 3, 4 and 5 share the Stack, 480 px each

    EXPECT_TRUE(ws.layoutMessage("resize taller", 4));

    EXPECT_TRUE(near(ws.rectOf(3), rightRow(0, 444)));
    EXPECT_TRUE(near(ws.rectOf(4), rightRow(444, 552)));
    EXPECT_TRUE(near(ws.rectOf(5), rightRow(996, 444)));
}

TEST(RowHeights, ShorterShrinksTheFocusedRowAStepAndTheOthersGrowInProportion) {
    Workspace ws = filled(5);

    EXPECT_TRUE(ws.layoutMessage("resize shorter", 4));

    EXPECT_TRUE(near(ws.rectOf(3), rightRow(0, 516)));
    EXPECT_TRUE(near(ws.rectOf(4), rightRow(516, 408)));
    EXPECT_TRUE(near(ws.rectOf(5), rightRow(924, 516)));
}

TEST(RowHeights, TallerStopsWhenTheOtherRowsAreAllAtTheMinimum) {
    Workspace ws = filled(5);
    for (int i = 0; i < 4; ++i)
        ws.layoutMessage("resize shorter", 5); // 5 at 192 px, 3 and 4 at 624

    // 5 reaches 144 px first and stays there while 4 keeps giving way.
    for (int i = 0; i < 20; ++i)
        ws.layoutMessage("resize taller", 3);
    EXPECT_FALSE(ws.layoutMessage("resize taller", 3));

    EXPECT_TRUE(near(ws.rectOf(3), rightRow(0, 1152)));
    EXPECT_TRUE(near(ws.rectOf(4), rightRow(1152, 144)));
    EXPECT_TRUE(near(ws.rectOf(5), rightRow(1296, 144)));
}

TEST(RowHeights, ShorterStopsAtTheMinimum) {
    Workspace ws = filled(5);

    for (int i = 0; i < 10; ++i)
        ws.layoutMessage("resize shorter", 4);
    EXPECT_FALSE(ws.layoutMessage("resize shorter", 4));

    EXPECT_TRUE(near(ws.rectOf(3), rightRow(0, 648)));
    EXPECT_TRUE(near(ws.rectOf(4), rightRow(648, 144)));
    EXPECT_TRUE(near(ws.rectOf(5), rightRow(792, 648)));
}

TEST(RowHeights, TallerAndShorterDoNothingOnMasterLeftOrALoneRow) {
    Workspace ws = filled(3); // 3 is the Stack's lone Row

    for (const WindowId id : {1, 2, 3}) {
        EXPECT_FALSE(ws.layoutMessage("resize taller", id));
        EXPECT_FALSE(ws.layoutMessage("resize shorter", id));
    }

    EXPECT_TRUE(near(ws.rectOf(3), rightRow(0, 1440)));
}

TEST(RowHeights, ANewRowGetsAnEqualShareAndTheOthersKeepTheirRatios) {
    Workspace ws = filled(4);
    ws.layoutMessage("resize taller", 3); // 792 / 648

    EXPECT_TRUE(near(std::optional<Rect>{ws.nextWindowRect()}, rightRow(960, 480)));
    ws.addWindow(5);

    EXPECT_TRUE(near(ws.rectOf(3), rightRow(0, 528)));
    EXPECT_TRUE(near(ws.rectOf(4), rightRow(528, 432)));
    EXPECT_TRUE(near(ws.rectOf(5), rightRow(960, 480)));
}

TEST(RowHeights, ClosingARowGrowsTheOthersInProportion) {
    Workspace ws = filled(5);
    ws.layoutMessage("resize taller", 3); // 552 / 444 / 444

    ws.removeWindow(5);

    // 552 : 444 over the whole 1440 px.
    EXPECT_TRUE(near(ws.rectOf(3), rightRow(0, 1440.0 * 552 / 996)));
    EXPECT_TRUE(near(ws.rectOf(4), rightRow(1440.0 * 552 / 996, 1440.0 * 444 / 996)));
}

TEST(RowHeights, ACrowdedStackSharesTheHeightEquallyAndTheProportionsComeBack) {
    Workspace          ws = filled(4);
    threecol::Settings settings;
    settings.minRowHeight = 0.3; // 432 px: three Rows fit, four don't
    ws.setSettings(settings);
    ws.layoutMessage("resize taller", 3); // 792 / 648
    ws.addWindow(5);                      // 528 / 432 / 480

    ws.addWindow(6); // the Stack always takes another window
    for (const WindowId id : {3, 4, 5, 6})
        EXPECT_NEAR(ws.rectOf(id)->h, 360, 1e-6);

    ws.removeWindow(6);
    EXPECT_TRUE(near(ws.rectOf(3), rightRow(0, 528)));
    EXPECT_TRUE(near(ws.rectOf(4), rightRow(528, 432)));
    EXPECT_TRUE(near(ws.rectOf(5), rightRow(960, 480)));
}

TEST(RowHeights, ACrowdedStackCantBeResized) {
    Workspace          ws = filled(6);
    threecol::Settings settings;
    settings.minRowHeight = 0.3;
    ws.setSettings(settings);

    EXPECT_FALSE(ws.layoutMessage("resize taller", 3));
    EXPECT_FALSE(ws.layoutMessage("resize shorter", 3));
    EXPECT_FALSE(ws.dragResize(4, 0, 100, Corner::TopLeft));
}

TEST(RowHeights, ResetMakesTheRowsEqualAgain) {
    Workspace ws = filled(5);
    ws.layoutMessage("resize taller", 3);

    EXPECT_TRUE(ws.layoutMessage("reset", 3));

    EXPECT_TRUE(near(ws.rectOf(3), rightRow(0, 480)));
    EXPECT_TRUE(near(ws.rectOf(4), rightRow(480, 480)));
    EXPECT_FALSE(ws.layoutMessage("reset", 3));
}

TEST(RowHeights, DraggingARowsBottomHalfMovesItsBottomEdgeOnly) {
    Workspace ws = filled(6); // 3, 4, 5 and 6 at 360 px each

    ws.dragResize(4, 0, 60, Corner::BottomRight);
    ws.dragResize(4, 0, 40, Corner::BottomRight);

    EXPECT_TRUE(near(ws.rectOf(3), rightRow(0, 360)));
    EXPECT_TRUE(near(ws.rectOf(4), rightRow(360, 460)));
    EXPECT_TRUE(near(ws.rectOf(5), rightRow(820, 260)));
    EXPECT_TRUE(near(ws.rectOf(6), rightRow(1080, 360)));
}

TEST(RowHeights, DraggingARowsTopHalfMovesItsTopEdgeOnly) {
    Workspace ws = filled(6);

    ws.dragResize(5, 0, -100, Corner::TopLeft);

    EXPECT_TRUE(near(ws.rectOf(3), rightRow(0, 360)));
    EXPECT_TRUE(near(ws.rectOf(4), rightRow(360, 260)));
    EXPECT_TRUE(near(ws.rectOf(5), rightRow(620, 460)));
    EXPECT_TRUE(near(ws.rectOf(6), rightRow(1080, 360)));
}

TEST(RowHeights, TheTopRowHasOnlyABottomEdge) {
    Workspace ws = filled(5);

    ws.dragResize(3, 0, -100, Corner::TopLeft);

    EXPECT_TRUE(near(ws.rectOf(3), rightRow(0, 380)));
    EXPECT_TRUE(near(ws.rectOf(4), rightRow(380, 580)));
    EXPECT_TRUE(near(ws.rectOf(5), rightRow(960, 480)));
}

TEST(RowHeights, TheBottomRowHasOnlyATopEdge) {
    Workspace ws = filled(5);

    ws.dragResize(5, 0, 100, Corner::BottomRight);

    EXPECT_TRUE(near(ws.rectOf(3), rightRow(0, 480)));
    EXPECT_TRUE(near(ws.rectOf(4), rightRow(480, 580)));
    EXPECT_TRUE(near(ws.rectOf(5), rightRow(1060, 380)));
}

TEST(RowHeights, ALoneRowResizesSidewaysOnly) {
    Workspace ws = filled(3);

    ws.dragResize(3, -256, 100, Corner::BottomLeft);

    EXPECT_TRUE(near(ws.rectOf(3), {3584, 0, 1536, 1440}));
}

TEST(RowHeights, OneDragChangesWidthAndHeightTogether) {
    Workspace ws = filled(4);

    EXPECT_TRUE(ws.dragResize(3, -256, 100, Corner::BottomLeft));

    EXPECT_TRUE(near(ws.rectOf(3), {3584, 0, 1536, 820}));
    EXPECT_TRUE(near(ws.rectOf(4), {3584, 820, 1536, 620}));
}

TEST(RowHeights, TheEdgeStopsAtTheMinimumAndFollowsAgainWhenTheCursorComesBack) {
    Workspace ws = filled(5);

    ws.dragResize(4, 0, 1000, Corner::BottomRight);
    EXPECT_TRUE(near(ws.rectOf(4), rightRow(480, 816)));
    EXPECT_TRUE(near(ws.rectOf(5), rightRow(1296, 144)));

    // 300 px past press: the cursor is back where the edge can follow.
    ws.dragResize(4, 0, -700, Corner::BottomRight);
    EXPECT_TRUE(near(ws.rectOf(4), rightRow(480, 780)));
    EXPECT_TRUE(near(ws.rectOf(5), rightRow(1260, 180)));
}

TEST(RowHeights, TheEdgeIsFixedAtPressForTheWholeDrag) {
    Workspace ws = filled(5);

    ws.dragResize(4, 0, 50, Corner::TopLeft);
    ws.dragResize(4, 0, 50, Corner::BottomLeft);

    EXPECT_TRUE(near(ws.rectOf(3), rightRow(0, 580)));
    EXPECT_TRUE(near(ws.rectOf(4), rightRow(580, 380)));
    EXPECT_TRUE(near(ws.rectOf(5), rightRow(960, 480)));
}

TEST(RowHeights, AWindowJoiningTheStackEndsAnEdgeDragKeepingItsSizes) {
    Workspace ws = filled(4);
    ws.dragResize(3, 0, 144, Corner::BottomRight); // 864 / 576

    ws.addWindow(5); // 576 / 384 / 480
    EXPECT_FALSE(ws.resizing());
    EXPECT_FALSE(ws.dragResize(3, 0, 100, Corner::BottomRight));

    EXPECT_TRUE(near(ws.rectOf(3), rightRow(0, 576)));
    EXPECT_TRUE(near(ws.rectOf(4), rightRow(576, 384)));
}

TEST(RowHeights, ARowClosingInTheGrabbedZoneEndsTheDrag) {
    Workspace ws = filled(5);
    ws.dragResize(3, 0, 100, Corner::BottomRight);

    ws.removeWindow(5);

    EXPECT_FALSE(ws.resizing());
}

TEST(RowHeights, AOneOffResizeMovesTheBottomEdgeOrTheBottomRowsTopEdge) {
    Workspace ws = filled(5);

    ws.resizeWindow(4, 0, 100);
    EXPECT_TRUE(near(ws.rectOf(4), rightRow(480, 580)));
    EXPECT_TRUE(near(ws.rectOf(5), rightRow(1060, 380)));

    ws.resizeWindow(5, 0, -100);
    EXPECT_TRUE(near(ws.rectOf(4), rightRow(480, 480)));
    EXPECT_TRUE(near(ws.rectOf(5), rightRow(960, 480)));
}
