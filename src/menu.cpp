#include "menu.hpp"
#include <imgui.h>
#include <array>
#include <string>

namespace {
bool open = false;
int tab = 0;
ImVec4 accent(0.55f, 0.32f, 1.0f, 1.0f);
float scale = 1.0f;
bool showClock = true, showFps = false, compactHud = false, notifications = true;
int language = 0;

void ToggleRow(const char* label, bool* value) {
    ImGui::PushID(label);
    ImGui::TextUnformatted(label);
    ImGui::SameLine(430.0f * scale);
    const ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("toggle", ImVec2(38, 20));
    if (ImGui::IsItemClicked()) *value = !*value;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p, p + ImVec2(38,20), *value ? ImGui::ColorConvertFloat4ToU32(accent) : IM_COL32(70,70,76,255), 10);
    dl->AddCircleFilled(p + ImVec2(*value ? 28 : 10,10), 7, IM_COL32_WHITE);
    ImGui::PopID();
}

void Page(const char* title, const char* subtitle) {
    ImGui::TextColored(ImVec4(1,1,1,1), "%s", title);
    ImGui::TextColored(ImVec4(.55f,.55f,.58f,1), "%s", subtitle);
    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
}
}

void Menu::Toggle() { open = !open; }
bool Menu::IsOpen() { return open; }

void Menu::Draw() {
    if (!open) return;
    ImGui::SetNextWindowSize(ImVec2(720 * scale, 520 * scale), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(.96f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 7);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0,0));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(.075f,.075f,.085f,1));
    ImGui::Begin("MirickMenu", nullptr, ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoCollapse);
    auto* dl = ImGui::GetWindowDrawList(); auto pos = ImGui::GetWindowPos();
    dl->AddRectFilled(pos, pos + ImVec2(720*scale,47*scale), IM_COL32(29,29,34,255), 7, ImDrawFlags_RoundCornersTop);
    dl->AddLine(pos+ImVec2(0,47*scale), pos+ImVec2(720*scale,47*scale), IM_COL32(255,255,255,18));
    dl->AddLine(pos+ImVec2(160*scale,47*scale), pos+ImVec2(160*scale,520*scale), IM_COL32(255,255,255,18));
    dl->AddCircleFilled(pos+ImVec2(304*scale,23*scale), 7*scale, ImGui::ColorConvertFloat4ToU32(accent));
    dl->AddText(pos+ImVec2(320*scale,14*scale), IM_COL32(225,225,230,255), "Mirick Menu");

    ImGui::SetCursorPos(ImVec2(8*scale,65*scale));
    ImGui::BeginChild("tabs", ImVec2(144*scale,440*scale));
    const char* tabs[] = {"Главная", "Интерфейс", "Уведомления", "Настройки", "Профиль"};
    for (int i=0;i<5;i++) {
        ImGui::PushStyleColor(ImGuiCol_Button, i==tab ? ImVec4(.16f,.16f,.19f,1) : ImVec4(0,0,0,0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(.20f,.20f,.23f,1));
        if (ImGui::Button(tabs[i], ImVec2(144*scale,32*scale))) tab=i;
        if (i==tab) dl->AddRectFilled(pos+ImVec2(156*scale,(65+36*i)*scale),pos+ImVec2(160*scale,(97+36*i)*scale),ImGui::ColorConvertFloat4ToU32(accent),2);
        ImGui::PopStyleColor(2); ImGui::Spacing();
    }
    ImGui::EndChild();

    ImGui::SetCursorPos(ImVec2(184*scale,70*scale));
    ImGui::BeginChild("content", ImVec2(510*scale,425*scale), ImGuiChildFlags_None);
    if (tab==0) { Page("Главная", "Безопасное меню без игровых преимуществ"); ToggleRow("Показывать часы", &showClock); ToggleRow("Показывать FPS", &showFps); ToggleRow("Компактный HUD", &compactHud); }
    else if (tab==1) { Page("Интерфейс", "Настройка внешнего вида меню"); ImGui::ColorEdit4("Акцент", &accent.x, ImGuiColorEditFlags_NoInputs); ImGui::SliderFloat("Масштаб", &scale, .80f, 1.20f, "%.0f%%", ImGuiSliderFlags_None); }
    else if (tab==2) { Page("Уведомления", "Параметры информационных сообщений"); ToggleRow("Разрешить уведомления", &notifications); }
    else if (tab==3) { Page("Настройки", "Общие параметры"); const char* langs[]={"Русский","English"}; ImGui::Combo("Язык", &language, langs, 2); ImGui::TextDisabled("INSERT — открыть или закрыть меню"); }
    else { Page("Профиль", "Локальная информация"); ImGui::Text("Mirick Menu"); ImGui::TextDisabled("Только интерфейс. Чит-функций нет."); }
    ImGui::EndChild();
    ImGui::End(); ImGui::PopStyleColor(); ImGui::PopStyleVar(2);
}
