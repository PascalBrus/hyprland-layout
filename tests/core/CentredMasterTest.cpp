#include "core/Workspace.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <optional>
#include <utility>
#include <string>
#include <vector>

using threecol::Corner;
using threecol::Dividers;
using threecol::LayoutOpts;
using threecol::Rect;
using threecol::Settings;
using threecol::WindowId;
using threecol::Workspace;

namespace {

    // A 1000x1000 workspace, no gaps, default widths 0.25 / 0.5 / 0.25
    // (250 / 500 / 250 px), min_width 0.10 (100 px), min_master_width 0.20
    // (200 px) and a step of 0.05 (50 px). Centred Master is on by default.
    constexpr Rect AREA{0, 0, 1000, 1000};

    // A workspace with windows 1..count added in Fill order: 1 Master,
    // 2 Left, then the Stack.
    Workspace filled(WindowId count = 3) {
        Workspace ws{AREA};
        for (WindowId id = 1; id <= count; ++id)
            ws.addWindow(id);
        return ws;
    }

    // Left, Master and Right widths in pixels, from windows 2, 1 and 3.
    struct Widths {
        double left;
        double master;
        double right;
    };

    ::testing::AssertionResult widths(const Workspace& ws, Widths expected) {
        const auto left   = ws.rectOf(2);
        const auto master = ws.rectOf(1);
        const auto right  = ws.rectOf(3);
        if (!left || !master || !right)
            return ::testing::AssertionFailure() << "a window has no rectangle";
        constexpr double TOLERANCE = 1e-6;
        const auto       close     = [](double a, double b) { return std::abs(a - b) < TOLERANCE; };
        if (close(left->w, expected.left) && close(master->w, expected.master) && close(right->w, expected.right) && close(master->x, left->w))
            return ::testing::AssertionSuccess();
        return ::testing::AssertionFailure() << "got " << left->w << " | " << master->w << " | " << right->w << ", expected " << expected.left << " | "
                                             << expected.master << " | " << expected.right;
    }

}

TEST(CentredMaster, IsOnByDefault) {
    EXPECT_TRUE(Settings{}.centreMaster);
}

TEST(CentredMaster, AStepOnLeftWidensBothSides) {
    Workspace ws = filled();

    ws.layoutMessage("resize wider", 2);

    EXPECT_TRUE(widths(ws, {300, 400, 300}));
}

TEST(CentredMaster, AStepOnRightNarrowsBothSides) {
    Workspace ws = filled();

    ws.layoutMessage("resize narrower", 3);

    EXPECT_TRUE(widths(ws, {200, 600, 200}));
}

TEST(CentredMaster, AStepOnMasterMovesEachDividerHalfAStep) {
    Workspace ws = filled();

    ws.layoutMessage("resize wider", 1);

    EXPECT_TRUE(widths(ws, {225, 550, 225}));
}

TEST(CentredMaster, BothDividersStopTogetherWhenTheSidesReachMinWidth) {
    Workspace ws = filled();
    ws.setSettings(Settings{.step = 0.2});

    ws.layoutMessage("resize narrower", 2);

    EXPECT_TRUE(widths(ws, {100, 800, 100}));
}

TEST(CentredMaster, BothDividersStopTogetherWhenMasterReachesItsMinimum) {
    Workspace ws = filled();
    ws.setSettings(Settings{.step = 0.2});

    ws.layoutMessage("resize narrower", 1);
    ws.layoutMessage("resize narrower", 1);

    EXPECT_TRUE(widths(ws, {400, 200, 400}));
}

TEST(CentredMaster, AStepAlreadyAtTheLimitChangesNothing) {
    Workspace ws = filled();
    ws.setSettings(Settings{.step = 0.2});
    ws.layoutMessage("resize narrower", 2);

    EXPECT_FALSE(ws.layoutMessage("resize narrower", 3));
}

TEST(CentredMaster, DraggingMastersLeftHalfMirrorsTheOtherDivider) {
    Workspace ws = filled();

    ws.dragResize(1, -100, 0, Corner::TopLeft);

    EXPECT_TRUE(widths(ws, {150, 700, 150}));
}

TEST(CentredMaster, DraggingMastersRightHalfMirrorsTheOtherDivider) {
    Workspace ws = filled();

    ws.dragResize(1, -100, 0, Corner::BottomRight);

    EXPECT_TRUE(widths(ws, {350, 300, 350}));
}

TEST(CentredMaster, DraggingALeftWindowMirrorsTheOtherDivider) {
    Workspace ws = filled();

    ws.dragResize(2, 60, 0, Corner::BottomRight);
    ws.dragResize(2, 40, 0, Corner::BottomRight);

    EXPECT_TRUE(widths(ws, {350, 300, 350}));
}

