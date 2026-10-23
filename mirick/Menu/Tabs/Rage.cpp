#include "Menu/Menu.hpp"

#include "Game/Features.h"

void Menu::DrawRage()
{
        ImGui::SetCursorPosX(0);
        ImGui::BeginChild("subtab_rage", ImVec2(), false);
        ImGui::Columns(2, NULL, false);
        ImGui::SetColumnOffset(1, 270.0f * GetScale());
        GroupBegin(tr("Оружие", "Weapon"));
        BindableCheckbox("rapidfire", tr("Рапид", "Rapid fire"), &g_cfg.rapidfire, &g_cfg.rapidfire_bind);
        BindableCheckbox("infammo", tr("Бесконечные патроны", "Infinite ammo"), &g_cfg.infammo, &g_cfg.infammo_bind);
        BindableCheckbox("fastcross", tr("Быстрый прицел", "Fast crosshair"), &g_cfg.fastcross, &g_cfg.fastcross_bind);
        GroupEnd();
        ImGui::NextColumn();
        GroupBegin(tr("Точность", "Accuracy"));
        BindableCheckbox("norecoil", tr("Без отдачи", "No recoil"), &g_cfg.norecoil, &g_cfg.norecoil_bind);
        BindableCheckbox("nospread", tr("Без разброса", "No spread"), &g_cfg.nospread, &g_cfg.nospread_bind);
        GroupEnd();
        ImGui::EndChild(false);
}
