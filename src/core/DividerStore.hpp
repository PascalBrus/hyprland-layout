#pragma once

#include "core/Workspace.hpp"

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace threecol {

    // A new state file, for the caller to write.
    struct StateFileWrite {
        std::string text;
        // Move the old file aside (as dividers.bak) before writing, then call
        // DividerStore::backedUp: it didn't fully parse, so writing over it
        // would lose what's in it for good.
        bool        backUpOldFile = false;
    };

    // Every workspace's resized Dividers, by workspace name, and the state
    // file that keeps them across reboots. Hyprland destroys a workspace's
    // layout with the workspace, so the Dividers live here, plugin-wide.
    // Only the text of the state file is handled here; reading and writing
    // it is up to the caller.
    class DividerStore {
      public:
        // The state file, handled by the caller. While persist is off it is
        // hidden (as dividers.off), so a reboot doesn't load it, but kept.
        struct StateFile {
            // Persist turned on: bring the hidden file back unless there is a
            // state file already, then return its text (nothing: no file).
            std::function<std::optional<std::string>()> read;
            // Persist turned off: hide the file.
            std::function<void()>                        hide;
        };

        // The config was loaded. With `persist`, the saved Dividers are read
        // the first time (missing or corrupt content counts as nothing saved),
        // then kept. Without it, every workspace goes back to its settings and
        // the file is neither read nor written.
        void                       configLoaded(bool persist, const StateFile& file);

        // `persist` was set at runtime, without a config load; nothing on
        // screen moves. Turned off, the file is no longer written. Turned on,
        // the file is read and its entries kept for workspaces not in `shown`
        // (the ones on screen keep what they show), and the new state file is
        // returned so what's on screen is saved. No change does nothing.
        std::optional<StateFileWrite> persistChanged(bool persist, const StateFile& file, const std::vector<std::string>& shown);

        // The workspace's Dividers, or nothing if it follows its settings.
        std::optional<Dividers>    find(std::string_view workspace) const;

        // Keeps the workspace's Dividers, or forgets them with nothing (reset).
        // Returns the new state file if it needs writing.
        std::optional<StateFileWrite> set(std::string_view workspace, std::optional<Dividers> dividers);

        // A workspace was renamed: its Dividers follow the new name. Returns
        // the new state file if it needs writing.
        std::optional<StateFileWrite> rename(std::string_view from, std::string_view to);

        // The old file was moved aside as a write asked; later writes may
        // replace it. Until then, every write asks again.
        void                          backedUp();

      private:
        std::map<std::string, Dividers, std::less<>> m_dividers;
        std::optional<bool>                          m_persist; // nothing until a config is loaded
        bool                                         m_read    = false; // the file, since persist turned on
        bool                                         m_unparsed = false; // the file read didn't fully parse and isn't backed up yet

        // The new state file if it needs writing.
        std::optional<StateFileWrite>                changed() const;
    };

}
