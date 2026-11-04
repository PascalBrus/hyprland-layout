#include "core/Settings.hpp"
#include "core/Workspace.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <iterator>

using threecol::LayoutOpts;
using threecol::Rect;
using threecol::Role;
using threecol::RowLimit;
using threecol::Settings;
using threecol::SettingsReport;
using threecol::WindowId;
using threecol::Workspace;

namespace {

    // One 5120x1440 ultrawide, no gaps.
    constexpr Rect AREA{0, 0, 5120, 1440};

    // A workspace with Master (1), Left (2) and one Stack Row (3).
    Workspace filled() {
        Workspace ws{AREA};
        for (WindowId id = 1; id <= 3; ++id)
            ws.addWindow(id);
        return ws;
    }

}

TEST(Settings, GlobalWidthsSetTheZonesAndMasterGetsTheRest) {
    Workspace ws = filled();

    const auto warnings = ws.setSettings(Settings{.leftWidth = 0.2, .rightWidth = 0.3}).warnings;

    EXPECT_TRUE(warnings.empty());
    EXPECT_EQ(ws.rectOf(2), (Rect{0, 0, 1024, 1440}));
    EXPECT_EQ(ws.rectOf(1), (Rect{1024, 0, 2560, 1440}));
    EXPECT_EQ(ws.rectOf(3), (Rect{3584, 0, 1536, 1440}));
}

TEST(Settings, LayoutOptsWidthsWinOverTheGlobalWidths) {
    Workspace ws = filled();

    const auto warnings = ws.setSettings(Settings{}, LayoutOpts{{"left_width", "0.2"}, {"right_width", "0.300000"}}, "3").warnings;

    EXPECT_TRUE(warnings.empty());
    EXPECT_EQ(ws.rectOf(2), (Rect{0, 0, 1024, 1440}));
    EXPECT_EQ(ws.rectOf(1), (Rect{1024, 0, 2560, 1440}));
    EXPECT_EQ(ws.rectOf(3), (Rect{3584, 0, 1536, 1440}));
}

TEST(Settings, LayoutOptsLeftRowsWinsOverTheGlobalRowLimit) {
    Workspace ws{AREA};

    ws.setSettings(Settings{.leftRows = 1}, LayoutOpts{{"left_rows", "3"}}, "3");

    EXPECT_EQ(ws.settings().leftRows, 3);
}

TEST(Settings, LayoutOptsRightRowsWinsOverTheGlobalRowLimit) {
    Workspace ws{AREA};

    const auto warnings = ws.setSettings(Settings{}, LayoutOpts{{"right_rows", "2"}, {"left_rows", "unlimited"}}, "3").warnings;

    EXPECT_TRUE(warnings.empty());
    EXPECT_EQ(ws.settings().rightRows, 2);
    EXPECT_EQ(ws.settings().leftRows, RowLimit::unlimited());
}

TEST(Settings, AWorkspaceWithoutLayoutOptsUsesTheGlobalValues) {
    Workspace ws{AREA};
    const Settings global{.leftWidth = 0.3, .rightWidth = 0.2, .leftRows = 2};

    ws.setSettings(global);

    EXPECT_EQ(ws.settings(), global);
}

TEST(Settings, AnUnknownLayoutOptsKeyIsIgnoredWithAWarning) {
    Workspace ws{AREA};

    const auto warnings = ws.setSettings(Settings{}, LayoutOpts{{"master_width", "0.5"}}, "3").warnings;

    EXPECT_EQ(ws.settings(), Settings{});
    ASSERT_EQ(warnings.size(), 1u);
    EXPECT_NE(warnings[0].find("workspace 3"), std::string::npos);
    EXPECT_NE(warnings[0].find("master_width"), std::string::npos);
}

TEST(Settings, ANonNumericLayoutOptsValueIsIgnoredAndTheGlobalValueUsed) {
    for (const char* bad : {"wide", "", "0.3x", "nan", "inf", "true"}) {
        Workspace ws{AREA};

        const auto warnings = ws.setSettings(Settings{.leftWidth = 0.3}, LayoutOpts{{"left_width", bad}}, "3").warnings;

        EXPECT_DOUBLE_EQ(ws.settings().leftWidth, 0.3) << "value: '" << bad << "'";
        ASSERT_EQ(warnings.size(), 1u) << "value: '" << bad << "'";
        EXPECT_NE(warnings[0].find("left_width"), std::string::npos);
    }
}

