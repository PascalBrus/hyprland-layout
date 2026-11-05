#include "plugin/SavedDividers.hpp"

#include "core/DividerStore.hpp"
#include "plugin/Config.hpp"
#include "plugin/ThreecolAlgorithm.hpp"

#include <hyprland/src/debug/log/Logger.hpp>
#include <hyprland/src/event/EventBus.hpp>
#include <hyprland/src/workspace/HLWorkspace.hpp>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

namespace threecol {

    namespace {

        DividerStore                       g_store;
        CHyprSignalListener                g_onConfigReloaded;
        CHyprSignalListener                g_onWorkspaceRenamed;
        CHyprSignalListener                g_onPropsRefreshed;

        // Each threecol workspace's name as last seen, so a rename knows
        // which name its Dividers were kept under.
        struct SKnownName {
            PHLWORKSPACEREF workspace;
            std::string     name;
        };
        std::vector<SKnownName>            g_names;

        SKnownName* knownName(const PHLWORKSPACE& workspace) {
            std::erase_if(g_names, [](const auto& known) { return known.workspace.expired(); });
            const auto it = std::ranges::find_if(g_names, [&](const auto& known) { return known.workspace.lock() == workspace; });
            return it == g_names.end() ? nullptr : &*it;
        }

        // $XDG_STATE_HOME/threecol/dividers, else ~/.local/state/threecol/dividers.
        // Nothing if neither is set.
        std::optional<std::filesystem::path> stateFile() {
            std::filesystem::path dir;
            if (const char* state = std::getenv("XDG_STATE_HOME"); state && std::filesystem::path{state}.is_absolute())
                dir = state;
            else if (const char* home = std::getenv("HOME"); home && *home)
                dir = std::filesystem::path{home} / ".local" / "state";
            else
                return std::nullopt;
            return dir / "threecol" / "dividers";
        }

        // Where the state file is kept while persist is off.
        std::filesystem::path hiddenStateFile(std::filesystem::path path) {
            path += ".off";
            return path;
        }

        // Persist is off: moves the state file aside, replacing an older
        // hidden one, so a reboot doesn't load it.
        void hideStateFile() {
            const auto path = stateFile();
            if (!path)
                return;
            const auto     hidden = hiddenStateFile(*path);
            std::error_code error;
            std::filesystem::rename(*path, hidden, error);
            if (!error)
                LOG(Log::INFO, "[threecol] persist is off; moved {} aside to {}", path->string(), hidden.string());
            else if (error != std::errc::no_such_file_or_directory)
                LOG(Log::ERR, "[threecol] could not move {} aside to {}: {}; a reboot will load it", path->string(), hidden.string(), error.message());
        }

        // Persist is on: brings back the file hidden while it was off, unless
        // there is a state file already, then reads it.
        std::optional<std::string> readStateFile() {
            const auto path = stateFile();
            if (!path)
                return std::nullopt;

            std::error_code error;
            const auto      hidden = hiddenStateFile(*path);
            if (!std::filesystem::exists(*path, error) && !error && std::filesystem::exists(hidden, error)) {
                std::filesystem::rename(hidden, *path, error);
                if (!error)
                    LOG(Log::INFO, "[threecol] persist is on; moved {} back to {}", hidden.string(), path->string());
                else
                    LOG(Log::WARN, "[threecol] could not move {} back to {}: {}; it is left as it is", hidden.string(), path->string(), error.message());
            }

            std::ifstream file{*path, std::ios::binary};
            if (!file)
                return std::nullopt;
            std::ostringstream text;
            text << file.rdbuf();
            if (file.bad())
                return std::nullopt;
            return std::move(text).str();
        }

