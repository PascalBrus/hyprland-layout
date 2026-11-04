#pragma once

#include <iosfwd>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace threecol {

    // What a side zone is, from its Row limit.
    enum class Role {
        Missing, // a Row limit of 0: no width, no Divider, no place in the Chain
        Bounded, // a Row limit of N >= 1
        Stack,   // no Row limit
    };

    // A side of Master.
    enum class Side {
        Left,
        Right,
    };

    // "left" or "right", as the settings spell it.
    std::string_view     sideName(Side side);
    std::ostream&        operator<<(std::ostream& out, Side side);

    // The side `text` names ("left" or "right"), or nothing.
    std::optional<Side>  parseSide(std::string_view text);

    // The most Rows a side zone may hold: a count, or unlimited. Unlimited is
    // no count at all, so it can't be mistaken for one.
    class RowLimit {
      public:
        // A count of Rows; 0 is a missing zone.
        constexpr RowLimit(int rows) : m_rows(rows) {}

        static constexpr RowLimit unlimited() {
            return RowLimit{};
        }

        // The count, or nothing when unlimited.
        constexpr std::optional<int> rows() const {
            return m_rows;
        }

        constexpr Role role() const {
            if (!m_rows)
                return Role::Stack;
            return *m_rows > 0 ? Role::Bounded : Role::Missing;
        }

        // The count, or "unlimited", as the settings spell it.
        std::string          text() const;

        friend bool          operator==(const RowLimit&, const RowLimit&) = default;
        friend std::ostream& operator<<(std::ostream& out, const RowLimit& limit);

      private:
        constexpr RowLimit() = default;

        std::optional<int> m_rows;
    };

    // threecol's settings, as set with plugin:threecol:* (the defaults are the
    // spec's). Sizes are fractions of the workspace area; Master gets what
    // Left and Right leave.
    struct Settings {
        double   leftWidth      = 0.25;
        double   rightWidth     = 0.25;
        double   minWidth       = 0.10;
        double   minMasterWidth = 0.20;
        double   minRowHeight   = 0.10;
        double   step           = 0.05;
        RowLimit leftRows       = 1;
        RowLimit rightRows      = RowLimit::unlimited();
        Side     fillFirst      = Side::Left; // the side new windows go to after Master
        bool     persist        = true;

        friend bool operator==(const Settings&, const Settings&) = default;
    };

    // A workspace rule's layout_opts: key → value, as Hyprland stores them.
    using LayoutOpts = std::map<std::string, std::string>;

    struct ResolvedSettings {
        Settings                 settings;
        std::vector<std::string> warnings; // what was ignored or adjusted
    };

    // Shrinks Left and Right in proportion, if needed, so Master keeps
    // min_master_width. Returns whether they shrank. Both settings and resized
    // Dividers are made to fit this way.
    bool leaveMasterItsMinimum(double& leftWidth, double& rightWidth, double minMasterWidth);

    // The settings one workspace uses: its layout_opts win over the global
    // values. `workspaceName` names the workspace in warnings.
    ResolvedSettings resolveSettings(const Settings& global, const LayoutOpts& opts, std::string_view workspaceName);

    // Decides when settings problems reach the user: one notification per
    // config load, listing each problem once. Within a load it stays quiet
    // unless a new problem shows up, e.g. on a workspace created later.
    class SettingsReport {
      public:
        // A config load: its problems are shown again, even if unchanged.
        void                       configLoaded();

        // Every workspace's current warnings. Returns the notification to show,
        // or nothing if there's nothing new to tell.
        std::optional<std::string> notice(const std::vector<std::string>& warnings);

      private:
        std::vector<std::string> m_shown; // since the last config load
    };

}
