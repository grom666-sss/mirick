#include "Menu/Menu.hpp"

#include "Utils/xorstr.h"
#include "Utils/Text.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <shellapi.h>

#include "Assets/Assets.hpp"
#include "Gfx/Blur.hpp"
#include "Gfx/Fonts.hpp"
#include "Gfx/Texture.hpp"
#include "Gfx/CharacterPreview.hpp"

#include "Game/Features.h"

Menu* menu = new Menu;

static bool s_dragging = false;
static ImVec2 s_drag_offset{};
static bool s_drag_pending = false;
static ImVec2 s_drag_pending_pos{};

namespace
{
    struct skin_range_t
    {
        int first;
        int last;
    };

    // IDs transcribed from the Next RP catalogue supplied by the user.
    constexpr skin_range_t kNextRpSkinRanges[] = {
        { 39, 41 }, { 43, 64 }, { 66, 73 }, { 75, 85 }, { 87, 118 },
        { 120, 148 }, { 150, 207 }, { 209, 224 }, { 226, 264 },
        { 267, 272 }, { 274, 301 }, { 303, 309 }, { 312, 312 },
        { 6712, 6726 }, { 6732, 6752 }, { 6809, 6855 }, { 6857, 6912 },
        { 10861, 10959 }, { 13891, 13919 }
    };

    bool IsNextRpSkinId(int id)
    {
        for (const skin_range_t& range : kNextRpSkinRanges)
        {
            if (id >= range.first && id <= range.last)
            {
                return true;
            }
        }

        return false;
    }

    int StepNextRpSkinId(int id, int direction)
    {
        if (direction > 0)
        {
            for (const skin_range_t& range : kNextRpSkinRanges)
            {
                if (id < range.first)
                {
                    return range.first;
                }
                if (id >= range.first && id < range.last)
                {
                    return id + 1;
                }
            }

            return kNextRpSkinRanges[0].first;
        }

        for (int i = IM_ARRAYSIZE(kNextRpSkinRanges) - 1; i >= 0; --i)
        {
            const skin_range_t& range = kNextRpSkinRanges[i];
            if (id > range.last)
            {
                return range.last;
            }
            if (id > range.first && id <= range.last)
            {
                return id - 1;
            }
        }

        return kNextRpSkinRanges[IM_ARRAYSIZE(kNextRpSkinRanges) - 1].last;
    }
}

uint32_t Menu::Hash_Label(const char* text)
{
    uint32_t hash = 2166136261u;

    while (*text)
    {
        hash ^= (uint8_t)(*text);
        hash *= 16777619u;
        text++;
    }

    return hash;
}

bool Menu::GetState()
{
    return g_cfg.menu_open;
}

bool Menu::ToggleState()
{
    g_cfg.menu_open = !g_cfg.menu_open;
    PlaySoundFeedback(4, g_cfg.menu_open);
    return g_cfg.menu_open;
}

void Menu::PlaySoundFeedback(unsigned int eventFlag, bool enabled)
{
    if (!g_cfg.ui_sounds || (g_cfg.sound_flags & eventFlag) == 0)
    {
        return;
    }

    UINT sound = MB_OK;
    if (g_cfg.sound_style == 0)
    {
        sound = enabled ? MB_ICONASTERISK : MB_ICONHAND;
    }
    else if (g_cfg.sound_style == 1)
    {
        sound = enabled ? MB_ICONEXCLAMATION : MB_ICONQUESTION;
    }
    MessageBeep(sound);
}

void Menu::PushNotification(const char* label, bool enabled)
{
    if (!g_cfg.ui_notifications || !label)
    {
        return;
    }

    std::string clean(label);
    const std::size_t hidden = clean.find("##");
    if (hidden != std::string::npos)
    {
        clean.resize(hidden);
    }
    if (!Text::IsStringText(clean))
    {
        return;
    }

    const std::string key = Text::ToLower(clean);
    for (ui_notification_t& item : notifications)
    {
        if (Text::ToLower(item.label) == key)
        {
            item.enabled = enabled;
            item.age = 0.0f;
            return;
        }
    }

    // Keep the overlay compact even if several binds are pressed together.
    if (notifications.size() >= 6)
    {
        notifications.erase(notifications.begin());
    }
    notifications.push_back({ clean, enabled, 0.0f });
}

void Menu::DrawNotifications()
{
    if (notifications.empty())
    {
        return;
    }

    const float dt = std::min(ImGui::GetIO().DeltaTime, 0.1f);
    const float s = GetScale();
    const float lifetime = 3.2f;
    ImDrawList* overlay = ImGui::GetForegroundDrawList();
    int visibleIndex = 0;

    for (ui_notification_t& item : notifications)
    {
        item.age += dt;
        const float fadeIn = std::clamp(item.age / 0.18f, 0.0f, 1.0f);
        const float fadeOut = std::clamp((lifetime - item.age) / 0.38f, 0.0f, 1.0f);
        const float opacity = fadeIn * fadeOut;
        if (opacity <= 0.0f)
        {
            continue;
        }

        const ImVec2 size(294.0f * s, 47.0f * s);
        const float slide = (1.0f - fadeIn) * 34.0f * s;
        const ImVec2 pos(
            ImGui::GetIO().DisplaySize.x - size.x - 18.0f * s + slide,
            18.0f * s + visibleIndex * 55.0f * s);
        const c_color accent = item.enabled ? c_color(115, 225, 155) : c_color(241, 126, 159);
        const int a = (int)(245.0f * opacity);

        overlay->AddRectFilled(pos + ImVec2(4.0f * s, 5.0f * s), pos + size + ImVec2(4.0f * s, 5.0f * s), IM_COL32(0, 0, 0, (int)(80.0f * opacity)), 9.0f * s);
        overlay->AddRectFilled(pos, pos + size, IM_COL32(71, 51, 62, a), 9.0f * s);
        overlay->AddRectFilled(pos, pos + ImVec2(4.0f * s, size.y), accent.new_alpha((int)(235.0f * opacity)).as_imcolor(), 3.0f * s);
        overlay->AddText(pos + ImVec2(14.0f * s, 7.0f * s), IM_COL32(253, 244, 249, (int)(255.0f * opacity)), item.label.c_str());
        overlay->AddText(pos + ImVec2(14.0f * s, 26.0f * s), accent.new_alpha((int)(245.0f * opacity)).as_imcolor(),
            item.enabled ? tr("ВКЛЮЧЕНО", "ENABLED") : tr("ВЫКЛЮЧЕНО", "DISABLED"));
        ++visibleIndex;
    }

    notifications.erase(std::remove_if(notifications.begin(), notifications.end(),
        [lifetime](const ui_notification_t& item) { return item.age >= lifetime; }), notifications.end());
}

