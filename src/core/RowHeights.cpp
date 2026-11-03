#include "core/RowHeights.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <utility>

namespace threecol {

    namespace {

        // Height sums pick up rounding error; differences below this aren't moves.
        constexpr double EPSILON = 1e-9;

        double sum(const std::vector<double>& heights) {
            return std::accumulate(heights.begin(), heights.end(), 0.0);
        }

        bool same(const std::vector<double>& a, const std::vector<double>& b) {
            return std::ranges::equal(a, b, [](double x, double y) { return std::abs(x - y) < EPSILON; });
        }

        // `heights` scaled to share `total`, each at least `minHeight`: a
        // height that would fall below it stays at it, and the rest share
        // what is left in proportion. All equal if `total` can't fit them.
        std::vector<double> fit(const std::vector<double>& heights, double total, double minHeight) {
            const auto n = heights.size();
            if (n == 0)
                return {};
            if (n * minHeight > total + EPSILON)
                return std::vector<double>(n, total / n);

            std::vector<bool> atMin(n, false);
            std::vector<double> result(n);
            while (true) {
                double free = total;
                double share = 0;
                for (std::size_t i = 0; i < n; ++i) {
                    if (atMin[i])
                        free -= minHeight;
                    else
                        share += heights[i];
                }
                bool clamped = false;
                for (std::size_t i = 0; i < n; ++i) {
                    if (atMin[i]) {
                        result[i] = minHeight;
                        continue;
                    }
                    result[i] = share > 0 ? heights[i] * free / share : free / std::ranges::count(atMin, false);
                    if (result[i] < minHeight - EPSILON) {
                        atMin[i] = true;
                        clamped  = true;
                    }
                }
                if (!clamped)
                    return result;
            }
        }

    }

    std::size_t RowHeights::count() const {
        return m_heights.size();
    }

    void RowHeights::add() {
        insert(m_heights.size());
    }

    void RowHeights::insert(std::size_t row) {
        const double n = m_heights.size() + 1;
        for (double& height : m_heights)
            height *= (n - 1) / n;
        m_heights.insert(m_heights.begin() + std::min(row, m_heights.size()), 1 / n);
    }

    void RowHeights::remove(std::size_t row) {
        if (row >= m_heights.size())
            return;
        m_heights.erase(m_heights.begin() + row);
        if (const double total = sum(m_heights); total > 0)
            for (double& height : m_heights)
                height /= total;
    }

    bool RowHeights::equalise(double minHeight) {
        const auto before = fitted(minHeight);
        std::ranges::fill(m_heights, 1.0 / m_heights.size());
        return !same(before, fitted(minHeight));
    }

    std::vector<double> RowHeights::fitted(double minHeight) const {
        return fit(m_heights, 1, minHeight);
    }

    bool RowHeights::crowded(double minHeight) const {
        return m_heights.size() * minHeight > 1 + EPSILON;
    }

    bool RowHeights::resize(std::size_t row, double by, double minHeight) {
        const auto n = m_heights.size();
        if (row >= n || n < 2 || crowded(minHeight))
            return false;

        std::vector<double> heights = fitted(minHeight);
        const double        height  = heights[row];
        // Taller takes only what the others have above minHeight; shorter
        // stops at minHeight itself.
        const double others = 1 - height;
        const double grown  = by > 0 ? height + std::max(0.0, std::min(by, others - (n - 1) * minHeight)) : height - std::max(0.0, std::min(-by, height - minHeight));

        std::vector<double> rest = heights;
        rest.erase(rest.begin() + row);
        rest = fit(rest, 1 - grown, minHeight);
        rest.insert(rest.begin() + row, grown);
        return set(std::move(rest), minHeight);
    }

    std::vector<double> RowHeights::movedEdge(std::vector<double> from, std::size_t row, double by, double minHeight) {
        if (row == 0 || row >= from.size())
            return from;
        double&      above = from[row - 1];
        double&      below = from[row];
        const double both  = above + below;
        // Too small to share: neither may shrink further.
        if (both < 2 * minHeight - EPSILON)
            return from;
        above = std::max(minHeight, std::min(above + by, both - minHeight));
        below = both - above;
        return from;
    }

    bool RowHeights::set(std::vector<double> heights, double minHeight) {
        if (heights.size() != m_heights.size() || same(heights, fitted(minHeight)))
            return false;
        m_heights = std::move(heights);
        return true;
    }

}
