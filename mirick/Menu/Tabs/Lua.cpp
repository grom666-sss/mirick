#include "Menu/Menu.hpp"

#include <algorithm>
#include <cstring>
#include <shellapi.h>
#include <string>
#include <string_view>

#include "Lua/LuaManager.hpp"

namespace
{
    int ResizeLuaText(ImGuiInputTextCallbackData* data)
    {
        if (data->EventFlag != ImGuiInputTextFlags_CallbackResize)
        {
            return 0;
        }

        auto* text = static_cast<std::string*>(data->UserData);
        text->resize(static_cast<size_t>(data->BufTextLen));
        data->Buf = text->data();
        return 0;
    }

    bool LuaTextEditor(std::string& text, const ImVec2& size)
    {
        // This mirrors ImGui's std::string adapter: use the allocated capacity
        // directly and update the logical string length after editing.
        const bool changed = ImGui::InputTextMultiline(
            "##lua_workspace_editor",
            text.data(),
            text.capacity() + 1,
            size,
            ImGuiInputTextFlags_AllowTabInput | ImGuiInputTextFlags_CallbackResize,
            ResizeLuaText,
            &text);
        text.resize(std::strlen(text.c_str()));
        return changed;
    }

    std::string CreateEditorBuffer()
    {
        std::string text =
            "-- mirick Lua workspace\n"
            "mirick.log(\"hello from the in-game editor\")\n";
        text.reserve(64 * 1024);
        return text;
    }

    size_t CountLines(std::string_view text)
    {
        if (text.empty())
        {
            return 0;
        }

        return 1 + static_cast<size_t>(std::count(text.begin(), text.end(), '\n'));
    }
}

