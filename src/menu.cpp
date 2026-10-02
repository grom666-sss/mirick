// Mirick Menu — визуальное меню в стиле ForkHack (https://github.com/gabrik1337/ForkHack).
// Реализация полностью самостоятельная: используется только публичный API Dear ImGui,
// никаких игровых функций, работы с памятью игры или читов здесь нет.

#include "menu.hpp"

#include <windows.h>

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

// ---------------------------------------------------------------------------
// Базовая геометрия окна (в «логических» пикселях, умножается на масштаб)
// ---------------------------------------------------------------------------
constexpr float kWidth = 720.0f;
constexpr float kHeight = 520.0f;
constexpr float kHeader = 47.0f;
constexpr float kSidebar = 160.0f;
constexpr float kShadow = 22.0f; // запас вокруг окна под тень
constexpr float kItemWidth = 248.0f;

// ---------------------------------------------------------------------------
// Анимации
// ---------------------------------------------------------------------------
struct ItemAnim {
    float hovered = 0.0f;
    float active = 0.0f;
};

struct TabAnim {
    float hovered = 0.0f;
    float selected = 0.0f;
};

void Animate(float& value, bool condition, float speed) {
    const float dt = ImGui::GetIO().DeltaTime;
    const float step = std::clamp(dt * speed, 0.0f, 1.0f);
    value += ((condition ? 1.0f : 0.0f) - value) * step;
    value = std::clamp(value, 0.0f, 1.0f);
}

// ---------------------------------------------------------------------------
// Состояние меню и демонстрационные настройки (только интерфейс)
// ---------------------------------------------------------------------------
struct State {
    bool open = false;
    float alpha = 0.0f;
    float scale = 1.0f;
    int tab = 0;
    std::array<int, 5> subtab{};
    std::array<TabAnim, 5> tabAnim{};
    std::array<std::array<TabAnim, 4>, 5> subAnim{};
    std::unordered_map<std::string, ItemAnim> items;
    float tabFade = 1.0f;
    int prevTab = 0;
    int prevSub = 0;
    bool centered = false;
    bool dragging = false;
    int toggleKey = VK_INSERT;
    int capturingKey = 0; // 0 — нет, 1 — ждём отпускания, 2 — ловим клавишу

    // Акцент интерфейса
    float accent[4] = {0.54f, 0.33f, 1.00f, 1.00f};

    // Демонстрационные (чисто визуальные) настройки
    bool showClock = true;
    bool showFps = false;
    bool compactHud = false;
    bool watermark = true;
    int watermarkPos = 0;
    float hudOpacity = 85.0f;
    int hudSpacing = 6;

    bool blurBackground = true;
    bool roundedCorners = true;
    bool animations = true;
    float animSpeed = 1.0f;
    int font = 0;

    bool notifications = true;
    bool notifySound = false;
    int notifyCorner = 1;
    float notifyTime = 4.0f;
    bool notifyConfig = true;
    bool notifyHotkeys = true;

    int language = 0;
    bool saveOnExit = true;
    bool showCursor = true;
    char profileName[32] = "default";
};

State g;

// ---------------------------------------------------------------------------
// Вспомогательные функции
// ---------------------------------------------------------------------------
float S() { return g.scale; }
float A() { return g.alpha * g.tabFade; }

ImU32 Col(int r, int gr, int b, float alpha255) {
    return IM_COL32(r, gr, b, (int)std::clamp(alpha255 * A(), 0.0f, 255.0f));
}

ImU32 Accent(float alpha255 = 255.0f) {
    return IM_COL32((int)(g.accent[0] * 255.0f), (int)(g.accent[1] * 255.0f), (int)(g.accent[2] * 255.0f),
                    (int)std::clamp(alpha255 * g.accent[3] * A(), 0.0f, 255.0f));
}

ItemAnim& Anim(const char* key) { return g.items[key]; }

const char* Tr(const char* ru, const char* en) { return g.language == 1 ? en : ru; }

const char* KeyName(int vk) {
    static char buffer[64];
    if (vk <= 0) return "—";
    switch (vk) {
        case VK_INSERT: return "INSERT";
        case VK_DELETE: return "DELETE";
        case VK_HOME: return "HOME";
        case VK_END: return "END";
        case VK_PRIOR: return "PAGE UP";
        case VK_NEXT: return "PAGE DOWN";
        case VK_LBUTTON: return "MOUSE 1";
        case VK_RBUTTON: return "MOUSE 2";
        case VK_MBUTTON: return "MOUSE 3";
        default: break;
    }
    const UINT scan = MapVirtualKeyA((UINT)vk, MAPVK_VK_TO_VSC);
    if (scan && GetKeyNameTextA((LONG)(scan << 16), buffer, (int)sizeof(buffer)) > 0) return buffer;
    std::snprintf(buffer, sizeof(buffer), "0x%02X", vk);
    return buffer;
}

