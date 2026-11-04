#include "core/Workspace.hpp"

#include <gtest/gtest.h>

using threecol::Rect;
using threecol::RowLimit;
using threecol::Settings;
using threecol::Side;
using threecol::WindowId;
using threecol::Workspace;

namespace {

    // One 5120x1440 ultrawide, no gaps, default widths 0.25 / 0.5 / 0.25.
    constexpr Rect AREA{0, 0, 5120, 1440};

    constexpr Rect LEFT_ZONE{0, 0, 1280, 1440};
    constexpr Rect MASTER_ZONE{1280, 0, 2560, 1440};
    constexpr Rect RIGHT_ZONE{3840, 0, 1280, 1440};

}

TEST(FillOrder, FirstWindowLandsInMasterZone) {
    Workspace ws{AREA};
    ws.addWindow(1);

    EXPECT_EQ(ws.rectOf(1), MASTER_ZONE);
}

TEST(FillOrder, LoneWindowSitsOnlyInMasterWithSpaceOnBothSides) {
    // A work area offset by a bar and an output position.
    Workspace ws{Rect{100, 40, 5120, 1400}};
    ws.addWindow(1);

    const auto rect = ws.rectOf(1);
    ASSERT_TRUE(rect);
    EXPECT_EQ(rect->x, 100 + 1280);
    EXPECT_EQ(rect->x + rect->w, 100 + 3840);
    EXPECT_EQ(rect->y, 40);
    EXPECT_EQ(rect->h, 1400);
}

TEST(FillOrder, SecondWindowLandsInLeftZone) {
    Workspace ws{AREA};
    ws.addWindow(1);
    ws.addWindow(2);

    EXPECT_EQ(ws.rectOf(1), MASTER_ZONE);
    EXPECT_EQ(ws.rectOf(2), LEFT_ZONE);
}

TEST(FillOrder, ThirdWindowTakesTheWholeStackZone) {
    Workspace ws{AREA};
    ws.addWindow(1);
    ws.addWindow(2);
    ws.addWindow(3);

    EXPECT_EQ(ws.rectOf(1), MASTER_ZONE);
    EXPECT_EQ(ws.rectOf(2), LEFT_ZONE);
    EXPECT_EQ(ws.rectOf(3), RIGHT_ZONE);
}

TEST(FillOrder, FurtherWindowsJoinTheBottomOfTheStackSharingHeightEqually) {
    Workspace ws{AREA};
    for (threecol::WindowId id = 1; id <= 5; ++id)
        ws.addWindow(id);

    // Windows 3, 4, 5 are Stack Rows, top to bottom, 480 px each.
    EXPECT_EQ(ws.rectOf(3), (Rect{3840, 0, 1280, 480}));
    EXPECT_EQ(ws.rectOf(4), (Rect{3840, 480, 1280, 480}));
    EXPECT_EQ(ws.rectOf(5), (Rect{3840, 960, 1280, 480}));

    EXPECT_EQ(ws.rectOf(1), MASTER_ZONE);
    EXPECT_EQ(ws.rectOf(2), LEFT_ZONE);
}

TEST(FillOrder, NextWindowRectFollowsFillOrder) {
    Workspace ws{AREA};
    EXPECT_EQ(ws.nextWindowRect(), MASTER_ZONE);

    ws.addWindow(1);
    EXPECT_EQ(ws.nextWindowRect(), LEFT_ZONE);

    ws.addWindow(2);
    EXPECT_EQ(ws.nextWindowRect(), RIGHT_ZONE);

    // With one Stack Row, the next window would be the bottom of two.
    ws.addWindow(3);
    EXPECT_EQ(ws.nextWindowRect(), (Rect{3840, 720, 1280, 720}));
}

TEST(FillOrder, NextWindowRectMatchesWhereTheWindowLands) {
    Workspace ws{AREA};
    for (threecol::WindowId id = 1; id <= 6; ++id) {
        const auto predicted = ws.nextWindowRect();
        ws.addWindow(id);
        EXPECT_EQ(ws.rectOf(id), predicted) << "window " << id;
    }
}

TEST(Workspace, RemovedWindowHasNoRectangle) {
    Workspace ws{AREA};
    ws.addWindow(1);
    ws.addWindow(2);
    ws.addWindow(3);

    ws.removeWindow(2);

    EXPECT_EQ(ws.rectOf(2), std::nullopt);
}

TEST(Workspace, WindowsFollowAChangedArea) {
    Workspace ws{AREA};
    ws.addWindow(1);

    ws.setArea(Rect{0, 0, 2560, 1440});

    EXPECT_EQ(ws.rectOf(1), (Rect{640, 0, 1280, 1440}));
}

TEST(Workspace, AddingAWindowTwiceKeepsItInOneZone) {
    Workspace ws{AREA};
    ws.addWindow(1);
    ws.addWindow(1);
    ws.addWindow(2);

    EXPECT_EQ(ws.rectOf(1), MASTER_ZONE);
    EXPECT_EQ(ws.rectOf(2), LEFT_ZONE);
}

// Mirrored (left_rows = "unlimited", right_rows = 1): Left is the Stack.
// With the default fill_first = "left", the Left Stack takes every window
// after Master and Right fills only by moves.

TEST(FillOrder, MirroredWithFillFirstLeftTheLeftStackTakesEveryWindowAfterMaster) {
    Workspace ws{AREA};
    ws.setSettings(Settings{.leftRows = RowLimit::unlimited(), .rightRows = 1});
    for (WindowId id = 1; id <= 3; ++id)
        ws.addWindow(id);

    EXPECT_EQ(ws.rectOf(1), MASTER_ZONE);
    EXPECT_EQ(ws.rectOf(2), (Rect{0, 0, 1280, 720}));
    EXPECT_EQ(ws.rectOf(3), (Rect{0, 720, 1280, 720}));
    EXPECT_EQ(ws.nextWindowRect(), (Rect{0, 960, 1280, 480}));
}