        // Writes a temporary file and renames it over the old one, so a crash
        // mid-write never leaves a half-written state file. There is no fsync:
        // ext4 and btrfs flush a file renamed over another, and a file lost
        // to a power cut only counts as nothing saved.
        void writeStateFile(const StateFileWrite& write) {
            const auto path = stateFile();
            if (!path) {
                LOG(Log::WARN, "[threecol] neither XDG_STATE_HOME nor HOME is set; Dividers aren't saved");
                return;
            }

            std::error_code error;
            std::filesystem::create_directories(path->parent_path(), error);
            if (error) {
                LOG(Log::ERR, "[threecol] could not create {}: {}", path->parent_path().string(), error.message());
                return;
            }
            if (write.backUpOldFile) {
                auto backup = *path;
                backup += ".bak";
                std::filesystem::rename(*path, backup, error);
                if (!error)
                    LOG(Log::WARN, "[threecol] {} didn't fully parse; moved it aside to {}", path->string(), backup.string());
                else if (error != std::errc::no_such_file_or_directory) {
                    LOG(Log::ERR, "[threecol] could not move {} aside to {}: {}; Dividers aren't saved", path->string(), backup.string(), error.message());
                    return;
                }
                g_store.backedUp();
            }
            auto temp = *path;
            temp += ".tmp";
            {
                std::ofstream file{temp, std::ios::binary | std::ios::trunc};
                file << write.text;
                file.close();
                if (!file) {
                    LOG(Log::ERR, "[threecol] could not write {}", temp.string());
                    return;
                }
            }
            std::filesystem::rename(temp, *path, error);
            if (error)
                LOG(Log::ERR, "[threecol] could not save Dividers to {}: {}", path->string(), error.message());
        }

        const DividerStore::StateFile g_stateFile{.read = readStateFile, .hide = hideStateFile};

        void write(const std::optional<StateFileWrite>& write) {
            if (write)
                writeStateFile(*write);
        }

        void configLoaded() {
            g_store.configLoaded(globalSettings().persist, g_stateFile);
            // Hyprland may lay out workspaces before this listener runs; do it
            // again so a reset from persist = false shows right away.
            recalculateAllWorkspaces();
        }

        // Settings were applied. After a config load, configLoaded has run
        // already (reloaded comes first), so a change to persist seen here
        // was made at runtime with hl.config.
        void propsRefreshed() {
            std::erase_if(g_names, [](const auto& known) { return known.workspace.expired(); });
            std::vector<std::string> shown;
            for (const auto& known : g_names)
                shown.push_back(known.name);
            write(g_store.persistChanged(globalSettings().persist, g_stateFile, shown));
        }

        // The name the workspace's Dividers are kept under. A workspace seen
        // under another name was renamed: its Dividers follow the new name.
        std::string nameOf(const PHLWORKSPACE& workspace) {
            const auto name  = workspace->displayName();
            auto*      known = knownName(workspace);
            if (!known)
                g_names.emplace_back(workspace, name);
            else if (known->name != name) {
                write(g_store.rename(known->name, name));
                known->name = name;
            }
            return name;
        }

    }

    void startSavingDividers() {
        g_store.configLoaded(globalSettings().persist, g_stateFile);
        g_onConfigReloaded   = Event::bus()->m_events.config.reloaded.listen([] { configLoaded(); });
        g_onPropsRefreshed   = Event::bus()->m_events.config.props_refreshed.listen([](bool) { propsRefreshed(); });
        g_onWorkspaceRenamed = Event::bus()->m_events.workspace.renamed.listen([](PHLWORKSPACEREF ref) {
            // Only a threecol workspace has Dividers kept under its old name.
            if (const auto workspace = ref.lock(); workspace && knownName(workspace))
                nameOf(workspace);
        });
    }

    void stopSavingDividers() {
        g_onConfigReloaded.reset();
        g_onWorkspaceRenamed.reset();
        g_onPropsRefreshed.reset();
    }

    std::optional<Dividers> savedDividers(const PHLWORKSPACE& workspace) {
        return g_store.find(nameOf(workspace));
    }

    void saveDividers(const PHLWORKSPACE& workspace, std::optional<Dividers> dividers) {
        write(g_store.set(nameOf(workspace), dividers));
    }

}