void Menu::DrawLua()
{
    const float s = GetScale();
    const float a = GetAlpha();
    const c_color accent = g_cfg.accent.to_color();
    static std::string code = CreateEditorBuffer();
    static bool last_execution_ok = true;

    ImGui::SetCursorPos(ImVec2(16.0f * s, 15.0f * s));
    const ImVec2 header_min = ImGui::GetCursorScreenPos();
    const float content_width = ImGui::GetContentRegionAvail().x - 16.0f * s;
    const ImVec2 header_max = header_min + ImVec2(content_width, 55.0f * s);

    draw_list->AddRectFilled(header_min, header_max, IM_COL32(17, 22, 34, static_cast<int>(245.0f * a)), 8.0f * s);
    draw_list->AddRectFilled(header_min, ImVec2(header_min.x + 4.0f * s, header_max.y), accent.new_alpha(static_cast<int>(235.0f * a)).as_imcolor(), 8.0f * s, ImDrawCornerFlags_Left);
    draw_list->AddCircleFilled(header_min + ImVec2(24.0f * s, 27.0f * s), 8.0f * s, accent.new_alpha(static_cast<int>(220.0f * a)).as_imcolor());
    draw_list->AddText(header_min + ImVec2(44.0f * s, 10.0f * s), IM_COL32(242, 245, 255, static_cast<int>(255.0f * a)), tr("РЕДАКТОР LUA", "LUA WORKSPACE"));
    draw_list->AddText(header_min + ImVec2(44.0f * s, 31.0f * s), IM_COL32(132, 143, 165, static_cast<int>(255.0f * a)), tr("Выполнение скриптов в реальном времени", "Real-time script execution"));
    ImGui::Dummy(ImVec2(content_width, 66.0f * s));

    const size_t bytes = code.size();
    const std::string statistics = std::to_string(CountLines(code)) + tr(" строк  •  ", " lines  •  ") + std::to_string(bytes) + tr(" байт", " bytes");
    ImGui::TextColored(ImVec4(0.72f, 0.76f, 0.86f, a), "%s", tr("КОД СКРИПТА", "SCRIPT CODE"));
    ImGui::SameLine();
    const float stats_width = ImGui::CalcTextSize(statistics.c_str()).x;
    const float statistics_x = ImGui::GetWindowContentRegionWidth() - stats_width - 18.0f * s;
    if (ImGui::GetCursorPosX() < statistics_x)
    {
        ImGui::SetCursorPosX(statistics_x);
    }
    ImGui::TextColored(ImVec4(0.42f, 0.47f, 0.58f, a), "%s", statistics.c_str());

    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.025f, 0.035f, 0.055f, a));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.035f, 0.050f, 0.078f, a));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.035f, 0.050f, 0.078f, a));
    ImGui::PushStyleColor(ImGuiCol_Border, accent.new_alpha(static_cast<int>(95.0f * a)).as_imvec4());
    ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, accent.new_alpha(static_cast<int>(110.0f * a)).as_imvec4());
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 7.0f * s);
    LuaTextEditor(code, ImVec2(content_width, 245.0f * s));
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(5);

    auto action_button = [&](const char* label, const ImVec2& size, bool primary)
    {
        const ImVec4 base = primary ? accent.new_alpha(static_cast<int>(220.0f * a)).as_imvec4() : ImVec4(0.075f, 0.095f, 0.14f, a);
        const ImVec4 hovered = primary ? accent.new_alpha(static_cast<int>(255.0f * a)).as_imvec4() : ImVec4(0.11f, 0.14f, 0.20f, a);
        ImGui::PushStyleColor(ImGuiCol_Button, base);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hovered);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(hovered.x * 0.82f, hovered.y * 0.82f, hovered.z * 0.82f, hovered.w));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f * s);
        const bool pressed = ImGui::Button(label, size);
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);
        return pressed;
    };

    const float gap = 7.0f * s;
    const float button_width = (content_width - gap * 3.0f) / 4.0f;
    if (action_button(tr("Выполнить", "Execute"), ImVec2(button_width, 34.0f * s), true))
    {
        last_execution_ok = LuaManager::Instance().Execute(code);
    }
    ImGui::SameLine(0.0f, gap);
    if (action_button(tr("Вставить", "Paste"), ImVec2(button_width, 34.0f * s), false))
    {
        if (const char* clipboard = ImGui::GetClipboardText())
        {
            code.assign(clipboard);
            if (code.capacity() < 64 * 1024)
            {
                code.reserve(64 * 1024);
            }
        }
    }
    ImGui::SameLine(0.0f, gap);
    if (action_button(tr("Очистить", "Clear"), ImVec2(button_width, 34.0f * s), false))
    {
        code.clear();
        if (code.capacity() < 64 * 1024)
        {
            code.reserve(64 * 1024);
        }
    }
    ImGui::SameLine(0.0f, gap);
    if (action_button(tr("Файлы", "Reload"), ImVec2(button_width, 34.0f * s), false))
    {
        LuaManager::Instance().Reload();
        last_execution_ok = true;
    }

    if (action_button(tr("Папка скриптов", "Scripts folder"), ImVec2((content_width - gap) * 0.5f, 30.0f * s), false))
    {
        const std::string folder = config_t::ConfigDir() + "\\scripts";
        CreateDirectoryA(folder.c_str(), nullptr);
        ShellExecuteA(NULL, "open", folder.c_str(), NULL, NULL, SW_SHOWNORMAL);
    }
    ImGui::SameLine(0.0f, gap);
    if (action_button(tr("Журнал Lua", "Lua log"), ImVec2((content_width - gap) * 0.5f, 30.0f * s), false))
    {
        const std::string log = config_t::ConfigDir() + "\\lua.log";
        ShellExecuteA(NULL, "open", log.c_str(), NULL, NULL, SW_SHOWNORMAL);
    }

    const std::string& message = LuaManager::Instance().GetLastMessage();
    if (!message.empty())
    {
        const ImVec4 status_color = last_execution_ok ? ImVec4(0.35f, 0.88f, 0.62f, a) : ImVec4(1.0f, 0.42f, 0.42f, a);
        ImGui::TextColored(status_color, "%s  %s", last_execution_ok ? "OK" : "!", message.c_str());
    }
}
