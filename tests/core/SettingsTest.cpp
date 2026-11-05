#include "core/Settings.hpp"
#include "core/Workspace.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <format>
#include <iterator>
#include <limits>
#include <tuple>

using threecol::LayoutOpts;
using threecol::Rect;
using threecol::Role;
using threecol::RowLimit;
using threecol::Settings;
using threecol::SettingsReport;
using threecol::Side;
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

    const auto warnings = ws.setSettings(Settings{.leftWidth = 0.2, .rightWidth = 0.3, .centreMaster = false}).warnings;

    EXPECT_TRUE(warnings.empty());
    EXPECT_EQ(ws.rectOf(2), (Rect{0, 0, 1024, 1440}));
    EXPECT_EQ(ws.rectOf(1), (Rect{1024, 0, 2560, 1440}));
    EXPECT_EQ(ws.rectOf(3), (Rect{3584, 0, 1536, 1440}));
}

TEST(Settings, LayoutOptsWidthsWinOverTheGlobalWidths) {
    Workspace ws = filled();

    const auto warnings = ws.setSettings(Settings{.centreMaster = false}, LayoutOpts{{"left_width", "0.2"}, {"right_width", "0.300000"}}, "3").warnings;

    EXPECT_TRUE(warnings.empty());
    EXPECT_EQ(ws.rectOf(2), (Rect{0, 0, 1024, 1440}));
    EXPECT_EQ(ws.rectOf(1), (Rect{1024, 0, 2560, 1440}));
    EXPECT_EQ(ws.rectOf(3), (Rect{3584, 0, 1536, 1440}));
}

TEST(Settings, LayoutOptsLeftRowsWinsOverTheGlobalRowLimit) {
    Workspace ws{AREA};

    ws.setSettings(Settings{.leftRows = 1, .centreMaster = false}, LayoutOpts{{"left_rows", "3"}}, "3");

    EXPECT_EQ(ws.settings().leftRows, 3);
}

TEST(Settings, LayoutOptsRightRowsWinsOverTheGlobalRowLimit) {
    Workspace ws{AREA};

    const auto warnings = ws.setSettings(Settings{.centreMaster = false}, LayoutOpts{{"right_rows", "2"}, {"left_rows", "unlimited"}}, "3").warnings;

    EXPECT_TRUE(warnings.empty());
    EXPECT_EQ(ws.settings().rightRows, 2);
    EXPECT_EQ(ws.settings().leftRows, RowLimit::unlimited());
}

TEST(Settings, AWorkspaceWithoutLayoutOptsUsesTheGlobalValues) {
    Workspace ws{AREA};
    const Settings global{.leftWidth = 0.3, .rightWidth = 0.2, .leftRows = 2, .centreMaster = false};

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

        const auto warnings = ws.setSettings(Settings{.leftWidth = 0.3, .centreMaster = false}, LayoutOpts{{"left_width", bad}}, "3").warnings;

        EXPECT_DOUBLE_EQ(ws.settings().leftWidth, 0.3) << "value: '" << bad << "'";
        ASSERT_EQ(warnings.size(), 1u) << "value: '" << bad << "'";
        EXPECT_NE(warnings[0].find("left_width"), std::string::npos);
    }
}

TEST(Settings, AnOutOfRangeLayoutOptsWidthIsIgnoredAndTheGlobalValueUsed) {
    for (const char* bad : {"-0.1", "1.5"}) {
        Workspace ws{AREA};

        const auto warnings = ws.setSettings(Settings{.rightWidth = 0.3, .centreMaster = false}, LayoutOpts{{"right_width", bad}}, "3").warnings;

        EXPECT_DOUBLE_EQ(ws.settings().rightWidth, 0.3) << "value: " << bad;
        EXPECT_EQ(warnings.size(), 1u) << "value: " << bad;
    }
}

TEST(Settings, ALeftRowsThatIsNotAWholeNumberIsIgnored) {
    for (const char* bad : {"-2", "1.5", "two"}) {
        Workspace ws{AREA};

        const auto warnings = ws.setSettings(Settings{.leftRows = 2, .centreMaster = false}, LayoutOpts{{"left_rows", bad}}, "3").warnings;

        EXPECT_EQ(ws.settings().leftRows, 2) << "value: " << bad;
        EXPECT_EQ(warnings.size(), 1u) << "value: " << bad;
    }
}

