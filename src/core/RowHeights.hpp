#pragma once

#include <cstddef>
#include <vector>

namespace threecol {

    // The heights of one zone's Rows, top to bottom, as fractions of the zone
    // height. A height belongs to the Row, not the window in it.
    //
    // The kept heights are proportions; `fitted` turns them into the heights
    // in use under min_row_height. When the Rows can't all meet it they share
    // the height equally, and the kept proportions come back once they fit.
    class RowHeights {
      public:
        std::size_t         count() const;

        // A new bottom Row gets 1/n; the others shrink, keeping their ratios.
        void                add();

        // A Row closes; the others grow in proportion.
        void                remove(std::size_t row);

        // Every Row the same height again. Returns whether that changed them.
        bool                equalise(double minHeight);

        // The heights in use: each at least `minHeight`, taking the room from
        // the others in proportion, or all equal if that can't fit.
        std::vector<double> fitted(double minHeight) const;

        // Whether the Rows can't all meet `minHeight` and share it equally.
        bool                crowded(double minHeight) const;

        // `row` made `by` taller (shorter if negative); the others give way
        // in proportion. Stops when `row` or all the others reach
        // `minHeight`. Returns whether the heights in use changed.
        bool                resize(std::size_t row, double by, double minHeight);

        // `from` with the edge above `row` moved `by` down, changing only the
        // two Rows either side of it and stopping at `minHeight`.
        static std::vector<double> movedEdge(std::vector<double> from, std::size_t row, double by, double minHeight);

        // Keeps `heights` if they differ from the ones in use.
        bool                set(std::vector<double> heights, double minHeight);

      private:
        std::vector<double> m_heights; // sums to 1
    };

}
