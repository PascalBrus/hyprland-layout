#include "plugin/DispatcherHooks.hpp"

#include "plugin/Config.hpp"
#include "plugin/ThreecolAlgorithm.hpp"

#include <hyprland/src/config/shared/actions/ConfigActions.hpp>
#include <hyprland/src/debug/log/Logger.hpp>
#include <hyprland/src/desktop/state/FocusState.hpp>
#include <hyprland/src/desktop/view/window/Window.hpp>
#include <hyprland/src/layout/LayoutManager.hpp>
#include <hyprland/src/layout/target/Target.hpp>
#include <hyprland/src/plugins/HookSystem.hpp>

#include <format>
#include <optional>
#include <string>
#include <vector>

// Hyprland 0.57 never asks the layout about direction focus, a swap's
// partner or cycle_next (research/intercepting-hyprland-binds.md), so the
// four Config::Actions functions are hooked. Each hook calls the original
// through its trampoline wherever threecol has nothing to do.

namespace threecol {

    namespace {

        using Config::Actions::ActionResult;

        using FnMoveFocus       = ActionResult (*)(Math::eDirection);
        using FnSwapInDirection = ActionResult (*)(Math::eDirection, std::optional<PHLWINDOW>);
        using FnSwapNext        = ActionResult (*)(bool, std::optional<PHLWINDOW>);
        using FnCycleNext       = ActionResult (*)(bool, std::optional<bool>, std::optional<bool>, std::optional<PHLWINDOW>);

        CFunctionHook* g_moveFocus       = nullptr;
        CFunctionHook* g_swapInDirection = nullptr;
        CFunctionHook* g_swapNext        = nullptr;
        CFunctionHook* g_cycleNext       = nullptr;

        std::optional<Direction> toDirection(Math::eDirection dir) {
            switch (dir) {
                case Math::DIRECTION_LEFT: return Direction::Left;
                case Math::DIRECTION_RIGHT: return Direction::Right;
                case Math::DIRECTION_UP: return Direction::Up;
                case Math::DIRECTION_DOWN: return Direction::Down;
                default: return std::nullopt;
            }
        }

        // The window a dispatcher acts on: the one it names, else the focused one.
        PHLWINDOW windowFor(const std::optional<PHLWINDOW>& window) {
            return window ? *window : Desktop::focusState()->window();
        }

        // Focus `window` as a bind does.
        ActionResult focusWindow(const PHLWINDOW& window) {
            return Config::Actions::focus(window);
        }

        // focus({ direction }): left / right walk the Chain, up / down the
        // Rows of a side zone. Where threecol can't go further, Hyprland's.
        ActionResult hkMoveFocus(Math::eDirection dir) {
            const auto window    = Desktop::focusState()->window();
            const auto direction = toDirection(dir);
            if (direction && bindSwitches().focus[static_cast<std::size_t>(*direction)]) {
                if (const auto* algo = CThreecolAlgorithm::tiling(window)) {
                    if (const auto to = algo->inDirection(window, *direction))
                        return focusWindow(to);
                }
            }
            return reinterpret_cast<FnMoveFocus>(g_moveFocus->m_original)(dir);
        }

        // window.swap({ direction }): swaps with the window focus in that
        // direction would go to, as Hyprland's does with its own pick
        // (ConfigActions.cpp:575-593 at 0.57.1).
        ActionResult hkSwapInDirection(Math::eDirection dir, std::optional<PHLWINDOW> w) {
            const auto window    = windowFor(w);
            const auto direction = toDirection(dir);
            if (direction && bindSwitches().swap[static_cast<std::size_t>(*direction)]) {
                if (const auto* algo = CThreecolAlgorithm::tiling(window)) {
                    if (const auto to = algo->inDirection(window, *direction)) {
                        g_layoutManager->switchTargets(window->layoutTarget(), to->layoutTarget(), true);
                        window->warpCursor();
                        return {};
                    }
                }
            }
            return reinterpret_cast<FnSwapInDirection>(g_swapInDirection->m_original)(dir, std::move(w));
        }

        // window.swap({ next | prev }): swaps with the next or previous window
        // on the Chain, focus staying on the swapped one, as Hyprland's does
        // with its own order (ConfigActions.cpp:742-763). It loops unless
        // wrap.swap_next is off; then at the Chain's end it does nothing, as
        // with a lone tile: it never hands over to Hyprland.
        ActionResult hkSwapNext(bool next, std::optional<PHLWINDOW> w) {
            const auto window   = windowFor(w);
            const auto switches = bindSwitches();
            if (switches.swapNext) {
                if (const auto* algo = CThreecolAlgorithm::tiling(window)) {
                    if (const auto to = algo->next(window, next, switches.wrap.swapNext)) {
                        g_layoutManager->switchTargets(window->layoutTarget(), to->layoutTarget(), false);
                        Desktop::focusState()->fullWindowFocus(window, Desktop::FOCUS_REASON_KEYBIND);
                    }
                    return {};
                }
            }
            return reinterpret_cast<FnSwapNext>(g_swapNext->m_original)(next, std::move(w));
        }