void Menu::DrawFunctionDescription(const char* label, bool hovered)
{
    if (!hovered || !label || !*label)
    {
        return;
    }

    const bool english = g_cfg.language == 1;
    auto contains = [label](const char* text) { return std::strstr(label, text) != nullptr; };
    const char* description = nullptr;

    if (contains("Валлхак") || contains("Wallhack")) description = english ? "Shows selected information for visible network player entities." : "Показывает выбранную информацию о сетевых игроках.";
    else if (contains("Дальность") || contains("Range")) description = english ? "Maximum distance at which the feature is rendered." : "Максимальная дистанция отображения функции.";
    else if (contains("Аирбрейк") || contains("Airbreak")) description = english ? "Free movement: W/S forward-back, A/D left-right, Space/Shift up-down." : "Свободный полёт: W/S вперёд-назад, A/D влево-вправо, Space/Shift вверх-вниз.";
    else if (contains("Годмод") || contains("Godmode")) description = english ? "Temporarily applies damage-protection flags to the player and current vehicle." : "Применяет защиту от урона к игроку и текущему транспорту.";
    else if (contains("Случайный") || contains("Random")) description = english ? "Enables protection only after a timed probability roll." : "Включает защиту по периодической проверке выбранного шанса.";
    else if (contains("падения") || contains("fall damage")) description = english ? "Prevents fall damage when landing." : "Предотвращает урон при падении и приземлении.";
    else if (contains("Рапид") || contains("Rapid fire")) description = english ? "Reduces the delay between weapon shots." : "Уменьшает задержку между выстрелами оружия.";
    else if (contains("Бесконечные") || contains("Infinite ammo")) description = english ? "Keeps ammunition available for the equipped weapon." : "Сохраняет патроны текущего оружия.";
    else if (contains("Быстрый прицел") || contains("Fast crosshair")) description = english ? "Speeds up weapon aiming and crosshair readiness." : "Ускоряет готовность оружия и прицела.";
    else if (contains("Без отдачи") || contains("No recoil")) description = english ? "Removes camera and weapon recoil." : "Убирает отдачу оружия и камеры.";
    else if (contains("Без разброса") || contains("No spread")) description = english ? "Removes weapon bullet spread." : "Убирает разброс выстрелов оружия.";
    else if (contains("Триггербот") || contains("Triggerbot")) description = english ? "Fires when the crosshair is over a valid target." : "Стреляет, когда прицел наведён на подходящую цель.";
    else if (contains("Анти OBS") || contains("OBS protection")) description = english ? "Moves menu rendering to the protected overlay used by the existing capture setting." : "Переносит меню в защищённый оверлей штатной настройки захвата.";
    else if (contains("Найтмод") || contains("Nightmode")) description = english ? "Applies a darker night environment preset." : "Применяет тёмный ночной пресет окружения.";
    else if (contains("Своё время") || contains("Custom time")) description = english ? "Overrides the displayed game time." : "Задаёт собственное отображаемое игровое время.";
    else if (contains("Заморозить") || contains("Freeze time")) description = english ? "Stops custom time from advancing." : "Останавливает изменение выбранного времени.";
    else if (contains("Чистка мира") || contains("World Removals")) description = english ? "Hides selected world effects such as shadows, sun, or clouds." : "Скрывает выбранные эффекты мира: тени, солнце или облака.";
    else if (contains("Небо") || contains("Sky")) description = english ? "Overrides the upper and lower sky colors." : "Заменяет цвета верхней и нижней части неба.";
    else if (contains("Свет и вода") || contains("Light & water")) description = english ? "Overrides ambient lighting and water colors." : "Заменяет цвета освещения и воды.";
    else if (contains("Фулбрайт") || contains("Fullbright")) description = english ? "Raises scene brightness for improved visibility." : "Повышает яркость сцены для лучшей видимости.";
    else if (contains("Аспект") || contains("Aspect")) description = english ? "Changes the rendered screen aspect ratio." : "Изменяет соотношение сторон изображения.";
    else if (contains("Фов") || contains("Field of view") || contains("Fov")) description = english ? "Changes the camera field of view." : "Изменяет угол обзора камеры.";
    else if (contains("Кейбинды") || contains("Keybinds")) description = english ? "Shows the active keybind panel." : "Показывает панель активных клавиш.";
    else if (contains("Модель персонажа") || contains("Character model")) description = english ? "Shows the off-screen character model preview beside the menu." : "Показывает 3D-превью модели персонажа рядом с меню.";
    else if (contains("Уведомления") || contains("Notifications")) description = english ? "Shows local animated notices when functions are enabled or disabled." : "Показывает локальные анимированные уведомления о переключении функций.";
    else if (contains("Звуки интерфейса") || contains("Interface sounds")) description = english ? "Enables sound feedback for selected interface events." : "Включает звуковые сигналы для выбранных событий интерфейса.";
    else if (contains("Стиль звука") || contains("Sound style")) description = english ? "Selects the Windows sound set used for feedback." : "Выбирает набор системных звуков для уведомлений.";
    else if (contains("Язык") || contains("Language")) description = english ? "Changes the menu language." : "Изменяет язык интерфейса меню.";
    else if (contains("список") || contains("player list")) description = english ? "Shows detected network player entities sorted by distance." : "Показывает найденных сетевых игроков по дистанции.";
    else description = english ? "Changes this function or its display setting." : "Изменяет работу функции или параметр её отображения.";

    const float s = GetScale();
    const float wrap = 300.0f * s;
    const ImVec2 text_size = ImGui::CalcTextSize(description, nullptr, false, wrap);
    ImVec2 pos = ImGui::GetIO().MousePos + ImVec2(16.0f * s, 18.0f * s);
    const ImVec2 size = text_size + ImVec2(22.0f * s, 18.0f * s);
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    pos.x = std::min(pos.x, display.x - size.x - 6.0f * s);
    pos.y = std::min(pos.y, display.y - size.y - 6.0f * s);

    ImDrawList* overlay = ImGui::GetForegroundDrawList();
    overlay->AddRectFilled(pos + ImVec2(3.0f * s, 4.0f * s), pos + size + ImVec2(3.0f * s, 4.0f * s), IM_COL32(0, 0, 0, 90), 8.0f * s);
    overlay->AddRectFilled(pos, pos + size, IM_COL32(73, 52, 63, 250), 8.0f * s);
    overlay->AddRect(pos, pos + size, g_cfg.accent.to_color().new_alpha(150).as_imcolor(), 8.0f * s);
    overlay->AddText(g_fonts.main, 14.0f * s, pos + ImVec2(11.0f * s, 9.0f * s), IM_COL32(250, 241, 246, 255), description, nullptr, wrap);
}

void Menu::SetDrawList(ImDrawList* list)
{
    draw_list = list;
}

void Menu::SetWindowPos(const ImVec2& pos)
{
    window_pos = pos;
}

ImVec2 Menu::GetWindowPos()
{
    const float s = GetScale();
    return window_pos + ImVec2(45.0f * s, 15.0f * s);
}

void Menu::CreateAnimation(float& mod, bool cond, float speed, unsigned int flags)
{
    float time = (ImGui::GetIO().DeltaTime * 5.0f * 5.0f) * speed;

    if ((flags & AnimSkipEnable) && cond)
    {
        mod = 1.0f;
    }
    else if ((flags & AnimSkipDisable) && !cond)
    {
        mod = 0.0f;
    }

    if (flags & AnimLerp)
    {
        mod = mod + ((float)cond - mod) * time;
    }
    else
    {
        if (cond && mod <= 1.0f)
        {
            mod += time;
        }
        else if (!cond && mod >= 0.0f)
        {
            mod -= time;
        }
    }

    if (mod < 0.0f)
    {
        mod = 0.0f;
    }

    if (mod > 1.0f)
    {
        mod = 1.0f;
    }
}

