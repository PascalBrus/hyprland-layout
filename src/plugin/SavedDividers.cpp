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

        std::optional<std::string> readStateFile() {
            const auto path = stateFile();
            if (!path)
                return std::nullopt;

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
        // mid-write never leaves a half-written state file.
        void writeStateFile(const std::string& text) {
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
            auto temp = *path;
            temp += ".tmp";
            {
                std::ofstream file{temp, std::ios::binary | std::ios::trunc};
                file << text;
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

        void write(const std::optional<std::string>& text) {
            if (text)
                writeStateFile(*text);
        }

        void configLoaded() {
            g_store.configLoaded(globalSettings().persist, readStateFile);
            // Hyprland may lay out workspaces before this listener runs; do it
            // again so a reset from persist = false shows right away.
            recalculateAllWorkspaces();
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
        g_store.configLoaded(globalSettings().persist, readStateFile);
        g_onConfigReloaded   = Event::bus()->m_events.config.reloaded.listen([] { configLoaded(); });
        g_onWorkspaceRenamed = Event::bus()->m_events.workspace.renamed.listen([](PHLWORKSPACEREF ref) {
            // Only a threecol workspace has Dividers kept under its old name.
            if (const auto workspace = ref.lock(); workspace && knownName(workspace))
                nameOf(workspace);
        });
    }

    void stopSavingDividers() {
        g_onConfigReloaded.reset();
        g_onWorkspaceRenamed.reset();
    }

    std::optional<Dividers> savedDividers(const PHLWORKSPACE& workspace) {
        return g_store.find(nameOf(workspace));
    }

    void saveDividers(const PHLWORKSPACE& workspace, std::optional<Dividers> dividers) {
        write(g_store.set(nameOf(workspace), dividers));
    }

}