TEST(Settings, AnOutOfRangeLayoutOptsWidthIsIgnoredAndTheGlobalValueUsed) {
    for (const char* bad : {"-0.1", "1.5"}) {
        Workspace ws{AREA};

        const auto warnings = ws.setSettings(Settings{.rightWidth = 0.3}, LayoutOpts{{"right_width", bad}}, "3").warnings;

        EXPECT_DOUBLE_EQ(ws.settings().rightWidth, 0.3) << "value: " << bad;
        EXPECT_EQ(warnings.size(), 1u) << "value: " << bad;
    }
}

TEST(Settings, ALeftRowsThatIsNotAWholeNumberOfAtLeastOneIsIgnored) {
    for (const char* bad : {"0", "-2", "1.5", "two"}) {
        Workspace ws{AREA};

        const auto warnings = ws.setSettings(Settings{.leftRows = 2}, LayoutOpts{{"left_rows", bad}}, "3").warnings;

        EXPECT_EQ(ws.settings().leftRows, 2) << "value: " << bad;
        EXPECT_EQ(warnings.size(), 1u) << "value: " << bad;
    }
}

TEST(Settings, ARightRowsThatIsNotAWholeNumberOrUnlimitedIsIgnored) {
    for (const char* bad : {"-2", "1.5", "infinite", "Unlimited"}) {
        Workspace ws{AREA};

        const auto warnings = ws.setSettings(Settings{}, LayoutOpts{{"right_rows", bad}}, "3").warnings;

        EXPECT_EQ(ws.settings().rightRows, RowLimit::unlimited()) << "value: " << bad;
        ASSERT_EQ(warnings.size(), 1u) << "value: " << bad;
        EXPECT_NE(warnings[0].find("right_rows"), std::string::npos) << warnings[0];
    }
}

TEST(Settings, BothSidesMayBeBounded) {
    Workspace ws{AREA};

    const auto warnings = ws.setSettings(Settings{.leftRows = 2, .rightRows = 3}).warnings;

    EXPECT_TRUE(warnings.empty());
    EXPECT_EQ(ws.settings().leftRows, 2);
    EXPECT_EQ(ws.settings().rightRows, 3);
}

TEST(Settings, LayoutOptsMayBoundBothSides) {
    Workspace ws{AREA};

    const auto warnings = ws.setSettings(Settings{}, LayoutOpts{{"right_rows", "2"}}, "3").warnings;

    EXPECT_TRUE(warnings.empty());
    EXPECT_EQ(ws.settings().leftRows, 1);
    EXPECT_EQ(ws.settings().rightRows, 2);
}

TEST(Settings, BothSidesMayBeStacks) {
    Workspace ws{AREA};

    const auto warnings = ws.setSettings(Settings{.leftRows = RowLimit::unlimited()}).warnings;

    EXPECT_TRUE(warnings.empty());
    EXPECT_EQ(ws.settings().leftRows, RowLimit::unlimited());
    EXPECT_EQ(ws.settings().rightRows, RowLimit::unlimited());
}

TEST(Settings, AWholeNumberLeftRowsWrittenAsAFloatIsAccepted) {
    Workspace ws{AREA};

    // Hyprland turns a Lua float 2.0 into "2.000000".
    const auto warnings = ws.setSettings(Settings{}, LayoutOpts{{"left_rows", "2.000000"}}, "3").warnings;

    EXPECT_EQ(ws.settings().leftRows, 2);
    EXPECT_TRUE(warnings.empty());
}

TEST(Settings, ABadLayoutOptsKeyLeavesTheGoodOnesInPlace) {
    Workspace ws{AREA};

    const auto warnings = ws.setSettings(Settings{}, LayoutOpts{{"left_width", "oops"}, {"right_width", "0.3"}}, "3").warnings;

    EXPECT_DOUBLE_EQ(ws.settings().leftWidth, 0.25);
    EXPECT_DOUBLE_EQ(ws.settings().rightWidth, 0.3);
    EXPECT_EQ(warnings.size(), 1u);
}

