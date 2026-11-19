#include "Gfx/Fonts.hpp"

#include "Assets/Assets.hpp"

Fonts g_fonts;

void Fonts::Init()
{
    ImGuiIO& io = ImGui::GetIO();
    ImFontConfig cfg;
    cfg.OversampleH = 3;
    cfg.OversampleV = 3;
    main = io.Fonts->AddFontFromMemoryTTF(SFUIDisplay_SemiBold, sizeof(SFUIDisplay_SemiBold), 14.0f, &cfg, io.Fonts->GetGlyphRangesCyrillic());
    dmg = io.Fonts->AddFontFromMemoryTTF(SFUIDisplay_SemiBold, sizeof(SFUIDisplay_SemiBold), 18.0f, &cfg, io.Fonts->GetGlyphRangesCyrillic());
    title = io.Fonts->AddFontFromMemoryTTF(SFUIDisplay_SemiBold, sizeof(SFUIDisplay_SemiBold), 26.0f, &cfg, io.Fonts->GetGlyphRangesCyrillic());
}

void Fonts::InitStyle()
{
    ImGuiStyle& style = ImGui::GetStyle();
    style.Colors[ImGuiCol_Text] = ImVec4(0.97f, 0.94f, 0.96f, 1.00f);
    style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.79f, 0.70f, 0.75f, 1.00f);
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.30f, 0.21f, 0.26f, 0.98f);
    style.Colors[ImGuiCol_Border] = ImVec4(0.84f, 0.72f, 0.78f, 0.48f);
    style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.49f, 0.41f, 0.45f, 0.72f);
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.58f, 0.47f, 0.52f, 0.86f);
    style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.68f, 0.53f, 0.59f, 0.96f);
    style.Colors[ImGuiCol_TitleBg] = ImVec4(0.35f, 0.25f, 0.31f, 1.00f);
    style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.35f, 0.25f, 0.31f, 0.88f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.43f, 0.31f, 0.37f, 1.00f);
    style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.35f, 0.25f, 0.31f, 1.00f);
    style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.25f, 0.17f, 0.21f, 0.70f);
    style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.72f, 0.54f, 0.62f, 0.88f);
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.87f, 0.68f, 0.77f, 1.00f);
    style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.92f, 0.70f, 0.78f, 1.00f);
    style.Colors[ImGuiCol_CheckMark] = ImVec4(1.00f, 0.98f, 0.99f, 1.00f);
    style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.92f, 0.70f, 0.78f, 1.00f);
    style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(1.00f, 0.83f, 0.89f, 1.00f);
    style.Colors[ImGuiCol_Button] = ImVec4(0.49f, 0.40f, 0.45f, 0.72f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.62f, 0.48f, 0.55f, 0.90f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.73f, 0.55f, 0.63f, 1.00f);
    style.Colors[ImGuiCol_Header] = ImVec4(0.57f, 0.44f, 0.48f, 0.88f);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.68f, 0.52f, 0.59f, 0.96f);
    style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.76f, 0.58f, 0.66f, 1.00f);
    style.Colors[ImGuiCol_ResizeGrip] = ImVec4(0.92f, 0.70f, 0.78f, 0.55f);
    style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(1.00f, 0.82f, 0.88f, 0.78f);
    style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(1.00f, 0.88f, 0.92f, 0.95f);
    style.Colors[ImGuiCol_PlotLines] = ImVec4(0.92f, 0.70f, 0.78f, 1.00f);
    style.Colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.84f, 0.90f, 1.00f);
    style.Colors[ImGuiCol_PlotHistogram] = ImVec4(0.92f, 0.70f, 0.78f, 1.00f);
    style.Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.84f, 0.90f, 1.00f);
    style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.92f, 0.70f, 0.78f, 0.52f);
    style.Colors[ImGuiCol_PopupBg] = ImVec4(0.31f, 0.22f, 0.27f, 0.99f);
    style.Alpha = 1.0f;
    style.WindowTitleAlign = ImVec2(0.5f, 0.5f);
    style.WindowRounding = 12.0f;
    style.ChildRounding = 8.0f;
    style.FrameRounding = 7.0f;
    style.PopupRounding = 8.0f;
    style.GrabRounding = 6.0f;
    style.FramePadding = ImVec2(5.0f, 4.0f);
    style.ItemSpacing = ImVec2(8.0f, 6.0f);
    style.ScrollbarSize = 8.0f;
    style.ScrollbarRounding = 5.0f;
    style.GrabMinSize = 7.0f;
}