void TextAt(ImDrawList* dl, ImVec2 pos, ImU32 color, const char* text) { dl->AddText(pos, color, text); }

void TextCentered(ImDrawList* dl, ImVec2 min, ImVec2 max, ImU32 color, const char* text) {
    const ImVec2 size = ImGui::CalcTextSize(text);
    dl->AddText(ImVec2(min.x + (max.x - min.x - size.x) * 0.5f, min.y + (max.y - min.y - size.y) * 0.5f), color, text);
}

// Простые векторные иконки вкладок (без внешних ресурсов)
void DrawTabIcon(ImDrawList* dl, int index, ImVec2 c, ImU32 color) {
    const float s = S();
    switch (index) {
        case 0: // домик
            dl->AddTriangleFilled(c + ImVec2(0, -7 * s), c + ImVec2(-8 * s, 0), c + ImVec2(8 * s, 0), color);
            dl->AddRectFilled(c + ImVec2(-5 * s, 0), c + ImVec2(5 * s, 7 * s), color, 1.5f * s);
            break;
        case 1: // слои
            dl->AddQuadFilled(c + ImVec2(0, -7 * s), c + ImVec2(8 * s, -2 * s), c + ImVec2(0, 3 * s), c + ImVec2(-8 * s, -2 * s), color);
            dl->AddQuad(c + ImVec2(0, -1 * s), c + ImVec2(8 * s, 4 * s), c + ImVec2(0, 9 * s), c + ImVec2(-8 * s, 4 * s), color, 1.4f * s);
            break;
        case 2: // колокольчик
            dl->AddCircleFilled(c + ImVec2(0, -1 * s), 6.0f * s, color, 16);
            dl->AddRectFilled(c + ImVec2(-7 * s, 1 * s), c + ImVec2(7 * s, 4 * s), color, 1.5f * s);
            dl->AddCircleFilled(c + ImVec2(0, 7 * s), 2.2f * s, color, 10);
            break;
        case 3: // шестерёнка
            dl->AddCircleFilled(c, 7.0f * s, color, 8);
            dl->AddCircleFilled(c, 3.0f * s, IM_COL32(24, 24, 29, (int)(255 * A())), 12);
            break;
        default: // профиль
            dl->AddCircleFilled(c + ImVec2(0, -3.5f * s), 3.4f * s, color, 14);
            dl->AddRectFilled(c + ImVec2(-6 * s, 1.5f * s), c + ImVec2(6 * s, 8 * s), color, 3.0f * s);
            break;
    }
}

// ---------------------------------------------------------------------------
// Виджеты в стилистике исходного интерфейса
// ---------------------------------------------------------------------------
void GroupLabel(const char* label) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 p = ImGui::GetCursorScreenPos();
    TextAt(dl, p, Col(255, 255, 255, 235), label);
    ImGui::Dummy(ImVec2(kItemWidth * S(), ImGui::GetTextLineHeight() + 6.0f * S()));
}

void Caption(const char* text) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 p = ImGui::GetCursorScreenPos();
    TextAt(dl, p, Col(255, 255, 255, 105), text);
    ImGui::Dummy(ImVec2(kItemWidth * S(), ImGui::GetTextLineHeight() + 4.0f * S()));
}

// Фон строки виджета: лёгкая подсветка при наведении
void RowBackground(ImDrawList* dl, ImVec2 min, ImVec2 max, float hovered) {
    const float base = 20.0f + 30.0f * hovered;
    const int tone = 217 + (int)(38 * hovered) > 255 ? 255 : 217 + (int)(38 * hovered);
    dl->AddRectFilled(min, max, Col(tone, tone, tone, base), 4.0f * S());
}

bool Checkbox(const char* label, bool* value) {
    const float s = S();
    ImGui::PushID(label);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const ImVec2 size(kItemWidth * s, 32.0f * s);

    ImGui::InvisibleButton("##row", size);
    const bool hovered = ImGui::IsItemHovered();
    const bool pressed = ImGui::IsItemClicked();
    if (pressed) *value = !*value;

    ItemAnim& anim = Anim(label);
    Animate(anim.hovered, hovered, 14.0f * g.animSpeed);
    Animate(anim.active, *value, 12.0f * g.animSpeed);

    RowBackground(dl, p, p + size, anim.hovered);

    const ImVec2 bodyMin = p + ImVec2(size.x - 40.0f * s, 9.0f * s);
    const ImVec2 bodyMax = bodyMin + ImVec2(28.0f * s, 14.0f * s);
    dl->AddRectFilled(bodyMin, bodyMax, Col(0, 0, 0, 80), 7.0f * s);
    dl->AddRectFilled(bodyMin, bodyMax, Accent(190.0f * anim.active), 7.0f * s);

    const ImVec2 knob = ImVec2(bodyMin.x + 7.0f * s + 14.0f * s * anim.active, (bodyMin.y + bodyMax.y) * 0.5f);
    dl->AddCircleFilled(knob, 5.0f * s, Col(255, 255, 255, 160 + 95 * anim.active), 18);

    TextAt(dl, p + ImVec2(12.0f * s, 8.0f * s), Col(255, 255, 255, 130 + 110 * anim.active), label);
    ImGui::PopID();
    return pressed;
}

