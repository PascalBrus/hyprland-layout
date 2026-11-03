#include "core/Workspace.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <optional>

using threecol::Rect;
using threecol::Settings;
using threecol::WindowId;
using threecol::Workspace;

namespace {

    // One 5120x1440 ultrawide, no gaps, default widths 0.25 / 0.5 / 0.25.
    constexpr Rect AREA{0, 0, 5120, 1440};

    constexpr Rect MASTER_ZONE{1280, 0, 2560, 1440};
    constexpr Rect LEFT_ZONE{0, 0, 1280, 1440};
    constexpr Rect STACK_ZONE{3840, 0, 1280, 1440};

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

    // Where focus goes after `message` with `focused` focused.
    std::optional<WindowId> focusAfter(Workspace& ws, std::string_view message, std::optional<WindowId> focused) {
        return ws.layoutMessage(message, focused).focus;
    }

}

// Focus

TEST(ChainFocus, RightWalksFromLeftToMasterToTheStack) {
    Workspace ws = filled(1, 3); // 1 Master, 2 Left, 3 Stack

    EXPECT_EQ(focusAfter(ws, "focus r", 2), 1u);
    EXPECT_EQ(focusAfter(ws, "focus r", 1), 3u);
}

TEST(ChainFocus, StopsAtTheEndsInsteadOfWrapping) {
    Workspace ws = filled(1, 3); // 1 Master, 2 Left, 3 Stack

    EXPECT_EQ(focusAfter(ws, "focus l", 2), std::nullopt);
    EXPECT_EQ(focusAfter(ws, "focus r", 3), std::nullopt);
}

TEST(ChainFocus, SkipsAMasterHole) {
    Workspace ws = filled(1, 3);
    ws.removeWindow(1); // Master Hole

    EXPECT_EQ(focusAfter(ws, "focus r", 2), 3u);
    EXPECT_EQ(focusAfter(ws, "focus l", 3), 2u);
}

TEST(ChainFocus, WalksEveryLeftAndStackRow) {
    Workspace ws = filled(2, 5); // 1 Master, 2 3 Left, 4 5 Stack

    EXPECT_EQ(focusAfter(ws, "focus r", 2), 3u);
    EXPECT_EQ(focusAfter(ws, "focus r", 3), 1u);
    EXPECT_EQ(focusAfter(ws, "focus r", 4), 5u);
    EXPECT_EQ(focusAfter(ws, "focus l", 4), 1u);
    EXPECT_EQ(focusAfter(ws, "focus l", 1), 3u);
}

TEST(ChainFocus, UpAndDownStayInsideLeftOrTheStack) {
    Workspace ws = filled(2, 5); // 1 Master, 2 3 Left, 4 5 Stack

    EXPECT_EQ(focusAfter(ws, "focus d", 2), 3u);
    EXPECT_EQ(focusAfter(ws, "focus d", 3), std::nullopt);
    EXPECT_EQ(focusAfter(ws, "focus u", 3), 2u);
    EXPECT_EQ(focusAfter(ws, "focus u", 4), std::nullopt);
    EXPECT_EQ(focusAfter(ws, "focus d", 4), 5u);
    EXPECT_EQ(focusAfter(ws, "focus u", 5), 4u);
}

TEST(ChainFocus, UpAndDownDoNothingOnMaster) {
    Workspace ws = filled(1, 4);

    EXPECT_EQ(focusAfter(ws, "focus u", 1), std::nullopt);
    EXPECT_EQ(focusAfter(ws, "focus d", 1), std::nullopt);
}

TEST(ChainFocus, FocusMoveDoesNotChangeTheLayout) {
    Workspace ws = filled(1, 3);

    EXPECT_FALSE(ws.layoutMessage("focus r", 2));
}

TEST(FocusMaster, FocusesMasterFromAnyZoneOrWithNothingFocused) {
    Workspace ws = filled(1, 3);

    EXPECT_EQ(focusAfter(ws, "focusmaster", 2), 1u);
    EXPECT_EQ(focusAfter(ws, "focusmaster", 3), 1u);
    EXPECT_EQ(focusAfter(ws, "focusmaster", std::nullopt), 1u);
}