void Menu::UpdateAlpha()
{
    CreateAnimation(alpha, g_cfg.menu_open, 0.3f, 0);
}

float Menu::GetAlpha()
{
    return alpha * widget_alpha_mul;
}

float Menu::GetScale()
{
    return g_cfg.ui_scale / 100.0f;
}

void Menu::ApplyScale()
{
    static int applied_scale = 0;

    if (applied_scale == g_cfg.ui_scale)
    {
        return;
    }

    applied_scale = g_cfg.ui_scale;
    ImGui::GetStyle() = ImGuiStyle();
    Fonts::InitStyle();
    ImGui::GetStyle().ScaleAllSizes(GetScale());
    ImGui::GetStyle().MouseCursorScale = GetScale();
}

bool Menu::Vector_Getter(void* vec, int idx, const char** out_text)
{
    std::vector<std::string>& vector = *static_cast<std::vector<std::string>*>(vec);

    if (idx < 0 || idx >= (int)vector.size())
    {
        return false;
    }

    *out_text = vector.at(idx).c_str();

    return true;
}

bool Menu::Items_Array_Getter(void* data, int idx, const char** out_text)
{
    const char* const* items = (const char* const*)data;

    if (out_text)
    {
        *out_text = items[idx];
    }

    return true;
}

float Menu::Calc_Max_Popup_Height(int items_count)
{
    ImGuiContext& g = *GImGui;

    if (items_count <= 0)
    {
        return FLT_MAX;
    }

    const float k = g.IO.FontGlobalScale > 0.0f ? g.IO.FontGlobalScale : 1.0f;
    return (g.FontSize + g.Style.ItemSpacing.y * k) * items_count - g.Style.ItemSpacing.y * k + (g.Style.WindowPadding.y * 2.0f * k);
}

void Menu::Render_Vertical_Arrows(ImDrawList* list, ImVec2 pos, ImVec2 half_sz, float bar_w, float alpha)
{
    ImU32 alpha8 = IM_F32_TO_INT8_SAT(alpha);
    ImGui::RenderArrowPointingAt(list, ImVec2(pos.x + half_sz.x + 1, pos.y), ImVec2(half_sz.x + 2, half_sz.y + 1), ImGuiDir_Right, IM_COL32(0, 0, 0, alpha8));
    ImGui::RenderArrowPointingAt(list, ImVec2(pos.x + half_sz.x, pos.y), half_sz, ImGuiDir_Right, IM_COL32(255, 255, 255, alpha8));
    ImGui::RenderArrowPointingAt(list, ImVec2(pos.x + bar_w - half_sz.x - 1, pos.y), ImVec2(half_sz.x + 2, half_sz.y + 1), ImGuiDir_Left, IM_COL32(0, 0, 0, alpha8));
    ImGui::RenderArrowPointingAt(list, ImVec2(pos.x + bar_w - half_sz.x, pos.y), half_sz, ImGuiDir_Left, IM_COL32(255, 255, 255, alpha8));
}

void Menu::InitTextures()
{
    IDirect3DDevice9* device = Blur::GetDevice();

    if (!device)
    {
        return;
    }

    if (!logo_texture)
    {
        Texture::FromMemory(device, osintLogo, sizeof(osintLogo), &logo_texture);
    }

    if (!kb_texture)
    {
        Texture::FromMemory(device, keyboard_icon, sizeof(keyboard_icon), &kb_texture);
    }

    if (!icon_textures[0])
    {
        Texture::FromMemory(device, rage_icon, sizeof(rage_icon), &icon_textures[0]);
    }

    if (!icon_textures[1])
    {
        Texture::FromMemory(device, legit_icon, sizeof(legit_icon), &icon_textures[1]);
    }

    if (!icon_textures[2])
    {
        Texture::FromMemory(device, visuals_icon, sizeof(visuals_icon), &icon_textures[2]);
    }

    if (!icon_textures[3])
    {
        Texture::FromMemory(device, misc_icon, sizeof(misc_icon), &icon_textures[3]);
    }

}

void Menu::WindowBegin()
{
    const float s = GetScale();
    ImVec2 window_size = ImVec2(800.0f * s, 550.0f * s);
    ImGui::SetNextWindowSize(window_size, ImGuiCond_Always);

    static int centered_scale = 0;
    static bool first_frame = true;

    if (first_frame || centered_scale != g_cfg.ui_scale)
    {
        first_frame = false;
        centered_scale = g_cfg.ui_scale;
        ImGui::SetNextWindowPos(ImVec2((ImGui::GetIO().DisplaySize.x - window_size.x) * 0.5f, (ImGui::GetIO().DisplaySize.y - window_size.y) * 0.5f));
    }
    if (s_drag_pending)
    {
        ImGui::SetNextWindowPos(s_drag_pending_pos);
        s_drag_pending = false;
    }

    ImGui::SetNextWindowBgAlpha(0.0f);
    ImGui::PushFont(g_fonts.main);
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    static bool keep_open = true;
    ImGui::Begin("##base_window", &keep_open, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar);
    ImGui::PushItemWidth(256.0f * s);
    SetDrawList(ImGui::GetWindowDrawList());
    SetWindowPos(ImGui::GetWindowPos());

    ImDrawList* list = draw_list;
    list->Flags |= ImDrawListFlags_AntiAliasedFill | ImDrawListFlags_AntiAliasedLines;

    ImVec2 wp = GetWindowPos();
    list->PushClipRect(wp, ImVec2(wp.x + 720.0f * s, wp.y + 520.0f * s));
    ImGui::PushClipRect(wp, ImVec2(wp.x + 720.0f * s, wp.y + 520.0f * s), false);
}

void Menu::WindowEnd()
{
    ImDrawList* list = draw_list;
    list->Flags &= ~(ImDrawListFlags_AntiAliasedFill | ImDrawListFlags_AntiAliasedLines);
    list->PopClipRect();
    ImGui::PopClipRect();
    ImGui::PopItemWidth();
    ImGui::End(false);

    ImGui::PopStyleColor();
    ImGui::PopFont();
    draw_list = nullptr;
}

