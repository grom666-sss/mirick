#include "Menu/Menu.hpp"

#include "Game/Features.h"

#include "CPools.h"
#include "ePedType.h"

#include <algorithm>
#include <cstdio>
#include <vector>

void Menu::DrawVisuals()
{
        static int sub2 = 0;
        static float wh_fade = 0.0f;
        static float time_fade = 0.0f;
        static float cl_fade = 0.0f;
        static float aspect_fade = 0.0f;
        static float fov_fade = 0.0f;

        std::vector<std::string> items = { tr("Противник", "Enemy"), tr("Эффекты", "Effects") };
        DrawSubTabs(sub2, items);
        UpdateSubFade(2, sub2);
        ImGui::SetCursorPosX(0);
        ImGui::BeginChild("subtab_vis", ImVec2(), false);
        ImGui::Columns(2, NULL, false);
        ImGui::SetColumnOffset(1, 270.0f * GetScale());
        if (sub2 == 0)
        {
            GroupBegin(tr("Валлхак", "Wallhack"));
            BindableCheckbox("wallhack", tr("Валлхак", "Wallhack"), &g_cfg.wh, &g_cfg.wh_bind);
            CreateAnimation(wh_fade, g_cfg.wh, 0.3f, AnimLerp);
            if (g_cfg.wh || wh_fade > 0.02f)
            {
                const float prev_mul = widget_alpha_mul;
                widget_alpha_mul = wh_fade;
                if (SliderFloat(tr("Дальность", "Range"), &g_cfg.whDistance, 10.0f, 1000.0f))
                {
                    SaveGeneralConfig();
                }
                widget_alpha_mul = prev_mul;
            }
            GroupEnd();

            GroupBegin(tr("Игроки онлайн", "Online players"));
            if (Checkbox(tr("Показывать список", "Show player list"), &g_cfg.show_player_list))
            {
                SaveGeneralConfig();
            }
            if (g_cfg.show_player_list)
            {
                struct player_row_t
                {
                    int pool_slot;
                    float distance;
                    float health;
                };

                std::vector<player_row_t> players;
                CPed* local = FindPlayerPed();
                if (local && CPools::ms_pPedPool)
                {
                    const CVector local_pos = local->GetPosition();
                    for (int slot = 0; slot < CPools::ms_pPedPool->m_nSize; ++slot)
                    {
                        CPed* ped = CPools::ms_pPedPool->GetAt(slot);
                        if (!ped || ped == local || ped->m_fHealth <= 0.0f)
                        {
                            continue;
                        }

                        // GTA exposes no Next RP nickname/server-ID API here.
                        // Restrict the list to peds explicitly marked as network
                        // players and label them by the local pool slot only.
                        if (ped->m_nPedType < PED_TYPE_PLAYER2
                            || ped->m_nPedType > PED_TYPE_PLAYER_UNUSED)
                        {
                            continue;
                        }

                        const CVector pos = ped->GetPosition();
                        const float distance = VecLength(CVector(
                            pos.x - local_pos.x, pos.y - local_pos.y, pos.z - local_pos.z));
                        players.push_back({ slot, distance, ped->m_fHealth });
                    }
                }

                std::sort(players.begin(), players.end(), [](const player_row_t& a, const player_row_t& b)
                {
                    return a.distance < b.distance;
                });

                const float list_height = 138.0f * GetScale();
                ImGui::BeginChild("##online_player_list", ImVec2(256.0f * GetScale(), list_height), true,
                    ImGuiWindowFlags_AlwaysVerticalScrollbar);
                if (players.empty())
                {
                    ImGui::TextDisabled("%s", tr("Сетевые игроки не обнаружены", "No network players detected"));
                }
                else
                {
                    for (const player_row_t& player : players)
                    {
                        char label[96]{};
                        std::snprintf(label, sizeof(label),
                            tr("Игрок (слот %d)  %.1f м  HP %.0f", "Player (slot %d)  %.1f m  HP %.0f"),
                            player.pool_slot, player.distance, player.health);
                        ImGui::TextUnformatted(label);
                    }
                }
                ImGui::EndChild();
                ImGui::TextDisabled("%s", tr("Ники и server ID недоступны", "Names and server IDs are unavailable"));
            }
            GroupEnd();

            ImGui::NextColumn();
            if (g_cfg.wh || wh_fade > 0.02f)
            {
                GroupBegin(tr("ЕСП", "ESP"));
                {
                    const float prev_mul = widget_alpha_mul;
                    widget_alpha_mul = wh_fade;
                    std::vector<std::string> esp_items = { tr("Бокс", "Box"), tr("Значение ХП", "HP value"), tr("Значение брони", "Armor value"), tr("Дистанция", "Distance"), tr("Скелет", "Skeleton"), tr("Оружие", "Weapon") };
                    const unsigned int prev_flags = g_cfg.wh_flags;
                    MultiCombo(tr("Рисовать", "Draw"), g_cfg.wh_flags, esp_items);
                    if (prev_flags != g_cfg.wh_flags)
                    {
                        SaveGeneralConfig();
                    }
                    if (g_cfg.wh_flags & 1)
                    {
                        if (ColorPicker(tr("Бокс", "Box"), g_cfg.whcol))
                        {
                            SaveGeneralConfig();
                        }
                    }
                    if (g_cfg.wh_flags & 2)
                    {
                        if (ColorPicker(tr("ХП", "HP"), g_cfg.hpcol))
                        {
                            SaveGeneralConfig();
                        }
                    }
                    if (g_cfg.wh_flags & 4)
                    {
                        if (ColorPicker(tr("Броня", "Armor"), g_cfg.armorcol))
                        {
                            SaveGeneralConfig();
                        }
                    }
                    if (g_cfg.wh_flags & 8)
                    {
                        if (ColorPicker(tr("Текст", "Text"), g_cfg.distcol))
                        {
                            SaveGeneralConfig();
                        }
                    }
                    if (g_cfg.wh_flags & 16)
                    {
                        if (ColorPicker(tr("Скелет видимый", "Visible skeleton"), g_cfg.skelcol))
                        {
                            SaveGeneralConfig();
                        }
                        if (ColorPicker(tr("Скелет за стеной", "Occluded skeleton"), g_cfg.skeloccludedcol))
                        {
                            SaveGeneralConfig();
                        }
                    }
                    if (g_cfg.wh_flags & 32)
                    {
                        if (ColorPicker(tr("Оружие", "Weapon"), g_cfg.weaponcol))
                        {
                            SaveGeneralConfig();
                        }
                    }
                    widget_alpha_mul = prev_mul;
                }
                GroupEnd();
            }
        }
        else
        {
            GroupBegin(tr("Время", "Time"));
            BindableCheckbox("nightmode", tr("Найтмод", "Nightmode"), &g_cfg.nightmode, &g_cfg.nightmode_bind);
            BindableCheckbox("customtime", tr("Своё время", "Custom time"), &g_cfg.customtime, &g_cfg.customtime_bind);
            CreateAnimation(time_fade, g_cfg.customtime, 0.3f, AnimLerp);
            if (g_cfg.customtime || time_fade > 0.02f)
            {
                const float prev_mul = widget_alpha_mul;
                widget_alpha_mul = time_fade;
                if (SliderFloat(tr("Час", "Hour"), &g_cfg.timehour, 0.0f, 23.0f, "%.0f"))
                {
                    SaveGeneralConfig();
                }
                BindableCheckbox("freezetime", tr("Заморозить время", "Freeze time"), &g_cfg.freezetime, &g_cfg.freezetime_bind);
                widget_alpha_mul = prev_mul;
            }
            const std::string weather_s[] = { tr("Выкл", "Off"), "XSunny LA", "Sunny LA", "XSunny Smog", "Sunny Smog", "Cloudy LA", "Sunny SF", "XSunny SF", "Cloudy SF", "Rainy SF", "Foggy SF", "Sunny Vegas", "XSunny Vegas", "Cloudy Vegas", "XSun Country", "Sunny Country", "Cloudy Country", "Rainy Country", "XSun Desert", "Sunny Desert", "Sandstorm" };
            const char* weather_items[] = { weather_s[0].c_str(), weather_s[1].c_str(), weather_s[2].c_str(), weather_s[3].c_str(), weather_s[4].c_str(), weather_s[5].c_str(), weather_s[6].c_str(), weather_s[7].c_str(), weather_s[8].c_str(), weather_s[9].c_str(), weather_s[10].c_str(), weather_s[11].c_str(), weather_s[12].c_str(), weather_s[13].c_str(), weather_s[14].c_str(), weather_s[15].c_str(), weather_s[16].c_str(), weather_s[17].c_str(), weather_s[18].c_str(), weather_s[19].c_str(), weather_s[20].c_str() };
            if (Combo(tr("Погода", "Weather"), &g_cfg.weather, weather_items, 21))
            {
                SaveGeneralConfig();
            }
            GroupEnd();
            GroupBegin(tr("Чистка мира", "World Removals"));
            BindableCheckbox("removals", tr("Чистка мира", "World Removals"), &g_cfg.removals, &g_cfg.removals_bind);
            CreateAnimation(cl_fade, g_cfg.removals, 0.3f, AnimLerp);
            if (g_cfg.removals || cl_fade > 0.02f)
            {
                const float prev_mul = widget_alpha_mul;
                widget_alpha_mul = cl_fade;
                std::vector<std::string> rem_items = { tr("Тени", "Shadows"), tr("Солнце", "Sun"), tr("Облака", "Clouds") };
                const unsigned int prev_flags = g_cfg.removal_flags;
                MultiCombo(tr("Удалять", "Remove"), g_cfg.removal_flags, rem_items);
                if (prev_flags != g_cfg.removal_flags)
                {
                    SaveGeneralConfig();
                }
                widget_alpha_mul = prev_mul;
            }
            GroupEnd();
            ImGui::NextColumn();
            GroupBegin(tr("Мир", "World"));
            BindableCheckbox("skychange", tr("Небо", "Sky"), &g_cfg.skychange, &g_cfg.skychange_bind);
            if (g_cfg.skychange)
            {
                if (ColorPicker(tr("Небо верх", "Sky top"), g_cfg.skycol))
                {
                    SaveGeneralConfig();
                }
                if (ColorPicker(tr("Небо низ", "Sky bottom"), g_cfg.skybotcol))
                {
                    SaveGeneralConfig();
                }
            }
            BindableCheckbox("customcolor", tr("Свет и вода", "Light & water"), &g_cfg.customcolor, &g_cfg.customcolor_bind);
            if (g_cfg.customcolor)
            {
                if (ColorPicker(tr("Освещение", "Ambient"), g_cfg.ambcol))
                {
                    SaveGeneralConfig();
                }
                if (ColorPicker(tr("Вода", "Water"), g_cfg.watercol))
                {
                    SaveGeneralConfig();
                }
            }
            BindableCheckbox("fullbright", tr("Фулбрайт", "Fullbright"), &g_cfg.fullbright, &g_cfg.fullbright_bind);
            BindableCheckbox("aspect", tr("Аспект", "Aspect"), &g_cfg.aspect, &g_cfg.aspect_bind);
            CreateAnimation(aspect_fade, g_cfg.aspect, 0.3f, AnimLerp);
            if (g_cfg.aspect || aspect_fade > 0.02f)
            {
                const float prev_mul = widget_alpha_mul;
                widget_alpha_mul = aspect_fade;
                if (SliderFloat(tr("Соотношение сторон", "Aspect ratio"), &g_cfg.aspectval, 0.5f, 3.5f, "%.2f"))
                {
                    SaveGeneralConfig();
                }
                widget_alpha_mul = prev_mul;
            }
            BindableCheckbox("fov", tr("Фов", "Fov"), &g_cfg.fov, &g_cfg.fov_bind);
            CreateAnimation(fov_fade, g_cfg.fov, 0.3f, AnimLerp);
            if (g_cfg.fov || fov_fade > 0.02f)
            {
                const float prev_mul = widget_alpha_mul;
                widget_alpha_mul = fov_fade;
                if (SliderFloat(tr("Поле зрения", "Field of view"), &g_cfg.fovval, 30.0f, 120.0f, "%.0f"))
                {
                    SaveGeneralConfig();
                }
                widget_alpha_mul = prev_mul;
            }
            BindableCheckbox("suncolor", tr("Солнце", "Sun"), &g_cfg.suncolor, &g_cfg.suncolor_bind);
            if (g_cfg.suncolor)
            {
                if (ColorPicker(tr("Цвет солнца", "Sun color"), g_cfg.suncol))
                {
                    SaveGeneralConfig();
                }
            }
            GroupEnd();
        }
        ImGui::EndChild(false);
}