TEST(Settings, ARightRowsThatIsNotAWholeNumberOrUnlimitedIsIgnored) {
    for (const char* bad : {"-2", "1.5", "infinite", "Unlimited"}) {
        Workspace ws{AREA};

        const auto warnings = ws.setSettings(Settings{.centreMaster = false}, LayoutOpts{{"right_rows", bad}}, "3").warnings;

        EXPECT_EQ(ws.settings().rightRows, RowLimit::unlimited()) << "value: " << bad;
        ASSERT_EQ(warnings.size(), 1u) << "value: " << bad;
        EXPECT_NE(warnings[0].find("right_rows"), std::string::npos) << warnings[0];
    }
}

TEST(Settings, LayoutOptsFillFirstWinsOverTheGlobalFillFirst) {
    for (const auto [text, side, global] : {std::tuple{"right", Side::Right, Side::Left}, std::tuple{"left", Side::Left, Side::Right}}) {
        Workspace ws{AREA};

        const auto warnings = ws.setSettings(Settings{.fillFirst = global, .centreMaster = false}, LayoutOpts{{"fill_first", text}}, "3").warnings;

        EXPECT_TRUE(warnings.empty()) << "value: " << text;
        EXPECT_EQ(ws.settings().fillFirst, side) << "value: " << text;
    }
}

TEST(Settings, AFillFirstThatIsNotLeftOrRightIsIgnored) {
    for (const char* bad : {"", "Right", "master", "0", "1"}) {
        Workspace ws{AREA};

        const auto warnings = ws.setSettings(Settings{.fillFirst = Side::Right, .centreMaster = false}, LayoutOpts{{"fill_first", bad}}, "3").warnings;

        EXPECT_EQ(ws.settings().fillFirst, Side::Right) << "value: " << bad;
        ASSERT_EQ(warnings.size(), 1u) << "value: " << bad;
        EXPECT_NE(warnings[0].find("fill_first"), std::string::npos) << warnings[0];
        EXPECT_NE(warnings[0].find("workspace 3"), std::string::npos) << warnings[0];
        EXPECT_NE(warnings[0].find("using right"), std::string::npos) << warnings[0];
    }
}

TEST(Settings, FillFirstDefaultsToLeft) {
    EXPECT_EQ(Settings{}.fillFirst, Side::Left);
}

TEST(Settings, BothSidesMayBeBounded) {
    Workspace ws{AREA};

    const auto warnings = ws.setSettings(Settings{.leftRows = 2, .rightRows = 3, .centreMaster = false}).warnings;

    EXPECT_TRUE(warnings.empty());
    EXPECT_EQ(ws.settings().leftRows, 2);
    EXPECT_EQ(ws.settings().rightRows, 3);
}

TEST(Settings, LayoutOptsMayBoundBothSides) {
    Workspace ws{AREA};

    const auto warnings = ws.setSettings(Settings{.centreMaster = false}, LayoutOpts{{"right_rows", "2"}}, "3").warnings;

    EXPECT_TRUE(warnings.empty());
    EXPECT_EQ(ws.settings().leftRows, 1);
    EXPECT_EQ(ws.settings().rightRows, 2);
}

TEST(Settings, BothSidesMayBeStacks) {
    Workspace ws{AREA};

    const auto warnings = ws.setSettings(Settings{.leftRows = RowLimit::unlimited(), .centreMaster = false}).warnings;

    EXPECT_TRUE(warnings.empty());
    EXPECT_EQ(ws.settings().leftRows, RowLimit::unlimited());
    EXPECT_EQ(ws.settings().rightRows, RowLimit::unlimited());
}

TEST(Settings, ARowLimitOfZeroInLayoutOptsMakesAMissingZone) {
    for (const char* key : {"left_rows", "right_rows"}) {
        Workspace ws{AREA};

        const auto warnings = ws.setSettings(Settings{.centreMaster = false}, LayoutOpts{{key, "0"}}, "3").warnings;

        const auto& limit = std::string{key} == "left_rows" ? ws.settings().leftRows : ws.settings().rightRows;
        EXPECT_EQ(limit, 0) << key;
        EXPECT_EQ(limit.role(), Role::Missing) << key;
        EXPECT_TRUE(warnings.empty()) << key;
    }
}

TEST(Settings, AMissingZonesWidthIsNotRaisedToMinWidthNorWarnedAbout) {
    Workspace ws{AREA};

    const auto warnings = ws.setSettings(Settings{.leftWidth = 0.05, .minWidth = 0.1, .leftRows = 0, .centreMaster = false}).warnings;

    EXPECT_TRUE(warnings.empty());
}