TEST(CentredMaster, DraggingARightWindowMirrorsTheOtherDivider) {
    Workspace ws = filled();

    ws.dragResize(3, 100, 0, Corner::TopLeft);

    EXPECT_TRUE(widths(ws, {150, 700, 150}));
}

TEST(CentredMaster, ADragStopsBothDividersTogetherAndFollowsAgainWhenTheCursorComesBack) {
    Workspace ws = filled();

    ws.dragResize(2, 300, 0, Corner::BottomRight); // past Master's 200 px
    EXPECT_TRUE(widths(ws, {400, 200, 400}));

    ws.dragResize(2, -250, 0, Corner::BottomRight);
    EXPECT_TRUE(widths(ws, {300, 400, 300}));

    ws.dragResize(2, -300, 0, Corner::BottomRight); // past min_width
    EXPECT_TRUE(widths(ws, {100, 800, 100}));
}

TEST(CentredMaster, AOneOffResizeMirrorsTheOtherDivider) {
    Workspace ws = filled();

    ws.resizeWindow(3, -50);

    EXPECT_TRUE(widths(ws, {300, 400, 300}));
}

TEST(CentredMaster, AOneOffResizeOnMasterWithNoCornerWidensItOnBothSides) {
    Workspace ws = filled();

    ws.resizeWindow(1, 50);

    EXPECT_TRUE(widths(ws, {200, 600, 200}));
}

TEST(CentredMaster, RowEdgeResizesAreUnchanged) {
    Workspace ws = filled(4); // 3 and 4 share the Stack

    ws.resizeWindow(3, 0, 100);

    ASSERT_TRUE(ws.rectOf(3));
    EXPECT_DOUBLE_EQ(ws.rectOf(3)->h, 600);
    EXPECT_DOUBLE_EQ(ws.rectOf(4)->h, 400);
    EXPECT_DOUBLE_EQ(ws.rectOf(2)->w, 250);
}

TEST(CentredMaster, LayoutOptsTurnItOffForOneWorkspace) {
    Workspace ws = filled();

    const auto warnings = ws.setSettings(Settings{}, LayoutOpts{{"centre_master", "false"}}, "3").warnings;
    ws.layoutMessage("resize wider", 2);

    EXPECT_TRUE(warnings.empty());
    EXPECT_FALSE(ws.settings().centreMaster);
    EXPECT_TRUE(widths(ws, {300, 450, 250}));
}

TEST(CentredMaster, LayoutOptsTurnItOnOverTheGlobalValue) {
    Workspace ws{AREA};

    const auto warnings = ws.setSettings(Settings{.centreMaster = false}, LayoutOpts{{"centre_master", "true"}}, "3").warnings;

    EXPECT_TRUE(warnings.empty());
    EXPECT_TRUE(ws.settings().centreMaster);
}

TEST(CentredMaster, ABadLayoutOptsValueIsWarnedAboutAndTheGlobalValueUsed) {
    Workspace ws{AREA};

    const auto warnings = ws.setSettings(Settings{.centreMaster = false}, LayoutOpts{{"centre_master", "maybe"}}, "3").warnings;

    EXPECT_EQ(warnings, std::vector<std::string>{"workspace 3: layout_opts centre_master = \"maybe\" is not true or false; using false"});
    EXPECT_FALSE(ws.settings().centreMaster);
}

TEST(CentredMaster, AMissingSideZoneFromLayoutOptsIsAConflictThatTurnsItOff) {
    Workspace ws{AREA};

    const auto warnings = ws.setSettings(Settings{}, LayoutOpts{{"right_rows", "0"}}, "3").warnings;

    EXPECT_EQ(warnings, std::vector<std::string>{"workspace 3: centre_master needs both side zones, but right_rows is 0; using centre_master = false"});
    EXPECT_FALSE(ws.settings().centreMaster);
}

TEST(CentredMaster, AGlobalMissingSideZoneIsWarnedAboutWithoutNamingTheWorkspace) {
    Workspace ws{AREA};

    const auto warnings = ws.setSettings(Settings{.leftRows = 0, .rightRows = 0}, {}, "3").warnings;

    EXPECT_EQ(warnings, std::vector<std::string>{"centre_master needs both side zones, but left_rows and right_rows are 0; using centre_master = false"});
    EXPECT_FALSE(ws.settings().centreMaster);
}