TEST(FocusMaster, DoesNothingOnAHole) {
    Workspace ws = filled(1, 3);
    ws.removeWindow(1);

    EXPECT_EQ(focusAfter(ws, "focusmaster", 2), std::nullopt);
    EXPECT_FALSE(ws.layoutMessage("focusmaster", 2));
}

// Move

TEST(ChainMove, DownSwapsWithTheNextRowTakingItsHeightAndKeepingFocus) {
    Workspace ws = filled(1, 4); // 1 Master, 2 Left, 3 4 Stack
    ws.layoutMessage("resize taller", 3); // 3 is 792 px, 4 is 648

    const auto result = ws.layoutMessage("move d", 3);

    EXPECT_TRUE(result.changed);
    EXPECT_EQ(result.focus, 3u);
    EXPECT_TRUE(near(ws.rectOf(4), stackRow(0, 792)));
    EXPECT_TRUE(near(ws.rectOf(3), stackRow(792, 648)));
}

TEST(ChainMove, UpAndDownStopAtTheZoneEdgesAndDoNothingOnMaster) {
    Workspace ws = filled(2, 5); // 1 Master, 2 3 Left, 4 5 Stack

    EXPECT_FALSE(ws.layoutMessage("move u", 2));
    EXPECT_FALSE(ws.layoutMessage("move d", 3));
    EXPECT_FALSE(ws.layoutMessage("move u", 4));
    EXPECT_FALSE(ws.layoutMessage("move d", 5));
    EXPECT_FALSE(ws.layoutMessage("move u", 1));
    EXPECT_FALSE(ws.layoutMessage("move d", 1));
}

TEST(ChainMove, UpSwapsLeftRows) {
    Workspace ws = filled(2, 3); // 1 Master, 2 3 Left

    EXPECT_TRUE(ws.layoutMessage("move u", 3));
    EXPECT_TRUE(near(ws.rectOf(3), leftRow(0, 720)));
    EXPECT_TRUE(near(ws.rectOf(2), leftRow(720, 720)));
}

TEST(ChainMove, RightFromAFullLeftSwapsWithAnOccupiedMaster) {
    Workspace ws = filled(1, 3); // 1 Master, 2 Left, 3 Stack

    EXPECT_TRUE(ws.layoutMessage("move r", 2));
    EXPECT_TRUE(near(ws.rectOf(2), MASTER_ZONE));
    EXPECT_TRUE(near(ws.rectOf(1), LEFT_ZONE));
}

TEST(ChainMove, RightFromMasterJoinsTheTopOfTheStackAndLeavesAHole) {
    Workspace ws = filled(1, 3); // 1 Master, 2 Left, 3 Stack

    EXPECT_TRUE(ws.layoutMessage("move r", 1));
    EXPECT_TRUE(near(ws.rectOf(1), stackRow(0, 720)));
    EXPECT_TRUE(near(ws.rectOf(3), stackRow(720, 720)));
    EXPECT_TRUE(near(ws.rectOf(2), LEFT_ZONE));
    // Nothing slid into the Hole: the next window fills it.
    EXPECT_TRUE(near(ws.nextWindowRect(), MASTER_ZONE));
}

TEST(ChainMove, LeftFromAStackTopIntoAMasterHoleBringsItBack) {
    Workspace ws = filled(1, 3);
    ws.layoutMessage("move r", 1); // 1 joins the Stack top, Master Hole

    EXPECT_TRUE(ws.layoutMessage("move l", 1));
    EXPECT_TRUE(near(ws.rectOf(1), MASTER_ZONE));
    EXPECT_TRUE(near(ws.rectOf(3), STACK_ZONE));
}

TEST(ChainMove, LeftFromTheStackTopSwapsWithAnOccupiedMaster) {
    Workspace ws = filled(1, 4); // 1 Master, 2 Left, 3 4 Stack

    EXPECT_TRUE(ws.layoutMessage("move l", 3));
    EXPECT_TRUE(near(ws.rectOf(3), MASTER_ZONE));
    EXPECT_TRUE(near(ws.rectOf(1), stackRow(0, 720)));
}