bool SliderBase(const char* label, float* value, float min, float max, const char* fmt, bool integer) {
    const float s = S();
    ImGui::PushID(label);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const ImVec2 size(kItemWidth * s, 54.0f * s);

    ImGui::InvisibleButton("##row", size);
    const bool hovered = ImGui::IsItemHovered();
    const bool active = ImGui::IsItemActive();

    ItemAnim& anim = Anim(label);
    Animate(anim.hovered, hovered || active, 14.0f * g.animSpeed);
    Animate(anim.active, active, 16.0f * g.animSpeed);

    const ImVec2 trackMin = p + ImVec2(12.0f * s, 36.0f * s);
    const ImVec2 trackMax = p + ImVec2(size.x - 12.0f * s, 42.0f * s);

    bool changed = false;
    if (active) {
        const float t = std::clamp((ImGui::GetIO().MousePos.x - trackMin.x) / (trackMax.x - trackMin.x), 0.0f, 1.0f);
        float next = min + (max - min) * t;
        if (integer) next = std::round(next);
        if (next != *value) {
            *value = next;
            changed = true;
        }
    }

    const float t = (max > min) ? std::clamp((*value - min) / (max - min), 0.0f, 1.0f) : 0.0f;

    RowBackground(dl, p, p + size, anim.hovered);
    dl->AddRectFilled(trackMin, trackMax, Col(0, 0, 0, 80), 3.0f * s);

    const float fillX = trackMin.x + (trackMax.x - trackMin.x) * t;
    if (fillX > trackMin.x) {
        dl->AddRectFilledMultiColor(trackMin, ImVec2(fillX, trackMax.y), Accent(110.0f), Accent(255.0f), Accent(255.0f), Accent(110.0f));
    }

    const ImVec2 knob(fillX, (trackMin.y + trackMax.y) * 0.5f);
    dl->AddCircleFilled(knob, (7.0f + anim.active) * s, Col(70, 70, 76, 255), 20);
    dl->AddCircleFilled(knob, (4.0f + anim.active) * s, Col(255, 255, 255, 255), 20);

    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), fmt, integer ? (double)(int)*value : (double)*value);
    const ImVec2 textSize = ImGui::CalcTextSize(buffer);

    TextAt(dl, p + ImVec2(12.0f * s, 8.0f * s), Col(255, 255, 255, 130 + 80 * anim.hovered), label);
    TextAt(dl, ImVec2(p.x + size.x - 12.0f * s - textSize.x, p.y + 8.0f * s), Col(255, 255, 255, 215), buffer);

    ImGui::PopID();
    return changed;
}

bool SliderFloat(const char* label, float* value, float min, float max, const char* fmt = "%.2f") {
    return SliderBase(label, value, min, max, fmt, false);
}

bool SliderInt(const char* label, int* value, int min, int max, const char* fmt = "%.0f") {
    float temp = (float)*value;
    const bool changed = SliderBase(label, &temp, (float)min, (float)max, fmt, true);
    if (changed) *value = (int)temp;
    return changed;
}

