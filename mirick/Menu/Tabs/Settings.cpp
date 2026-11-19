#include "Menu/Menu.hpp"

void Menu::DrawSettings()
{
    static int sub = 0;
    std::vector<std::string> tabs = { tr("Основные", "General"), tr("Звуки", "Sounds") };
    DrawSubTabs(sub, tabs);
    UpdateSubFade(4, sub);

    ImGui::SetCursorPosX(0);
    ImGui::BeginChild("settings_tab", ImVec2(), false);
    ImGui::Columns(2, NULL, false);
    ImGui::SetColumnOffset(1, 300.0f * GetScale());

    if (sub == 0)
    {
        GroupBegin(tr("Интерфейс", "Interface"));
        if (Checkbox(tr("Кейбинды", "Keybinds"), &g_cfg.showbinds))
        {
            SaveGeneralConfig();
        }
        if (Checkbox(tr("Модель персонажа", "Character model"), &g_cfg.show_character_model))
        {
            SaveGeneralConfig();
        }
        if (Checkbox(tr("Уведомления", "Notifications"), &g_cfg.ui_notifications))
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
    }
    else
    {
        GroupBegin(tr("Звуки интерфейса", "Interface sounds"));
        if (Checkbox(tr("Звуки интерфейса", "Interface sounds"), &g_cfg.ui_sounds))
        {
            SaveGeneralConfig();
        }

        if (g_cfg.ui_sounds)
        {
            std::vector<std::string> events = {
                tr("Включение функций", "Functions enabled"),
                tr("Выключение функций", "Functions disabled"),
                tr("Открытие и закрытие меню", "Menu open and close"),
                tr("Переключение модулей", "Module switching")
            };
            const unsigned int previous = g_cfg.sound_flags;
            MultiCombo(tr("События со звуком", "Sound events"), g_cfg.sound_flags, events);
            if (previous != g_cfg.sound_flags)
            {
                SaveGeneralConfig();
            }
        }
        GroupEnd();

        ImGui::NextColumn();
        GroupBegin(tr("Звук для события", "Sound per event"));
        static const std::string presets[] = {
            tr("Мягкий", "Soft"),
            tr("Контрастный", "Contrast"),
            tr("Системный", "System"),
            "iOS 27"
        };
        const char* preset_items[] = {
            presets[0].c_str(), presets[1].c_str(), presets[2].c_str(), presets[3].c_str()
        };
        auto soundCombo = [&](unsigned int flag, const char* label, int& style, bool enabledSound)
        {
            if ((g_cfg.sound_flags & flag) == 0)
            {
                return;
            }
            if (Combo(label, &style, preset_items, 4))
            {
                SaveGeneralConfig();
                PlaySoundPreset(style, enabledSound);
            }
        };

        soundCombo(1, tr("Включение функций", "Functions enabled"), g_cfg.sound_enable_style, true);
        soundCombo(2, tr("Выключение функций", "Functions disabled"), g_cfg.sound_disable_style, false);
        soundCombo(4, tr("Открытие меню", "Menu open"), g_cfg.sound_menu_style, true);
        soundCombo(8, tr("Переключение модулей", "Module switching"), g_cfg.sound_tab_style, true);

        if (Button(tr("Проверить звук", "Test sound")))
        {
            PlaySoundPreset(g_cfg.sound_enable_style, true);
        }
        GroupEnd();
    }

    ImGui::EndChild(false);
}
