#pragma once

#include "imgui.h"

struct Fonts
{
    ImFont* main = nullptr;
    ImFont* dmg = nullptr;
    ImFont* title = nullptr;
    void Init();
    static void InitStyle();
};

extern Fonts g_fonts;
