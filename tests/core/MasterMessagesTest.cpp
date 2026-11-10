#include "core/Workspace.hpp"

#include <gtest/gtest.h>

#include <optional>
#include <string>
#include <utility>
#include <vector>

using threecol::Rect;
using threecol::Settings;
using threecol::Side;
using threecol::WindowId;
using threecol::Workspace;

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

    // fill_first right, one Right Row: 1 Master, 3 Right, 2 4 Left, so
    // Right's top and Left's top tell the sides apart.
    Workspace fillingRight() {
        Workspace ws{AREA};
        ws.setSettings(Settings{.leftRows = threecol::RowLimit::unlimited(), .rightRows = 1, .fillFirst = Side::Right});
        for (WindowId id = 1; id <= 4; ++id)
            ws.addWindow(id); // 1 Master, 2 Right, 3 4 Left
        ws.swapWindows(2, 3);
        return ws;
    }

    // Where each of windows 1..count is.
    std::vector<std::optional<Rect>> places(const Workspace& ws, WindowId count) {
        std::vector<std::optional<Rect>> out;
        for (WindowId id = 1; id <= count; ++id)
            out.push_back(ws.rectOf(id));
        return out;
    }

}

// The source cited below is Hyprland's master layout,
// src/layout/algorithm/tiled/master/MasterAlgorithm.cpp at acc1aa0b, the
// commit the README links to. Its "first non-master window" is the first
// window after Master in Fill order here.

TEST(MasterMessages, FocusmasterFromASideZoneFocusesMaster) {
    Workspace  ws     = filled();
    const auto result = ws.layoutMessage("focusmaster", 2);
    EXPECT_FALSE(result.changed);
    EXPECT_EQ(result.focus, 1u);
}

// focusmaster
// (FocusmasterFromASideZoneFocusesMaster above is :614-615 too.)

// :614-615: from another window, every argument focuses Master.
TEST(MasterMessages, FocusmasterFocusesMasterFromAnotherWindowWithEveryArgument) {
    Workspace ws = filled(4);
    for (const char* message : {"focusmaster", "focusmaster auto", "focusmaster master", "focusmaster previous"}) {
        SCOPED_TRACE(message);
        EXPECT_EQ(ws.layoutMessage(message, 4).focus, 1u);
    }
}

// :614-615 again: the master layout runs it with a floating window focused.
TEST(MasterMessages, FocusmasterFocusesMasterFromAFloatingWindow) {
    Workspace ws = filled(4);
    EXPECT_EQ(ws.layoutMessage("focusmaster", std::nullopt).focus, 1u);
}

// :622-630, :640: auto, or no argument, on Master focuses the first other window.
TEST(MasterMessages, FocusmasterAutoOnMasterFocusesTheFirstWindowAfterMaster) {
    Workspace ws = filled(4); // 1 Master, 2 Left, 3 4 Stack
    EXPECT_EQ(ws.layoutMessage("focusmaster", 1).focus, 2u);
    EXPECT_EQ(ws.layoutMessage("focusmaster auto", 1).focus, 2u);
    EXPECT_EQ(ws.layoutMessage("focusmaster bogus", 1).focus, 2u);
}

TEST(MasterMessages, FocusmasterAutoFollowsFillFirst) {
    Workspace ws = fillingRight();
    EXPECT_EQ(ws.layoutMessage("focusmaster", 1).focus, 3u);
}

// A Hole holds no window, so the first window after Master is past it.
TEST(MasterMessages, FocusmasterAutoSkipsALeftHole) {
    Workspace ws = filled(4);
    ws.removeWindow(2); // Left Hole
    EXPECT_EQ(ws.layoutMessage("focusmaster", 1).focus, 3u);
}

// :624-629 finds no other window, so focus stays; previous with
// nothing saved falls back to that (:638).
TEST(MasterMessages, FocusmasterAutoOnALoneMasterDoesNothing) {
    Workspace ws = filled(1);
    EXPECT_EQ(ws.layoutMessage("focusmaster", 1).focus, std::nullopt);
    EXPECT_EQ(ws.layoutMessage("focusmaster previous", 1).focus, std::nullopt);
}

// :632-633: master on Master stays.
TEST(MasterMessages, FocusmasterMasterOnMasterDoesNothing) {
    Workspace ws = filled(4);
    EXPECT_EQ(ws.layoutMessage("focusmaster master", 1).focus, std::nullopt);
    EXPECT_FALSE(ws.layoutMessage("focusmaster master", 1).changed);
}

// :617-618 saves the window previous came from, :635-638 goes back to it.
TEST(MasterMessages, FocusmasterPreviousGoesBackToTheWindowItCameFrom) {
    Workspace ws = filled(4);
    ws.layoutMessage("focusmaster previous", 4);
    EXPECT_EQ(ws.layoutMessage("focusmaster previous", 1).focus, 4u);
    // It stays saved until previous leaves another window.
    EXPECT_EQ(ws.layoutMessage("focusmaster previous", 1).focus, 4u);
}

