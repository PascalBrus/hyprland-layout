#include "core/DividerStore.hpp"

#include <gtest/gtest.h>

#include <optional>
#include <string>

using threecol::Dividers;
using threecol::DividerStore;
using threecol::StateFileWrite;

namespace {

    // A store with persist on that read `file` (nothing: no file).
    DividerStore persisting(std::optional<std::string> file = std::nullopt) {
        DividerStore store;
        store.configLoaded(true, [&] { return file; });
        return store;
    }

    // The file a write left behind (nothing: no write).
    std::optional<std::string> text(const std::optional<StateFileWrite>& write) {
        return write ? std::optional{write->text} : std::nullopt;
    }

    DividerStore persisting(const std::optional<StateFileWrite>& write) {
        return persisting(text(write));
    }

}

TEST(DividerStore, SavedDividersComeBackAfterAReboot) {
    DividerStore before = persisting();
    before.set("1", Dividers{0.3, 0.2});
    const auto file = before.set("web", Dividers{0.15, 0.35});
    ASSERT_TRUE(file);

    DividerStore after = persisting(file);

    EXPECT_EQ(after.find("1"), (Dividers{0.3, 0.2}));
    EXPECT_EQ(after.find("web"), (Dividers{0.15, 0.35}));
    EXPECT_EQ(after.find("2"), std::nullopt);
}

TEST(DividerStore, ResetRemovesOnlyThatWorkspacesEntry) {
    DividerStore before = persisting();
    before.set("1", Dividers{0.3, 0.2});
    before.set("2", Dividers{0.2, 0.2});
    const auto file = before.set("1", std::nullopt);
    ASSERT_TRUE(file);

    DividerStore after = persisting(file);

    EXPECT_EQ(after.find("1"), std::nullopt);
    EXPECT_EQ(after.find("2"), (Dividers{0.2, 0.2}));
}

TEST(DividerStore, ResettingAWorkspaceWithNothingSavedWritesNothing) {
    DividerStore store = persisting();
    store.set("1", Dividers{0.3, 0.2});

    EXPECT_EQ(store.set("2", std::nullopt), std::nullopt);
}

TEST(DividerStore, KeepingTheSameDividersWritesNothing) {
    DividerStore store = persisting();
    store.set("1", Dividers{0.3, 0.2});

    EXPECT_EQ(store.set("1", Dividers{0.3, 0.2}), std::nullopt);
}

TEST(DividerStore, NamesAndWidthsRoundTripExactly) {
    const std::string awkward = "my web \\ws\nnext line ";
    const double      left    = 0.1 + 0.2; // not 0.3
    const double      stack   = 1.0 / 3;

    DividerStore before = persisting();
    const auto   file   = before.set(awkward, Dividers{left, stack});
    ASSERT_TRUE(file);

    EXPECT_EQ(persisting(file).find(awkward), (Dividers{left, stack}));
}

TEST(DividerStore, AMissingFileMeansNothingSaved) {
    EXPECT_EQ(persisting().find("1"), std::nullopt);
}

TEST(DividerStore, AFileThatIsNotAStateFileMeansNothingSaved) {
    for (const std::string file : {"", "garbage", "0.3 0.2 1\n", "threecol dividers 2\n0.3 0.2 1\n", "\xff\xfe threecol dividers 1"})
        EXPECT_EQ(persisting(file).find("1"), std::nullopt) << file;
}

TEST(DividerStore, ACorruptLineMeansNothingSavedForIt) {
    const DividerStore store = persisting("threecol dividers 1\n"
                                          "0.3 0.2 good\n"
                                          "0.3 bad\n"
                                          "0.3  0.2 twospaces\n"
                                          "1.5 0.2 toowide\n"
                                          "-0.1 0.2 negative\n"
                                          "nan 0.2 nan\n"
                                          "0.3 0.2 bad\\escape\n"
                                          "0.3 0.2\n");

    EXPECT_EQ(store.find("good"), (Dividers{0.3, 0.2}));
    for (const char* name : {"bad", "0.2 bad", "twospaces", " 0.2 twospaces", "toowide", "negative", "nan", "bad\\escape", ""})
        EXPECT_EQ(store.find(name), std::nullopt) << name;
}

TEST(DividerStore, WithoutPersistTheFileIsNeitherReadNorWritten) {
    DividerStore store;
    bool         read = false;
    store.configLoaded(false, [&] {
        read = true;
        return std::optional<std::string>{"threecol dividers 1\n0.3 0.2 1\n"};
    });

    EXPECT_FALSE(read);
    EXPECT_EQ(store.find("1"), std::nullopt);
    EXPECT_EQ(store.set("1", Dividers{0.3, 0.2}), std::nullopt);
    EXPECT_EQ(store.find("1"), (Dividers{0.3, 0.2})); // kept until the next reload
    EXPECT_EQ(store.set("1", std::nullopt), std::nullopt);
}

TEST(DividerStore, WithoutPersistEveryReloadResetsEveryWorkspace) {
    DividerStore store;
    store.configLoaded(false, [] { return std::nullopt; });
    store.set("1", Dividers{0.3, 0.2});
    store.set("2", Dividers{0.2, 0.3});

    store.configLoaded(false, [] { return std::nullopt; });

    EXPECT_EQ(store.find("1"), std::nullopt);
    EXPECT_EQ(store.find("2"), std::nullopt);
}