bool Combo(const char* label, int* current, const char* const items[], int count) {
    const float s = S();
    ImGui::PushID(label);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const ImVec2 size(kItemWidth * s, 54.0f * s);

    ImGui::InvisibleButton("##row", size);
    const bool hovered = ImGui::IsItemHovered();
    ItemAnim& anim = Anim(label);
    Animate(anim.hovered, hovered, 14.0f * g.animSpeed);

    if (ImGui::IsItemClicked()) ImGui::OpenPopup("##combo_popup");

    RowBackground(dl, p, p + size, anim.hovered);
    TextAt(dl, p + ImVec2(12.0f * s, 8.0f * s), Col(255, 255, 255, 130 + 80 * anim.hovered), label);

    const ImVec2 boxMin = p + ImVec2(12.0f * s, 28.0f * s);
    const ImVec2 boxMax = p + ImVec2(size.x - 12.0f * s, 46.0f * s);
    dl->AddRectFilled(boxMin, boxMax, Col(0, 0, 0, 80), 3.0f * s);
    TextAt(dl, boxMin + ImVec2(8.0f * s, 1.0f * s), Col(255, 255, 255, 220), items[*current]);

    const ImVec2 arrow(boxMax.x - 12.0f * s, (boxMin.y + boxMax.y) * 0.5f);
    dl->AddTriangleFilled(arrow + ImVec2(-4 * s, -2 * s), arrow + ImVec2(4 * s, -2 * s), arrow + ImVec2(0, 3 * s), Accent(230.0f));

    bool changed = false;
    ImGui::SetNextWindowPos(ImVec2(boxMin.x, boxMax.y + 4.0f * s));
    ImGui::SetNextWindowSize(ImVec2(boxMax.x - boxMin.x, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4.0f * s, 4.0f * s));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 4.0f * s);
    ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.08f, 0.08f, 0.10f, 0.98f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.0f, 1.0f, 1.0f, 0.08f));
    if (ImGui::BeginPopup("##combo_popup")) {
        for (int i = 0; i < count; ++i) {
            const bool selected = (i == *current);
            ImGui::PushStyleColor(ImGuiCol_Text, selected ? ImVec4(1, 1, 1, 1) : ImVec4(1, 1, 1, 0.55f));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(1, 1, 1, 0.08f));
            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(g.accent[0], g.accent[1], g.accent[2], 0.30f));
            if (ImGui::Selectable(items[i], selected, 0, ImVec2(0.0f, 22.0f * s))) {
                *current = i;
                changed = true;
            }
            ImGui::PopStyleColor(3);
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);

    ImGui::PopID();
    return changed;
}

bool Button(const char* label) {
    const float s = S();
    ImGui::PushID(label);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const ImVec2 size(kItemWidth * s, 30.0f * s);

    ImGui::InvisibleButton("##row", size);
    const bool hovered = ImGui::IsItemHovered();
    const bool held = ImGui::IsItemActive();
    const bool pressed = ImGui::IsItemClicked();

    ItemAnim& anim = Anim(label);
    Animate(anim.hovered, hovered, 14.0f * g.animSpeed);
    Animate(anim.active, held, 18.0f * g.animSpeed);

    dl->AddRectFilled(p, p + size, Col(217, 217, 217, 18 + 22 * anim.hovered), 4.0f * s);
    dl->AddRectFilled(p, p + size, Accent(45.0f + 70.0f * anim.hovered), 4.0f * s);
    dl->AddRect(p, p + size, Accent(60.0f + 120.0f * anim.hovered), 4.0f * s);
    TextCentered(dl, p, p + size, Col(255, 255, 255, 200 + 55 * anim.hovered), label);

    ImGui::PopID();
    return pressed;
}

bool ColorRow(const char* label, float color[4]) {
    const float s = S();
    ImGui::PushID(label);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const ImVec2 size(kItemWidth * s, 32.0f * s);

    ImGui::SetCursorScreenPos(p);
    ImGui::InvisibleButton("##row", size);
    const bool hovered = ImGui::IsItemHovered();
    ItemAnim& anim = Anim(label);
    Animate(anim.hovered, hovered, 14.0f * g.animSpeed);
    RowBackground(dl, p, p + size, anim.hovered);
    TextAt(dl, p + ImVec2(12.0f * s, 8.0f * s), Col(255, 255, 255, 130 + 80 * anim.hovered), label);

    ImGui::SetCursorScreenPos(ImVec2(p.x + size.x - 42.0f * s, p.y + 7.0f * s));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f * s);
    const bool changed = ImGui::ColorEdit4("##picker", color,
                                           ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel |
                                               ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreviewHalf);
    ImGui::PopStyleVar();

    ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + size.y + ImGui::GetStyle().ItemSpacing.y));
    ImGui::PopID();
    return changed;
}

bool KeybindRow(const char* label, int* key) {
    const float s = S();
    ImGui::PushID(label);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const ImVec2 size(kItemWidth * s, 32.0f * s);

    ImGui::InvisibleButton("##row", size);
    const bool hovered = ImGui::IsItemHovered();
    ItemAnim& anim = Anim(label);
    Animate(anim.hovered, hovered, 14.0f * g.animSpeed);

    if (ImGui::IsItemClicked() && g.capturingKey == 0) g.capturingKey = 1;

    bool changed = false;
    if (g.capturingKey == 1 && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        g.capturingKey = 2;
    } else if (g.capturingKey == 2) {
        if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) {
            g.capturingKey = 0;
        } else {
            for (int vk = 0x01; vk <= 0xFE; ++vk) {
                if (vk == VK_LBUTTON || vk == VK_ESCAPE) continue;
                if (GetAsyncKeyState(vk) & 0x8000) {
                    *key = vk;
                    g.capturingKey = 0;
                    changed = true;
                    break;
                }
            }
        }
    }

    RowBackground(dl, p, p + size, anim.hovered);
    TextAt(dl, p + ImVec2(12.0f * s, 8.0f * s), Col(255, 255, 255, 130 + 80 * anim.hovered), label);

    const ImVec2 boxMin = p + ImVec2(size.x - 86.0f * s, 5.0f * s);
    const ImVec2 boxMax = p + ImVec2(size.x - 8.0f * s, 27.0f * s);
    dl->AddRectFilled(boxMin, boxMax, Col(0, 0, 0, 80), 3.0f * s);
    TextCentered(dl, boxMin, boxMax, g.capturingKey ? Accent(255.0f) : Col(255, 255, 255, 215),
                 g.capturingKey ? "..." : KeyName(*key));

    ImGui::PopID();
    return changed;
}