TEST(Settings, AMissingZonesWidthIsNotCountedAgainstMastersMinimum) {
    Workspace ws{AREA};
    ws.addWindow(1);
    ws.addWindow(2); // Right

    // 0.7 + 0.25 would leave Master less than 0.2, but Left is missing.
    const auto warnings = ws.setSettings(Settings{.leftWidth = 0.7, .leftRows = 0, .centreMaster = false}).warnings;

    EXPECT_TRUE(warnings.empty());
    EXPECT_EQ(ws.rectOf(2), (Rect{3840, 0, 1280, 1440}));
}

TEST(Settings, AMasterMinimumWarningLeavesAMissingZoneOut) {
    for (const auto& [missing, present, opts] : {std::tuple{"left_width", "right_width", Settings{.rightWidth = 0.9, .leftRows = 0, .centreMaster = false}},
                                                 std::tuple{"right_width", "left_width", Settings{.leftWidth = 0.9, .rightRows = 0, .centreMaster = false}}}) {
        Workspace ws{AREA};

        // The missing zone's width comes from layout_opts; the problem doesn't.
        const auto warnings = ws.setSettings(opts, LayoutOpts{{missing, "0.3"}}, "3").warnings;

        ASSERT_EQ(warnings.size(), 1u) << missing;
        EXPECT_NE(warnings[0].find(present), std::string::npos) << warnings[0];
        EXPECT_NE(warnings[0].find("min_master_width"), std::string::npos) << warnings[0];
        EXPECT_EQ(warnings[0].find(missing), std::string::npos) << warnings[0];
        EXPECT_EQ(warnings[0].find("workspace 3"), std::string::npos) << warnings[0];
    }
}

TEST(Settings, AWholeNumberLeftRowsWrittenAsAFloatIsAccepted) {
    Workspace ws{AREA};

    // Hyprland turns a Lua float 2.0 into "2.000000".
    const auto warnings = ws.setSettings(Settings{.centreMaster = false}, LayoutOpts{{"left_rows", "2.000000"}}, "3").warnings;

    EXPECT_EQ(ws.settings().leftRows, 2);
    EXPECT_TRUE(warnings.empty());
}

TEST(Settings, ABadLayoutOptsKeyLeavesTheGoodOnesInPlace) {
    Workspace ws{AREA};

    const auto warnings = ws.setSettings(Settings{.centreMaster = false}, LayoutOpts{{"left_width", "oops"}, {"right_width", "0.3"}}, "3").warnings;

    EXPECT_DOUBLE_EQ(ws.settings().leftWidth, 0.25);
    EXPECT_DOUBLE_EQ(ws.settings().rightWidth, 0.3);
    EXPECT_EQ(warnings.size(), 1u);
}

TEST(Settings, WidthsThatSqueezeMasterBelowItsMinimumShrinkLeftAndStackInProportion) {
    Workspace ws = filled();

    const auto warnings = ws.setSettings(Settings{.leftWidth = 0.6, .rightWidth = 0.3, .minMasterWidth = 0.2, .centreMaster = false}).warnings;

    // Master keeps 0.2; Left and Stack share 0.8 at their 2:1 ratio.
    EXPECT_NEAR(ws.rectOf(1)->w, 1024, 1e-6);
    EXPECT_NEAR(ws.rectOf(2)->w, 5120 * 0.8 * 2 / 3, 1e-6);
    EXPECT_NEAR(ws.rectOf(3)->w, 5120 * 0.8 / 3, 1e-6);
    EXPECT_EQ(warnings.size(), 1u);
}

TEST(Settings, AWidthBelowMinWidthIsRaisedToMinWidth) {
    Workspace ws = filled();

    const auto warnings = ws.setSettings(Settings{.leftWidth = 0.05, .minWidth = 0.1, .centreMaster = false}).warnings;

    EXPECT_EQ(ws.rectOf(2), (Rect{0, 0, 512, 1440}));
    EXPECT_EQ(ws.rectOf(1), (Rect{512, 0, 3328, 1440}));
    EXPECT_EQ(warnings.size(), 1u);
}

TEST(Settings, ImpossibleLimitsStillLeaveMasterItsMinimum) {
    Workspace ws = filled();

    // Two Left/Stack minimums of 0.45 and a Master minimum of 0.2 add up to 1.1.
    const auto warnings = ws.setSettings(Settings{.minWidth = 0.45, .minMasterWidth = 0.2, .centreMaster = false}).warnings;

    EXPECT_EQ(ws.rectOf(2), (Rect{0, 0, 2048, 1440}));
    EXPECT_EQ(ws.rectOf(1), (Rect{2048, 0, 1024, 1440}));
    EXPECT_EQ(ws.rectOf(3), (Rect{3072, 0, 2048, 1440}));
    EXPECT_FALSE(warnings.empty());
}

