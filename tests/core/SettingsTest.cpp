#include "core/Settings.hpp"
#include "core/Workspace.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <iterator>

using threecol::LayoutOpts;
using threecol::Rect;
using threecol::Settings;
using threecol::settingsNotice;
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

    const auto warnings = ws.setSettings(Settings{.leftWidth = 0.2, .stackWidth = 0.3});

    EXPECT_TRUE(warnings.empty());
    EXPECT_EQ(ws.rectOf(2), (Rect{0, 0, 1024, 1440}));
    EXPECT_EQ(ws.rectOf(1), (Rect{1024, 0, 2560, 1440}));
    EXPECT_EQ(ws.rectOf(3), (Rect{3584, 0, 1536, 1440}));
}

TEST(Settings, LayoutOptsWidthsWinOverTheGlobalWidths) {
    Workspace ws = filled();

    const auto warnings = ws.setSettings(Settings{}, LayoutOpts{{"left_width", "0.2"}, {"stack_width", "0.300000"}}, "3");

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

TEST(Settings, AWorkspaceWithoutLayoutOptsUsesTheGlobalValues) {
    Workspace ws{AREA};
    const Settings global{.leftWidth = 0.3, .stackWidth = 0.2, .leftRows = 2};

    ws.setSettings(global);

    EXPECT_EQ(ws.settings(), global);
}

TEST(Settings, AnUnknownLayoutOptsKeyIsIgnoredWithAWarning) {
    Workspace ws{AREA};

    const auto warnings = ws.setSettings(Settings{}, LayoutOpts{{"master_width", "0.5"}}, "3");

    EXPECT_EQ(ws.settings(), Settings{});
    ASSERT_EQ(warnings.size(), 1u);
    EXPECT_NE(warnings[0].find("workspace 3"), std::string::npos);
    EXPECT_NE(warnings[0].find("master_width"), std::string::npos);
}

TEST(Settings, ANonNumericLayoutOptsValueIsIgnoredAndTheGlobalValueUsed) {
    for (const char* bad : {"wide", "", "0.3x", "nan", "inf", "true"}) {
        Workspace ws{AREA};

        const auto warnings = ws.setSettings(Settings{.leftWidth = 0.3}, LayoutOpts{{"left_width", bad}}, "3");

        EXPECT_DOUBLE_EQ(ws.settings().leftWidth, 0.3) << "value: '" << bad << "'";
        ASSERT_EQ(warnings.size(), 1u) << "value: '" << bad << "'";
        EXPECT_NE(warnings[0].find("left_width"), std::string::npos);
    }
}

TEST(Settings, AnOutOfRangeLayoutOptsWidthIsIgnoredAndTheGlobalValueUsed) {
    for (const char* bad : {"-0.1", "1.5"}) {
        Workspace ws{AREA};

        const auto warnings = ws.setSettings(Settings{.stackWidth = 0.3}, LayoutOpts{{"stack_width", bad}}, "3");

        EXPECT_DOUBLE_EQ(ws.settings().stackWidth, 0.3) << "value: " << bad;
        EXPECT_EQ(warnings.size(), 1u) << "value: " << bad;
    }
}

TEST(Settings, ALeftRowsThatIsNotAWholeNumberOfAtLeastOneIsIgnored) {
    for (const char* bad : {"0", "-2", "1.5", "two"}) {
        Workspace ws{AREA};

        const auto warnings = ws.setSettings(Settings{.leftRows = 2}, LayoutOpts{{"left_rows", bad}}, "3");

        EXPECT_EQ(ws.settings().leftRows, 2) << "value: " << bad;
        EXPECT_EQ(warnings.size(), 1u) << "value: " << bad;
    }
}

TEST(Settings, AWholeNumberLeftRowsWrittenAsAFloatIsAccepted) {
    Workspace ws{AREA};

    // Hyprland turns a Lua float 2.0 into "2.000000".
    const auto warnings = ws.setSettings(Settings{}, LayoutOpts{{"left_rows", "2.000000"}}, "3");

    EXPECT_EQ(ws.settings().leftRows, 2);
    EXPECT_TRUE(warnings.empty());
}

TEST(Settings, ABadLayoutOptsKeyLeavesTheGoodOnesInPlace) {
    Workspace ws{AREA};

    const auto warnings = ws.setSettings(Settings{}, LayoutOpts{{"left_width", "oops"}, {"stack_width", "0.3"}}, "3");

    EXPECT_DOUBLE_EQ(ws.settings().leftWidth, 0.25);
    EXPECT_DOUBLE_EQ(ws.settings().stackWidth, 0.3);
    EXPECT_EQ(warnings.size(), 1u);
}

TEST(Settings, WidthsThatSqueezeMasterBelowItsMinimumShrinkLeftAndStackInProportion) {
    Workspace ws = filled();

    const auto warnings = ws.setSettings(Settings{.leftWidth = 0.6, .stackWidth = 0.3, .minMasterWidth = 0.2});

    // Master keeps 0.2; Left and Stack share 0.8 at their 2:1 ratio.
    EXPECT_NEAR(ws.rectOf(1)->w, 1024, 1e-6);
    EXPECT_NEAR(ws.rectOf(2)->w, 5120 * 0.8 * 2 / 3, 1e-6);
    EXPECT_NEAR(ws.rectOf(3)->w, 5120 * 0.8 / 3, 1e-6);
    EXPECT_EQ(warnings.size(), 1u);
}

TEST(Settings, AWidthBelowMinWidthIsRaisedToMinWidth) {
    Workspace ws = filled();

    const auto warnings = ws.setSettings(Settings{.leftWidth = 0.05, .minWidth = 0.1});

    EXPECT_EQ(ws.rectOf(2), (Rect{0, 0, 512, 1440}));
    EXPECT_EQ(ws.rectOf(1), (Rect{512, 0, 3328, 1440}));
    EXPECT_EQ(warnings.size(), 1u);
}

TEST(Settings, ImpossibleLimitsStillLeaveMasterItsMinimum) {
    Workspace ws = filled();

    // Two Left/Stack minimums of 0.45 and a Master minimum of 0.2 add up to 1.1.
    const auto warnings = ws.setSettings(Settings{.minWidth = 0.45, .minMasterWidth = 0.2});

    EXPECT_EQ(ws.rectOf(2), (Rect{0, 0, 2048, 1440}));
    EXPECT_EQ(ws.rectOf(1), (Rect{2048, 0, 1024, 1440}));
    EXPECT_EQ(ws.rectOf(3), (Rect{3072, 0, 2048, 1440}));
    EXPECT_FALSE(warnings.empty());
}

TEST(Settings, LayoutOptsWidthsAreMadeToFitTooAndTheWarningNamesTheWorkspace) {
    Workspace ws = filled();

    const auto warnings = ws.setSettings(Settings{}, LayoutOpts{{"left_width", "0.5"}, {"stack_width", "0.5"}}, "web");

    EXPECT_EQ(ws.rectOf(2), (Rect{0, 0, 2048, 1440}));
    EXPECT_EQ(ws.rectOf(1), (Rect{2048, 0, 1024, 1440}));
    EXPECT_DOUBLE_EQ(ws.settings().leftWidth, 0.4);
    ASSERT_EQ(warnings.size(), 1u);
    EXPECT_NE(warnings[0].find("workspace web"), std::string::npos);
}

TEST(Settings, AGlobalWidthMadeToFitGivesTheSameWarningOnEveryWorkspace) {
    const Settings global{.leftWidth = 0.5, .stackWidth = 0.5};
    Workspace      a{AREA};
    Workspace      b{AREA};

    const auto     warningsA = a.setSettings(global, {}, "1");
    const auto     warningsB = b.setSettings(global, {}, "2");

    ASSERT_EQ(warningsA.size(), 1u);
    EXPECT_EQ(warningsA, warningsB);
}

TEST(SettingsNotice, OneConfigLoadGivesOneNoticeListingEachProblemOnce) {
    const Settings global{.leftWidth = 0.5, .stackWidth = 0.5};
    Workspace      a{AREA};
    Workspace      b{AREA};

    std::vector<std::string> warnings = a.setSettings(global, LayoutOpts{{"typo", "1"}}, "1");
    std::ranges::copy(b.setSettings(global, {}, "2"), std::back_inserter(warnings));

    const auto notice = settingsNotice(warnings);

    ASSERT_TRUE(notice);
    EXPECT_NE(notice->find("typo"), std::string::npos);
    const auto fit = notice->find("min_master_width");
    ASSERT_NE(fit, std::string::npos);
    EXPECT_EQ(notice->find("min_master_width", fit + 1), std::string::npos) << *notice;
}

TEST(SettingsNotice, NoProblemsGiveNoNotice) {
    Workspace ws{AREA};

    EXPECT_FALSE(settingsNotice(ws.setSettings(Settings{}, LayoutOpts{{"left_width", "0.3"}}, "1")));
}