void TextRow(const char* left, const char* right) {
    const float s = S();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const ImVec2 size(kItemWidth * s, 28.0f * s);
    const ImVec2 rightSize = ImGui::CalcTextSize(right);
    dl->AddRectFilled(p, p + size, Col(217, 217, 217, 14), 4.0f * s);
    TextAt(dl, p + ImVec2(12.0f * s, 6.0f * s), Col(255, 255, 255, 130), left);
    TextAt(dl, ImVec2(p.x + size.x - 12.0f * s - rightSize.x, p.y + 6.0f * s), Col(255, 255, 255, 225), right);
    ImGui::Dummy(size);
}

// ---------------------------------------------------------------------------
// Шапка, вкладки, подвкладки
// ---------------------------------------------------------------------------
void DrawFrame(ImDrawList* dl, ImVec2 wp) {
    const float s = S();
    const ImVec2 wmax = wp + ImVec2(kWidth * s, kHeight * s);
    const float round = g.roundedCorners ? 6.0f * s : 0.0f;

    // Тень
    for (int i = 6; i > 0; --i) {
        const float o = (float)i * 2.5f * s;
        dl->AddRect(wp - ImVec2(o, o), wmax + ImVec2(o, o), Col(0, 0, 0, 10), round + o, 0, 1.6f * s);
    }

    // Корпус
    dl->AddRectFilled(wp, wmax, Col(16, 16, 19, g.blurBackground ? 242.0f : 255.0f), round);
    // Шапка
    dl->AddRectFilled(wp, ImVec2(wmax.x, wp.y + kHeader * s), Col(26, 26, 31, 255), round, ImDrawFlags_RoundCornersTop);
    // Левая колонка
    dl->AddRectFilled(ImVec2(wp.x, wp.y + kHeader * s), ImVec2(wp.x + kSidebar * s, wmax.y), Col(21, 21, 25, 255), round,
                      ImDrawFlags_RoundCornersBottomLeft);

    dl->AddLine(ImVec2(wp.x, wp.y + kHeader * s), ImVec2(wmax.x, wp.y + kHeader * s), Col(255, 255, 255, 14), 1.0f * s);
    dl->AddLine(ImVec2(wp.x + kSidebar * s, wp.y + kHeader * s), ImVec2(wp.x + kSidebar * s, wmax.y), Col(255, 255, 255, 14), 1.0f * s);
    dl->AddRect(wp, wmax, Col(120, 120, 130, 90), round, 0, 1.0f * s);

    // Логотип и название по центру шапки
    const char* title = "MIRICK";
    const char* sub = Tr("меню", "menu");
    const ImVec2 titleSize = ImGui::CalcTextSize(title);
    const ImVec2 subSize = ImGui::CalcTextSize(sub);
    const float block = 18.0f * s + titleSize.x + 6.0f * s + subSize.x;
    const float startX = wp.x + (kWidth * s - block) * 0.5f;
    const float cy = wp.y + kHeader * s * 0.5f;

    dl->AddNgonFilled(ImVec2(startX + 6.0f * s, cy), 7.0f * s, Accent(255.0f), 4);
    dl->AddNgon(ImVec2(startX + 6.0f * s, cy), 11.0f * s, Accent(110.0f), 4, 1.4f * s);
    TextAt(dl, ImVec2(startX + 18.0f * s, cy - titleSize.y * 0.5f), Col(255, 255, 255, 235), title);
    TextAt(dl, ImVec2(startX + 18.0f * s + titleSize.x + 6.0f * s, cy - subSize.y * 0.5f), Col(255, 255, 255, 95), sub);

    // Подпись справа
    const char* hint = Tr("INSERT — скрыть", "INSERT — hide");
    const ImVec2 hintSize = ImGui::CalcTextSize(hint);
    TextAt(dl, ImVec2(wmax.x - hintSize.x - 14.0f * s, cy - hintSize.y * 0.5f), Col(255, 255, 255, 70), hint);
}