// :616-618: only previous saves, so auto and master leave the saved one alone.
TEST(MasterMessages, FocusmasterOnlyPreviousSavesWhereItCameFrom) {
    Workspace ws = filled(4);
    ws.layoutMessage("focusmaster previous", 4);
    ws.layoutMessage("focusmaster", 3);
    ws.layoutMessage("focusmaster master", 2);
    EXPECT_EQ(ws.layoutMessage("focusmaster previous", 1).focus, 4u);

    Workspace fresh = filled(4);
    fresh.layoutMessage("focusmaster", 4);
    EXPECT_EQ(fresh.layoutMessage("focusmaster previous", 1).focus, 2u);
}

// :637-638: with nothing saved, or the saved window gone, previous acts as auto.
TEST(MasterMessages, FocusmasterPreviousFallsBackToAuto) {
    Workspace ws = filled(4);
    EXPECT_EQ(ws.layoutMessage("focusmaster previous", 1).focus, 2u);

    ws.layoutMessage("focusmaster previous", 4);
    ws.removeWindow(4);
    EXPECT_EQ(ws.layoutMessage("focusmaster previous", 1).focus, 2u);
}

// The master layout saves the floating window as the one to go back to
// (:617-618); it has no node, so `previous` on Master
// then falls back to auto (:636-638).
TEST(MasterMessages, FocusmasterPreviousFromAFloatingWindowForgetsTheTile) {
    Workspace ws = filled(4); // 1 Master, 2 Left, 3 4 Stack
    ws.layoutMessage("focusmaster previous", 4);
    ws.layoutMessage("focusmaster previous", std::nullopt); // a floating window

    EXPECT_EQ(ws.layoutMessage("focusmaster previous", 1).focus, 2u);
}

// :637: the saved window now Master is no window to go back to.
TEST(MasterMessages, FocusmasterPreviousFallsBackToAutoWhenTheSavedWindowIsMaster) {
    Workspace ws = filled(4);
    ws.layoutMessage("focusmaster previous", 4);
    ws.layoutMessage("swapwithmaster", 4); // 4 Master, 1 in its Row
    EXPECT_EQ(ws.layoutMessage("focusmaster previous", 4).focus, 2u);
}

// :605-606: no Master, nothing to focus. A Master Hole is threecol's.
TEST(MasterMessages, FocusmasterOnAMasterHoleDoesNothing) {
    Workspace ws = filled(3);
    ws.removeWindow(1);
    EXPECT_EQ(ws.layoutMessage("focusmaster", 2).focus, std::nullopt);
    EXPECT_EQ(ws.layoutMessage("focusmaster", std::nullopt).focus, std::nullopt);
}

// swapwithmaster

// :573-578: from another window it swaps with Master, focus staying on it
// for auto, master or no argument.
TEST(MasterMessages, SwapwithmasterFromAnotherWindowSwapsAndKeepsFocus) {
    for (const std::string message : {"swapwithmaster", "swapwithmaster auto", "swapwithmaster master"}) {
        SCOPED_TRACE(message);
        Workspace  ws     = filled(4);
        const auto before = places(ws, 4);

        const auto result = ws.layoutMessage(message, 4);

        EXPECT_TRUE(result.changed);
        EXPECT_EQ(result.focus, 4u);
        EXPECT_EQ(ws.rectOf(4), before[0]);
        EXPECT_EQ(ws.rectOf(1), before[3]);
        EXPECT_EQ(ws.rectOf(2), before[1]);
        EXPECT_EQ(ws.rectOf(3), before[2]);
    }
}

// :575-578: child focuses the old Master, now in the window's Row.
TEST(MasterMessages, SwapwithmasterChildFocusesTheOldMaster) {
    Workspace  ws     = filled(4);
    const auto before = places(ws, 4);

    const auto result = ws.layoutMessage("swapwithmaster child", 3);

    EXPECT_TRUE(result.changed);
    EXPECT_EQ(result.focus, 1u);
    EXPECT_EQ(ws.rectOf(3), before[0]);
    EXPECT_EQ(ws.rectOf(1), before[2]);
}

// :571, :579: ignoremaster only matters on Master.
TEST(MasterMessages, SwapwithmasterIgnoremasterStillSwapsFromAnotherWindow) {
    Workspace  ws     = filled(4);
    const auto before = places(ws, 4);

    const auto result = ws.layoutMessage("swapwithmaster child ignoremaster", 2);

    EXPECT_TRUE(result.changed);
    EXPECT_EQ(result.focus, 1u);
    EXPECT_EQ(ws.rectOf(2), before[0]);
}