TEST(Settings, LayoutOptsWidthsAreMadeToFitTooAndTheWarningNamesTheWorkspace) {
    Workspace ws = filled();

    const auto warnings = ws.setSettings(Settings{.centreMaster = false}, LayoutOpts{{"left_width", "0.5"}, {"right_width", "0.5"}}, "web").warnings;

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

    EXPECT_FALSE(report.notice(ws.setSettings(Settings{.centreMaster = false}, LayoutOpts{{"left_width", "0.3"}}, "1").warnings));
}

TEST(SettingsReport, ProblemsAlreadyShownForThisConfigLoadAreNotShownAgain) {
    Workspace      ws{AREA};
    SettingsReport report;
    const auto     warnings = ws.setSettings(Settings{.centreMaster = false}, LayoutOpts{{"typo", "1"}}, "1").warnings;

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
    auto           warnings = first.setSettings(Settings{.centreMaster = false}, LayoutOpts{{"typo", "1"}}, "1").warnings;

    report.configLoaded();
    report.notice(warnings);
    std::ranges::copy(later.setSettings(Settings{.centreMaster = false}, LayoutOpts{{"left_rows", "-1"}}, "2").warnings, std::back_inserter(warnings));
    const auto notice = report.notice(warnings);

    ASSERT_TRUE(notice);
    EXPECT_NE(notice->find("left_rows"), std::string::npos);
}

TEST(SettingsReport, EveryConfigLoadShowsItsProblemsAgain) {
    Workspace      ws{AREA};
    SettingsReport report;
    const auto     warnings = ws.setSettings(Settings{.centreMaster = false}, LayoutOpts{{"typo", "1"}}, "1").warnings;

    report.configLoaded();
    report.notice(warnings);
    report.configLoaded();

    EXPECT_TRUE(report.notice(warnings));
}