void DrawTabs(ImDrawList* dl, ImVec2 wp) {
    const float s = S();
    const char* names[5] = {Tr("Главная", "Home"), Tr("Интерфейс", "Interface"), Tr("Уведомления", "Alerts"),
                            Tr("Настройки", "Settings"), Tr("Профиль", "Profile")};

    for (int i = 0; i < 5; ++i) {
        TabAnim& anim = g.tabAnim[i];
        ImGui::SetCursorScreenPos(wp + ImVec2(8.0f * s, (kHeader + 16.0f + 40.0f * i) * s));
        ImGui::PushID(i);
        ImGui::InvisibleButton("##tab", ImVec2(144.0f * s, 32.0f * s));
        const bool hovered = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked()) g.tab = i;
        ImGui::PopID();

        Animate(anim.hovered, hovered, 14.0f * g.animSpeed);
        Animate(anim.selected, g.tab == i, 12.0f * g.animSpeed);

        const ImVec2 tmin = wp + ImVec2(8.0f * s, (kHeader + 16.0f + 40.0f * i) * s);
        const ImVec2 tmax = tmin + ImVec2(144.0f * s, 32.0f * s);

        if (anim.selected > 0.01f || anim.hovered > 0.01f) {
            dl->AddRectFilled(tmin, tmax, Col(255, 255, 255, 10 * anim.selected + 8 * anim.hovered), 4.0f * s);
        }
        if (anim.selected > 0.01f) {
            dl->AddRectFilled(ImVec2(tmax.x - 2.0f * s, tmin.y + 4.0f * s), ImVec2(tmax.x + 2.0f * s, tmax.y - 4.0f * s),
                              Accent(255.0f * anim.selected), 2.0f * s);
        }

        const float tone = 150.0f + 105.0f * std::max(anim.selected, anim.hovered);
        const ImU32 color = Col(255, 255, 255, tone);
        DrawTabIcon(dl, i, tmin + ImVec2(20.0f * s, 16.0f * s), color);
        TextAt(dl, tmin + ImVec2(38.0f * s, 8.0f * s), color, names[i]);
    }
}

void DrawSubTabs(ImDrawList* dl, ImVec2 wp, const std::vector<const char*>& tabs) {
    const float s = S();
    const ImVec2 barMin = wp + ImVec2((kSidebar + 18.0f) * s, (kHeader + 15.0f) * s);
    const ImVec2 barMax = barMin + ImVec2(524.0f * s, 38.0f * s);
    dl->AddRectFilled(barMin, barMax, Col(217, 217, 217, 16), 4.0f * s);

    const float width = 118.0f * s;
    for (int i = 0; i < (int)tabs.size(); ++i) {
        TabAnim& anim = g.subAnim[g.tab][i];
        const ImVec2 tmin = barMin + ImVec2(width * i, 0.0f);
        const ImVec2 tmax = ImVec2(tmin.x + width, barMax.y);

        ImGui::SetCursorScreenPos(tmin);
        ImGui::PushID(1000 + i);
        ImGui::InvisibleButton("##sub", ImVec2(width, 38.0f * s));
        const bool hovered = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked()) g.subtab[g.tab] = i;
        ImGui::PopID();

        const bool selected = g.subtab[g.tab] == i;
        Animate(anim.hovered, hovered, 14.0f * g.animSpeed);
        Animate(anim.selected, selected, 12.0f * g.animSpeed);

        if (anim.selected > 0.01f) {
            dl->AddRectFilled(tmin + ImVec2(3.0f * s, 3.0f * s), tmax - ImVec2(3.0f * s, 3.0f * s),
                              Col(255, 255, 255, 16 * anim.selected), 4.0f * s);
            dl->AddRectFilled(ImVec2(tmin.x + 16.0f * s, tmax.y - 2.0f * s), ImVec2(tmax.x - 16.0f * s, tmax.y),
                              Accent(255.0f * anim.selected), 2.0f * s);
        }

        const float tone = 150.0f + 105.0f * std::max(anim.selected, anim.hovered);
        TextCentered(dl, tmin, tmax, Col(255, 255, 255, tone), tabs[i]);
    }
}

