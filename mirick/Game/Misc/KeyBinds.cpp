#include "Game/Features.h"

#include "Game/Misc/KeyBinds.hpp"
#include "Menu/Menu.hpp"

#include <map>

namespace
{
    void ProcessBind(keybind_t& bind, bool& value, const char* label)
    {
        if (bind.key < 0)
        {
            return;
        }

        const bool down = (GetAsyncKeyState(bind.key) & 0x8000) != 0;

        if (bind.mode == 1)
        {
            if (value != down)
            {
                value = down;
                menu->PlaySoundFeedback(value ? 1u : 2u, value);
                menu->PushNotification(label, value);
            }
            return;
        }

        // Track each bind separately. Two features are allowed to use the same
        // key without the first processed bind consuming the edge for all the
        // others.
        static std::map<keybind_t*, bool> previousState;
        const bool wasDown = previousState[&bind];

        if (down && !wasDown)
        {
            value = !value;
            menu->PlaySoundFeedback(value ? 1u : 2u, value);
            menu->PushNotification(label, value);
        }

        previousState[&bind] = down;
    }
}

void KeyBinds::Update()
{
    ProcessBind(g_cfg.godmode_bind, g_cfg.godmode, tr("Годмод", "Godmode"));
    ProcessBind(g_cfg.nofall_bind, g_cfg.nofall, tr("Без урона от падения", "No fall damage"));
    ProcessBind(g_cfg.wh_bind, g_cfg.wh, tr("Валлхак", "Wallhack"));
    ProcessBind(g_cfg.airbreake_bind, g_cfg.airbreake, tr("Аирбрейк", "Airbreak"));
    ProcessBind(g_cfg.nightmode_bind, g_cfg.nightmode, tr("Найтмод", "Nightmode"));
    ProcessBind(g_cfg.customtime_bind, g_cfg.customtime, tr("Своё время", "Custom time"));
    ProcessBind(g_cfg.freezetime_bind, g_cfg.freezetime, tr("Заморозить время", "Freeze time"));
    ProcessBind(g_cfg.suncolor_bind, g_cfg.suncolor, tr("Солнце", "Sun"));
    ProcessBind(g_cfg.customcolor_bind, g_cfg.customcolor, tr("Свет и вода", "Light & water"));
    ProcessBind(g_cfg.fullbright_bind, g_cfg.fullbright, tr("Фулбрайт", "Fullbright"));
    ProcessBind(g_cfg.rapidfire_bind, g_cfg.rapidfire, tr("Рапид", "Rapid fire"));
    ProcessBind(g_cfg.infammo_bind, g_cfg.infammo, tr("Бесконечные патроны", "Infinite ammo"));
    ProcessBind(g_cfg.fastcross_bind, g_cfg.fastcross, tr("Быстрый прицел", "Fast crosshair"));
    ProcessBind(g_cfg.norecoil_bind, g_cfg.norecoil, tr("Без отдачи", "No recoil"));
    ProcessBind(g_cfg.nospread_bind, g_cfg.nospread, tr("Без разброса", "No spread"));
    ProcessBind(g_cfg.trigger_bind, g_cfg.trigger, tr("Триггербот", "Triggerbot"));
    ProcessBind(g_cfg.aspect_bind, g_cfg.aspect, tr("Аспект", "Aspect"));
    ProcessBind(g_cfg.fov_bind, g_cfg.fov, tr("Фов", "Fov"));
    ProcessBind(g_cfg.removals_bind, g_cfg.removals, tr("Чистка мира", "World removals"));
    ProcessBind(g_cfg.skychange_bind, g_cfg.skychange, tr("Небо", "Sky"));
}