// threecol's own: into a Master Hole the window moves, leaving its place
// under the close rules; with child there is no old Master, so focus stays.
TEST(MasterMessages, SwapwithmasterIntoAMasterHoleMovesThere) {
    for (const std::string message : {"swapwithmaster", "swapwithmaster master", "swapwithmaster child"}) {
        SCOPED_TRACE(message);
        Workspace ws = filled(4);
        const auto masterRect = ws.rectOf(1);
        ws.removeWindow(1); // Master Hole

        const auto result = ws.layoutMessage(message, 4);

        EXPECT_TRUE(result.changed);
        EXPECT_EQ(result.focus, 4u);
        EXPECT_EQ(ws.rectOf(4), masterRect);
    }
}

// :579-589: on Master it swaps with the first other window; focus stays on
// the old Master (:585, auto and child) or goes to the new one (:584, master).
TEST(MasterMessages, SwapwithmasterOnMasterSwapsWithTheFirstWindowAfterMaster) {
    const std::vector<std::pair<std::string, WindowId>> cases{
        {"swapwithmaster", 1}, {"swapwithmaster auto", 1}, {"swapwithmaster child", 1}, {"swapwithmaster master", 2}};
    for (const auto& [message, focus] : cases) {
        SCOPED_TRACE(message);
        Workspace  ws     = filled(4);
        const auto before = places(ws, 4);

        const auto result = ws.layoutMessage(message, 1);

        EXPECT_TRUE(result.changed);
        EXPECT_EQ(result.focus, focus);
        EXPECT_EQ(ws.rectOf(2), before[0]);
        EXPECT_EQ(ws.rectOf(1), before[1]);
    }
}

TEST(MasterMessages, SwapwithmasterOnMasterFollowsFillFirst) {
    Workspace  ws     = fillingRight();
    const auto before = places(ws, 4);

    EXPECT_TRUE(ws.layoutMessage("swapwithmaster", 1));
    EXPECT_EQ(ws.rectOf(3), before[0]);
    EXPECT_EQ(ws.rectOf(1), before[2]);
    EXPECT_EQ(ws.rectOf(2), before[1]);
}

// :571, :579: ignoremaster on Master does nothing, whatever comes first.
TEST(MasterMessages, SwapwithmasterIgnoremasterOnMasterDoesNothing) {
    for (const std::string message : {"swapwithmaster ignoremaster", "swapwithmaster auto ignoremaster", "swapwithmaster master ignoremaster"}) {
        SCOPED_TRACE(message);
        Workspace  ws     = filled(4);
        const auto before = places(ws, 4);

        const auto result = ws.layoutMessage(message, 1);

        EXPECT_FALSE(result.changed);
        EXPECT_EQ(result.focus, std::nullopt);
        EXPECT_EQ(places(ws, 4), before);
    }
}

// :559-560: not from a floating window.
TEST(MasterMessages, SwapwithmasterFromAFloatingWindowDoesNothing) {
    for (const std::string message : {"swapwithmaster", "swapwithmaster master", "swapwithmaster child"}) {
        SCOPED_TRACE(message);
        Workspace  ws     = filled(4);
        const auto before = places(ws, 4);

        const auto result = ws.layoutMessage(message, std::nullopt);

        EXPECT_FALSE(result.changed);
        EXPECT_EQ(result.focus, std::nullopt);
        EXPECT_EQ(places(ws, 4), before);
    }
}

// :580-589 finds no other window: nothing happens.
TEST(MasterMessages, SwapwithmasterOnALoneMasterDoesNothing) {
    Workspace ws = filled(1);
    EXPECT_FALSE(ws.layoutMessage("swapwithmaster", 1));
    EXPECT_FALSE(ws.layoutMessage("swapwithmaster master", 1));
}

// Ignored messages: the master and dwindle messages threecol has no use for.

TEST(MasterMessages, IgnoredMessagesChangeNothing) {
    const std::vector<std::string> ignored{"addmaster", "removemaster", "orientationleft", "orientationright", "orientationtop",
                                           "orientationbottom", "orientationcenter", "orientationnext", "orientationprev",
                                           "orientationcycle left top", "mfact 0.1", "mfact exact 0.5", "togglesplit", "swapsplit",
                                           "preselect l"};
    for (const auto& message : ignored) {
        for (const std::optional<WindowId> focused : {std::optional<WindowId>{1}, std::optional<WindowId>{3}, std::optional<WindowId>{std::nullopt}}) {
            SCOPED_TRACE(message + (focused ? " on " + std::to_string(*focused) : " floating"));
            Workspace  ws     = filled(4);
            const auto before = places(ws, 4);

            const auto result = ws.layoutMessage(message, focused);

            EXPECT_FALSE(result.changed);
            EXPECT_EQ(result.focus, std::nullopt);
            EXPECT_EQ(places(ws, 4), before);
            EXPECT_EQ(ws.dividers(), std::nullopt);
        }
    }
}
