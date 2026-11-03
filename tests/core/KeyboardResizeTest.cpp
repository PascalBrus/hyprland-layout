#include "core/Workspace.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <optional>

using threecol::Rect;
using threecol::WindowId;
using threecol::Workspace;

namespace {

    // One 5120x1440 ultrawide, no gaps, default widths 0.25 / 0.5 / 0.25 and
    // a step of 0.05 (256 px).
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

TEST(KeyboardResize, WideningLeftMovesOnlyItsDividerAFullStep) {
    Workspace ws = filled(3);

    ws.layoutMessage("resize wider", 2);

    EXPECT_TRUE(near(ws.rectOf(2), {0, 0, 1536, 1440}));
    EXPECT_TRUE(near(ws.rectOf(1), {1536, 0, 2304, 1440}));
    EXPECT_TRUE(near(ws.rectOf(3), {3840, 0, 1280, 1440}));
}

TEST(KeyboardResize, WideningTheStackFromAnyRowMovesOnlyItsDivider) {
    Workspace ws = filled(4); // 3 and 4 share the Stack

    ws.layoutMessage("resize wider", 4);

    EXPECT_TRUE(near(ws.rectOf(2), {0, 0, 1280, 1440}));
    EXPECT_TRUE(near(ws.rectOf(1), {1280, 0, 2304, 1440}));
    EXPECT_TRUE(near(ws.rectOf(3), {3584, 0, 1536, 720}));
    EXPECT_TRUE(near(ws.rectOf(4), {3584, 720, 1536, 720}));
}

TEST(KeyboardResize, NarrowingLeftGivesTheStepToMaster) {
    Workspace ws = filled(3);

    ws.layoutMessage("resize narrower", 2);

    EXPECT_TRUE(near(ws.rectOf(2), {0, 0, 1024, 1440}));
    EXPECT_TRUE(near(ws.rectOf(1), {1024, 0, 2816, 1440}));
    EXPECT_TRUE(near(ws.rectOf(3), {3840, 0, 1280, 1440}));
}

TEST(KeyboardResize, WideningMasterMovesBothDividersHalfAStep) {
    Workspace ws = filled(3);

    ws.layoutMessage("resize wider", 1);

    EXPECT_TRUE(near(ws.rectOf(2), {0, 0, 1152, 1440}));
    EXPECT_TRUE(near(ws.rectOf(1), {1152, 0, 2816, 1440}));
    EXPECT_TRUE(near(ws.rectOf(3), {3968, 0, 1152, 1440}));
}

TEST(KeyboardResize, NarrowingMasterMovesBothDividersHalfAStep) {
    Workspace ws = filled(3);

    ws.layoutMessage("resize narrower", 1);

    EXPECT_TRUE(near(ws.rectOf(2), {0, 0, 1408, 1440}));
    EXPECT_TRUE(near(ws.rectOf(1), {1408, 0, 2304, 1440}));
    EXPECT_TRUE(near(ws.rectOf(3), {3712, 0, 1408, 1440}));
}

TEST(KeyboardResize, WideningLeftStopsWhereMasterReachesItsMinimum) {
    Workspace ws = filled(3);
    ws.setSettings({.step = 0.2});

    ws.layoutMessage("resize wider", 2); // Master 0.5 -> 0.3
    ws.layoutMessage("resize wider", 2); // would leave Master 0.1; stops at 0.2

    EXPECT_TRUE(near(ws.rectOf(2), {0, 0, 2816, 1440}));
    EXPECT_TRUE(near(ws.rectOf(1), {2816, 0, 1024, 1440}));
    EXPECT_TRUE(near(ws.rectOf(3), {3840, 0, 1280, 1440}));
}

TEST(KeyboardResize, NarrowingTheStackStopsAtTheMinimumWidth) {
    Workspace ws = filled(3);
    ws.setSettings({.step = 0.1});

    ws.layoutMessage("resize narrower", 3); // 0.25 -> 0.15
    ws.layoutMessage("resize narrower", 3); // would be 0.05; stops at 0.10

    EXPECT_TRUE(near(ws.rectOf(1), {1280, 0, 3328, 1440}));
    EXPECT_TRUE(near(ws.rectOf(3), {4608, 0, 512, 1440}));
}

TEST(KeyboardResize, WideningMasterWithLeftAtItsLimitMovesOnlyTheStackSide) {
    Workspace ws = filled(3);
    ws.setSettings({.leftWidth = 0.1});

    ws.layoutMessage("resize wider", 1);

    EXPECT_TRUE(near(ws.rectOf(2), {0, 0, 512, 1440}));
    EXPECT_TRUE(near(ws.rectOf(1), {512, 0, 3456, 1440}));
    EXPECT_TRUE(near(ws.rectOf(3), {3968, 0, 1152, 1440}));
}

TEST(KeyboardResize, NarrowingMasterStopsAtItsMinimumAndStaysCentred) {
    Workspace ws = filled(3);
    ws.setSettings({.step = 0.2});

    ws.layoutMessage("resize narrower", 1); // 0.5 -> 0.3
    ws.layoutMessage("resize narrower", 1); // would be 0.1; stops at 0.2

    EXPECT_TRUE(near(ws.rectOf(2), {0, 0, 2048, 1440}));
    EXPECT_TRUE(near(ws.rectOf(1), {2048, 0, 1024, 1440}));
    EXPECT_TRUE(near(ws.rectOf(3), {3072, 0, 2048, 1440}));
}

TEST(KeyboardResize, ALoneMasterGrowsIntoTheEmptySideZones) {
    Workspace ws = filled(1);

    ws.layoutMessage("resize wider", 1);

    EXPECT_TRUE(near(ws.rectOf(1), {1152, 0, 2816, 1440}));
}

TEST(KeyboardResize, AMasterHoleResizesWhenLeftMovesTheSharedDivider) {
    Workspace ws = filled(3);
    ws.removeWindow(1);

    ws.layoutMessage("resize wider", 2);

    EXPECT_TRUE(near(ws.nextWindowRect(), {1536, 0, 2304, 1440}));
}

TEST(KeyboardResize, ResizedDividersWinOverChangedWidthSettings) {
    Workspace ws = filled(3);
    ws.layoutMessage("resize wider", 2);

    ws.setSettings({.leftWidth = 0.3, .stackWidth = 0.3});

    EXPECT_TRUE(near(ws.rectOf(2), {0, 0, 1536, 1440}));
    EXPECT_TRUE(near(ws.rectOf(3), {3840, 0, 1280, 1440}));
}

TEST(KeyboardResize, ARaisedLimitClampsResizedDividers) {
    Workspace ws = filled(3);
    ws.setSettings({.step = 0.15});
    ws.layoutMessage("resize narrower", 3); // Stack 0.25 -> 0.10

    ws.setSettings({.minWidth = 0.15});

    EXPECT_TRUE(near(ws.rectOf(1), {1280, 0, 3072, 1440}));
    EXPECT_TRUE(near(ws.rectOf(3), {4352, 0, 768, 1440}));
}

TEST(KeyboardResize, ResetPutsTheDividersBackToTheLayoutOpts) {
    Workspace ws = filled(3);
    ws.setSettings({}, {{"left_width", "0.3"}});
    ws.layoutMessage("resize wider", 1);
    ws.layoutMessage("resize wider", 3);

    ws.layoutMessage("reset", 1);

    EXPECT_TRUE(near(ws.rectOf(2), {0, 0, 1536, 1440}));
    EXPECT_TRUE(near(ws.rectOf(1), {1536, 0, 2304, 1440}));
    EXPECT_TRUE(near(ws.rectOf(3), {3840, 0, 1280, 1440}));
}

TEST(KeyboardResize, ResetWorksOnAnEmptyWorkspace) {
    Workspace ws = filled(1);
    ws.layoutMessage("resize wider", 1);
    ws.removeWindow(1);

    ws.layoutMessage("reset", std::nullopt);

    EXPECT_TRUE(near(ws.nextWindowRect(), {1280, 0, 2560, 1440}));
}

TEST(KeyboardResize, ResizedDividersCarryOverToANewInstanceOfTheWorkspace) {
    Workspace before = filled(3);
    before.layoutMessage("resize wider", 2);

    Workspace after = filled(3);
    after.setDividers(before.dividers());

    EXPECT_TRUE(near(after.rectOf(2), {0, 0, 1536, 1440}));
    EXPECT_TRUE(near(after.rectOf(1), {1536, 0, 2304, 1440}));
}

TEST(KeyboardResize, AWorkspaceThatWasNeverResizedHasNoDividersToKeep) {
    Workspace ws = filled(3);

    EXPECT_EQ(ws.dividers(), std::nullopt);
}

TEST(KeyboardResize, ResetLeavesNoDividersToKeep) {
    Workspace ws = filled(3);
    ws.layoutMessage("resize wider", 2);

    EXPECT_TRUE(ws.layoutMessage("reset", 2));

    EXPECT_EQ(ws.dividers(), std::nullopt);
}

TEST(KeyboardResize, AResizeReportsThatTheDividersChanged) {
    Workspace ws = filled(3);

    EXPECT_TRUE(ws.layoutMessage("resize narrower", 1));
}

TEST(KeyboardResize, AResizeAlreadyAtItsLimitChangesNothing) {
    Workspace ws = filled(3);
    ws.setSettings({.leftWidth = 0.1});

    EXPECT_FALSE(ws.layoutMessage("resize narrower", 2));
    EXPECT_EQ(ws.dividers(), std::nullopt);
}

TEST(KeyboardResize, UnknownOrInapplicableMessagesDoNothing) {
    Workspace ws = filled(3);

    EXPECT_FALSE(ws.layoutMessage("resize sideways", 2));
    EXPECT_FALSE(ws.layoutMessage("resize", 2));
    EXPECT_FALSE(ws.layoutMessage("", 2));
    EXPECT_FALSE(ws.layoutMessage("resize wider", std::nullopt)); // nothing focused
    EXPECT_FALSE(ws.layoutMessage("resize wider", 99));           // not on this workspace

    EXPECT_TRUE(near(ws.rectOf(2), {0, 0, 1280, 1440}));
    EXPECT_TRUE(near(ws.rectOf(1), {1280, 0, 2560, 1440}));
    EXPECT_TRUE(near(ws.rectOf(3), {3840, 0, 1280, 1440}));
}

TEST(KeyboardResize, AResizeStoppedAtMastersMinimumStaysPut) {
    Workspace ws = filled(3);
    ws.setSettings({.stackWidth = 0.3, .minMasterWidth = 0.3, .step = 0.3});
    ws.layoutMessage("resize wider", 2); // stops where Master is 0.3

    for (int press = 0; press < 5; ++press)
        EXPECT_FALSE(ws.layoutMessage("resize wider", 2));
    EXPECT_FALSE(ws.layoutMessage("resize wider", 3));
}
