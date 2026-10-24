#include "Menu/Menu.hpp"

#include "Game/Features.h"

void Menu::DrawMisc()
{
        ImGui::SetCursorPosX(0);
        ImGui::BeginChild("misc_player", ImVec2(), false);
        ImGui::Columns(2, NULL, false);
        ImGui::SetColumnOffset(1, 270.0f * GetScale());

        GroupBegin(tr("Игрок", "Player"));
        BindableCheckbox("airbreak", tr("Аирбрейк", "Airbreak"), &g_cfg.airbreake, &g_cfg.airbreake_bind);
        if (g_cfg.airbreake)
        {
            if (SliderFloat(tr("Скорость полёта", "Flight speed"), &g_cfg.airbreake_speed, 0.05f, 2.0f, "%.2f"))
            {
                SaveGeneralConfig();
            }
            ImGui::TextDisabled("%s", tr("W/S вперёд-назад, A/D влево-вправо", "W/S forward-back, A/D left-right"));
            ImGui::TextDisabled("%s", tr("SPACE вверх, SHIFT вниз", "SPACE up, SHIFT down"));
        }
        GroupEnd();

        ImGui::NextColumn();
        GroupBegin(tr("Защита", "Protection"));
        BindableCheckbox("godmode", tr("Годмод", "Godmode"), &g_cfg.godmode, &g_cfg.godmode_bind);
        if (Checkbox(tr("Случайный годмод", "Random godmode"), &g_cfg.godmode_random))
        {
            SaveGeneralConfig();
        }
        if (g_cfg.godmode_random)
        {
            if (SliderInt(tr("Шанс защиты", "Protection chance"), &g_cfg.godmode_chance, 0, 100, "%d%%"))
            {
                SaveGeneralConfig();
            }
        }
        BindableCheckbox("nofall", tr("Без урона от падения", "No fall damage"), &g_cfg.nofall, &g_cfg.nofall_bind);
        GroupEnd();

        ImGui::EndChild(false);
}