TEST(FillOrder, WithTwoStacksAndFillFirstLeftLeftTakesEveryWindowAfterMaster) {
    Workspace ws{AREA};
    ws.setSettings(Settings{.leftRows = RowLimit::unlimited()});
    for (WindowId id = 1; id <= 3; ++id)
        ws.addWindow(id);

    EXPECT_EQ(ws.rectOf(1), MASTER_ZONE);
    EXPECT_EQ(ws.rectOf(2), (Rect{0, 0, 1280, 720}));
    EXPECT_EQ(ws.rectOf(3), (Rect{0, 720, 1280, 720}));
}

// fill_first = "right": after Master, Right fills up to its Row limit
// before Left.

TEST(FillOrder, FillFirstRightFillsRightBeforeLeft) {
    Workspace ws{AREA};
    ws.setSettings(Settings{.leftRows = RowLimit::unlimited(), .rightRows = 1, .fillFirst = Side::Right});

    EXPECT_EQ(ws.nextWindowRect(), MASTER_ZONE);
    ws.addWindow(1);
    EXPECT_EQ(ws.nextWindowRect(), RIGHT_ZONE);
    ws.addWindow(2);
    EXPECT_EQ(ws.nextWindowRect(), LEFT_ZONE);
    ws.addWindow(3);
    ws.addWindow(4);

    EXPECT_EQ(ws.rectOf(1), MASTER_ZONE);
    EXPECT_EQ(ws.rectOf(2), RIGHT_ZONE);
    EXPECT_EQ(ws.rectOf(3), (Rect{0, 0, 1280, 720}));
    EXPECT_EQ(ws.rectOf(4), (Rect{0, 720, 1280, 720}));
}

TEST(FillOrder, AStackAsTheFillFirstSideStarvesTheOtherSide) {
    // Default Row limits: Left holds 1, Right is the Stack.
    Workspace ws{AREA};
    ws.setSettings(Settings{.fillFirst = Side::Right});
    for (WindowId id = 1; id <= 3; ++id)
        ws.addWindow(id);

    EXPECT_EQ(ws.rectOf(1), MASTER_ZONE);
    EXPECT_EQ(ws.rectOf(2), (Rect{3840, 0, 1280, 720}));
    EXPECT_EQ(ws.rectOf(3), (Rect{3840, 720, 1280, 720}));
    EXPECT_EQ(ws.nextWindowRect(), (Rect{3840, 960, 1280, 480}));
}

TEST(FillOrder, MasterTakesAMissingLeftZonesWidthAndReachesTheEdge) {
    Workspace ws{AREA};
    ws.setSettings(Settings{.leftRows = 0});
    for (WindowId id = 1; id <= 2; ++id)
        ws.addWindow(id);

    EXPECT_EQ(ws.rectOf(1), (Rect{0, 0, 3840, 1440}));
    EXPECT_EQ(ws.rectOf(2), RIGHT_ZONE);
}

TEST(FillOrder, MasterTakesAMissingRightZonesWidthAndReachesTheEdge) {
    Workspace ws{AREA};
    ws.setSettings(Settings{.leftRows = RowLimit::unlimited(), .rightRows = 0});
    for (WindowId id = 1; id <= 2; ++id)
        ws.addWindow(id);

    EXPECT_EQ(ws.rectOf(1), (Rect{1280, 0, 3840, 1440}));
    EXPECT_EQ(ws.rectOf(2), LEFT_ZONE);
}

TEST(FillOrder, AMissingZonesWidthSettingIsIgnored) {
    Workspace ws{AREA};
    ws.setSettings(Settings{.leftWidth = 0.4, .leftRows = 0});
    ws.addWindow(1);

    EXPECT_EQ(ws.rectOf(1), (Rect{0, 0, 3840, 1440}));
}

TEST(FillOrder, WithBothSidesMissingMasterFillsTheWorkspaceAndTheNextWindowIsNotPlaced) {
    Workspace ws{AREA};
    ws.setSettings(Settings{.leftRows = 0, .rightRows = 0});

    EXPECT_TRUE(ws.addWindow(1));
    EXPECT_FALSE(ws.addWindow(2));

    EXPECT_EQ(ws.rectOf(1), AREA);
    EXPECT_FALSE(ws.rectOf(2));
    EXPECT_EQ(ws.nextWindowRect(), std::nullopt);
}

TEST(FillOrder, FillOrderSkipsAMissingFillFirstSide) {
    Workspace ws{AREA};
    ws.setSettings(Settings{.leftRows = 0});
    ws.addWindow(1);

    EXPECT_EQ(ws.nextWindowRect(), RIGHT_ZONE);
}

TEST(FillOrder, FillFirstRightWithRightMissingFillsLeft) {
    Workspace ws{AREA};
    ws.setSettings(Settings{.leftRows = RowLimit::unlimited(), .rightRows = 0, .fillFirst = Side::Right});
    for (WindowId id = 1; id <= 3; ++id)
        ws.addWindow(id);

    EXPECT_EQ(ws.rectOf(1), (Rect{1280, 0, 3840, 1440}));
    EXPECT_EQ(ws.rectOf(2), (Rect{0, 0, 1280, 720}));
    EXPECT_EQ(ws.rectOf(3), (Rect{0, 720, 1280, 720}));
}