// ---------------------------------------------------------------------------
// Содержимое вкладок
// ---------------------------------------------------------------------------
void ContentHome(int sub) {
    if (sub == 0) {
        GroupLabel(Tr("HUD", "HUD"));
        Checkbox(Tr("Показывать часы", "Show clock"), &g.showClock);
        Checkbox(Tr("Показывать FPS", "Show FPS"), &g.showFps);
        Checkbox(Tr("Компактный HUD", "Compact HUD"), &g.compactHud);
        SliderFloat(Tr("Прозрачность HUD", "HUD opacity"), &g.hudOpacity, 10.0f, 100.0f, "%.0f%%");
    } else {
        GroupLabel(Tr("Водяной знак", "Watermark"));
        Checkbox(Tr("Включить", "Enabled"), &g.watermark);
        const char* corners[] = {Tr("Слева сверху", "Top left"), Tr("Справа сверху", "Top right"),
                                 Tr("Слева снизу", "Bottom left"), Tr("Справа снизу", "Bottom right")};
        Combo(Tr("Положение", "Position"), &g.watermarkPos, corners, 4);
        SliderInt(Tr("Отступ элементов", "Element spacing"), &g.hudSpacing, 0, 24, "%.0f px");
    }
}

void ContentInterface(int sub) {
    if (sub == 0) {
        GroupLabel(Tr("Окно", "Window"));
        SliderFloat(Tr("Масштаб меню", "Menu scale"), &g.scale, 0.80f, 1.40f, "%.2fx");
        Checkbox(Tr("Затемнение фона", "Background dim"), &g.blurBackground);
        Checkbox(Tr("Скруглённые углы", "Rounded corners"), &g.roundedCorners);
    } else {
        GroupLabel(Tr("Оформление", "Appearance"));
        ColorRow(Tr("Акцентный цвет", "Accent color"), g.accent);
        Checkbox(Tr("Анимации", "Animations"), &g.animations);
        SliderFloat(Tr("Скорость анимаций", "Animation speed"), &g.animSpeed, 0.3f, 2.0f, "%.1fx");
        const char* fonts[] = {"Arial", "Verdana", "Tahoma"};
        Combo(Tr("Шрифт", "Font"), &g.font, fonts, 3);
    }
}

void ContentAlerts(int sub) {
    if (sub == 0) {
        GroupLabel(Tr("Уведомления", "Notifications"));
        Checkbox(Tr("Включить уведомления", "Enable notifications"), &g.notifications);
        Checkbox(Tr("Звук уведомлений", "Notification sound"), &g.notifySound);
        SliderFloat(Tr("Время показа", "Display time"), &g.notifyTime, 1.0f, 10.0f, "%.1f c");
    } else {
        GroupLabel(Tr("Фильтры", "Filters"));
        const char* corners[] = {Tr("Слева сверху", "Top left"), Tr("Справа сверху", "Top right"),
                                 Tr("Слева снизу", "Bottom left"), Tr("Справа снизу", "Bottom right")};
        Combo(Tr("Угол экрана", "Screen corner"), &g.notifyCorner, corners, 4);
        Checkbox(Tr("Изменение настроек", "Settings changes"), &g.notifyConfig);
        Checkbox(Tr("Горячие клавиши", "Hotkeys"), &g.notifyHotkeys);
    }
}

void ContentSettings(int sub) {
    if (sub == 0) {
        GroupLabel(Tr("Основные", "General"));
        const char* langs[] = {"Русский", "English"};
        Combo(Tr("Язык", "Language"), &g.language, langs, 2);
        Checkbox(Tr("Сохранять при выходе", "Save on exit"), &g.saveOnExit);
        Checkbox(Tr("Показывать курсор", "Show cursor"), &g.showCursor);
    } else {
        GroupLabel(Tr("Управление", "Controls"));
        KeybindRow(Tr("Открыть меню", "Open menu"), &g.toggleKey);
        Caption(Tr("Нажмите на поле и выберите клавишу. ESC — отмена.",
                   "Click the field and press a key. ESC to cancel."));
        if (Button(Tr("Сбросить клавишу", "Reset key"))) g.toggleKey = VK_INSERT;
    }
}

void ContentProfile(int sub) {
    if (sub == 0) {
        GroupLabel(Tr("Профиль", "Profile"));
        TextRow(Tr("Активный профиль", "Active profile"), g.profileName);
        TextRow(Tr("Вкладок", "Tabs"), "5");
        if (Button(Tr("Сбросить настройки", "Reset settings"))) {
            const int key = g.toggleKey;
            const float scale = g.scale;
            const int lang = g.language;
            g = State();
            g.open = true;
            g.alpha = 1.0f;
            g.toggleKey = key;
            g.scale = scale;
            g.language = lang;
        }
    } else {
        GroupLabel(Tr("О программе", "About"));
        TextRow(Tr("Название", "Name"), "Mirick Menu");
        TextRow(Tr("Версия", "Version"), "2.0");
        TextRow(Tr("Стиль", "Style"), "ForkHack-like");
        Caption(Tr("Только интерфейс: игровых функций и чит-возможностей нет.",
                   "Interface only: no gameplay or cheat features."));
    }
}