TEST(DividerStore, TurningPersistOffResetsAndTurningItBackOnRestoresTheFile) {
    DividerStore store = persisting();
    const auto   file  = store.set("1", Dividers{0.3, 0.2});

    store.configLoaded(false, [&] { return text(file); });
    EXPECT_EQ(store.find("1"), std::nullopt);

    store.configLoaded(true, [&] { return text(file); });
    EXPECT_EQ(store.find("1"), (Dividers{0.3, 0.2}));
}

TEST(DividerStore, WithPersistAReloadKeepsTheDividersInUse) {
    DividerStore store = persisting();
    store.set("1", Dividers{0.3, 0.2});

    store.configLoaded(true, [] { return std::optional<std::string>{"garbage"}; });

    EXPECT_EQ(store.find("1"), (Dividers{0.3, 0.2}));
}

TEST(DividerStore, RenamingAWorkspaceCarriesItsDividersToTheNewName) {
    DividerStore store = persisting();
    store.set("3", Dividers{0.3, 0.2});

    const auto file = store.rename("3", "code");
    ASSERT_TRUE(file);

    const DividerStore after = persisting(file);
    EXPECT_EQ(after.find("code"), (Dividers{0.3, 0.2}));
    EXPECT_EQ(after.find("3"), std::nullopt);
}

TEST(DividerStore, ARenamedWorkspaceThatFollowsItsSettingsDropsAStaleEntryUnderItsNewName) {
    DividerStore store = persisting();
    store.set("code", Dividers{0.3, 0.2}); // left over from an earlier workspace

    ASSERT_TRUE(store.rename("3", "code"));

    EXPECT_EQ(store.find("code"), std::nullopt);
}

TEST(DividerStore, ARenameThatChangesNothingWritesNothing) {
    DividerStore store = persisting();
    store.set("1", Dividers{0.3, 0.2});

    EXPECT_EQ(store.rename("3", "code"), std::nullopt);
    EXPECT_EQ(store.rename("1", "1"), std::nullopt);
    EXPECT_EQ(store.find("1"), (Dividers{0.3, 0.2}));
}

TEST(DividerStore, WithoutPersistARenameStillCarriesTheDividersButWritesNothing) {
    DividerStore store;
    store.configLoaded(false, [] { return std::nullopt; });
    store.set("3", Dividers{0.3, 0.2});

    EXPECT_EQ(store.rename("3", "code"), std::nullopt);
    EXPECT_EQ(store.find("code"), (Dividers{0.3, 0.2}));
}

TEST(DividerStore, AFileWithWindowsLineEndingsStillParses) {
    const DividerStore store = persisting("threecol dividers 1\r\n"
                                          "# left_width stack_width workspace\r\n"
                                          "0.3 0.2 1\r\n"
                                          "0.15 0.35 web\r\n");

    EXPECT_EQ(store.find("1"), (Dividers{0.3, 0.2}));
    EXPECT_EQ(store.find("web"), (Dividers{0.15, 0.35}));
}

TEST(DividerStore, ANameEndingInACarriageReturnRoundTrips) {
    DividerStore before = persisting();
    const auto   file   = before.set("web\r", Dividers{0.3, 0.2});
    ASSERT_TRUE(file);

    const DividerStore after = persisting(file);
    EXPECT_EQ(after.find("web\r"), (Dividers{0.3, 0.2}));
    EXPECT_EQ(after.find("web"), std::nullopt);
}

TEST(DividerStore, AWriteOverAFileThatDidNotFullyParseBacksItUpOnce) {
    for (const std::string file : {"garbage", "threecol dividers 2\n0.3 0.2 1\n", "threecol dividers 1\n0.3 0.2 good\n0.3 bad\n"}) {
        DividerStore store = persisting(file);

        const auto first = store.set("1", Dividers{0.3, 0.2});
        ASSERT_TRUE(first) << file;
        EXPECT_TRUE(first->backUpOldFile) << file;
        store.backedUp();

        const auto second = store.set("1", Dividers{0.2, 0.2});
        ASSERT_TRUE(second) << file;
        EXPECT_FALSE(second->backUpOldFile) << file;
    }
}

TEST(DividerStore, AWriteOverAMissingEmptyOrCleanFileDoesNotBackItUp) {
    DividerStore saved = persisting();
    const auto   clean = text(saved.set("2", Dividers{0.3, 0.2}));

    for (const auto& file : {std::optional<std::string>{}, std::optional<std::string>{""}, clean}) {
        DividerStore store = persisting(file);

        const auto write = store.set("1", Dividers{0.3, 0.2});
        ASSERT_TRUE(write);
        EXPECT_FALSE(write->backUpOldFile);
    }
}

TEST(DividerStore, ARenameOverAFileThatDidNotFullyParseBacksItUp) {
    DividerStore store = persisting("threecol dividers 1\n0.3 0.2 3\n0.3 bad\n");

    const auto write = store.rename("3", "code");
    ASSERT_TRUE(write);
    EXPECT_TRUE(write->backUpOldFile);
}

TEST(DividerStore, EveryWriteBacksUpTheOldFileUntilTheBackupIsMade) {
    DividerStore store = persisting("garbage");

    const auto failed = store.set("1", Dividers{0.3, 0.2});
    ASSERT_TRUE(failed);
    EXPECT_TRUE(failed->backUpOldFile);

    const auto retried = store.set("1", Dividers{0.2, 0.2});
    ASSERT_TRUE(retried);
    EXPECT_TRUE(retried->backUpOldFile);

    store.backedUp();
    const auto after = store.set("1", Dividers{0.3, 0.3});
    ASSERT_TRUE(after);
    EXPECT_FALSE(after->backUpOldFile);
}
