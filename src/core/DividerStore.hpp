#pragma once

#include "core/Workspace.hpp"

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace threecol {

    // Every workspace's resized Dividers, by workspace name, and the state
    // file that keeps them across reboots. Hyprland destroys a workspace's
    // layout with the workspace, so the Dividers live here, plugin-wide.
    // Only the text of the state file is handled here; reading and writing
    // it is up to the caller.
    class DividerStore {
      public:
        // The state file's text, or nothing if there is none.
        using ReadFile = std::function<std::optional<std::string>()>;

        // The config was loaded. With `persist`, the saved Dividers are read
        // with `readFile` the first time (missing or corrupt content counts as
        // nothing saved), then kept. Without it, every workspace goes back to
        // its settings and the file is neither read nor written.
        void                       configLoaded(bool persist, const ReadFile& readFile);

        // The workspace's Dividers, or nothing if it follows its settings.
        std::optional<Dividers>    find(std::string_view workspace) const;

        // Keeps the workspace's Dividers, or forgets them with nothing (reset).
        // Returns the state file's new text if it needs writing.
        std::optional<std::string> set(std::string_view workspace, std::optional<Dividers> dividers);

        // A workspace was renamed: its Dividers follow the new name. Returns
        // the state file's new text if it needs writing.
        std::optional<std::string> rename(std::string_view from, std::string_view to);

      private:
        std::map<std::string, Dividers, std::less<>> m_dividers;
        bool                                         m_persist = false;
        bool                                         m_read    = false; // the file, since persist turned on

        // The state file's text if it needs writing.
        std::optional<std::string>                   changed() const;
    };

}