TEST(ChainMove, RightFromMasterIntoAnEmptyStackMovesThere) {
    Workspace ws = filled(1, 2); // 1 Master, 2 Left

    EXPECT_TRUE(ws.layoutMessage("move r", 1));
    EXPECT_TRUE(near(ws.rectOf(1), STACK_ZONE));
    EXPECT_TRUE(near(ws.nextWindowRect(), MASTER_ZONE));
}

TEST(ChainMove, RightFromLeftIntoAMasterHoleMovesThereLeavingALeftHole) {
    Workspace ws = filled(1, 3);
    ws.removeWindow(1);

    EXPECT_TRUE(ws.layoutMessage("move r", 2));
    EXPECT_TRUE(near(ws.rectOf(2), MASTER_ZONE));
    EXPECT_TRUE(near(ws.rectOf(3), STACK_ZONE));
    EXPECT_TRUE(near(ws.nextWindowRect(), LEFT_ZONE));
}

TEST(ChainMove, RightFromTheBottomLeftRowIntoAMasterHoleClosesTheGap) {
    Workspace ws = filled(2, 3); // 1 Master, 2 3 Left
    ws.removeWindow(1);

    EXPECT_TRUE(ws.layoutMessage("move r", 3));
    EXPECT_TRUE(near(ws.rectOf(3), MASTER_ZONE));
    EXPECT_TRUE(near(ws.rectOf(2), LEFT_ZONE));
}

TEST(ChainMove, LeftFromMasterJoinsTheBottomOfALeftBelowItsRowLimit) {
    Workspace ws = filled(2, 2); // 1 Master, 2 Left; room for one more Left Row

    EXPECT_TRUE(ws.layoutMessage("move l", 1));
    EXPECT_TRUE(near(ws.rectOf(2), leftRow(0, 720)));
    EXPECT_TRUE(near(ws.rectOf(1), leftRow(720, 720)));
    EXPECT_TRUE(near(ws.nextWindowRect(), MASTER_ZONE));
}

TEST(ChainMove, LeftFromMasterIntoALeftHoleMovesThere) {
    Workspace ws = filled(1, 3);
    ws.removeWindow(2); // Left Hole

    EXPECT_TRUE(ws.layoutMessage("move l", 1));
    EXPECT_TRUE(near(ws.rectOf(1), LEFT_ZONE));
    EXPECT_TRUE(near(ws.nextWindowRect(), MASTER_ZONE));
}

TEST(ChainMove, LeftFromMasterSwapsWithTheBottomRowOfAFullLeft) {
    Workspace ws = filled(2, 3); // 1 Master, 2 3 Left
    ws.layoutMessage("resize taller", 3); // 2 is 648 px, 3 is 792

    EXPECT_TRUE(ws.layoutMessage("move l", 1));
    EXPECT_TRUE(near(ws.rectOf(3), MASTER_ZONE));
    EXPECT_TRUE(near(ws.rectOf(1), leftRow(648, 792)));
    EXPECT_TRUE(near(ws.rectOf(2), leftRow(0, 648)));
}

TEST(ChainMove, RightAndLeftSwapNeighbouringRowsInsideAZone) {
    Workspace ws = filled(2, 5); // 1 Master, 2 3 Left, 4 5 Stack

    EXPECT_TRUE(ws.layoutMessage("move r", 2));
    EXPECT_TRUE(near(ws.rectOf(2), leftRow(720, 720)));
    EXPECT_TRUE(near(ws.rectOf(3), leftRow(0, 720)));

    EXPECT_TRUE(ws.layoutMessage("move l", 5));
    EXPECT_TRUE(near(ws.rectOf(5), stackRow(0, 720)));
    EXPECT_TRUE(near(ws.rectOf(4), stackRow(720, 720)));
}

TEST(ChainMove, StopsAtTheChainEnds) {
    Workspace ws = filled(1, 3); // 1 Master, 2 Left, 3 Stack

    EXPECT_FALSE(ws.layoutMessage("move l", 2));
    EXPECT_FALSE(ws.layoutMessage("move r", 3));
    EXPECT_TRUE(near(ws.rectOf(2), LEFT_ZONE));
    EXPECT_TRUE(near(ws.rectOf(3), STACK_ZONE));
}

