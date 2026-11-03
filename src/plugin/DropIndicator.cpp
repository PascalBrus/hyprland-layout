#include "plugin/DropIndicator.hpp"

#include "plugin/ThreecolAlgorithm.hpp"

#include <hyprland/src/config/ConfigValue.hpp>
#include <hyprland/src/config/shared/complex/ComplexDataTypes.hpp>
#include <hyprland/src/desktop/view/window/Window.hpp>
#include <hyprland/src/event/EventBus.hpp>
#include <hyprland/src/layout/LayoutManager.hpp>
#include <hyprland/src/layout/supplementary/DragController.hpp>
#include <hyprland/src/managers/eventLoop/EventLoopManager.hpp>
#include <hyprland/src/output/Monitor.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/render/pass/BorderPassElement.hpp>
#include <hyprland/src/render/pass/RectPassElement.hpp>

#include <cmath>
#include <optional>
#include <utility>

namespace threecol {

    namespace {

        // The fill, as a share of the border colour's alpha.
        constexpr float               FILL_ALPHA = 0.2F;

        std::optional<SDropIndicator> g_shown; // what the last frame drew
        CHyprSignalListener           g_onPreChecks;
        CHyprSignalListener           g_onRenderStage;
        CHyprSignalListener           g_onDragEnded;
        UP<SEventLoopDoLaterLock>     g_pendingUpdate;

        int                           borderSize() {
            static auto PBORDERSIZE = CConfigValue<Config::INTEGER>("general:border_size");
            return *PBORDERSIZE;
        }

        void damage(const std::optional<SDropIndicator>& indicator) {
            // One more pixel for the border's anti-aliasing.
            if (indicator && g_pHyprRenderer)
                g_pHyprRenderer->damageBox(indicator->box.copy().expand(borderSize() + 1));
        }

        // Follows the cursor: what moved is drawn again.
        void update() {
            const auto now = currentDropIndicator();
            if (now == g_shown)
                return;
            damage(g_shown);
            damage(now);
            g_shown = now;
        }

        // Just before the dragged window is drawn, so it sits above the tiles
        // and below the window. Hyprland draws floating windows, the dragged
        // one among them, after every tile.
        void draw(const Event::SRenderStageEvent& event) {
            if (event.stage != RENDER_PRE_WINDOW || !event.context || !g_shown)
                return;
            auto&      ctx     = event.context->get();
            const auto monitor = g_shown->monitor.lock();
            if (!monitor || event.monitor != monitor || ctx.m_data.currentWindow.lock() != g_shown->window.lock())
                return;

            static auto PACTIVECOL     = CConfigValue<Config::IComplexConfigValue>("general:col.active_border");
            static auto PROUNDING      = CConfigValue<Config::INTEGER>("decoration:rounding");
            static auto PROUNDINGPOWER = CConfigValue<Config::FLOAT>("decoration:rounding_power");
            const auto* gradient       = sc<Config::CGradientValueData*>(PACTIVECOL.ptr());
            const int   border         = borderSize();
            const float power          = *PROUNDINGPOWER;
            const auto  box            = g_shown->box.copy().translate(-monitor->m_position).scale(monitor->m_scale).round();
            if (box.width < 1 || box.height < 1)
                return;

            if (!gradient->m_colors.empty()) {
                CHyprColor fill = gradient->m_colors.front();
                fill.a *= FILL_ALPHA;
                CRectPassElement::SRectData rect;
                rect.box           = box;
                rect.color         = fill;
                rect.round         = std::round(*PROUNDING * monitor->m_scale);
                rect.roundingPower = power;
                g_pHyprRenderer->addPassElement(ctx, makeUnique<CRectPassElement>(rect));
            }

            if (border > 0) {
                // The outer rounding as Hyprland's window borders work it out.
                const double correction = border * (M_SQRT2 - 1) * std::max(2.0 - power, 0.0);
                CBorderPassElement::SBorderData data;
                data.box           = box;
                data.grad1         = *gradient;
                data.round         = std::round(*PROUNDING * monitor->m_scale);
                data.outerRound    = std::round((*PROUNDING + border - correction) * monitor->m_scale);
                data.roundingPower = power;
                data.borderSize    = border;
                g_pHyprRenderer->addPassElement(ctx, makeUnique<CBorderPassElement>(data));
            }
        }

    }

    void startDropIndicator() {
        g_onPreChecks   = Event::bus()->m_events.render.preChecks.listen([](PHLMONITOR) { update(); });
        g_onRenderStage = Event::bus()->m_events.render.stage.listen([](const Event::SRenderStageEvent& event) { draw(event); });
        // Released with no frame to follow, the indicator still goes. Later,
        // once the layout has seen the release too.
        g_onDragEnded = g_layoutManager->dragController()->m_events.ended.listen([] {
            if (!g_pendingUpdate && g_pEventLoopManager)
                g_pendingUpdate = g_pEventLoopManager->doLaterLock([] {
                    g_pendingUpdate.reset();
                    update();
                });
        });
    }

    void stopDropIndicator() {
        g_onPreChecks.reset();
        g_onRenderStage.reset();
        g_onDragEnded.reset();
        g_pendingUpdate.reset();
        damage(std::exchange(g_shown, std::nullopt));
    }

}