void Menu::DrawBackground()
{
    const float s = GetScale();
    ImDrawList* list = draw_list;
    const float alpha = GetAlpha();
    const ImVec2 window_pos = GetWindowPos();
    const ImVec2 window_max = window_pos + ImVec2(720.0f * s, 520.0f * s);
    const ImVec2 header_size(720.0f * s, 52.0f * s);
    const float rail_width = 76.0f * s;
    const c_color rose(234, 179, 200, 255);
    const c_color panel(90, 65, 78, 255);

    // Adapted from the supplied Grape ImGui reference. Keep the native Win32 /
    // D3D9 backend, but reproduce its rounded rose shell and soft lighting.
    list->AddRectFilled(window_pos + ImVec2(0.0f, 7.0f * s),
        window_max + ImVec2(0.0f, 9.0f * s), IM_COL32(0, 0, 0, (int)(90.0f * alpha)), 13.0f * s);
    list->AddRectFilled(window_pos, window_max, panel.new_alpha((int)(247.0f * alpha)).as_imcolor(), 13.0f * s);

    // A soft animated lightning ribbon replaces the Android/OpenGL-only
    // procedural mesh while retaining the look on the existing D3D9 renderer.
    const float time = (float)ImGui::GetTime();
    ImVec2 previous{};
    for (int i = 0; i <= 52; ++i)
    {
        const float v = (float)i / 52.0f;
        const float wave = 0.53f
            + 0.040f * std::sin(v * 18.5f - time * 1.2f)
            + 0.026f * std::sin(v * 39.0f + time * 0.8f);
        const ImVec2 point(window_pos.x + wave * 720.0f * s, window_pos.y + v * 520.0f * s);
        if (i > 0)
        {
            list->AddLine(previous, point, rose.new_alpha((int)(22.0f * alpha)).as_imcolor(), 24.0f * s);
            list->AddLine(previous, point, rose.new_alpha((int)(42.0f * alpha)).as_imcolor(), 11.0f * s);
            list->AddLine(previous, point, IM_COL32(255, 239, 247, (int)(115.0f * alpha)), 2.0f * s);
        }
        previous = point;
    }

    list->AddRectFilled(window_pos + ImVec2(8.0f * s, 9.0f * s),
        window_pos + ImVec2(rail_width - 7.0f * s, 511.0f * s),
        IM_COL32(67, 48, 59, (int)(118.0f * alpha)), 9.0f * s);
    list->AddLine(window_pos + ImVec2(86.0f * s, 52.0f * s),
        window_pos + ImVec2(700.0f * s, 52.0f * s), IM_COL32(228, 209, 218, (int)(45.0f * alpha)), 1.0f * s);

    // Keep the logo centered over the module rail and render it without a
    // tint so the cleaned high-resolution pink artwork stays crisp.
    const ImVec2 logo_min = window_pos + ImVec2(21.5f * s, 9.0f * s);
    const ImVec2 logo_max = logo_min + ImVec2(33.0f * s, 33.0f * s);
    if (logo_texture)
    {
        list->AddImage((void*)logo_texture, logo_min, logo_max, ImVec2(0, 0), ImVec2(1, 1),
            IM_COL32(255, 255, 255, (int)(255.0f * alpha)));
    }

    ImGui::PushFont(g_fonts.title);
    const ImVec2 title_size = ImGui::CalcTextSize("OSINT");
    list->AddText(window_pos + ImVec2((720.0f * s - title_size.x) * 0.5f, 4.0f * s),
        IM_COL32(255, 247, 251, (int)(255.0f * alpha)), "OSINT");
    ImGui::PopFont();

    static const char* build_text = "build 1.0";
    const ImVec2 build_size = ImGui::CalcTextSize(build_text);
    list->AddText(window_pos + ImVec2((720.0f * s - build_size.x) * 0.5f, 34.0f * s),
        IM_COL32(225, 198, 211, (int)(220.0f * alpha)), build_text);

    const ImVec2 close_center = window_pos + ImVec2(700.0f * s, 26.0f * s);
    const ImVec2 close_min = close_center - ImVec2(12.0f * s, 12.0f * s);
    const ImVec2 close_max = close_center + ImVec2(12.0f * s, 12.0f * s);
    const bool close_hovered = ImGui::IsMouseHoveringRect(close_min, close_max);
    const ImU32 close_color = close_hovered
        ? IM_COL32(255, 255, 255, (int)(255.0f * alpha))
        : IM_COL32(237, 218, 227, (int)(220.0f * alpha));
    list->AddLine(close_center - ImVec2(4.5f * s, 4.5f * s), close_center + ImVec2(4.5f * s, 4.5f * s), close_color, 1.7f * s);
    list->AddLine(close_center + ImVec2(4.5f * s, -4.5f * s), close_center + ImVec2(-4.5f * s, 4.5f * s), close_color, 1.7f * s);
    if (close_hovered && ImGui::IsMouseClicked(0))
    {
        ToggleState();
    }

    list->AddRect(window_pos, window_max, IM_COL32(242, 220, 230, (int)(62.0f * alpha)), 13.0f * s);

    ImGuiIO& io = ImGui::GetIO();
    const ImVec2 header_max(window_pos.x + header_size.x, window_pos.y + header_size.y);
    if (!s_dragging && !close_hovered
        && ImGui::IsMouseHoveringRect(window_pos, header_max) && ImGui::IsMouseClicked(0))
    {
        s_dragging = true;
        s_drag_offset = ImVec2(io.MousePos.x - window_pos.x, io.MousePos.y - window_pos.y);
    }

    s_drag_pending = false;
    if (s_dragging)
    {
        if ((GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0)
        {
            s_drag_pending = true;
            s_drag_pending_pos = ImVec2(io.MousePos.x - s_drag_offset.x - 45.0f * s,
                io.MousePos.y - s_drag_offset.y - 15.0f * s);
        }
        else
        {
            s_dragging = false;
        }
    }
}

void Menu::DrawCharacterPanel()
{
    const float s = GetScale();
    const float panel_alpha = GetAlpha();
    const ImVec2 panel_size(212.0f * s, 520.0f * s);
    const ImVec2 panel_pos = GetWindowPos() - ImVec2(224.0f * s, 0.0f);
    const c_color accent = g_cfg.accent.to_color();
    static int preview_skin_id = 39;
    static bool preview_initialized = false;
    if (!preview_initialized)
    {
        preview_skin_id = g_cfg.player_skin_id;
        preview_initialized = true;
    }

    ImGui::SetNextWindowPos(panel_pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(panel_size, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 9.0f * s);
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
    ImGui::Begin("##osint_character_panel", nullptr,
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove);

    ImDrawList* panel_draw = ImGui::GetWindowDrawList();
    const ImVec2 panel_max = panel_pos + panel_size;
    panel_draw->AddRectFilled(panel_pos + ImVec2(4.0f * s, 6.0f * s), panel_max + ImVec2(4.0f * s, 6.0f * s), IM_COL32(0, 0, 0, (int)(95.0f * panel_alpha)), 9.0f * s);
    panel_draw->AddRectFilled(panel_pos, panel_max, IM_COL32(82, 59, 71, (int)(250.0f * panel_alpha)), 9.0f * s);
    panel_draw->AddRectFilled(panel_pos, panel_pos + ImVec2(panel_size.x, 43.0f * s), IM_COL32(105, 77, 91, (int)(245.0f * panel_alpha)), 9.0f * s, ImDrawCornerFlags_Top);
    panel_draw->AddRectFilledMultiColor(
        panel_pos + ImVec2(0, 42.0f * s), panel_pos + ImVec2(panel_size.x, 44.0f * s),
        accent.new_alpha(0).as_imcolor(), accent.new_alpha((int)(220.0f * panel_alpha)).as_imcolor(),
        accent.new_alpha((int)(220.0f * panel_alpha)).as_imcolor(), accent.new_alpha(0).as_imcolor());
    panel_draw->AddText(panel_pos + ImVec2(15.0f * s, 13.0f * s), IM_COL32(250, 243, 247, (int)(255.0f * panel_alpha)), tr("МОДЕЛЬ ПЕРСОНАЖА", "CHARACTER MODEL"));

    char preview_model[64]{};
    ImFormatString(preview_model, IM_ARRAYSIZE(preview_model), "%s: %d", tr("Скин в превью", "Preview skin"), preview_skin_id);
    panel_draw->AddText(panel_pos + ImVec2(15.0f * s, 54.0f * s), IM_COL32(211, 187, 199, (int)(235.0f * panel_alpha)), preview_model);

    CharacterPreview::SelectModel(preview_skin_id);
    IDirect3DTexture9* preview_texture = CharacterPreview::GetTexture();
    const ImVec2 preview_min = panel_pos + ImVec2(15.0f * s, 76.0f * s);
    const ImVec2 preview_max = panel_pos + ImVec2(197.0f * s, 358.0f * s);
    panel_draw->AddRectFilled(preview_min, preview_max, IM_COL32(55, 39, 47, (int)(245.0f * panel_alpha)), 6.0f * s);
    if (preview_texture)
    {
        panel_draw->AddImage(
            reinterpret_cast<ImTextureID>(preview_texture), preview_min, preview_max,
            ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, (int)(255.0f * panel_alpha)));
    }

    static float preview_yaw = 0.0f;
    static CharacterPreview::PosePart pose_part = CharacterPreview::PosePart::Head;
    ImGui::SetCursorScreenPos(preview_min);
    ImGui::InvisibleButton("##character_preview_view", preview_max - preview_min);
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
    {
        preview_yaw = 0.0f;
        CharacterPreview::ResetPose();
    }
    else if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0))
    {
        preview_yaw += ImGui::GetIO().MouseDelta.x * 0.8f;
        CharacterPreview::SetRotation(preview_yaw);
    }
    if (ImGui::IsItemHovered() && ImGui::IsMouseDragging(1))
    {
        CharacterPreview::AdjustPose(pose_part, ImGui::GetIO().MouseDelta.x * 0.35f);
    }

    ImGui::SetCursorPos(ImVec2(15.0f * s, 365.0f * s));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f * s, 0.0f));
    if (ImGui::SmallButton(tr("Голова##pose_head", "Head##pose_head")))
    {
        pose_part = CharacterPreview::PosePart::Head;
    }
    ImGui::SameLine();
    if (ImGui::SmallButton(tr("Руки##pose_arms", "Arms##pose_arms")))
    {
        pose_part = CharacterPreview::PosePart::Arms;
    }
    ImGui::SameLine();
    if (ImGui::SmallButton(tr("Ноги##pose_legs", "Legs##pose_legs")))
    {
        pose_part = CharacterPreview::PosePart::Legs;
    }
    ImGui::PopStyleVar();

    panel_draw->AddText(panel_pos + ImVec2(15.0f * s, 395.0f * s),
        IM_COL32(211, 187, 199, (int)(220.0f * panel_alpha)),
        tr("ЛКМ: обзор, ПКМ: поза", "LMB: rotate, RMB: pose"));

    ImGui::SetCursorPos(ImVec2(15.0f * s, 417.0f * s));
    ImGui::PushItemWidth(182.0f * s);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.42f, 0.32f, 0.37f, panel_alpha));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.52f, 0.40f, 0.46f, panel_alpha));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.94f, 0.96f, 1.0f, panel_alpha));
    ImGui::InputInt("##next_rp_skin_id", &g_cfg.player_skin_id, 0, 0, ImGuiInputTextFlags_CharsDecimal);
    ImGui::PopStyleColor(3);
    ImGui::PopItemWidth();

    ImGui::SetCursorPos(ImVec2(15.0f * s, 452.0f * s));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.45f, 0.34f, 0.40f, panel_alpha));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(accent.r() / 255.0f, accent.g() / 255.0f, accent.b() / 255.0f, 0.28f * panel_alpha));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(accent.r() / 255.0f, accent.g() / 255.0f, accent.b() / 255.0f, 0.42f * panel_alpha));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.94f, 0.96f, 1.0f, panel_alpha));

    if (ImGui::Button("<##skin_prev", ImVec2(42.0f * s, 27.0f * s)))
    {
        g_cfg.player_skin_id = StepNextRpSkinId(g_cfg.player_skin_id, -1);
    }
    ImGui::SameLine(0.0f, 5.0f * s);
    if (ImGui::Button(tr("В превью##skin_preview", "Preview##skin_preview"), ImVec2(88.0f * s, 27.0f * s)))
    {
        const int id = g_cfg.player_skin_id;
        preview_skin_id = id;
        CharacterPreview::SelectModel(id);
        if (id >= 0 && id <= 32767)
        {
            SaveGeneralConfig();
        }
    }
    ImGui::SameLine(0.0f, 5.0f * s);
    if (ImGui::Button(">##skin_next", ImVec2(42.0f * s, 27.0f * s)))
    {
        g_cfg.player_skin_id = StepNextRpSkinId(g_cfg.player_skin_id, 1);
    }
    ImGui::PopStyleColor(4);

    const char* status_text = tr("Ожидание модели", "Waiting for model");
    bool status_ok = false;
    switch (CharacterPreview::GetState())
    {
    case CharacterPreview::State::Ready:
        status_text = tr("Runtime-модель готова", "Runtime model is ready");
        status_ok = true;
        break;
    case CharacterPreview::State::Loading:
        status_text = tr("Загрузка runtime-модели...", "Loading runtime model...");
        break;
    case CharacterPreview::State::InvalidId:
        status_text = tr("ID вне таблицы моделей", "ID is outside model table");
        break;
    case CharacterPreview::State::NotPed:
        status_text = tr("ID не является моделью персонажа", "ID is not a ped model");
        break;
    case CharacterPreview::State::Unavailable:
        status_text = tr("Runtime-модель не зарегистрирована", "Runtime model is not registered");
        break;
    case CharacterPreview::State::RenderError:
        status_text = tr("Ошибка off-screen рендера", "Off-screen render error");
        break;
    default:
        break;
    }
    const ImU32 status_color = status_ok
        ? accent.new_alpha((int)(220.0f * panel_alpha)).u32()
        : IM_COL32(225, 160, 112, (int)(220.0f * panel_alpha));
    panel_draw->AddText(panel_pos + ImVec2(15.0f * s, 489.0f * s), status_color, status_text);
    panel_draw->AddRect(panel_pos, panel_max, IM_COL32(224, 197, 210, (int)(92.0f * panel_alpha)), 9.0f * s);

    ImGui::End(false);
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}

