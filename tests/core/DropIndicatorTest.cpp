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

    // A workspace with Row limit `leftRows` and windows 1..count added in
    // Fill order: 1 Master, then Left up to the limit, then the Stack.
    Workspace filled(int leftRows, WindowId count) {
        Workspace ws{AREA};
        ws.setSettings(Settings{.leftRows = leftRows});
        for (WindowId id = 1; id <= count; ++id)
            ws.addWindow(id);
        return ws;
    }

    Rect rightRow(double y, double h) {
        return {3840, y, 1280, h};
    }

    // Heights come from fractions, so compare to well under a pixel.
    ::testing::AssertionResult near(const std::optional<Rect>& actual, const Rect& expected) {
        if (!actual)
            return ::testing::AssertionFailure() << "no rectangle";
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

TEST(DropIndicator, OnAWindowShowsThatWindowsPlace) {
    const Workspace ws = filled(1, 3); // 1 Master, 2 Left, 3 Stack

    EXPECT_TRUE(near(ws.dropIndicator(2, 2000, 700), MASTER_ZONE)); // Left window over Master
}

TEST(DropIndicator, OnTheAddAreaShowsTheNewBottomRow) {
    const Workspace ws = filled(1, 4); // 1 Master, 2 Left, 3 and 4 Stack

    // Master onto the bottom quarter of 4: the Stack gets a third Row.
    EXPECT_TRUE(near(ws.dropIndicator(1, 4000, 1400), rightRow(960, 480)));
}

TEST(DropIndicator, IntoAMasterHoleShowsTheMasterZone) {
    Workspace ws = filled(1, 3); // 1 Master, 2 Left, 3 Stack
    ws.removeWindow(1);           // Master is a Hole

    EXPECT_TRUE(near(ws.dropIndicator(3, 2000, 700), MASTER_ZONE));
}

TEST(DropIndicator, InAGapShowsTheNearestRow) {
    const Workspace ws = filled(1, 4); // 1 Master, 2 Left, 3 and 4 Stack

    // Just below the Stack's top Row: still the top Row, 3.
    EXPECT_TRUE(near(ws.dropIndicator(1, 4000, 719), rightRow(0, 720)));
}

TEST(DropIndicator, IsNothingWhenTheDropChangesNothing) {
    const Workspace ws = filled(1, 3); // 1 Master, 2 Left, 3 Stack

    EXPECT_EQ(ws.dropIndicator(1, 2000, 700), std::nullopt);  // on its own place
    EXPECT_EQ(ws.dropIndicator(3, 4000, 1400), std::nullopt); // add area of its own lone Row
    EXPECT_EQ(ws.dropIndicator(1, 6000, 700), std::nullopt);  // outside the workspace
    EXPECT_EQ(ws.dropIndicator(9, 2000, 700), std::nullopt);  // not on the workspace
}

TEST(DropIndicator, LeavesTheLayoutAlone) {
    Workspace ws = filled(1, 3); // 1 Master, 2 Left, 3 Stack

    ws.dropIndicator(2, 2000, 700);

    EXPECT_TRUE(near(ws.rectOf(1), MASTER_ZONE));
    EXPECT_TRUE(near(ws.rectOf(2), Rect{0, 0, 1280, 1440}));
}

TEST(DropIndicator, AgreesWithTheDropEverywhere) {
    // A full Left of two, three Stack Rows, uneven heights: every kind of target.
    Workspace start = filled(2, 6);
    start.layoutMessage("resize taller", 4);

    for (WindowId dragged = 1; dragged <= 6; ++dragged) {
        for (double x = 40; x < 5120; x += 160) {
            for (double y = 20; y < 1440; y += 60) {
                Workspace  dropped   = start;
                const auto indicator = start.dropIndicator(dragged, x, y);
                if (dropped.drop(dragged, x, y))
                    EXPECT_TRUE(near(indicator, *dropped.rectOf(dragged))) << "window " << dragged << " at " << x << ", " << y;
                else
                    EXPECT_EQ(indicator, std::nullopt) << "window " << dragged << " at " << x << ", " << y;
            }
        }
    }
}

TEST(DropIndicator, AgreesWithTheDropEverywhereWhenMirrored) {
    // A Left Stack of three, a full Right of two, uneven heights.
    Workspace start = mirrored(2, {2, 3, 4}, 1, {5, 6});
    start.layoutMessage("resize taller", 3);

    for (WindowId dragged = 1; dragged <= 6; ++dragged) {
        for (double x = 40; x < 5120; x += 160) {
            for (double y = 20; y < 1440; y += 60) {
                Workspace  dropped   = start;
                const auto indicator = start.dropIndicator(dragged, x, y);
                if (dropped.drop(dragged, x, y))
                    EXPECT_TRUE(near(indicator, *dropped.rectOf(dragged))) << "window " << dragged << " at " << x << ", " << y;
                else
                    EXPECT_EQ(indicator, std::nullopt) << "window " << dragged << " at " << x << ", " << y;
            }
        }
    }
}

TEST(DropIndicator, OnTheAddAreaOfAFullRightShowsItsBottomRow) {
    const Workspace ws = mirrored(1, {2, 3}, 4, {1});

    EXPECT_TRUE(near(ws.dropIndicator(2, 4000, 1400), rightRow(0, 1440)));
}