        // window.cycle_next(): one step along the Cycle, from a tile or a
        // floating window of a threecol workspace. It loops unless
        // wrap.cycle_next is off; then at the Cycle's end it does nothing, as
        // with a lone window: it never hands over to Hyprland. With only
        // `tiled`, the Chain alone, and from a floating window Hyprland's.
        // With only `floating`, Hyprland's. Both at once count as neither, as
        // Hyprland reads them (ConfigActions.cpp:1789-1792).
        ActionResult hkCycleNext(bool next, std::optional<bool> onlyTiled, std::optional<bool> onlyFloating, std::optional<PHLWINDOW> w) {
            const auto window       = windowFor(w);
            const auto switches     = bindSwitches();
            const bool tiledOnly    = onlyTiled.value_or(false) && !onlyFloating.value_or(false);
            const bool floatingOnly = onlyFloating.value_or(false) && !onlyTiled.value_or(false);
            if (switches.cycleNext && !floatingOnly && window && !(tiledOnly && window->isFloating())) {
                if (const auto* algo = CThreecolAlgorithm::onWorkspace(window)) {
                    if (const auto to = algo->cycleNext(window, next, switches.wrap.cycleNext, tiledOnly))
                        return focusWindow(to);
                    return {};
                }
            }
            return reinterpret_cast<FnCycleNext>(g_cycleNext->m_original)(next, onlyTiled, onlyFloating, std::move(w));
        }

        // Hooks the function whose demangled name is `demangled`. Returns the
        // hook, or nullptr if it isn't there or couldn't be hooked (another
        // plugin hooked it, or this isn't x86_64).
        CFunctionHook* hook(HANDLE handle, const std::string& name, const std::string& demangled, void* destination) {
            for (const auto& match : HyprlandAPI::findFunctionsByName(handle, name)) {
                if (match.demangled != demangled)
                    continue;
                auto* const hook = HyprlandAPI::createFunctionHook(handle, match.address, destination);
                if (hook && hook->hook())
                    return hook;
                if (hook)
                    HyprlandAPI::removeFunctionHook(handle, hook);
                return nullptr;
            }
            return nullptr;
        }

        const CHyprColor WARNING_COLOR{1.0, 0.6, 0.2, 1.0};

    }

    void installDispatcherHooks(HANDLE handle) {
        constexpr auto WINDOW = "std::optional<Hyprutils::Memory::CSharedPointer<Desktop::View::CWindow> >";

        struct SWanted {
            CFunctionHook*& hook;
            const char*     name;
            std::string     demangled;
            void*           destination;
            const char*     dispatcher;
        };
        const std::vector<SWanted> wanted{
            {g_moveFocus, "moveFocus", "Config::Actions::moveFocus(Math::eDirection)", reinterpret_cast<void*>(&hkMoveFocus), "focus({ direction })"},
            {g_swapInDirection, "swapInDirection", std::format("Config::Actions::swapInDirection(Math::eDirection, {})", WINDOW), reinterpret_cast<void*>(&hkSwapInDirection),
             "window.swap({ direction })"},
            {g_swapNext, "swapNext", std::format("Config::Actions::swapNext(bool, {})", WINDOW), reinterpret_cast<void*>(&hkSwapNext), "window.swap({ next | prev })"},
            {g_cycleNext, "cycleNext", std::format("Config::Actions::cycleNext(bool, std::optional<bool>, std::optional<bool>, {})", WINDOW),
             reinterpret_cast<void*>(&hkCycleNext), "window.cycle_next()"},
        };

        std::vector<std::string> failed;
        for (const auto& want : wanted) {
            want.hook = hook(handle, want.name, want.demangled, want.destination);
            if (!want.hook) {
                LOG(Log::ERR, "[threecol] could not hook {}", want.demangled);
                failed.emplace_back(want.dispatcher);
            }
        }
        if (failed.empty())
            return;

        std::string list;
        for (const auto& dispatcher : failed)
            list += (list.empty() ? "" : ", ") + dispatcher;
        HyprlandAPI::addNotification(handle, std::format("[threecol] {} keep Hyprland's behaviour: threecol could not hook them (x86_64 only, one plugin per hook)", list),
                                     WARNING_COLOR, 10000);
    }

}