TEST(CentredMaster, AMissingSideZoneWithItOffIsNoConflict) {
    Workspace ws{AREA};

    const auto warnings = ws.setSettings(Settings{.leftRows = 0, .centreMaster = false}).warnings;

    EXPECT_TRUE(warnings.empty());
}

TEST(CentredMaster, UnequalSideWidthsAreAConflictWhereMasterKeepsItsWidth) {
    Workspace ws = filled();

    const auto warnings = ws.setSettings(Settings{}, LayoutOpts{{"left_width", "0.2"}, {"right_width", "0.3"}}, "3").warnings;

    EXPECT_EQ(warnings, std::vector<std::string>{"workspace 3: centre_master needs equal side widths, but left_width is 0.2 and right_width is 0.3; using 0.25 each"});
    EXPECT_TRUE(widths(ws, {250, 500, 250}));
}

TEST(CentredMaster, UnequalGlobalSideWidthsAreWarnedAboutWithoutNamingTheWorkspace) {
    Workspace ws = filled();

    const auto warnings = ws.setSettings(Settings{.leftWidth = 0.1, .rightWidth = 0.3}, {}, "3").warnings;

    EXPECT_EQ(warnings, std::vector<std::string>{"centre_master needs equal side widths, but left_width is 0.1 and right_width is 0.3; using 0.2 each"});
    EXPECT_TRUE(widths(ws, {200, 600, 200}));
}

TEST(CentredMaster, UnequalSideWidthsWithItOffAreUsedAsSet) {
    Workspace ws = filled();

    const auto warnings = ws.setSettings(Settings{.leftWidth = 0.1, .rightWidth = 0.3, .centreMaster = false}).warnings;

    EXPECT_TRUE(warnings.empty());
    EXPECT_TRUE(widths(ws, {100, 600, 300}));
}

TEST(CentredMaster, ResetGoesBackToTheCentredSettingsWidths) {
    Workspace ws = filled();
    ws.setSettings(Settings{.leftWidth = 0.1, .rightWidth = 0.3});
    ws.layoutMessage("resize wider", 2);

    ws.layoutMessage("reset", std::nullopt);

    EXPECT_TRUE(widths(ws, {200, 600, 200}));
}

TEST(CentredMaster, TurningItOnCentresKeptDividersAndReplacesThem) {
    Workspace ws = filled();
    ws.setSettings(Settings{.centreMaster = false});
    ws.layoutMessage("resize wider", 2); // 300 | 450 | 250

    const auto warnings = ws.setSettings(Settings{}).warnings;

    EXPECT_TRUE(warnings.empty());
    EXPECT_TRUE(widths(ws, {275, 450, 275}));
    EXPECT_EQ(ws.dividers(), (Dividers{0.275, 0.275}));

    // Turned off again, the old ones don't come back.
    ws.setSettings(Settings{.centreMaster = false});
    EXPECT_TRUE(widths(ws, {275, 450, 275}));
}

TEST(CentredMaster, SavedDividersAreCentredWhenTakenBack) {
    Workspace ws = filled();

    ws.setDividers(Dividers{0.2, 0.3});

    EXPECT_TRUE(widths(ws, {250, 500, 250}));
    EXPECT_EQ(ws.dividers(), (Dividers{0.25, 0.25}));
}

TEST(CentredMaster, SavedDividersAreUsedAsSavedWithItOff) {
    Workspace ws = filled();
    ws.setSettings(Settings{.centreMaster = false});

    ws.setDividers(Dividers{0.2, 0.3});

    EXPECT_EQ(ws.dividers(), (Dividers{0.2, 0.3}));
}

TEST(CentredMaster, TurningItOnMidDragEndsTheDragCentred) {
    Workspace ws = filled();
    ws.setSettings(Settings{.centreMaster = false});
    ws.dragResize(2, 50, 0, Corner::BottomRight); // 300 | 450 | 250

    ws.setSettings(Settings{});

    EXPECT_FALSE(ws.resizing());
    EXPECT_FALSE(ws.dragResize(2, 50, 0, Corner::BottomRight));
    EXPECT_TRUE(widths(ws, {275, 450, 275}));
}

TEST(CentredMaster, LayoutOptsAcceptOneAndZero) {
    for (const auto& [text, on] : {std::pair{"1", true}, std::pair{"0", false}}) {
        Workspace ws{AREA};

        const auto warnings = ws.setSettings(Settings{.centreMaster = !on}, LayoutOpts{{"centre_master", text}}, "3").warnings;

        EXPECT_TRUE(warnings.empty()) << text;
        EXPECT_EQ(ws.settings().centreMaster, on) << text;
    }
}