void DrawContent(ImVec2 wp) {
    const float s = S();
    static const std::vector<std::vector<const char*>> subs = {
        {"Обзор", "HUD"}, {"Окно", "Тема"}, {"Общие", "Фильтры"}, {"Основные", "Клавиши"}, {"Профиль", "О меню"}};
    static const std::vector<std::vector<const char*>> subsEn = {
        {"Overview", "HUD"}, {"Window", "Theme"}, {"General", "Filters"}, {"General", "Keys"}, {"Profile", "About"}};

    const std::vector<const char*>& tabs = (g.language == 1) ? subsEn[g.tab] : subs[g.tab];
    DrawSubTabs(ImGui::GetWindowDrawList(), wp, tabs);

    // Плавная смена вкладки / подвкладки
    const int key = g.tab * 16 + g.subtab[g.tab];
    if (key != g.prevTab * 16 + g.prevSub) {
        g.tabFade = 0.0f;
        g.prevTab = g.tab;
        g.prevSub = g.subtab[g.tab];
    }
    Animate(g.tabFade, true, 9.0f * g.animSpeed);

    ImGui::SetCursorScreenPos(wp + ImVec2((kSidebar + 18.0f) * s, (kHeader + 64.0f) * s));
    ImGui::BeginChild("##content", ImVec2(524.0f * s, 432.0f * s), ImGuiChildFlags_None,
                      ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.0f * s, 6.0f * s));

    ImGui::BeginChild("##left", ImVec2(kItemWidth * s, 426.0f * s), ImGuiChildFlags_None,
                      ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar);
    switch (g.tab) {
        case 0: ContentHome(0); break;
        case 1: ContentInterface(0); break;
        case 2: ContentAlerts(0); break;
        case 3: ContentSettings(0); break;
        default: ContentProfile(0); break;
    }
    ImGui::EndChild();

    ImGui::SameLine(0.0f, 16.0f * s);

    ImGui::BeginChild("##right", ImVec2(kItemWidth * s, 426.0f * s), ImGuiChildFlags_None,
                      ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar);
    switch (g.tab) {
        case 0: ContentHome(1); break;
        case 1: ContentInterface(1); break;
        case 2: ContentAlerts(1); break;
        case 3: ContentSettings(1); break;
        default: ContentProfile(1); break;
    }
    ImGui::EndChild();

    ImGui::PopStyleVar();
    ImGui::EndChild();
}

} // namespace

// ---------------------------------------------------------------------------
// Публичный интерфейс
// ---------------------------------------------------------------------------
void Menu::Toggle() { g.open = !g.open; }

bool Menu::IsOpen() { return g.open || g.alpha > 0.01f; }

int Menu::ToggleKey() { return g.toggleKey; }

void Menu::Draw() {
    Animate(g.alpha, g.open, g.animations ? 10.0f : 1000.0f);
    if (!g.open && g.alpha < 0.01f) {
        g.capturingKey = 0;
        return;
    }

    const float s = S();
    ImGuiIO& io = ImGui::GetIO();
    const ImVec2 outer(kWidth * s + kShadow * 2.0f, kHeight * s + kShadow * 2.0f);

    ImGui::SetNextWindowSize(outer, ImGuiCond_Always);
    if (!g.centered) {
        ImGui::SetNextWindowPos(ImVec2((io.DisplaySize.x - outer.x) * 0.5f, (io.DisplaySize.y - outer.y) * 0.5f),
                                ImGuiCond_Always);
        g.centered = true;
    }

    // Перетаскивание только за шапку окна
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
                             ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                             ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground |
                             ImGuiWindowFlags_NoBringToFrontOnFocus;
    if (!g.dragging) flags |= ImGuiWindowFlags_NoMove;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.0f * s, 6.0f * s));
    ImGui::SetNextWindowBgAlpha(0.0f);
    ImGui::Begin("##mirick_root", nullptr, flags);

    const ImVec2 wp = ImGui::GetWindowPos() + ImVec2(kShadow, kShadow);
    ImDrawList* dl = ImGui::GetWindowDrawList();

    const ImVec2 headerMin = wp;
    const ImVec2 headerMax = wp + ImVec2(kWidth * s, kHeader * s);
    const bool overHeader = io.MousePos.x >= headerMin.x && io.MousePos.x <= headerMax.x && io.MousePos.y >= headerMin.y &&
                            io.MousePos.y <= headerMax.y;
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) g.dragging = false;
    else if (overHeader && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) g.dragging = true;

    const float saved = g.tabFade;
    g.tabFade = 1.0f;
    DrawFrame(dl, wp);
    DrawTabs(dl, wp);
    g.tabFade = saved;

    DrawContent(wp);

    ImGui::End();
    ImGui::PopStyleVar(3);
}
