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
using threecol::Wrap;

namespace {

    // Every dispatcher stops at the Chain's ends and a zone's edges.
    constexpr Wrap NO_LOOPING{.focus = false, .swap = false, .cycleNext = false, .swapNext = false};

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

    // Where focus goes after `message` with `focused` focused.
    std::optional<WindowId> focusAfter(Workspace& ws, std::string_view message, std::optional<WindowId> focused) {
        return ws.layoutMessage(message, focused).focus;
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

// Focus

TEST(ChainFocus, RightWalksFromLeftToMasterToTheStack) {
    Workspace ws = filled(1, 3); // 1 Master, 2 Left, 3 Stack

    EXPECT_EQ(focusAfter(ws, "focus r", 2), 1u);
    EXPECT_EQ(focusAfter(ws, "focus r", 1), 3u);
}

TEST(ChainFocus, LoopsFromOneEndOfTheChainToTheOtherByDefault) {
    Workspace ws = filled(1, 3); // 1 Master, 2 Left, 3 Stack

    EXPECT_EQ(focusAfter(ws, "focus l", 2), 3u);
    EXPECT_EQ(focusAfter(ws, "focus r", 3), 2u);
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
    EXPECT_EQ(focusAfter(ws, "focus u", 3), 2u);
    EXPECT_EQ(focusAfter(ws, "focus d", 4), 5u);
    EXPECT_EQ(focusAfter(ws, "focus u", 5), 4u);
}

TEST(ChainFocus, UpAndDownLoopInsideTheSideZoneByDefault) {
    Workspace ws = filled(2, 6); // 1 Master, 2 3 Left, 4 5 6 Stack

    EXPECT_EQ(focusAfter(ws, "focus d", 3), 2u);
    EXPECT_EQ(focusAfter(ws, "focus u", 2), 3u);
    EXPECT_EQ(focusAfter(ws, "focus d", 6), 4u);
    EXPECT_EQ(focusAfter(ws, "focus u", 4), 6u);
}

TEST(ChainFocus, NoloopStopsAtTheChainsEndsAndTheZonesEdges) {
    Workspace ws = filled(2, 5); // 1 Master, 2 3 Left, 4 5 Stack

    EXPECT_EQ(focusAfter(ws, "focus l noloop", 2), std::nullopt);
    EXPECT_EQ(focusAfter(ws, "focus r noloop", 5), std::nullopt);
    EXPECT_EQ(focusAfter(ws, "focus u noloop", 2), std::nullopt);
    EXPECT_EQ(focusAfter(ws, "focus d noloop", 5), std::nullopt);
    // Short of an end it steps as usual.
    EXPECT_EQ(focusAfter(ws, "focus r noloop", 3), 1u);
    EXPECT_EQ(focusAfter(ws, "focus d noloop", 4), 5u);
}

// inDirection is what the focus and swap hooks ask, with wrap.focus or
// wrap.swap.

TEST(ChainFocus, InDirectionLoopsAtTheChainsEndsOnlyWithWrap) {
    const Workspace ws = filled(1, 3); // Chain 2, 1, 3

    EXPECT_EQ(ws.inDirection(3, Direction::Right, /*wrap=*/true), 2u);
    EXPECT_EQ(ws.inDirection(2, Direction::Left, true), 3u);
    EXPECT_EQ(ws.inDirection(3, Direction::Right, /*wrap=*/false), std::nullopt);
    EXPECT_EQ(ws.inDirection(2, Direction::Left, false), std::nullopt);
}

TEST(ChainFocus, InDirectionLoopsAtAZonesTopAndBottomRowOnlyWithWrap) {
    const Workspace ws = filled(2, 6); // 1 Master, 2 3 Left, 4 5 6 Stack

    EXPECT_EQ(ws.inDirection(6, Direction::Down, /*wrap=*/true), 4u);
    EXPECT_EQ(ws.inDirection(4, Direction::Up, true), 6u);
    EXPECT_EQ(ws.inDirection(3, Direction::Down, true), 2u);
    EXPECT_EQ(ws.inDirection(6, Direction::Down, /*wrap=*/false), std::nullopt);
    EXPECT_EQ(ws.inDirection(4, Direction::Up, false), std::nullopt);
    EXPECT_EQ(ws.inDirection(2, Direction::Up, false), std::nullopt);
}

TEST(ChainFocus, InDirectionUpOrDownOnMasterOrALoneRowIsNothingEvenWithWrap) {
    const Workspace ws = filled(1, 3); // 1 Master, 2 Left, 3 Stack

    for (const Direction d : {Direction::Up, Direction::Down}) {
        EXPECT_EQ(ws.inDirection(1, d, true), std::nullopt);
        EXPECT_EQ(ws.inDirection(2, d, true), std::nullopt);
        EXPECT_EQ(ws.inDirection(3, d, true), std::nullopt);
    }
}

TEST(ChainFocus, InDirectionLoopsPastAMasterHoleAndAloneIsNothing) {
    Workspace ws = filled(1, 3);
    ws.removeWindow(1); // Master Hole: Chain 2, 3

    EXPECT_EQ(ws.inDirection(3, Direction::Right, true), 2u);
    EXPECT_EQ(ws.inDirection(2, Direction::Left, true), 3u);

    Workspace lone = filled(1, 1);
    EXPECT_EQ(lone.inDirection(1, Direction::Right, true), std::nullopt);
    EXPECT_EQ(lone.inDirection(1, Direction::Left, true), std::nullopt);
}

TEST(ChainFocus, WrapFocusOffStopsTheMessageAtTheEnds) {
    Workspace ws = filled(2, 5); // 1 Master, 2 3 Left, 4 5 Stack

    EXPECT_EQ(ws.layoutMessage("focus l", 2, NO_LOOPING).focus, std::nullopt);
    EXPECT_EQ(ws.layoutMessage("focus r", 5, NO_LOOPING).focus, std::nullopt);
    EXPECT_EQ(ws.layoutMessage("focus d", 5, NO_LOOPING).focus, std::nullopt);
    EXPECT_EQ(ws.layoutMessage("focus u", 4, NO_LOOPING).focus, std::nullopt);
}

TEST(ChainFocus, OtherWrapSwitchesLeaveFocusLooping) {
    Workspace ws = filled(1, 3);

    EXPECT_EQ(ws.layoutMessage("focus r", 3, Wrap{.swap = false, .cycleNext = false, .swapNext = false}).focus, 2u);
}

TEST(ChainFocus, FocusMessageIsADirectionThenOptionallyNoloop) {
    using threecol::parseFocusMessage;

    EXPECT_EQ(parseFocusMessage("focus l"), (threecol::FocusMessage{Direction::Left, false}));
    EXPECT_EQ(parseFocusMessage("focus d noloop"), (threecol::FocusMessage{Direction::Down, true}));
    for (const auto* other : {"focus", "focus x", "focus lr", "focus l x", "focus  l", "focus l noloop x", "move l", "focusmaster"})
        EXPECT_EQ(parseFocusMessage(other), std::nullopt) << other;
}

TEST(ChainFocus, AFocusMessageWithOtherWordsDoesNothing) {
    Workspace ws = filled(1, 3);

    EXPECT_EQ(focusAfter(ws, "focus r x", 2), std::nullopt);
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

TEST(FocusMaster, FocusesMasterFromAnyZoneOrAFloatingWindow) {
    Workspace ws = filled(1, 3);

    EXPECT_EQ(focusAfter(ws, "focusmaster", 2), 1u);
    EXPECT_EQ(focusAfter(ws, "focusmaster", 3), 1u);
    EXPECT_EQ(focusAfter(ws, "focusmaster", std::nullopt), 1u);
}

// The plugin drops a message when a floating window has focus, unless it
// needs no focused tile.
TEST(FocusMaster, NeedsNoFocusedTileSoItWorksFromAFloatingWindow) {
    EXPECT_FALSE(threecol::needsFocusedTile("focusmaster"));
    EXPECT_FALSE(threecol::needsFocusedTile("reset"));
    EXPECT_TRUE(threecol::needsFocusedTile("focus l"));
    EXPECT_TRUE(threecol::needsFocusedTile("move r"));
    EXPECT_TRUE(threecol::needsFocusedTile("swapwithmaster"));
    EXPECT_TRUE(threecol::needsFocusedTile("resize wider"));
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
    EXPECT_TRUE(near(ws.rectOf(4), rightRow(0, 792)));
    EXPECT_TRUE(near(ws.rectOf(3), rightRow(792, 648)));
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
    EXPECT_TRUE(near(ws.rectOf(1), rightRow(0, 720)));
    EXPECT_TRUE(near(ws.rectOf(3), rightRow(720, 720)));
    EXPECT_TRUE(near(ws.rectOf(2), LEFT_ZONE));
    // Nothing slid into the Hole: the next window fills it.
    EXPECT_TRUE(near(ws.nextWindowRect(), MASTER_ZONE));
}

TEST(ChainMove, LeftFromAStackTopIntoAMasterHoleBringsItBack) {
    Workspace ws = filled(1, 3);
    ws.layoutMessage("move r", 1); // 1 joins the Stack top, Master Hole

    EXPECT_TRUE(ws.layoutMessage("move l", 1));
    EXPECT_TRUE(near(ws.rectOf(1), MASTER_ZONE));
    EXPECT_TRUE(near(ws.rectOf(3), RIGHT_ZONE));
}

TEST(ChainMove, LeftFromTheStackTopSwapsWithAnOccupiedMaster) {
    Workspace ws = filled(1, 4); // 1 Master, 2 Left, 3 4 Stack

    EXPECT_TRUE(ws.layoutMessage("move l", 3));
    EXPECT_TRUE(near(ws.rectOf(3), MASTER_ZONE));
    EXPECT_TRUE(near(ws.rectOf(1), rightRow(0, 720)));
}

TEST(ChainMove, RightFromMasterIntoAnEmptyStackMovesThere) {
    Workspace ws = filled(1, 2); // 1 Master, 2 Left

    EXPECT_TRUE(ws.layoutMessage("move r", 1));
    EXPECT_TRUE(near(ws.rectOf(1), RIGHT_ZONE));
    EXPECT_TRUE(near(ws.nextWindowRect(), MASTER_ZONE));
}

TEST(ChainMove, RightFromLeftIntoAMasterHoleMovesThereLeavingALeftHole) {
    Workspace ws = filled(1, 3);
    ws.removeWindow(1);

    EXPECT_TRUE(ws.layoutMessage("move r", 2));
    EXPECT_TRUE(near(ws.rectOf(2), MASTER_ZONE));
    EXPECT_TRUE(near(ws.rectOf(3), RIGHT_ZONE));
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
    EXPECT_TRUE(near(ws.rectOf(5), rightRow(0, 720)));
    EXPECT_TRUE(near(ws.rectOf(4), rightRow(720, 720)));
}

TEST(ChainMove, StopsAtTheChainEnds) {
    Workspace ws = filled(1, 3); // 1 Master, 2 Left, 3 Stack

    EXPECT_FALSE(ws.layoutMessage("move l", 2));
    EXPECT_FALSE(ws.layoutMessage("move r", 3));
    EXPECT_TRUE(near(ws.rectOf(2), LEFT_ZONE));
    EXPECT_TRUE(near(ws.rectOf(3), RIGHT_ZONE));
}

TEST(ChainMove, AWindowJoiningTheStackTopGetsANewRow) {
    Workspace ws = filled(1, 4); // 1 Master, 2 Left, 3 4 Stack

    ws.layoutMessage("move r", 1);

    EXPECT_TRUE(near(ws.rectOf(1), rightRow(0, 480)));
    EXPECT_TRUE(near(ws.rectOf(3), rightRow(480, 480)));
    EXPECT_TRUE(near(ws.rectOf(4), rightRow(960, 480)));
}

TEST(ChainMove, MoveCallsRunTheSameRulesAsTheMessage) {
    Workspace ws = filled(1, 3);

    EXPECT_TRUE(ws.move(1, threecol::Direction::Right));
    EXPECT_TRUE(near(ws.rectOf(1), rightRow(0, 720)));
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
    EXPECT_TRUE(near(ws.rectOf(1), rightRow(648, 792)));
}

TEST(SwapWithMaster, OnAHoleTheWindowMovesIn) {
    Workspace ws = filled(1, 4);
    ws.removeWindow(1);

    EXPECT_TRUE(ws.layoutMessage("swapwithmaster", 3));
    EXPECT_TRUE(near(ws.rectOf(3), MASTER_ZONE));
    EXPECT_TRUE(near(ws.rectOf(4), RIGHT_ZONE));
}

TEST(SwapWithMaster, DoesNothingOnMasterWithIgnoremasterOrWithNoFocus) {
    Workspace ws = filled(1, 3);

    EXPECT_FALSE(ws.layoutMessage("swapwithmaster auto ignoremaster", 1));
    EXPECT_FALSE(ws.layoutMessage("swapwithmaster", std::nullopt));
    EXPECT_TRUE(near(ws.rectOf(1), MASTER_ZONE));
}

// Native swaps

TEST(SwapWindows, TwoWindowsTradePlacesEachTakingTheOthersRow) {
    Workspace ws = filled(1, 4); // 1 Master, 2 Left, 3 4 Stack
    ws.layoutMessage("resize taller", 4); // 3 is 648 px, 4 is 792

    EXPECT_TRUE(ws.swapWindows(2, 4));
    EXPECT_TRUE(near(ws.rectOf(4), LEFT_ZONE));
    EXPECT_TRUE(near(ws.rectOf(2), rightRow(648, 792)));
}

TEST(SwapWindows, DoesNothingUnlessBothAreHereAndDifferent) {
    Workspace ws = filled(1, 3);

    EXPECT_FALSE(ws.swapWindows(1, 99));
    EXPECT_FALSE(ws.swapWindows(1, 1));
    EXPECT_TRUE(near(ws.rectOf(1), MASTER_ZONE));
}

// Mirrored: the Chain still runs left to right on screen, and moves off
// Master join or swap at each side's end nearest Master.

TEST(ChainFocusMirrored, WalksTheLeftStackTopToBottomThenMasterThenRight) {
    Workspace ws = mirrored(1, {2, 3}, 4, {1});

    EXPECT_EQ(focusAfter(ws, "focus r", 2), 3u);
    EXPECT_EQ(focusAfter(ws, "focus r", 3), 4u);
    EXPECT_EQ(focusAfter(ws, "focus r", 4), 1u);
    EXPECT_EQ(focusAfter(ws, "focus r", 1), 2u); // loops
    EXPECT_EQ(focusAfter(ws, "focus l", 4), 3u);
}

TEST(ChainMoveMirrored, LeftFromMasterJoinsTheBottomOfTheLeftStackAndLeavesAHole) {
    Workspace ws = mirrored(1, {2, 3}, 4, {1});

    EXPECT_TRUE(ws.layoutMessage("move l", 4));

    EXPECT_TRUE(near(ws.rectOf(2), leftRow(0, 480)));
    EXPECT_TRUE(near(ws.rectOf(3), leftRow(480, 480)));
    EXPECT_TRUE(near(ws.rectOf(4), leftRow(960, 480)));
    EXPECT_EQ(ws.nextWindowRect(), MASTER_ZONE);
}

TEST(ChainMoveMirrored, RightFromMasterJoinsTheTopOfARightBelowItsRowLimit) {
    Workspace ws = mirrored(2, {2}, 4, {1});

    EXPECT_TRUE(ws.layoutMessage("move r", 4));

    EXPECT_TRUE(near(ws.rectOf(4), rightRow(0, 720)));
    EXPECT_TRUE(near(ws.rectOf(1), rightRow(720, 720)));
    EXPECT_EQ(ws.nextWindowRect(), MASTER_ZONE);
}

TEST(ChainMoveMirrored, RightFromMasterSwapsWithTheTopRowOfAFullRight) {
    Workspace ws = mirrored(2, {2}, 4, {5, 1});

    EXPECT_TRUE(ws.layoutMessage("move r", 4));

    EXPECT_TRUE(near(ws.rectOf(5), MASTER_ZONE));
    EXPECT_TRUE(near(ws.rectOf(4), rightRow(0, 720)));
    EXPECT_TRUE(near(ws.rectOf(1), rightRow(720, 720)));
}

TEST(ChainMoveMirrored, LeftFromTheTopOfRightSwapsWithMaster) {
    Workspace ws = mirrored(2, {2}, 4, {5, 1});

    EXPECT_TRUE(ws.layoutMessage("move l", 5));

    EXPECT_TRUE(near(ws.rectOf(5), MASTER_ZONE));
    EXPECT_TRUE(near(ws.rectOf(4), rightRow(0, 720)));
}

TEST(ChainMoveMirrored, RightFromTheBottomOfTheLeftStackSwapsWithMaster) {
    Workspace ws = mirrored(1, {2, 3}, 4, {1});

    EXPECT_TRUE(ws.layoutMessage("move r", 3));

    EXPECT_TRUE(near(ws.rectOf(3), MASTER_ZONE));
    EXPECT_TRUE(near(ws.rectOf(4), leftRow(720, 720)));
}

TEST(ChainMoveMirrored, StopsAtTheChainEnds) {
    Workspace ws = mirrored(1, {2, 3}, 4, {1});

    EXPECT_FALSE(ws.layoutMessage("move l", 2));
    EXPECT_FALSE(ws.layoutMessage("move r", 1));
}

// Two Stacks: moves off Master join either side, never swapping.

TEST(ChainMoveTwoStacks, MovesOffMasterJoinEachSideAtItsEndNearestMaster) {
    Workspace ws = mirrored(RowLimit::unlimited(), {2}, 4, {1});

    EXPECT_TRUE(ws.layoutMessage("move r", 4));
    EXPECT_TRUE(near(ws.rectOf(4), rightRow(0, 720)));
    EXPECT_TRUE(near(ws.rectOf(1), rightRow(720, 720)));

    ws.addWindow(5); // fills the Master Hole
    EXPECT_TRUE(ws.layoutMessage("move l", 5));
    EXPECT_TRUE(near(ws.rectOf(2), leftRow(0, 720)));
    EXPECT_TRUE(near(ws.rectOf(5), leftRow(720, 720)));
}

TEST(ChainFocusTwoStacks, WalksLeftMasterThenRight) {
    Workspace ws = mirrored(RowLimit::unlimited(), {2, 3}, 4, {5, 1});

    EXPECT_EQ(focusAfter(ws, "focus r", 3), 4u);
    EXPECT_EQ(focusAfter(ws, "focus r", 4), 5u);
    EXPECT_EQ(focusAfter(ws, "focus l", 5), 4u);
    EXPECT_EQ(focusAfter(ws, "focus l", 4), 3u);
}

TEST(Chain, AMissingLeftZoneIsNoStepAndMasterIsTheChainsLeftEnd) {
    Workspace ws = filled(0, 3); // 1 Master, Right 2 3

    EXPECT_EQ(ws.layoutMessage("focus l", 1, NO_LOOPING).focus, std::nullopt);
    EXPECT_EQ(ws.layoutMessage("focus r", 1).focus, 2u);
    EXPECT_EQ(ws.layoutMessage("focus l", 2).focus, 1u);
}

TEST(Chain, MovingMasterTowardAMissingLeftZoneDoesNothing) {
    Workspace ws = filled(0, 2); // 1 Master, Right 2

    EXPECT_FALSE(ws.layoutMessage("move l", 1));
    EXPECT_FALSE(ws.move(1, Direction::Left));

    EXPECT_EQ(ws.rectOf(1), (Rect{0, 0, 3840, 1440}));
    EXPECT_EQ(ws.rectOf(2), RIGHT_ZONE);
}

TEST(Chain, MovingMasterTowardAMissingRightZoneDoesNothing) {
    Workspace ws{AREA};
    ws.setSettings(Settings{.leftRows = RowLimit::unlimited(), .rightRows = 0});
    ws.addWindow(1);
    ws.addWindow(2); // Left

    EXPECT_EQ(ws.layoutMessage("focus r", 1, NO_LOOPING).focus, std::nullopt);
    EXPECT_FALSE(ws.layoutMessage("move r", 1));

    EXPECT_EQ(ws.rectOf(1), (Rect{1280, 0, 3840, 1440}));
    EXPECT_EQ(ws.rectOf(2), LEFT_ZONE);
}

TEST(Chain, WithBothSidesMissingMasterGoesNowhere) {
    Workspace ws{AREA};
    ws.setSettings(Settings{.leftRows = 0, .rightRows = 0});
    ws.addWindow(1);

    for (const char* message : {"move l", "move r", "move u", "move d"})
        EXPECT_FALSE(ws.layoutMessage(message, 1)) << message;
    EXPECT_EQ(ws.rectOf(1), AREA);
}
