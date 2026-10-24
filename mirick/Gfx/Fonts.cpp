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
}

void Fonts::InitStyle()
{
    ImGuiStyle& style = ImGui::GetStyle();
    style.Colors[ImGuiCol_Text] = ImVec4(0.91f, 0.95f, 1.00f, 1.00f);
    style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.43f, 0.55f, 0.68f, 1.00f);
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.02f, 0.04f, 0.08f, 0.98f);
    style.Colors[ImGuiCol_Border] = ImVec4(0.10f, 0.27f, 0.43f, 0.85f);
    style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.035f, 0.08f, 0.14f, 1.00f);
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.06f, 0.23f, 0.40f, 1.00f);
    style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.08f, 0.34f, 0.60f, 1.00f);
    style.Colors[ImGuiCol_TitleBg] = ImVec4(0.03f, 0.08f, 0.14f, 1.00f);
    style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.03f, 0.08f, 0.14f, 0.85f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.04f, 0.13f, 0.23f, 1.00f);
    style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.03f, 0.07f, 0.12f, 1.00f);
    style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.015f, 0.035f, 0.065f, 1.00f);
    style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.10f, 0.38f, 0.65f, 1.00f);
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.18f, 0.55f, 0.92f, 1.00f);
    style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.20f, 0.62f, 1.00f, 1.00f);
    style.Colors[ImGuiCol_CheckMark] = ImVec4(0.20f, 0.62f, 1.00f, 1.00f);
    style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.20f, 0.58f, 0.95f, 1.00f);
    style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.38f, 0.72f, 1.00f, 1.00f);
    style.Colors[ImGuiCol_Button] = ImVec4(0.04f, 0.12f, 0.21f, 1.00f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.07f, 0.27f, 0.47f, 1.00f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.10f, 0.42f, 0.72f, 1.00f);
    style.Colors[ImGuiCol_Header] = ImVec4(0.08f, 0.31f, 0.54f, 1.00f);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.10f, 0.39f, 0.67f, 1.00f);
    style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.14f, 0.50f, 0.84f, 1.00f);
    style.Colors[ImGuiCol_ResizeGrip] = ImVec4(0.20f, 0.58f, 0.95f, 0.55f);
    style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.30f, 0.68f, 1.00f, 0.75f);
    style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.38f, 0.74f, 1.00f, 0.95f);
    style.Colors[ImGuiCol_PlotLines] = ImVec4(0.20f, 0.62f, 1.00f, 1.00f);
    style.Colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.40f, 0.76f, 1.00f, 1.00f);
    style.Colors[ImGuiCol_PlotHistogram] = ImVec4(0.20f, 0.62f, 1.00f, 1.00f);
    style.Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.40f, 0.76f, 1.00f, 1.00f);
    style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.10f, 0.42f, 0.72f, 0.80f);
    style.Colors[ImGuiCol_PopupBg] = ImVec4(0.025f, 0.065f, 0.12f, 0.98f);
    style.Alpha = 1.0f;
    style.WindowTitleAlign = ImVec2(0.5f, 0.5f);
    style.WindowRounding = 10.0f;
    style.ChildRounding = 7.0f;
    style.FrameRounding = 5.0f;
    style.PopupRounding = 7.0f;
    style.GrabRounding = 4.0f;
    style.FramePadding = ImVec2(5.0f, 4.0f);
    style.ItemSpacing = ImVec2(8.0f, 6.0f);
    style.ScrollbarSize = 8.0f;
    style.ScrollbarRounding = 4.0f;
    style.GrabMinSize = 7.0f;
}