void Menu::DrawAuthorPanel()
{
    const float s = GetScale();
    const float menu_alpha = GetAlpha();
    const ImVec2 menu_pos = GetWindowPos();
    const c_color accent = g_cfg.accent.to_color();

    // The drawer is a separate ImGui window so it can open outside the main
    // menu instead of covering its content. The header badge toggles it in
    // DrawBackground, independently of content-tab clipping rectangles.
    CreateAnimation(author_panel_anim, author_panel_open, 0.32f, AnimLerp);
    if (author_panel_anim <= 0.01f)
    {
        return;
    }

    const float eased = 1.0f - std::pow(1.0f - author_panel_anim, 3.0f);
    const float panel_width = 266.0f * s;
    const float panel_height = 472.0f * s;
    const ImVec2 panel_position(
        menu_pos.x + 728.0f * s - (1.0f - eased) * 24.0f * s,
        menu_pos.y + 48.0f * s);
    const float panel_alpha = menu_alpha * author_panel_anim;

    ImGui::SetNextWindowPos(panel_position, ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(panel_width * eased, panel_height), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 9.0f * s);
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
    ImGui::Begin(
        "##osint_author_drawer",
        nullptr,
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse);

    ImDrawList* panel_draw = ImGui::GetWindowDrawList();
    const ImVec2 panel_min = ImGui::GetWindowPos();
    const ImVec2 panel_max(panel_min.x + panel_width, panel_min.y + panel_height);

    panel_draw->AddRectFilled(panel_min + ImVec2(4.0f * s, 6.0f * s), panel_max + ImVec2(4.0f * s, 6.0f * s), IM_COL32(0, 0, 0, static_cast<int>(80.0f * panel_alpha)), 9.0f * s);
    panel_draw->AddRectFilled(panel_min, panel_max, IM_COL32(82, 59, 71, static_cast<int>(250.0f * panel_alpha)), 9.0f * s);
    panel_draw->AddRectFilled(panel_min, ImVec2(panel_min.x + 3.0f * s, panel_max.y), accent.new_alpha(static_cast<int>(210.0f * panel_alpha)).as_imcolor(), 2.0f * s);

    const ImVec2 avatar_center(panel_min.x + 34.0f * s, panel_min.y + 38.0f * s);
    panel_draw->AddCircleFilled(avatar_center, 17.0f * s, accent.new_alpha(static_cast<int>(45.0f * panel_alpha)).as_imcolor());
    panel_draw->AddText(avatar_center - ImVec2(5.0f * s, 8.0f * s), accent.new_alpha(static_cast<int>(255.0f * panel_alpha)).as_imcolor(), "M");
    panel_draw->AddText(panel_min + ImVec2(61.0f * s, 22.0f * s), IM_COL32(250, 243, 247, static_cast<int>(255.0f * panel_alpha)), "m1racl");
    panel_draw->AddText(panel_min + ImVec2(61.0f * s, 43.0f * s), IM_COL32(210, 186, 198, static_cast<int>(245.0f * panel_alpha)), tr("Автор OSINT", "OSINT author"));
    panel_draw->AddLine(panel_min + ImVec2(18.0f * s, 75.0f * s), panel_min + ImVec2(248.0f * s, 75.0f * s), IM_COL32(220, 194, 207, static_cast<int>(100.0f * panel_alpha)));

    auto contact_card = [&](const char* id, const char* service, const char* value, float y, const ImU32 service_color)
    {
        const ImVec2 card_min(panel_min.x + 18.0f * s, panel_min.y + y * s);
        const ImVec2 card_max(panel_max.x - 18.0f * s, card_min.y + 58.0f * s);
        bool clicked = false;
        bool hovered = false;
        if (eased > 0.82f)
        {
            ImGui::SetCursorScreenPos(card_min);
            clicked = ImGui::InvisibleButton(id, card_max - card_min);
            hovered = ImGui::IsItemHovered();
        }
        panel_draw->AddRectFilled(card_min, card_max, hovered ? IM_COL32(126, 95, 110, static_cast<int>(235.0f * panel_alpha)) : IM_COL32(104, 77, 90, static_cast<int>(225.0f * panel_alpha)), 7.0f * s);
        panel_draw->AddRectFilled(card_min + ImVec2(10.0f * s, 13.0f * s), card_min + ImVec2(14.0f * s, 45.0f * s), service_color, 2.0f * s);
        panel_draw->AddText(card_min + ImVec2(25.0f * s, 10.0f * s), IM_COL32(250, 243, 247, static_cast<int>(255.0f * panel_alpha)), service);
        panel_draw->AddText(card_min + ImVec2(25.0f * s, 32.0f * s), IM_COL32(215, 192, 203, static_cast<int>(255.0f * panel_alpha)), value);
        panel_draw->AddText(card_max - ImVec2(18.0f * s, 41.0f * s), IM_COL32(231, 209, 219, static_cast<int>(220.0f * panel_alpha)), ">");
        return clicked;
    };

    if (contact_card("##author_telegram", "Telegram", "@mirick1337", 94.0f, IM_COL32(52, 164, 225, static_cast<int>(255.0f * panel_alpha))))
    {
        ShellExecuteA(NULL, "open", "https://t.me/mirick1337", NULL, NULL, SW_SHOWNORMAL);
    }

    static float discord_copied_timer = 0.0f;
    if (contact_card("##author_discord", "Discord", "m1racl09", 162.0f, IM_COL32(111, 133, 255, static_cast<int>(255.0f * panel_alpha))))
    {
        ImGui::SetClipboardText("m1racl09");
        discord_copied_timer = 2.5f;
    }
    if (discord_copied_timer > 0.0f)
    {
        discord_copied_timer -= ImGui::GetIO().DeltaTime;
        panel_draw->AddText(panel_min + ImVec2(18.0f * s, 231.0f * s), accent.new_alpha(static_cast<int>(255.0f * panel_alpha)).as_imcolor(), tr("Discord скопирован", "Discord copied"));
    }
    else
    {
        panel_draw->AddText(panel_min + ImVec2(18.0f * s, 231.0f * s), IM_COL32(203, 177, 190, static_cast<int>(230.0f * panel_alpha)), tr("Нажмите, чтобы скопировать Discord", "Click to copy Discord"));
    }

    panel_draw->AddText(panel_min + ImVec2(18.0f * s, 416.0f * s), IM_COL32(181, 153, 167, static_cast<int>(210.0f * panel_alpha)), "OSINT  /  m1racl");
    ImGui::End(false);
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}
void Menu::DrawTabs()
{
    const float s = GetScale();
    ImDrawList* list = draw_list;
    ImVec2 prev = ImGui::GetCursorPos();
    ImVec2 child_pos = GetWindowPos() + ImVec2(0, 54.0f * s);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, GetAlpha()));

    const std::string names[5] = { tr("Рейдж", "Rage"), tr("Легит", "Legit"), tr("Визуалы", "Visuals"), tr("Разное", "Misc"), tr("Настройки", "Settings") };
    c_color accent = g_cfg.accent.to_color();

    for (int i = 0; i < 5; i++)
    {
        tab_animation_t& info = tab_info[i];
        ImGui::SetCursorPos(ImVec2(53.0f * s, (74.0f + 58.0f * i) * s));

        std::string id = std::string("##tab_") + std::to_string(i);
        bool clicked = ImGui::ButtonEx(id.c_str(), ImVec2(58.0f * s, 48.0f * s), 0, &info.hovered);

        if (clicked)
        {
            tab_selector = i;
            PlaySoundFeedback(8, true);
        }

        CreateAnimation(info.hovered_alpha, info.hovered, 1.0f, AnimLerp);
        CreateAnimation(info.alpha, tab_selector == i, 0.8f, AnimSkipDisable | AnimLerp);

        ImVec2 tmin = child_pos + ImVec2(8.0f * s, (5.0f + 58.0f * i) * s);
        ImVec2 tmax = tmin + ImVec2(58.0f * s, 48.0f * s);
        const int hover_add = (int)(18.0f * info.hovered_alpha);
        c_color text_clr = tab_selector == i
            ? c_color(250, 224, 235, (int)(255.0f * GetAlpha()))
            : c_color(222 + hover_add, 205 + hover_add, 215 + hover_add, (int)(225.0f * GetAlpha()));

        if (tab_selector == i)
        {
            list->AddRectFilled(tmin + ImVec2(6.0f * s, 3.0f * s), tmax - ImVec2(6.0f * s, 3.0f * s),
                IM_COL32(145, 112, 123, (int)(225.0f * info.alpha * GetAlpha())), 9.0f * s);
            list->AddRect(tmin + ImVec2(6.0f * s, 3.0f * s), tmax - ImVec2(6.0f * s, 3.0f * s),
                accent.new_alpha((int)(105.0f * info.alpha * GetAlpha())).as_imcolor(), 9.0f * s);
        }

        const ImU32 icon_color = text_clr.as_imcolor();
        const float stroke = 1.6f * s;
        const ImVec2 center = tmin + ImVec2(29.0f * s, 24.0f * s);

        // Crisp vector module icons replace the old bitmap symbols. They stay
        // sharp at every UI scale and inherit the tab hover/selection color.
        switch (i)
        {
        case 0: // Crosshair.
            list->AddCircle(center, 5.0f * s, icon_color, 16, stroke);
            list->AddCircleFilled(center, 1.5f * s, icon_color, 8);
            list->AddLine(center - ImVec2(0.0f, 8.0f * s), center - ImVec2(0.0f, 5.5f * s), icon_color, stroke);
            list->AddLine(center + ImVec2(0.0f, 5.5f * s), center + ImVec2(0.0f, 8.0f * s), icon_color, stroke);
            list->AddLine(center - ImVec2(8.0f * s, 0.0f), center - ImVec2(5.5f * s, 0.0f), icon_color, stroke);
            list->AddLine(center + ImVec2(5.5f * s, 0.0f), center + ImVec2(8.0f * s, 0.0f), icon_color, stroke);
            break;
        case 1: // Shield with a check mark.
        {
            const ImVec2 top = center - ImVec2(0.0f, 7.5f * s);
            list->AddLine(top, center + ImVec2(6.0f * s, -5.0f * s), icon_color, stroke);
            list->AddLine(center + ImVec2(6.0f * s, -5.0f * s), center + ImVec2(5.0f * s, 3.0f * s), icon_color, stroke);
            list->AddLine(center + ImVec2(5.0f * s, 3.0f * s), center + ImVec2(0.0f, 7.5f * s), icon_color, stroke);
            list->AddLine(center + ImVec2(0.0f, 7.5f * s), center + ImVec2(-5.0f * s, 3.0f * s), icon_color, stroke);
            list->AddLine(center + ImVec2(-5.0f * s, 3.0f * s), center + ImVec2(-6.0f * s, -5.0f * s), icon_color, stroke);
            list->AddLine(center + ImVec2(-6.0f * s, -5.0f * s), top, icon_color, stroke);
            list->AddLine(center + ImVec2(-3.0f * s, 0.0f), center + ImVec2(-0.5f * s, 2.5f * s), icon_color, stroke);
            list->AddLine(center + ImVec2(-0.5f * s, 2.5f * s), center + ImVec2(3.5f * s, -2.0f * s), icon_color, stroke);
            break;
        }
        case 2: // Eye.
            list->AddLine(center - ImVec2(7.5f * s, 0.0f), center + ImVec2(-3.5f * s, -4.0f * s), icon_color, stroke);
            list->AddLine(center + ImVec2(-3.5f * s, -4.0f * s), center + ImVec2(3.5f * s, -4.0f * s), icon_color, stroke);
            list->AddLine(center + ImVec2(3.5f * s, -4.0f * s), center + ImVec2(7.5f * s, 0.0f), icon_color, stroke);
            list->AddLine(center + ImVec2(7.5f * s, 0.0f), center + ImVec2(3.5f * s, 4.0f * s), icon_color, stroke);
            list->AddLine(center + ImVec2(3.5f * s, 4.0f * s), center + ImVec2(-3.5f * s, 4.0f * s), icon_color, stroke);
            list->AddLine(center + ImVec2(-3.5f * s, 4.0f * s), center - ImVec2(7.5f * s, 0.0f), icon_color, stroke);
            list->AddCircleFilled(center, 2.5f * s, icon_color, 12);
            break;
        case 3: // Adjustment sliders.
            list->AddLine(center + ImVec2(-7.0f * s, -4.5f * s), center + ImVec2(7.0f * s, -4.5f * s), icon_color, stroke);
            list->AddLine(center + ImVec2(-7.0f * s, 0.0f), center + ImVec2(7.0f * s, 0.0f), icon_color, stroke);
            list->AddLine(center + ImVec2(-7.0f * s, 4.5f * s), center + ImVec2(7.0f * s, 4.5f * s), icon_color, stroke);
            list->AddCircleFilled(center + ImVec2(-2.5f * s, -4.5f * s), 2.0f * s, icon_color, 10);
            list->AddCircleFilled(center + ImVec2(3.0f * s, 0.0f), 2.0f * s, icon_color, 10);
            list->AddCircleFilled(center + ImVec2(-1.0f * s, 4.5f * s), 2.0f * s, icon_color, 10);
            break;
        default: // Solid eight-tooth settings gear.
            for (int tooth = 0; tooth < 8; ++tooth)
            {
                const float angle = tooth * 6.2831853f / 8.0f;
                const float root = 5.3f * s;
                const float tip = 8.2f * s;
                const float rootHalf = 0.21f;
                const float tipHalf = 0.13f;
                const ImVec2 p0(center.x + std::cos(angle - rootHalf) * root, center.y + std::sin(angle - rootHalf) * root);
                const ImVec2 p1(center.x + std::cos(angle - tipHalf) * tip, center.y + std::sin(angle - tipHalf) * tip);
                const ImVec2 p2(center.x + std::cos(angle + tipHalf) * tip, center.y + std::sin(angle + tipHalf) * tip);
                const ImVec2 p3(center.x + std::cos(angle + rootHalf) * root, center.y + std::sin(angle + rootHalf) * root);
                list->AddQuadFilled(p0, p1, p2, p3, icon_color);
            }
            list->AddCircleFilled(center, 5.5f * s, icon_color, 20);
            list->AddCircleFilled(center, 2.2f * s, IM_COL32(90, 65, 78, (int)(255.0f * GetAlpha())), 14);
            break;
        }

        if (info.hovered)
        {
            ImGui::BeginTooltip();
            ImGui::TextUnformatted(names[i].c_str());
            ImGui::EndTooltip();
        }
    }

    ImGui::PopStyleColor(4);
    ImGui::SetCursorPos(prev);
}

