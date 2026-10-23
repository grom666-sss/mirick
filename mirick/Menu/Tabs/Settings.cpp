#include "Menu/Menu.hpp"

void Menu::DrawSettings()
{
    ImGui::SetCursorPosX(0);
    ImGui::BeginChild("settings_tab", ImVec2(), false);
    ImGui::Columns(2, NULL, false);
    ImGui::SetColumnOffset(1, 270.0f * GetScale());

    GroupBegin(tr("Интерфейс", "Interface"));
    if (Checkbox(tr("Кейбинды", "Keybinds"), &g_cfg.showbinds))
    {
        SaveGeneralConfig();
    }
    if (Checkbox(tr("Модель персонажа", "Character model"), &g_cfg.show_character_model))
    {
        SaveGeneralConfig();
    }
    GroupEnd();

    ImGui::NextColumn();
    GroupBegin(tr("Язык", "Language"));
    static const std::string languages[] = { "Русский", "English" };
    const char* language_items[] = { languages[0].c_str(), languages[1].c_str() };
    if (Combo(tr("Язык интерфейса", "Interface language"), &g_cfg.language, language_items, 2))
    {
        SaveGeneralConfig();
    }
    GroupEnd();

    ImGui::EndChild(false);
}
