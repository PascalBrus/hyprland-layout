#include "core/Cycle.hpp"
#include "core/Workspace.hpp"

#include <gtest/gtest.h>

#include <vector>

using threecol::cycleStep;
using threecol::WindowId;

namespace {

    // Tiles 2, 1, 3 in Chain order (Left, Master, Right); floating 7, 8 in
    // Hyprland's order.
    const std::vector<WindowId> CHAIN{2, 1, 3};
    const std::vector<WindowId> FLOATING{7, 8};

}

TEST(Cycle, FromTheChainsEndGoesToTheFirstFloatingWindow) {
    EXPECT_EQ(cycleStep(CHAIN, FLOATING, WindowId{3}, /*forward=*/true, /*wrap=*/true), 7u);
}

TEST(Cycle, FromAFloatingWindowGoesToTheNextFloatingWindow) {
    EXPECT_EQ(cycleStep(CHAIN, FLOATING, WindowId{7}, true, true), 8u);
}

TEST(Cycle, FromTheLastFloatingWindowLoopsBackToTheStartOfTheChain) {
    EXPECT_EQ(cycleStep(CHAIN, FLOATING, WindowId{8}, true, true), 2u);
}

TEST(Cycle, BackFromTheFirstFloatingWindowGoesToTheChainsEnd) {
    EXPECT_EQ(cycleStep(CHAIN, FLOATING, WindowId{7}, /*forward=*/false, true), 3u);
}

TEST(Cycle, BackFromTheStartOfTheChainLoopsToTheLastFloatingWindow) {
    EXPECT_EQ(cycleStep(CHAIN, FLOATING, WindowId{2}, false, true), 8u);
}

TEST(Cycle, WithoutWrapItStopsAtTheLastFloatingWindow) {
    EXPECT_EQ(cycleStep(CHAIN, FLOATING, WindowId{8}, true, /*wrap=*/false), std::nullopt);
}

TEST(Cycle, WithoutWrapItStopsAtTheStartOfTheChain) {
    EXPECT_EQ(cycleStep(CHAIN, FLOATING, WindowId{2}, false, false), std::nullopt);
}

TEST(Cycle, WithoutWrapTheChainsEndStillLeadsToTheFloatingWindows) {
    EXPECT_EQ(cycleStep(CHAIN, FLOATING, WindowId{3}, true, false), 7u);
}

TEST(Cycle, WithoutFloatingWindowsItIsTheChain) {
    EXPECT_EQ(cycleStep(CHAIN, {}, WindowId{3}, true, true), 2u);
    EXPECT_EQ(cycleStep(CHAIN, {}, WindowId{3}, true, false), std::nullopt);
}

TEST(Cycle, ALoneWindowGoesNowhere) {
    EXPECT_EQ(cycleStep({WindowId{1}}, {}, WindowId{1}, true, true), std::nullopt);
    EXPECT_EQ(cycleStep({}, {WindowId{7}}, WindowId{7}, true, true), std::nullopt);
}

TEST(Cycle, ALoneTileReachesTheFloatingWindows) {
    EXPECT_EQ(cycleStep({WindowId{1}}, FLOATING, WindowId{1}, true, true), 7u);
    EXPECT_EQ(cycleStep({WindowId{1}}, FLOATING, WindowId{1}, false, true), 8u);
}

TEST(Cycle, WithFloatingWindowsOnlyItLoopsThroughThem) {
    EXPECT_EQ(cycleStep({}, FLOATING, WindowId{8}, true, true), 7u);
    EXPECT_EQ(cycleStep({}, FLOATING, WindowId{8}, true, false), std::nullopt);
}

TEST(Cycle, AWindowOnNeitherListGoesNowhere) {
    EXPECT_EQ(cycleStep(CHAIN, FLOATING, WindowId{9}, true, true), std::nullopt);
}