void Menu::DrawSubTabs(int& selector, const std::vector<std::string>& tabs)
{
    const float s = GetScale();
    ImGuiStyle& style = ImGui::GetStyle();
    float a = GetAlpha();
    float wa = 255.0f * a;
    ImVec2 child_pos = GetWindowPos() + ImVec2(94.0f * s, 64.0f * s);
    ImVec2 prev = ImGui::GetCursorPos();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, a));
    draw_list->AddRectFilled(child_pos, child_pos + ImVec2(612.0f * s, 58.0f * s), IM_COL32(104, 79, 92, (int)(150.0f * a)), 8.0f * s);
    draw_list->AddRect(child_pos, child_pos + ImVec2(612.0f * s, 58.0f * s), IM_COL32(224, 199, 211, (int)(78.0f * a)), 8.0f * s);

    c_color accent = g_cfg.accent.to_color();

    for (int i = 0; i < (int)tabs.size(); i++)
    {
        tab_animation_t& info = subtab_info[tabs[0]][i];
        ImVec2 cur = ImVec2((27.0f + 80.0f * i) * s, 14.0f * s);
        ImGui::SetCursorPos(cur);
        ImVec2 tsize = ImVec2(70.0f * s, 32.0f * s);

        std::string id = std::string("##sub_") + tabs[i];
        bool clicked = ImGui::ButtonEx(id.c_str(), tsize, 0, &info.hovered);

        if (clicked)
        {
            selector = i;
            PlaySoundFeedback(8, true);
        }

        CreateAnimation(info.hovered_alpha, info.hovered, 1.0f, AnimLerp);
        CreateAnimation(info.alpha, selector == i, 0.8f, AnimSkipDisable | AnimLerp);

        ImVec2 tmin = child_pos + cur - ImVec2(8.0f * s, 0);
        ImVec2 tmax = tmin + tsize;
        ImRect bb = ImRect(tmin, tmax);

        if (selector == i)
        {
            draw_list->AddRectFilled(bb.Min, bb.Max, ImColor(217, 217, 217, (int)(20 * a * info.alpha)), 4.0f * s);
            draw_list->PushClipRect(ImVec2(bb.Min.x + 15.0f * s, bb.Max.y - 2.0f * s), ImVec2(bb.Max.x - 15.0f * s, bb.Max.y));
            draw_list->AddRectFilled(ImVec2(bb.Min.x + 15.0f * s, bb.Max.y - 2.0f * s), ImVec2(bb.Max.x - 15.0f * s, bb.Max.y + 4.0f * s), accent.new_alpha((int)(info.alpha * wa)).as_imcolor(), 2.0f * s, ImDrawCornerFlags_Top);
            draw_list->PopClipRect();
        }

        float rgb = selector == i ? 255 : 150 + 105 * info.hovered_alpha;
        c_color tc = c_color((int)rgb, (int)rgb, (int)rgb, (int)(rgb * a));
        ImVec2 ls = ImGui::CalcTextSize(tabs[i].c_str());
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(tc.r() / 255.0f, tc.g() / 255.0f, tc.b() / 255.0f, tc.a() / 255.0f));
        ImGui::RenderTextClipped(bb.Min, bb.Max, tabs[i].c_str(), NULL, &ls, style.ButtonTextAlign, &bb);
        ImGui::PopStyleColor();
    }

    ImGui::PopStyleColor(4);
    ImGui::SetCursorPos(prev);
    ImGui::ItemSize(ImVec2(0, 62.0f * s));
    ImGui::PushClipRect(child_pos + ImVec2(0.0f, 62.0f * s), child_pos + ImVec2(620.0f * s, 457.0f * s), false);
    draw_list->PushClipRect(child_pos + ImVec2(0.0f, 62.0f * s), child_pos + ImVec2(620.0f * s, 457.0f * s));
}

