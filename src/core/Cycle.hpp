#pragma once

#include <algorithm>
#include <iterator>
#include <optional>
#include <vector>

namespace threecol {

    // The one after `from` in `order` (`forward`) or before it. Nothing at an
    // end, unless `wrap`: then the far end leads back to the start. Nothing
    // if `from` is alone or isn't there.
    template <typename T>
    std::optional<T> step(const std::vector<T>& order, const T& from, bool forward, bool wrap) {
        const auto it = std::ranges::find(order, from);
        if (it == order.end() || order.size() < 2)
            return std::nullopt;
        if (forward) {
            if (std::next(it) != order.end())
                return *std::next(it);
            return wrap ? std::optional{order.front()} : std::nullopt;
        }
        if (it != order.begin())
            return *std::prev(it);
        return wrap ? std::optional{order.back()} : std::nullopt;
    }

    // The window after `from` on the Cycle (`forward`) or before it: the
    // tiles in Chain order, then the floating windows in Hyprland's order,
    // then back to the start of the Chain. It stops at either end without
    // `wrap`. Nothing if `from` is alone or on neither list.
    template <typename T>
    std::optional<T> cycleStep(const std::vector<T>& chain, const std::vector<T>& floating, const T& from, bool forward, bool wrap) {
        std::vector<T> cycle = chain;
        cycle.insert(cycle.end(), floating.begin(), floating.end());
        return step(cycle, from, forward, wrap);
    }

}
