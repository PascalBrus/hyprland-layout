#include "core/Workspace.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <optional>

using threecol::Corner;
using threecol::Dividers;
using threecol::Rect;
using threecol::WindowId;
using threecol::Workspace;

namespace {

    // One 5120x1440 ultrawide, no gaps, default widths 0.25 / 0.5 / 0.25
    // (1280 / 2560 / 1280 px), min_width 0.10 (512 px) and min_master_width
    // 0.20 (1024 px).
    constexpr Rect AREA{0, 0, 5120, 1440};

    // A workspace with windows 1..count added in Fill order: 1 Master,
    // 2 Left, then the Stack.
    Workspace filled(WindowId count) {
        Workspace ws{AREA};
        for (WindowId id = 1; id <= count; ++id)
            ws.addWindow(id);
        return ws;
    }

    // Widths come from fractions, so compare to well under a pixel.
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

TEST(MouseResize, DraggingALeftWindowMovesLeftMasterUnderTheCursor) {
    Workspace ws = filled(3);

    ws.dragResize(2, 100, 0, Corner::BottomRight);
    ws.dragResize(2, 156, 0, Corner::BottomRight);

    EXPECT_TRUE(near(ws.rectOf(2), {0, 0, 1536, 1440}));
    EXPECT_TRUE(near(ws.rectOf(1), {1536, 0, 2304, 1440}));
    EXPECT_TRUE(near(ws.rectOf(3), {3840, 0, 1280, 1440}));
}

TEST(MouseResize, DraggingAStackWindowMovesMasterRightUnderTheCursor) {
    Workspace ws = filled(4); // 3 and 4 share the Stack

    ws.dragResize(4, -256, 0, Corner::TopLeft);

    EXPECT_TRUE(near(ws.rectOf(2), {0, 0, 1280, 1440}));
    EXPECT_TRUE(near(ws.rectOf(1), {1280, 0, 2304, 1440}));
    EXPECT_TRUE(near(ws.rectOf(3), {3584, 0, 1536, 720}));
    EXPECT_TRUE(near(ws.rectOf(4), {3584, 720, 1536, 720}));
}

TEST(MouseResize, DraggingMastersLeftHalfMovesLeftMaster) {
    Workspace ws = filled(3);

    ws.dragResize(1, -256, 0, Corner::TopLeft);

    EXPECT_TRUE(near(ws.rectOf(2), {0, 0, 1024, 1440}));
    EXPECT_TRUE(near(ws.rectOf(1), {1024, 0, 2816, 1440}));
    EXPECT_TRUE(near(ws.rectOf(3), {3840, 0, 1280, 1440}));
}

TEST(MouseResize, DraggingMastersRightHalfMovesMasterRight) {
    Workspace ws = filled(3);

    ws.dragResize(1, 256, 0, Corner::BottomRight);

    EXPECT_TRUE(near(ws.rectOf(2), {0, 0, 1280, 1440}));
    EXPECT_TRUE(near(ws.rectOf(1), {1280, 0, 2816, 1440}));
    EXPECT_TRUE(near(ws.rectOf(3), {4096, 0, 1024, 1440}));
}

TEST(MouseResize, TheDividerStopsAtTheLimitAndFollowsAgainWhenTheCursorComesBack) {
    Workspace ws = filled(3);

    // Master can't go below 1024 px, so Left stops at 2816 px.
    ws.dragResize(2, 2000, 0, Corner::TopRight);
    EXPECT_TRUE(near(ws.rectOf(2), {0, 0, 2816, 1440}));

    // 1000 px past press: the cursor is back where the Divider can follow.
    ws.dragResize(2, -1000, 0, Corner::TopRight);
    EXPECT_TRUE(near(ws.rectOf(2), {0, 0, 2280, 1440}));

    // Left can't go below 512 px.
    ws.dragResize(2, -3000, 0, Corner::TopRight);
    EXPECT_TRUE(near(ws.rectOf(2), {0, 0, 512, 1440}));
}

TEST(MouseResize, TheStackStopsAtItsMinimumWidth) {
    Workspace ws = filled(3);

    ws.dragResize(3, 2000, 0, Corner::TopLeft);

    EXPECT_TRUE(near(ws.rectOf(3), {4608, 0, 512, 1440}));
    EXPECT_TRUE(near(ws.rectOf(1), {1280, 0, 3328, 1440}));
}

TEST(MouseResize, TheDividerIsFixedAtPressForTheWholeDrag) {
    Workspace ws = filled(3);

    // Grabbed on its left half, Master keeps moving Left|Master even if a
    // later tick names the other half.
    ws.dragResize(1, -128, 0, Corner::TopLeft);
    ws.dragResize(1, -128, 0, Corner::TopRight);

    EXPECT_TRUE(near(ws.rectOf(2), {0, 0, 1024, 1440}));
    EXPECT_TRUE(near(ws.rectOf(3), {3840, 0, 1280, 1440}));
}

TEST(MouseResize, ReleaseKeepsTheSizesAndTheNextDragChoosesAgain) {
    Workspace ws = filled(3);
    ws.dragResize(1, -256, 0, Corner::TopLeft);

    ws.endResize();
    EXPECT_FALSE(ws.resizing());
    EXPECT_EQ(ws.dividers().has_value(), true);

    ws.dragResize(1, 256, 0, Corner::TopRight);

    EXPECT_TRUE(near(ws.rectOf(2), {0, 0, 1024, 1440}));
    EXPECT_TRUE(near(ws.rectOf(1), {1024, 0, 3072, 1440}));
    EXPECT_TRUE(near(ws.rectOf(3), {4096, 0, 1024, 1440}));
}

TEST(MouseResize, ClosingTheGrabbedWindowEndsTheDragKeepingItsSizes) {
    Workspace ws = filled(4);
    ws.dragResize(4, -256, 0, Corner::TopLeft);

    ws.removeWindow(4);

    EXPECT_FALSE(ws.resizing());
    EXPECT_TRUE(near(ws.rectOf(3), {3584, 0, 1536, 1440}));
}

TEST(MouseResize, AWindowJoiningTheZoneEndsTheDragAndLaterTicksDoNothing) {
    Workspace ws = filled(3);
    ws.dragResize(3, -256, 0, Corner::TopLeft);

    ws.addWindow(4); // joins the Stack
    EXPECT_FALSE(ws.resizing());

    // Hyprland keeps sending the same drag's ticks until release.
    EXPECT_FALSE(ws.dragResize(3, -256, 0, Corner::TopLeft));
    EXPECT_TRUE(near(ws.rectOf(3), {3584, 0, 1536, 720}));

    // After release, a new drag works again.
    ws.endResize();
    ws.dragResize(3, 256, 0, Corner::TopLeft);
    EXPECT_TRUE(near(ws.rectOf(3), {3840, 0, 1280, 720}));
}

TEST(MouseResize, AWindowJoiningAnotherZoneLeavesTheDragGoing) {
    Workspace ws = filled(3);
    ws.removeWindow(1); // a Master Hole
    ws.dragResize(3, -128, 0, Corner::TopLeft);

    ws.addWindow(4); // fills the Hole
    ws.dragResize(3, -128, 0, Corner::TopLeft);

    EXPECT_TRUE(ws.resizing());
    EXPECT_TRUE(near(ws.rectOf(3), {3584, 0, 1536, 1440}));
}

TEST(MouseResize, AOneOffResizeMovesTheSameDividerByTheAmount) {
    Workspace ws = filled(3);

    ws.resizeWindow(2, 256);
    EXPECT_FALSE(ws.resizing());
    ws.resizeWindow(3, 256);

    EXPECT_TRUE(near(ws.rectOf(2), {0, 0, 1536, 1440}));
    EXPECT_TRUE(near(ws.rectOf(1), {1536, 0, 2560, 1440}));
    EXPECT_TRUE(near(ws.rectOf(3), {4096, 0, 1024, 1440}));
}

TEST(MouseResize, AOneOffResizeOnMasterWithNoCornerMovesItsRightDivider) {
    Workspace ws = filled(3);

    ws.resizeWindow(1, 256);

    EXPECT_TRUE(near(ws.rectOf(1), {1280, 0, 2816, 1440}));
    EXPECT_TRUE(near(ws.rectOf(3), {4096, 0, 1024, 1440}));
}

TEST(MouseResize, AOneOffResizeStopsAtTheLimit) {
    Workspace ws = filled(3);

    ws.resizeWindow(2, -1000);

    EXPECT_TRUE(near(ws.rectOf(2), {0, 0, 512, 1440}));
}

TEST(MouseResize, AOneOffResizeOnMastersLeftHalfMovesLeftMasterAndLeavesNoDrag) {
    Workspace ws = filled(3);

    // A trackpad gesture names the corner under the cursor but never releases.
    ws.resizeWindow(1, -128, 0, Corner::TopLeft);
    EXPECT_FALSE(ws.resizing());
    ws.resizeWindow(1, -128, 0, Corner::TopLeft);

    EXPECT_TRUE(near(ws.rectOf(2), {0, 0, 1024, 1440}));
    EXPECT_TRUE(near(ws.rectOf(3), {3840, 0, 1280, 1440}));
}

TEST(MouseResize, AKeyPressDuringADragEndsItAndIsntUndoneByLaterTicks) {
    Workspace ws = filled(3);
    ws.dragResize(2, 256, 0, Corner::TopRight);

    ws.layoutMessage("reset", 2);
    EXPECT_FALSE(ws.resizing());
    ws.dragResize(2, 256, 0, Corner::TopRight);

    EXPECT_TRUE(near(ws.rectOf(2), {0, 0, 1280, 1440}));
}