TEST(Settings, WidthsThatSqueezeMasterBelowItsMinimumShrinkLeftAndStackInProportion) {
    Workspace ws = filled();

    const auto warnings = ws.setSettings(Settings{.leftWidth = 0.6, .rightWidth = 0.3, .minMasterWidth = 0.2}).warnings;

    // Master keeps 0.2; Left and Stack share 0.8 at their 2:1 ratio.
    EXPECT_NEAR(ws.rectOf(1)->w, 1024, 1e-6);
    EXPECT_NEAR(ws.rectOf(2)->w, 5120 * 0.8 * 2 / 3, 1e-6);
    EXPECT_NEAR(ws.rectOf(3)->w, 5120 * 0.8 / 3, 1e-6);
    EXPECT_EQ(warnings.size(), 1u);
}

TEST(Settings, AWidthBelowMinWidthIsRaisedToMinWidth) {
    Workspace ws = filled();

    const auto warnings = ws.setSettings(Settings{.leftWidth = 0.05, .minWidth = 0.1}).warnings;

    EXPECT_EQ(ws.rectOf(2), (Rect{0, 0, 512, 1440}));
    EXPECT_EQ(ws.rectOf(1), (Rect{512, 0, 3328, 1440}));
    EXPECT_EQ(warnings.size(), 1u);
}

TEST(Settings, ImpossibleLimitsStillLeaveMasterItsMinimum) {
    Workspace ws = filled();

    // Two Left/Stack minimums of 0.45 and a Master minimum of 0.2 add up to 1.1.
    const auto warnings = ws.setSettings(Settings{.minWidth = 0.45, .minMasterWidth = 0.2}).warnings;

    EXPECT_EQ(ws.rectOf(2), (Rect{0, 0, 2048, 1440}));
    EXPECT_EQ(ws.rectOf(1), (Rect{2048, 0, 1024, 1440}));
    EXPECT_EQ(ws.rectOf(3), (Rect{3072, 0, 2048, 1440}));
    EXPECT_FALSE(warnings.empty());
}

TEST(Settings, LayoutOptsWidthsAreMadeToFitTooAndTheWarningNamesTheWorkspace) {
    Workspace ws = filled();

    const auto warnings = ws.setSettings(Settings{}, LayoutOpts{{"left_width", "0.5"}, {"right_width", "0.5"}}, "web").warnings;

    EXPECT_EQ(ws.rectOf(2), (Rect{0, 0, 2048, 1440}));
    EXPECT_EQ(ws.rectOf(1), (Rect{2048, 0, 1024, 1440}));
    EXPECT_DOUBLE_EQ(ws.settings().leftWidth, 0.4);
    ASSERT_EQ(warnings.size(), 1u);
    EXPECT_NE(warnings[0].find("workspace web"), std::string::npos);
}

TEST(Settings, AGlobalWidthMadeToFitGivesTheSameWarningOnEveryWorkspace) {
    const Settings global{.leftWidth = 0.5, .rightWidth = 0.5};
    Workspace      a{AREA};
    Workspace      b{AREA};

    const auto     warningsA = a.setSettings(global, {}, "1").warnings;
    const auto     warningsB = b.setSettings(global, {}, "2").warnings;

    ASSERT_EQ(warningsA.size(), 1u);
    EXPECT_EQ(warningsA, warningsB);
}

TEST(SettingsReport, OneConfigLoadGivesOneNoticeListingEachProblemOnce) {
    const Settings global{.leftWidth = 0.5, .rightWidth = 0.5};
    Workspace      a{AREA};
    Workspace      b{AREA};
    SettingsReport report;

    std::vector<std::string> warnings = a.setSettings(global, LayoutOpts{{"typo", "1"}}, "1").warnings;
    std::ranges::copy(b.setSettings(global, {}, "2").warnings, std::back_inserter(warnings));

    report.configLoaded();
    const auto notice = report.notice(warnings);

    ASSERT_TRUE(notice);
    EXPECT_NE(notice->find("typo"), std::string::npos);
    const auto fit = notice->find("min_master_width");
    ASSERT_NE(fit, std::string::npos);
    EXPECT_EQ(notice->find("min_master_width", fit + 1), std::string::npos) << *notice;
}