void Menu::UpdateSubFade(int tab, int& sub)
{
    if (sub != prev_sub[tab])
    {
        for (auto& a : item_animations)
        {
            a.second.reset();
        }

        subtab_alpha[tab] = 0.0f;
        prev_sub[tab] = sub;
    }

    CreateAnimation(subtab_alpha[tab], g_cfg.menu_open && sub == prev_sub[tab], 0.2f, AnimSkipDisable | AnimLerp);
    alpha = subtab_alpha[tab] * tab_alpha;
}

void Menu::GroupBegin(const char* label)
{
    const float s = GetScale();
    const float a = GetAlpha();
    const float cursor_x = ImGui::GetCursorPosX();
    const ImVec2 screen = ImGui::GetCursorScreenPos();
    const c_color accent = g_cfg.accent.to_color();

    draw_list->AddRectFilled(screen + ImVec2(0.0f, 2.0f * s),
        screen + ImVec2(3.0f * s, 17.0f * s),
        accent.new_alpha((int)(245.0f * a)).as_imcolor(), 2.0f * s);
    ImGui::SetCursorPosX(cursor_x + 11.0f * s);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.98f, 0.91f, 0.95f, a));
    ImGui::TextUnformatted(label);
    ImGui::PopStyleColor();
    draw_list->AddLine(screen + ImVec2(11.0f * s, 21.0f * s),
        screen + ImVec2(256.0f * s, 21.0f * s),
        IM_COL32(218, 190, 203, (int)(92.0f * a)), 1.0f * s);
    ImGui::SetCursorPosX(cursor_x);
    ImGui::ItemSize(ImVec2(0, 5.0f * s));
    ImGui::BeginGroup();
}