TEST(ChainMove, AWindowJoiningTheStackTopGetsANewRow) {
    Workspace ws = filled(1, 4); // 1 Master, 2 Left, 3 4 Stack

    ws.layoutMessage("move r", 1);

    EXPECT_TRUE(near(ws.rectOf(1), stackRow(0, 480)));
    EXPECT_TRUE(near(ws.rectOf(3), stackRow(480, 480)));
    EXPECT_TRUE(near(ws.rectOf(4), stackRow(960, 480)));
}

TEST(ChainMove, MoveCallsRunTheSameRulesAsTheMessage) {
    Workspace ws = filled(1, 3);

    EXPECT_TRUE(ws.move(1, threecol::Direction::Right));
    EXPECT_TRUE(near(ws.rectOf(1), stackRow(0, 720)));
    EXPECT_FALSE(ws.move(3, threecol::Direction::Right));
}

TEST(ChainMove, AMoveEndsAMouseDrag) {
    Workspace ws = filled(1, 4);
    ws.dragResize(3, 10, 0, threecol::Corner::BottomLeft);

    ws.move(3, threecol::Direction::Down);

    EXPECT_FALSE(ws.resizing());
}

TEST(ChainMove, NothingMovesWithNoFocusOrAWindowNotHere) {
    Workspace ws = filled(1, 3);

    EXPECT_FALSE(ws.layoutMessage("move r", std::nullopt));
    EXPECT_FALSE(ws.layoutMessage("move r", 99));
    EXPECT_FALSE(ws.move(99, threecol::Direction::Left));
}

// swapwithmaster

TEST(SwapWithMaster, OldMasterTakesTheMovedWindowsRowAndFocusStays) {
    Workspace ws = filled(1, 4); // 1 Master, 2 Left, 3 4 Stack
    ws.layoutMessage("resize taller", 4); // 3 is 648 px, 4 is 792

    const auto result = ws.layoutMessage("swapwithmaster", 4);

    EXPECT_TRUE(result.changed);
    EXPECT_EQ(result.focus, 4u);
    EXPECT_TRUE(near(ws.rectOf(4), MASTER_ZONE));
    EXPECT_TRUE(near(ws.rectOf(1), stackRow(648, 792)));
}

TEST(SwapWithMaster, OnAHoleTheWindowMovesIn) {
    Workspace ws = filled(1, 4);
    ws.removeWindow(1);

    EXPECT_TRUE(ws.layoutMessage("swapwithmaster", 3));
    EXPECT_TRUE(near(ws.rectOf(3), MASTER_ZONE));
    EXPECT_TRUE(near(ws.rectOf(4), STACK_ZONE));
}

TEST(SwapWithMaster, DoesNothingOnMasterOrWithNoFocus) {
    Workspace ws = filled(1, 3);

    EXPECT_FALSE(ws.layoutMessage("swapwithmaster", 1));
    EXPECT_FALSE(ws.layoutMessage("swapwithmaster", std::nullopt));
    EXPECT_TRUE(near(ws.rectOf(1), MASTER_ZONE));
}

// Native swaps

TEST(SwapWindows, TwoWindowsTradePlacesEachTakingTheOthersRow) {
    Workspace ws = filled(1, 4); // 1 Master, 2 Left, 3 4 Stack
    ws.layoutMessage("resize taller", 4); // 3 is 648 px, 4 is 792

    EXPECT_TRUE(ws.swapWindows(2, 4));
    EXPECT_TRUE(near(ws.rectOf(4), LEFT_ZONE));
    EXPECT_TRUE(near(ws.rectOf(2), stackRow(648, 792)));
}

TEST(SwapWindows, DoesNothingUnlessBothAreHereAndDifferent) {
    Workspace ws = filled(1, 3);

    EXPECT_FALSE(ws.swapWindows(1, 99));
    EXPECT_FALSE(ws.swapWindows(1, 1));
    EXPECT_TRUE(near(ws.rectOf(1), MASTER_ZONE));
}