TEST(SettingsReport, NoProblemsGiveNoNotice) {
    Workspace      ws{AREA};
    SettingsReport report;

    report.configLoaded();

    EXPECT_FALSE(report.notice(ws.setSettings(Settings{}, LayoutOpts{{"left_width", "0.3"}}, "1").warnings));
}

TEST(SettingsReport, ProblemsAlreadyShownForThisConfigLoadAreNotShownAgain) {
    Workspace      ws{AREA};
    SettingsReport report;
    const auto     warnings = ws.setSettings(Settings{}, LayoutOpts{{"typo", "1"}}, "1").warnings;

    report.configLoaded();
    ASSERT_TRUE(report.notice(warnings));

    // Hyprland can announce the same load more than once; a later workspace
    // can bring the same global problem.
    EXPECT_FALSE(report.notice(warnings));
}

TEST(SettingsReport, AWorkspaceCreatedLaterWithANewProblemGivesANotice) {
    Workspace      first{AREA};
    Workspace      later{AREA};
    SettingsReport report;
    auto           warnings = first.setSettings(Settings{}, LayoutOpts{{"typo", "1"}}, "1").warnings;

    report.configLoaded();
    report.notice(warnings);
    std::ranges::copy(later.setSettings(Settings{}, LayoutOpts{{"left_rows", "0"}}, "2").warnings, std::back_inserter(warnings));
    const auto notice = report.notice(warnings);

    ASSERT_TRUE(notice);
    EXPECT_NE(notice->find("left_rows"), std::string::npos);
}

TEST(SettingsReport, EveryConfigLoadShowsItsProblemsAgain) {
    Workspace      ws{AREA};
    SettingsReport report;
    const auto     warnings = ws.setSettings(Settings{}, LayoutOpts{{"typo", "1"}}, "1").warnings;

    report.configLoaded();
    report.notice(warnings);
    report.configLoaded();

    EXPECT_TRUE(report.notice(warnings));
}

TEST(Settings, AGlobalProblemIsNotPinnedOnAWorkspaceThatOverridesAnotherWidth) {
    const Settings global{.rightWidth = 0.05, .minWidth = 0.1};
    Workspace      plain{AREA};
    Workspace      overriding{AREA};

    const auto     warningsPlain      = plain.setSettings(global, {}, "1").warnings;
    const auto     warningsOverriding = overriding.setSettings(global, LayoutOpts{{"left_width", "0.3"}}, "web").warnings;

    // Only the global right_width is too small; both workspaces report it alike.
    EXPECT_EQ(warningsPlain, warningsOverriding);
}

TEST(Settings, StackWidthIsNowAnUnknownLayoutOptsKey) {
    Workspace ws{AREA};

    const auto warnings = ws.setSettings(Settings{}, LayoutOpts{{"stack_width", "0.3"}}, "3").warnings;

    EXPECT_EQ(ws.settings(), Settings{});
    ASSERT_EQ(warnings.size(), 1u);
    EXPECT_NE(warnings[0].find("unknown layout_opts key \"stack_width\""), std::string::npos) << warnings[0];
}

TEST(Settings, ByDefaultLeftIsBoundedToOneRowAndRightIsAStack) {
    const Settings defaults;

    EXPECT_EQ(defaults.leftRows, RowLimit{1});
    EXPECT_EQ(defaults.leftRows.role(), Role::Bounded);
    EXPECT_EQ(defaults.rightRows, RowLimit::unlimited());
    EXPECT_EQ(defaults.rightRows.role(), Role::Stack);
}

TEST(Settings, AnUnlimitedRowLimitIsNoCount) {
    const auto unlimited = RowLimit::unlimited();

    EXPECT_EQ(unlimited.rows(), std::nullopt);
    EXPECT_NE(unlimited, RowLimit{0});
    EXPECT_NE(unlimited, RowLimit{-1});
    EXPECT_EQ(RowLimit{0}.role(), Role::Missing);
    EXPECT_EQ(RowLimit{3}.rows(), 3);
    EXPECT_EQ(RowLimit{3}.role(), Role::Bounded);
}
