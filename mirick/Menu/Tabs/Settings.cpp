#include "Menu/Menu.hpp"

#include <algorithm>

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
        if (Checkbox(tr("Уведомления", "Notifications"), &g_cfg.ui_notifications))
        {
            SaveGeneralConfig();
        }

        static const char* effects_ru[] = { "Снежинки", "Кристаллы", "Маленькие молнии" };
        static const char* effects_en[] = { "Snowflakes", "Crystals", "Small lightning" };
        const char* const* effect_items = g_cfg.language == 1 ? effects_en : effects_ru;
        if (Combo(tr("Эффект фона", "Background effect"), &g_cfg.backdrop_effect, effect_items, 3))
        {
            SaveGeneralConfig();
        }
        GroupEnd();

        ImGui::NextColumn();
        GroupBegin(tr("Оформление", "Appearance"));
        if (ColorPicker(tr("Цвет меню", "Menu color"), g_cfg.menu_color, no_alpha_picker))
        {
            SaveGeneralConfig();
        }
        if (ColorPicker(tr("Цвет акцента", "Accent color"), g_cfg.accent, no_alpha_picker))
        {
            SaveGeneralConfig();
        }

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
        static int selected_event = 0;
        selected_event = std::clamp(selected_event, 0, 3);

        GroupBegin(tr("События со звуком", "Sound events"));
        if (Checkbox(tr("Звуки интерфейса", "Interface sounds"), &g_cfg.ui_sounds))
        {
            SaveGeneralConfig();
        }

        static const char* events_ru[] = {
            "Включение функций",
            "Выключение функций",
            "Открытие меню",
            "Смена модулей"
        };
        static const char* events_en[] = {
            "Functions enabled",
            "Functions disabled",
            "Menu open and close",
            "Module switching"
        };
        const char* const* event_items = g_cfg.language == 1 ? events_en : events_ru;
        Combo("##sound_event", &selected_event, event_items, 4);
        GroupEnd();

        ImGui::NextColumn();
        GroupBegin(tr("Звук для события", "Sound for event"));
        static const std::string presets[] = {
            tr("Мягкий", "Soft"),
            tr("Контрастный", "Contrast"),
            tr("Системный", "System"),
            "iOS 27"
        };
        const char* preset_items[] = {
            presets[0].c_str(), presets[1].c_str(), presets[2].c_str(), presets[3].c_str()
        };
        int* styles[] = {
            &g_cfg.sound_enable_style,
            &g_cfg.sound_disable_style,
            &g_cfg.sound_menu_style,
            &g_cfg.sound_tab_style
        };
        const bool enabled_preview[] = { true, false, true, true };
        if (Combo("##event_sound", styles[selected_event], preset_items, 4))
        {
            SaveGeneralConfig();
            PlaySoundPreset(*styles[selected_event], enabled_preview[selected_event]);
        }

        if (Button(tr("Проверить звук", "Test sound")))
        {
            PlaySoundPreset(*styles[selected_event], enabled_preview[selected_event]);
        }
        GroupEnd();
    }

    ImGui::EndChild(false);
}
