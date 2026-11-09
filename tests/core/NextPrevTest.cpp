#include "core/Workspace.hpp"

#include <gtest/gtest.h>

using threecol::Rect;
using threecol::WindowId;
using threecol::Workspace;
using threecol::Wrap;

namespace {

    constexpr Rect AREA{0, 0, 1000, 1000};

    // A workspace with windows 1..count added in Fill order: 1 Master,
    // 2 Left, then the Stack.
    Workspace filled(WindowId count = 3) {
        Workspace ws{AREA};
        for (WindowId id = 1; id <= count; ++id)
            ws.addWindow(id);
        return ws;
    }

}

// The Chain runs Left, Master, Right: 2, 1, 3.
TEST(NextPrev, NextFollowsTheChainFromLeftThroughMasterToRight) {
    const Workspace ws = filled();
    EXPECT_EQ(ws.next(2, /*forward=*/true, /*wrap=*/false), 1u);
    EXPECT_EQ(ws.next(1, /*forward=*/true, /*wrap=*/false), 3u);
}

TEST(NextPrev, CyclenextLoopsFromTheChainsEndByDefault) {
    Workspace ws = filled();
    EXPECT_EQ(ws.layoutMessage("cyclenext", 3).focus, 2u);
}

TEST(NextPrev, PreviousFollowsTheChainBackFromRightThroughMasterToLeft) {
    const Workspace ws = filled();
    EXPECT_EQ(ws.next(3, /*forward=*/false, /*wrap=*/false), 1u);
    EXPECT_EQ(ws.next(1, false, false), 2u);
}

TEST(NextPrev, NextLoopsAtBothEndsOnlyWithWrap) {
    const Workspace ws = filled();
    EXPECT_EQ(ws.next(3, true, /*wrap=*/true), 2u);
    EXPECT_EQ(ws.next(2, false, true), 3u);
    EXPECT_EQ(ws.next(3, true, /*wrap=*/false), std::nullopt);
    EXPECT_EQ(ws.next(2, false, false), std::nullopt);
}

TEST(NextPrev, CycleprevLoopsFromTheChainsStartByDefault) {
    Workspace ws = filled();
    EXPECT_EQ(ws.layoutMessage("cycleprev", 2).focus, 3u);
}

TEST(NextPrev, CyclenextStopsAtTheChainsEndsWithWrapCycleNextOff) {
    Workspace ws = filled();
    const Wrap off{.cycleNext = false};
    EXPECT_EQ(ws.layoutMessage("cyclenext", 3, off).focus, std::nullopt);
    EXPECT_EQ(ws.layoutMessage("cycleprev", 2, off).focus, std::nullopt);
    EXPECT_EQ(ws.layoutMessage("cyclenext", 2, off).focus, 1u);
}

TEST(NextPrev, NoloopStopsCyclenextAtTheChainsEnds) {
    Workspace ws = filled();
    EXPECT_EQ(ws.layoutMessage("cyclenext noloop", 3).focus, std::nullopt);
    EXPECT_EQ(ws.layoutMessage("cycleprev noloop", 2).focus, std::nullopt);
    EXPECT_EQ(ws.layoutMessage("cyclenext noloop", 2).focus, 1u);
}

TEST(NextPrev, SwapnextLoopsFromTheChainsEndByDefault) {
    Workspace ws         = filled();
    const auto leftRect  = ws.rectOf(2);
    const auto rightRect = ws.rectOf(3);
    const auto result    = ws.layoutMessage("swapnext", 3);
    EXPECT_TRUE(result.changed);
    EXPECT_EQ(result.focus, 3u);
    EXPECT_EQ(ws.rectOf(3), leftRect);
    EXPECT_EQ(ws.rectOf(2), rightRect);
}

TEST(NextPrev, SwapprevLoopsFromTheChainsStartByDefault) {
    Workspace ws         = filled();
    const auto leftRect  = ws.rectOf(2);
    const auto rightRect = ws.rectOf(3);
    const auto result    = ws.layoutMessage("swapprev", 2);
    EXPECT_TRUE(result.changed);
    EXPECT_EQ(result.focus, 2u);
    EXPECT_EQ(ws.rectOf(2), rightRect);
    EXPECT_EQ(ws.rectOf(3), leftRect);
}

TEST(NextPrev, SwapnextStopsAtTheChainsEndsWithWrapSwapNextOff) {
    Workspace ws      = filled();
    const auto before = ws.rectOf(3);
    const Wrap off{.swapNext = false};
    EXPECT_FALSE(ws.layoutMessage("swapnext", 3, off).changed);
    EXPECT_FALSE(ws.layoutMessage("swapprev", 2, off).changed);
    EXPECT_EQ(ws.rectOf(3), before);
}

TEST(NextPrev, EachWrapSwitchTurnsOffOnlyItsOwnLoop) {
    Workspace ws = filled();
    EXPECT_EQ(ws.layoutMessage("cyclenext", 3, Wrap{.swapNext = false}).focus, 2u);
    EXPECT_TRUE(ws.layoutMessage("swapnext", 3, Wrap{.cycleNext = false}).changed);
}

TEST(NextPrev, NoloopStopsSwapnextAtTheChainsEnds) {
    Workspace ws = filled();
    EXPECT_FALSE(ws.layoutMessage("swapnext noloop", 3).changed);
    EXPECT_FALSE(ws.layoutMessage("swapprev noloop", 2).changed);
}

// Closing Master leaves a Hole: the Chain is 2, 3, and next skips the Hole.
TEST(NextPrev, NextSkipsAMasterHole) {
    Workspace ws = filled();
    ws.removeWindow(1);
    EXPECT_EQ(ws.layoutMessage("cyclenext", 2).focus, 3u);
    EXPECT_EQ(ws.layoutMessage("cyclenext", 3).focus, 2u);
    EXPECT_EQ(ws.layoutMessage("cycleprev noloop", 3).focus, 2u);
}

TEST(NextPrev, ALoneWindowGoesNowhereEvenLooping) {
    Workspace ws = filled(1);
    EXPECT_EQ(ws.layoutMessage("cyclenext", 1).focus, std::nullopt);
    EXPECT_EQ(ws.layoutMessage("cycleprev", 1).focus, std::nullopt);
    EXPECT_FALSE(ws.layoutMessage("swapnext", 1).changed);
    EXPECT_FALSE(ws.layoutMessage("swapprev", 1).changed);
}
