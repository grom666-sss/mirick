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
        GroupEnd();

        ImGui::NextColumn();
        GroupBegin(tr("Защита", "Protection"));
        BindableCheckbox("godmode", tr("Годмод", "Godmode"), &g_cfg.godmode, &g_cfg.godmode_bind);
        BindableCheckbox("nofall", tr("Без урона от падения", "No fall damage"), &g_cfg.nofall, &g_cfg.nofall_bind);
        GroupEnd();

        ImGui::EndChild(false);
}