TEST(Settings, AGlobalProblemIsNotPinnedOnAWorkspaceThatOverridesAnotherWidth) {
    const Settings global{.rightWidth = 0.05, .minWidth = 0.1, .centreMaster = false};
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

// Hyprland stores settings as floats; widened to double, 0.1F is
// 0.10000000149011612, and warnings must not print that noise.
TEST(Settings, ARowLimitWarningPrintsAFloatStoredMinRowHeightWithoutNoise) {
    Workspace ws{AREA};

    const auto warnings = ws.setSettings(Settings{.minRowHeight = 0.1F, .leftRows = 99, .centreMaster = false}).warnings;

    ASSERT_EQ(warnings.size(), 1u);
    EXPECT_EQ(warnings[0], "left_rows 99 doesn't fit min_row_height 0.1; using 9");
}

TEST(Settings, AMinWidthWarningPrintsFloatStoredWidthsWithoutNoise) {
    Workspace ws = filled();

    const auto warnings = ws.setSettings(Settings{.leftWidth = 0.05F, .minWidth = 0.1F, .centreMaster = false}).warnings;

    ASSERT_EQ(warnings.size(), 1u);
    EXPECT_EQ(warnings[0], "left_width 0.05 is below min_width 0.1; using 0.1");
}

TEST(Settings, AMasterMinimumWarningPrintsFloatStoredWidthsWithoutNoise) {
    Workspace ws = filled();

    const auto warnings = ws.setSettings(Settings{.leftWidth = 0.6F, .rightWidth = 0.3F, .minMasterWidth = 0.2F, .centreMaster = false}).warnings;

    ASSERT_EQ(warnings.size(), 1u);
    EXPECT_EQ(warnings[0], "left_width 0.6 and right_width 0.3 leave Master less than min_master_width 0.2; using 0.533 and 0.267");
}

TEST(Settings, AOneSidedMasterMinimumWarningPrintsFloatStoredWidthsWithoutNoise) {
    Workspace ws{AREA};

    const auto warnings = ws.setSettings(Settings{.leftWidth = 0.9F, .minMasterWidth = 0.2F, .rightRows = 0, .centreMaster = false}).warnings;

    ASSERT_EQ(warnings.size(), 1u);
    EXPECT_EQ(warnings[0], "left_width 0.9 leaves Master less than min_master_width 0.2; using 0.8");
}

TEST(Settings, AnIgnoredLayoutOptsWidthPrintsTheFloatStoredGlobalWidthWithoutNoise) {
    Workspace ws = filled();

    const auto warnings = ws.setSettings(Settings{.leftWidth = 0.3F, .centreMaster = false}, LayoutOpts{{"left_width", "oops"}}, "3").warnings;

    ASSERT_EQ(warnings.size(), 1u);
    EXPECT_EQ(warnings[0], "workspace 3: layout_opts left_width = \"oops\" is not a number from 0 to 1; using 0.3");
}

TEST(Settings, AnOutOfRangeGlobalWidthIsIgnoredWithAWarningAndTheDefaultUsed) {
    Workspace ws{AREA};

    const auto warnings = ws.setSettings(Settings{.leftWidth = 2.0, .centreMaster = false}, {}, "3").warnings;

    EXPECT_DOUBLE_EQ(ws.settings().leftWidth, 0.25);
    ASSERT_EQ(warnings.size(), 1u);
    EXPECT_EQ(warnings[0], "left_width 2 is not a number from 0 to 1; using 0.25");
}

TEST(Settings, EveryOutOfRangeGlobalFractionIsIgnoredWithAWarningAndTheDefaultUsed) {
    const auto nan = std::numeric_limits<double>::quiet_NaN();
    const std::tuple<const char*, double Settings::*, const char*> fractions[] = {
        {"left_width", &Settings::leftWidth, "0.25"},   {"right_width", &Settings::rightWidth, "0.25"},
        {"min_width", &Settings::minWidth, "0.1"},      {"min_master_width", &Settings::minMasterWidth, "0.2"},
        {"min_row_height", &Settings::minRowHeight, "0.1"}, {"step", &Settings::step, "0.05"},
    };
    for (const auto& [key, member, def] : fractions) {
        for (const double bad : {-1.0, 1.5, nan}) {
            Settings global;
            global.*member = bad;
            Workspace ws{AREA};

            const auto warnings = ws.setSettings(global, {}, "3").warnings;

            EXPECT_EQ(ws.settings(), Settings{}) << key << " = " << bad;
            ASSERT_EQ(warnings.size(), 1u) << key << " = " << bad;
            EXPECT_EQ(warnings[0], std::format("{} {} is not a number from 0 to 1; using {}", key, static_cast<float>(bad), def));
        }
    }
}

TEST(Settings, GlobalFractionsOfZeroAndOneAreInRange) {
    Workspace ws{AREA};

    const auto warnings = ws.setSettings(Settings{.leftWidth = 1, .rightWidth = 0, .minWidth = 0, .minMasterWidth = 0, .minRowHeight = 0, .step = 1, .centreMaster = false}, {}, "3").warnings;

    EXPECT_TRUE(std::ranges::none_of(warnings, [](const std::string& w) { return w.find("is not a number") != std::string::npos; }));
}

TEST(Settings, ANegativeGlobalRowLimitIsIgnoredWithAWarningAndTheDefaultUsed) {
    Workspace ws{AREA};

    const auto warnings = ws.setSettings(Settings{.leftRows = -3, .rightRows = -1, .centreMaster = false}, {}, "3").warnings;

    EXPECT_EQ(ws.settings().leftRows, 1);
    EXPECT_EQ(ws.settings().rightRows, RowLimit::unlimited());
    EXPECT_EQ(warnings, (std::vector<std::string>{"left_rows -3 is not a whole number of at least 0 or \"unlimited\"; using 1",
                                                  "right_rows -1 is not a whole number of at least 0 or \"unlimited\"; using unlimited"}));
}

TEST(Settings, ABadLayoutOptsWidthFallsBackToTheDefaultWhenTheGlobalIsOutOfRangeToo) {
    Workspace ws{AREA};

    const auto warnings = ws.setSettings(Settings{.leftWidth = 2.0, .centreMaster = false}, LayoutOpts{{"left_width", "oops"}}, "3").warnings;

    EXPECT_DOUBLE_EQ(ws.settings().leftWidth, 0.25);
    EXPECT_EQ(warnings, (std::vector<std::string>{"left_width 2 is not a number from 0 to 1; using 0.25",
                                                  "workspace 3: layout_opts left_width = \"oops\" is not a number from 0 to 1; using 0.25"}));
}

TEST(Settings, AGoodLayoutOptsWidthWinsOverAnOutOfRangeGlobal) {
    Workspace ws{AREA};

    ws.setSettings(Settings{.leftWidth = 2.0, .centreMaster = false}, LayoutOpts{{"left_width", "0.3"}}, "3");

    EXPECT_DOUBLE_EQ(ws.settings().leftWidth, 0.3);
}