void Menu::GroupEnd()
{
    ImGui::EndGroup();
    ImGui::ItemSize(ImVec2(0, 9.0f * GetScale()));
}

void Menu::DrawContent()
{
    ImVec2 window_pos = GetWindowPos();
    ImVec2 prev_pos = ImGui::GetCursorPos();
    float old_alpha = GetAlpha();

    if (tab_selector != prev_tab)
    {
        for (auto& a : item_animations)
        {
            a.second.reset();
        }

        tab_alpha = 0.0f;
        prev_tab = tab_selector;
    }

    CreateAnimation(tab_alpha, g_cfg.menu_open && tab_selector == prev_tab, 0.2f, AnimSkipDisable | AnimLerp);
    alpha = tab_alpha;

    const float s = GetScale();
    ImGui::SetCursorPos(ImVec2(131.0f * s, 79.0f * s));
    ImRect window_bb = ImRect(window_pos + ImVec2(76.0f * s, 52.0f * s), window_pos + ImVec2(725.0f * s, 520.0f * s));
    ImGui::PushClipRect(window_bb.Min, window_bb.Max, false);
    ImGui::BeginChild("##tab_child", ImVec2(), true);

    if (tab_selector == 0)
    {
        DrawRage();
    }
    else if (tab_selector == 1)
    {
        DrawLegit();
    }

    else if (tab_selector == 2)
    {
        DrawVisuals();
    }

    else if (tab_selector == 3)
    {
        DrawMisc();
    }
    else if (tab_selector == 4)
    {
        DrawSettings();
    }

    ImGui::EndChild(false);
    alpha = old_alpha;
    ImGui::PopClipRect();
    ImGui::SetCursorPos(prev_pos);
}

void Menu::Draw()
{
    InitTextures();
    UpdateAlpha();
    DrawNotifications();
    CharacterPreview::SetVisible(g_cfg.show_character_model && (g_cfg.menu_open || alpha > 0.01f));

    if (!g_cfg.menu_open && alpha <= 0.01f)
    {
        return;
    }

    ApplyScale();
    ImGui::SetColorEditOptions(ImGuiColorEditFlags_PickerHueBar | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoOptions | ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_DisplayRGB);
    WindowBegin();
    DrawBackground();
    DrawTabs();
    DrawContent();
    if (g_cfg.show_character_model)
    {
        DrawCharacterPanel();
    }
    draw_list->PopClipRect();
    ImGui::PopClipRect();
    WindowEnd();
}




