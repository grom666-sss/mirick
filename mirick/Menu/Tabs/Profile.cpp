#include "Menu/Menu.hpp"

#include <shellapi.h>

#include "Lua/LuaManager.hpp"

void Menu::DrawProfile()
{
        static int sub4 = 0;
        static bool cfg_scanned = false;

        if (!cfg_scanned)
        {
            config_t::RefreshList();
            cfg_scanned = true;
        }

        std::vector<std::string> items = { tr("Конфиг", "Config"), tr("Настройки", "Settings"), "Lua" };
        DrawSubTabs(sub4, items);
        UpdateSubFade(4, sub4);
        ImGui::SetCursorPosX(0);
        ImGui::BeginChild("subtab_cfg", ImVec2(), false);
        ImGui::Columns(2, NULL, false);
        ImGui::SetColumnOffset(1, 270.0f * GetScale());

        if (sub4 == 0)
        {
            const bool wrong_config = g_cfg.cfg_list.empty() || g_cfg.cfg_selected < 0 || g_cfg.cfg_selected >= (int)g_cfg.cfg_list.size();
            const char* selected_cfg = wrong_config ? nullptr : g_cfg.cfg_list[g_cfg.cfg_selected].c_str();

            GroupBegin(tr("Общее", "General"));
            Textbox(tr("Имя конфига", "Config name"), g_cfg.cfg_name, sizeof(g_cfg.cfg_name));
            if (Button(tr("Создать", "Create")))
            {
                if (g_cfg.cfg_name[0] != 0)
                {
                    bool exists = false;

                    for (const auto& name : g_cfg.cfg_list)
                    {
                        if (name == g_cfg.cfg_name)
                        {
                            exists = true;
                            break;
                        }
                    }

                    if (!exists)
                    {
                        g_cfg.cfg_list.emplace_back(g_cfg.cfg_name);
                        g_cfg.cfg_selected = (int)g_cfg.cfg_list.size() - 1;
                        g_cfg.Save(g_cfg.cfg_name);
                        g_cfg.cfg_name[0] = 0;
                    }
                }
            }
            if (Button(tr("Обновить", "Refresh")))
            {
                config_t::RefreshList();
            }
            if (Button(tr("Папка конфигов", "Configs folder")))
            {
                const std::string folder = config_t::ConfigDir();
                CreateDirectoryA(folder.c_str(), nullptr);

                if ((INT_PTR)ShellExecuteA(NULL, "open", folder.c_str(), NULL, NULL, SW_SHOWNORMAL) <= 32)
                {
                    MessageBoxA(NULL, tr("Не удалось открыть папку конфигов", "Failed to open configs folder"), "mirick", MB_OK | MB_ICONERROR);
                }
            }
            GroupEnd();

            std::string config_title = std::string(tr("Конфиг: ", "Config: ")) + (wrong_config ? tr("Нет", "None") : selected_cfg);
            GroupBegin(config_title.c_str());
            if (Button(tr("Загрузить", "Load")) && !wrong_config)
            {
                g_cfg.Load(selected_cfg);
            }
            if (Button(tr("Сохранить", "Save")) && !wrong_config)
            {
                g_cfg.Save(selected_cfg);
            }
            if (Button(tr("Сбросить", "Reset")) && !wrong_config)
            {
                std::string target = selected_cfg;
                const bool was_open = g_cfg.menu_open;
                std::vector<std::string> saved_list = g_cfg.cfg_list;
                const int saved_selected = g_cfg.cfg_selected;
                g_cfg = config_t();
                g_cfg.menu_open = was_open;
                g_cfg.cfg_list = saved_list;
                g_cfg.cfg_selected = saved_selected;
                g_cfg.Save(target);
            }
            if (Button(tr("Удалить", "Delete")) && !wrong_config)
            {
                g_cfg.Remove(selected_cfg);
            }
            GroupEnd();
            ImGui::NextColumn();

            std::string list_title = std::string(tr("Конфигов | ", "Configs | ")) + std::to_string((int)g_cfg.cfg_list.size()) + tr(" всего", " total");
            GroupBegin(list_title.c_str());
            Textbox(tr("Поиск", "Search"), g_cfg.cfg_search, sizeof(g_cfg.cfg_search));
            Listbox("configlist___", &g_cfg.cfg_selected, g_cfg.cfg_list, 12, g_cfg.cfg_search);
            GroupEnd();
        }
        else if (sub4 == 1)
        {
            GroupBegin(tr("Интерфейс", "Interface"));
            ColorPicker(tr("Акцент", "Accent"), g_cfg.accent);
            if (Checkbox(tr("Кейбинды", "Keybinds"), &g_cfg.showbinds))
            {
                SaveGeneralConfig();
            }

            static const std::string dpi_s[] = { "75%", "100%", "125%", "150%", "175%", "200%" };
            const char* dpi_items[] = { dpi_s[0].c_str(), dpi_s[1].c_str(), dpi_s[2].c_str(), dpi_s[3].c_str(), dpi_s[4].c_str(), dpi_s[5].c_str() };
            const int dpi_values[] = { 75, 100, 125, 150, 175, 200 };
            int dpi_selected = 1;

    for (int i = 0; i < 6; i++)
            {
                if (dpi_values[i] == g_cfg.ui_scale)
                {
                    dpi_selected = i;
                    break;
                }
            }

            if (Combo(tr("Масштаб DPI", "DPI scale"), &dpi_selected, dpi_items, 6))
            {
                g_cfg.ui_scale = dpi_values[dpi_selected];
            }

            static const std::string lang_s[] = { "Русский", "English" };
            const char* lang_items[] = { lang_s[0].c_str(), lang_s[1].c_str() };
            if (Combo(tr("Язык", "Language"), &g_cfg.language, lang_items, 2))
            {
                SaveGeneralConfig();
            }
            GroupEnd();
        }
        else if (sub4 == 2)
        {
            ImGui::Columns(1, NULL, false);
            static std::array<char, 32768> lua_code{};

            GroupBegin(tr("Редактор Lua", "Lua editor"));
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.055f, 0.065f, 0.08f, GetAlpha()));
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.22f, 0.24f, 0.29f, GetAlpha()));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f * GetScale());
            ImGui::InputTextMultiline(
                "##lua_editor",
                lua_code.data(),
                lua_code.size(),
                ImVec2(-1.0f, 245.0f * GetScale()),
                ImGuiInputTextFlags_AllowTabInput);
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(2);

            if (Button(tr("Выполнить", "Execute")))
            {
                LuaManager::Instance().Execute(lua_code.data());
            }
            if (Button(tr("Очистить редактор", "Clear editor")))
            {
                lua_code.fill(0);
            }
            GroupEnd();

            GroupBegin(tr("Управление", "Management"));
            if (Button(tr("Перезагрузить файлы Lua", "Reload Lua files")))
            {
                LuaManager::Instance().Reload();
            }
            if (Button(tr("Открыть папку скриптов", "Open scripts folder")))
            {
                const std::string folder = config_t::ConfigDir() + "\\scripts";
                CreateDirectoryA(folder.c_str(), nullptr);
                ShellExecuteA(NULL, "open", folder.c_str(), NULL, NULL, SW_SHOWNORMAL);
            }
            if (Button(tr("Открыть журнал Lua", "Open Lua log")))
            {
                const std::string log = config_t::ConfigDir() + "\\lua.log";
                ShellExecuteA(NULL, "open", log.c_str(), NULL, NULL, SW_SHOWNORMAL);
            }

            const std::string& status = LuaManager::Instance().GetLastMessage();
            if (!status.empty())
            {
                ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x);
                ImGui::TextUnformatted(status.c_str());
                ImGui::PopTextWrapPos();
            }
            GroupEnd();
        }

        ImGui::EndChild(false);
}
