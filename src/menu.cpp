// Mirick Menu — визуальное меню (Dear ImGui / DirectX 9).
// Оформление: тёмные карточки, тумблеры-пилюли, тонкие слайдеры, сегментные
// переключатели. Реализация самостоятельная, используется только публичный API ImGui.
// В проекте нет игровых функций: все настройки влияют исключительно на сам интерфейс.

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
// Геометрия (логические пиксели, умножаются на масштаб)
// ---------------------------------------------------------------------------
constexpr float kWidth = 936.0f;
constexpr float kHeight = 698.0f;
constexpr float kSidebar = 210.0f;
constexpr float kShadow = 26.0f;
constexpr float kColLeftX = 230.0f;
constexpr float kColRightX = 578.0f;
constexpr float kColLeftW = 336.0f;
constexpr float kColRightW = 334.0f;
constexpr float kContentTop = 96.0f;

// ---------------------------------------------------------------------------
// Анимация
// ---------------------------------------------------------------------------
struct ItemAnim {
    float hovered = 0.0f;
    float active = 0.0f;
};

void Animate(float& value, bool condition, float speed) {
    const float dt = ImGui::GetIO().DeltaTime;
    const float step = std::clamp(dt * speed, 0.0f, 1.0f);
    value += ((condition ? 1.0f : 0.0f) - value) * step;
    value = std::clamp(value, 0.0f, 1.0f);
}

// ---------------------------------------------------------------------------
// Состояние
// ---------------------------------------------------------------------------
struct State {
    bool open = false;
    float alpha = 0.0f;
    float tabFade = 1.0f;
    int tab = 0;
    int prevTab = 0;
    float scale = 1.0f;
    bool centered = false;
    bool dragging = false;
    int toggleKey = VK_INSERT;
    int capturingKey = 0;
    int language = 0;

    std::unordered_map<std::string, ItemAnim> items;
    std::array<ItemAnim, 6> tabs{};

    // Акцент (используется для цветных элементов интерфейса)
    float accent[4] = {0.91f, 0.16f, 0.16f, 1.00f};

    // Демонстрационные настройки интерфейса
    bool showClock = true;
    bool showFps = false;
    bool compactHud = false;
    float hudOpacity = 85.0f;

    bool watermark = true;
    int watermarkStyle = 1;
    bool watermarkShadow = true;
    bool watermarkIcon = false;
    int watermarkPos = 1;
    float watermarkScale = 1.1f;

    bool animations = true;
    float animSpeed = 1.1f;
    bool shadow = true;
    bool rounded = true;
    int theme = 1;
    bool glow = false;
    bool separators = true;
    float cardOpacity = 96.0f;

    bool notifications = true;
    bool notifySound = false;
    int notifyCorner = 1;
    float notifyTime = 4.0f;
    bool notifyConfig = true;
    bool notifyKeys = true;

    bool saveOnExit = true;
    bool showCursor = true;
    bool fadeOnClose = true;
};

State g;
ImFont* fontBig = nullptr;
ImFont* fontReg = nullptr;
ImFont* fontSmall = nullptr;

// ---------------------------------------------------------------------------
// Утилиты
// ---------------------------------------------------------------------------
float S() { return g.scale; }
float A() { return g.alpha; }
float AF() { return g.alpha * g.tabFade; }

ImU32 Col(int r, int gr, int b, float a255, bool fade = true) {
    const float mul = fade ? AF() : A();
    return IM_COL32(r, gr, b, (int)std::clamp(a255 * mul, 0.0f, 255.0f));
}

ImU32 Accent(float a255 = 255.0f) {
    return IM_COL32((int)(g.accent[0] * 255.0f), (int)(g.accent[1] * 255.0f), (int)(g.accent[2] * 255.0f),
                    (int)std::clamp(a255 * g.accent[3] * AF(), 0.0f, 255.0f));
}

ItemAnim& Anim(const char* key) { return g.items[key]; }

const char* Tr(const char* ru, const char* en) { return g.language == 1 ? en : ru; }

ImFont* FontOr(ImFont* f) { return f ? f : ImGui::GetFont(); }

float SzBig() { return 22.0f * S(); }
float SzCard() { return 15.0f * S(); }
float SzRow() { return 14.0f * S(); }
float SzSmall() { return 11.5f * S(); }

ImVec2 Measure(ImFont* font, float size, const char* text) {
    return FontOr(font)->CalcTextSizeA(size, FLT_MAX, 0.0f, text);
}

void Text(ImDrawList* dl, ImFont* font, float size, ImVec2 pos, ImU32 color, const char* text) {
    dl->AddText(FontOr(font), size, pos, color, text);
}

void TextRight(ImDrawList* dl, ImFont* font, float size, float rightX, float y, ImU32 color, const char* text) {
    const ImVec2 sz = Measure(font, size, text);
    dl->AddText(FontOr(font), size, ImVec2(rightX - sz.x, y), color, text);
}

void TextCenter(ImDrawList* dl, ImFont* font, float size, ImVec2 min, ImVec2 max, ImU32 color, const char* text) {
    const ImVec2 sz = Measure(font, size, text);
    dl->AddText(FontOr(font), size, ImVec2(min.x + (max.x - min.x - sz.x) * 0.5f, min.y + (max.y - min.y - sz.y) * 0.5f),
                color, text);
}

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
        case VK_RBUTTON: return "MOUSE 2";
        case VK_MBUTTON: return "MOUSE 3";
        default: break;
    }
    const UINT scan = MapVirtualKeyA((UINT)vk, MAPVK_VK_TO_VSC);
    if (scan && GetKeyNameTextA((LONG)(scan << 16), buffer, (int)sizeof(buffer)) > 0) return buffer;
    std::snprintf(buffer, sizeof(buffer), "0x%02X", vk);
    return buffer;
}

// ---------------------------------------------------------------------------
// Колонка с карточками
// ---------------------------------------------------------------------------
struct Column {
    ImDrawList* dl = nullptr;
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float cardTop = 0.0f;
    bool inCard = false;
};

void CardBegin(Column& c, const char* title) {
    const float s = S();
    c.dl->ChannelsSplit(2);
    c.dl->ChannelsSetCurrent(1);
    c.cardTop = c.y;
    c.inCard = true;
    Text(c.dl, fontBig, SzCard(), ImVec2(c.x + 18.0f * s, c.y + 15.0f * s), Col(255, 255, 255, 240), title);
    c.y += 46.0f * s;
}

void CardEnd(Column& c) {
    const float s = S();
    const float bottom = c.y + 12.0f * s;
    c.dl->ChannelsSetCurrent(0);
    c.dl->AddRectFilled(ImVec2(c.x, c.cardTop), ImVec2(c.x + c.w, bottom), Col(21, 21, 23, 255 * (g.cardOpacity / 100.0f)),
                        g.rounded ? 12.0f * s : 0.0f);
    c.dl->AddRect(ImVec2(c.x, c.cardTop), ImVec2(c.x + c.w, bottom), Col(255, 255, 255, 10), g.rounded ? 12.0f * s : 0.0f);
    c.dl->ChannelsMerge();
    c.inCard = false;
    c.y = bottom + 14.0f * s;
}

// Невидимая кнопка в абсолютных координатах
bool Hit(const char* id, ImVec2 min, ImVec2 size, bool* hovered = nullptr) {
    ImGui::SetCursorScreenPos(min);
    ImGui::InvisibleButton(id, size);
    if (hovered) *hovered = ImGui::IsItemHovered();
    return ImGui::IsItemClicked();
}

// ---------------------------------------------------------------------------
// Виджеты
// ---------------------------------------------------------------------------
bool Toggle(Column& c, const char* label, bool* value, bool dim = false) {
    const float s = S();
    ImGui::PushID(label);
    const ImVec2 rowMin(c.x + 12.0f * s, c.y);
    const ImVec2 rowSize(c.w - 24.0f * s, 32.0f * s);

    bool hovered = false;
    const bool clicked = Hit("##row", rowMin, rowSize, &hovered);
    if (clicked) *value = !*value;

    ItemAnim& anim = Anim(label);
    Animate(anim.hovered, hovered, 14.0f * g.animSpeed);
    Animate(anim.active, *value, 13.0f * g.animSpeed);

    if (anim.hovered > 0.01f) {
        c.dl->AddRectFilled(rowMin, rowMin + rowSize, Col(255, 255, 255, 9 * anim.hovered), 8.0f * s);
    }

    const float textAlpha = dim ? (95.0f + 40.0f * anim.hovered) : (170.0f + 85.0f * anim.active);
    Text(c.dl, fontReg, SzRow(), ImVec2(rowMin.x + 8.0f * s, rowMin.y + 8.0f * s), Col(255, 255, 255, textAlpha), label);

    // Пилюля-тумблер
    const ImVec2 trackMax(rowMin.x + rowSize.x - 6.0f * s, rowMin.y + rowSize.y * 0.5f + 11.0f * s);
    const ImVec2 trackMin(trackMax.x - 42.0f * s, rowMin.y + rowSize.y * 0.5f - 11.0f * s);
    const float h = trackMax.y - trackMin.y;
    const ImU32 trackOff = Col(58, 58, 63, 255);
    const ImU32 trackOn = Col(245, 245, 247, 255);
    c.dl->AddRectFilled(trackMin, trackMax, trackOff, h * 0.5f);
    if (anim.active > 0.01f) {
        c.dl->AddRectFilled(trackMin, trackMax, Col(245, 245, 247, 255 * anim.active), h * 0.5f);
    }
    (void)trackOn;

    const float knobR = h * 0.5f - 3.0f * s;
    const ImVec2 knob(trackMin.x + h * 0.5f + (trackMax.x - trackMin.x - h) * anim.active, (trackMin.y + trackMax.y) * 0.5f);
    c.dl->AddCircleFilled(knob, knobR, Col(255, 255, 255, 255), 20);
    c.dl->AddCircle(knob, knobR, Col(0, 0, 0, 40 + 60 * anim.active), 20, 1.2f * s);

    c.y += 34.0f * s;
    ImGui::PopID();
    return clicked;
}

bool SliderBase(Column& c, const char* label, float* value, float min, float max, const char* fmt, bool integer) {
    const float s = S();
    ImGui::PushID(label);
    const ImVec2 rowMin(c.x + 12.0f * s, c.y);
    const ImVec2 rowSize(c.w - 24.0f * s, 46.0f * s);

    bool hovered = false;
    Hit("##row", rowMin, rowSize, &hovered);
    const bool active = ImGui::IsItemActive();

    ItemAnim& anim = Anim(label);
    Animate(anim.hovered, hovered || active, 14.0f * g.animSpeed);
    Animate(anim.active, active, 18.0f * g.animSpeed);

    const ImVec2 railMin(rowMin.x + 8.0f * s, rowMin.y + 30.0f * s);
    const ImVec2 railMax(rowMin.x + rowSize.x - 8.0f * s, railMin.y + 3.0f * s);

    bool changed = false;
    if (active) {
        const float t = std::clamp((ImGui::GetIO().MousePos.x - railMin.x) / (railMax.x - railMin.x), 0.0f, 1.0f);
        float next = min + (max - min) * t;
        if (integer) next = std::round(next);
        if (next != *value) {
            *value = next;
            changed = true;
        }
    }

    const float t = (max > min) ? std::clamp((*value - min) / (max - min), 0.0f, 1.0f) : 0.0f;

    Text(c.dl, fontReg, SzRow(), ImVec2(rowMin.x + 8.0f * s, rowMin.y + 4.0f * s), Col(255, 255, 255, 150 + 60 * anim.hovered),
         label);

    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), fmt, integer ? (double)(int)*value : (double)*value);
    TextRight(c.dl, fontBig, SzRow(), rowMin.x + rowSize.x - 8.0f * s, rowMin.y + 4.0f * s, Col(255, 255, 255, 250), buffer);

    c.dl->AddRectFilled(railMin, railMax, Col(52, 52, 57, 255), 2.0f * s);
    const float fillX = railMin.x + (railMax.x - railMin.x) * t;
    if (fillX > railMin.x) c.dl->AddRectFilled(railMin, ImVec2(fillX, railMax.y), Col(255, 255, 255, 255), 2.0f * s);

    const ImVec2 knob(fillX, (railMin.y + railMax.y) * 0.5f);
    c.dl->AddCircleFilled(knob, (5.5f + 1.5f * anim.hovered) * s, Col(255, 255, 255, 255), 20);

    c.y += 48.0f * s;
    ImGui::PopID();
    return changed;
}

bool SliderFloat(Column& c, const char* label, float* value, float min, float max, const char* fmt = "%.2f") {
    return SliderBase(c, label, value, min, max, fmt, false);
}

bool SliderInt(Column& c, const char* label, int* value, int min, int max, const char* fmt = "%.0f") {
    float temp = (float)*value;
    const bool changed = SliderBase(c, label, &temp, (float)min, (float)max, fmt, true);
    if (changed) *value = (int)temp;
    return changed;
}

bool Segmented(Column& c, const char* label, int* value, const char* const items[], int count) {
    const float s = S();
    ImGui::PushID(label);
    const float x = c.x + 20.0f * s;
    const float w = c.w - 40.0f * s;

    if (label && *label) {
        Text(c.dl, fontReg, SzRow(), ImVec2(x, c.y), Col(255, 255, 255, 170), label);
        c.y += 24.0f * s;
    }

    const ImVec2 barMin(x, c.y);
    const ImVec2 barMax(x + w, c.y + 30.0f * s);
    const float seg = w / (float)count;
    bool changed = false;

    for (int i = 0; i < count; ++i) {
        const ImVec2 segMin(barMin.x + seg * i, barMin.y);
        const ImVec2 segMax(segMin.x + seg, barMax.y);
        ImGui::PushID(i);
        bool hovered = false;
        if (Hit("##seg", segMin, ImVec2(seg, segMax.y - segMin.y), &hovered)) {
            *value = i;
            changed = true;
        }
        ImGui::PopID();

        const std::string key = std::string(label) + "##seg" + std::to_string(i);
        ItemAnim& anim = g.items[key];
        Animate(anim.hovered, hovered, 14.0f * g.animSpeed);
        Animate(anim.active, *value == i, 14.0f * g.animSpeed);

        if (anim.active > 0.01f) {
            c.dl->AddRectFilled(segMin + ImVec2(2.0f * s, 0.0f), segMax - ImVec2(2.0f * s, 0.0f),
                                Col(48, 48, 53, 255 * anim.active), 7.0f * s);
        } else if (anim.hovered > 0.01f) {
            c.dl->AddRectFilled(segMin + ImVec2(2.0f * s, 0.0f), segMax - ImVec2(2.0f * s, 0.0f),
                                Col(255, 255, 255, 10 * anim.hovered), 7.0f * s);
        }
        TextCenter(c.dl, fontReg, SzRow(), segMin, segMax, Col(255, 255, 255, 110 + 145 * anim.active), items[i]);
    }

    c.y += 38.0f * s;
    ImGui::PopID();
    return changed;
}

bool ColorRow(Column& c, const char* label, float color[4]) {
    const float s = S();
    ImGui::PushID(label);
    const ImVec2 rowMin(c.x + 12.0f * s, c.y);
    const ImVec2 rowSize(c.w - 24.0f * s, 32.0f * s);

    Text(c.dl, fontReg, SzRow(), ImVec2(rowMin.x + 8.0f * s, rowMin.y + 8.0f * s), Col(255, 255, 255, 190), label);

    const ImVec2 swatchMax(rowMin.x + rowSize.x - 6.0f * s, rowMin.y + rowSize.y * 0.5f + 9.0f * s);
    const ImVec2 swatchMin(swatchMax.x - 26.0f * s, rowMin.y + rowSize.y * 0.5f - 9.0f * s);

    ImGui::SetCursorScreenPos(swatchMin);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f * s);
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1, 1, 1, 0.12f));
    const bool changed = ImGui::ColorEdit4("##picker", color,
                                           ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel |
                                               ImGuiColorEditFlags_NoBorder | ImGuiColorEditFlags_AlphaBar);
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    (void)swatchMax;

    c.y += 34.0f * s;
    ImGui::PopID();
    return changed;
}

bool KeyRow(Column& c, const char* label, int* key) {
    const float s = S();
    ImGui::PushID(label);
    const ImVec2 rowMin(c.x + 12.0f * s, c.y);
    const ImVec2 rowSize(c.w - 24.0f * s, 32.0f * s);

    bool hovered = false;
    const bool clicked = Hit("##row", rowMin, rowSize, &hovered);
    if (clicked && g.capturingKey == 0) g.capturingKey = 1;

    bool changed = false;
    if (g.capturingKey == 1 && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        g.capturingKey = 2;
    } else if (g.capturingKey == 2) {
        if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) {
            g.capturingKey = 0;
        } else {
            for (int vk = 0x02; vk <= 0xFE; ++vk) {
                if (vk == VK_ESCAPE) continue;
                if (GetAsyncKeyState(vk) & 0x8000) {
                    *key = vk;
                    g.capturingKey = 0;
                    changed = true;
                    break;
                }
            }
        }
    }

    ItemAnim& anim = Anim(label);
    Animate(anim.hovered, hovered, 14.0f * g.animSpeed);
    if (anim.hovered > 0.01f) c.dl->AddRectFilled(rowMin, rowMin + rowSize, Col(255, 255, 255, 9 * anim.hovered), 8.0f * s);

    Text(c.dl, fontReg, SzRow(), ImVec2(rowMin.x + 8.0f * s, rowMin.y + 8.0f * s), Col(255, 255, 255, 190), label);

    const char* shown = g.capturingKey ? "..." : KeyName(*key);
    const ImVec2 boxMax(rowMin.x + rowSize.x - 6.0f * s, rowMin.y + rowSize.y - 4.0f * s);
    const ImVec2 boxMin(boxMax.x - std::max(72.0f * s, Measure(fontReg, SzRow(), shown).x + 22.0f * s), rowMin.y + 4.0f * s);
    c.dl->AddRectFilled(boxMin, boxMax, Col(38, 38, 42, 255), 7.0f * s);
    TextCenter(c.dl, fontReg, SzRow(), boxMin, boxMax, g.capturingKey ? Accent(255.0f) : Col(255, 255, 255, 230), shown);

    c.y += 34.0f * s;
    ImGui::PopID();
    return changed;
}

void InfoRow(Column& c, const char* label, const char* value) {
    const float s = S();
    const ImVec2 rowMin(c.x + 12.0f * s, c.y);
    const float rowW = c.w - 24.0f * s;
    Text(c.dl, fontReg, SzRow(), ImVec2(rowMin.x + 8.0f * s, rowMin.y + 6.0f * s), Col(255, 255, 255, 120), label);
    TextRight(c.dl, fontBig, SzRow(), rowMin.x + rowW - 8.0f * s, rowMin.y + 6.0f * s, Col(255, 255, 255, 235), value);
    c.y += 30.0f * s;
}

void CaptionRow(Column& c, const char* text) {
    const float s = S();
    Text(c.dl, fontSmall, SzSmall(), ImVec2(c.x + 20.0f * s, c.y), Col(255, 255, 255, 95), text);
    c.y += 24.0f * s;
}

bool ButtonRow(Column& c, const char* label) {
    const float s = S();
    ImGui::PushID(label);
    const ImVec2 bMin(c.x + 18.0f * s, c.y);
    const ImVec2 bSize(c.w - 36.0f * s, 34.0f * s);

    bool hovered = false;
    const bool clicked = Hit("##btn", bMin, bSize, &hovered);
    ItemAnim& anim = Anim(label);
    Animate(anim.hovered, hovered, 14.0f * g.animSpeed);

    c.dl->AddRectFilled(bMin, bMin + bSize, Col(255, 255, 255, 16 + 20 * anim.hovered), 8.0f * s);
    TextCenter(c.dl, fontReg, SzRow(), bMin, bMin + bSize, Col(255, 255, 255, 200 + 55 * anim.hovered), label);

    c.y += 42.0f * s;
    ImGui::PopID();
    return clicked;
}

// ---------------------------------------------------------------------------
// Шапка, боковая панель, переключатель языка
// ---------------------------------------------------------------------------
void DrawPill(ImDrawList* dl, ImVec2 pos, const char* text, bool icon, float& outWidth) {
    const float s = S();
    const ImVec2 ts = Measure(fontReg, SzRow(), text);
    const float w = ts.x + (icon ? 48.0f : 28.0f) * s;
    const ImVec2 min = ImVec2(pos.x - w, pos.y);
    const ImVec2 max = ImVec2(pos.x, pos.y + 30.0f * s);
    dl->AddRectFilled(min, max, Col(26, 26, 29, 255, false), 9.0f * s);
    dl->AddRect(min, max, Col(255, 255, 255, 12, false), 9.0f * s);

    float textX = min.x + 14.0f * s;
    if (icon) {
        const ImVec2 c0(min.x + 18.0f * s, (min.y + max.y) * 0.5f);
        dl->AddTriangleFilled(c0 + ImVec2(-6 * s, -5 * s), c0 + ImVec2(7 * s, 0), c0 + ImVec2(-6 * s, 5 * s),
                              Col(255, 255, 255, 190, false));
        textX = min.x + 32.0f * s;
    }
    dl->AddText(FontOr(fontReg), SzRow(), ImVec2(textX, (min.y + max.y) * 0.5f - ts.y * 0.5f), Col(230, 230, 235, 230, false),
                text);
    outWidth = w;
}

void DrawHeader(ImDrawList* dl, ImVec2 wp) {
    const float s = S();
    const char* titles[5] = {Tr("Основное", "General"), Tr("Визуал", "Visuals"), Tr("Уведомления", "Alerts"),
                             Tr("Разное", "Misc"), Tr("Профиль", "Profile")};
    const char* subs[5] = {Tr("Интерфейс и HUD", "Interface and HUD"), Tr("Оформление меню", "Menu appearance"),
                           Tr("Информационные сообщения", "Information messages"), Tr("Управление и поведение", "Controls and behaviour"),
                           Tr("Данные о сборке", "Build information")};

    Text(dl, fontBig, SzBig(), wp + ImVec2(kColLeftX * s, 24.0f * s), Col(255, 255, 255, 250, false), titles[g.tab]);
    Text(dl, fontSmall, SzSmall(), wp + ImVec2(kColLeftX * s + 2.0f * s, 58.0f * s), Col(255, 255, 255, 105, false), subs[g.tab]);

    float width = 0.0f;
    const float right = wp.x + (kWidth - 24.0f) * s;
    DrawPill(dl, ImVec2(right, wp.y + 20.0f * s), "build v2.0", false, width);
    DrawPill(dl, ImVec2(right - width - 10.0f * s, wp.y + 20.0f * s), "@mirick", true, width);
}

void DrawSidebar(ImDrawList* dl, ImVec2 wp) {
    const float s = S();
    const ImVec2 min = wp;
    const ImVec2 max = wp + ImVec2(kSidebar * s, kHeight * s);
    dl->AddRectFilled(min, max, Col(14, 14, 16, 255, false), g.rounded ? 16.0f * s : 0.0f,
                      ImDrawFlags_RoundCornersLeft);
    dl->AddLine(ImVec2(max.x, min.y + 14.0f * s), ImVec2(max.x, max.y - 14.0f * s), Col(255, 255, 255, 10, false), 1.0f * s);

    // Логотип
    dl->AddNgonFilled(wp + ImVec2(32.0f * s, 38.0f * s), 7.0f * s, Accent(255.0f), 4);
    Text(dl, fontBig, SzCard(), wp + ImVec2(48.0f * s, 30.0f * s), Col(255, 255, 255, 240, false), "MIRICK");
    Text(dl, fontSmall, SzSmall(), wp + ImVec2(48.0f * s, 48.0f * s), Col(255, 255, 255, 80, false), Tr("меню", "menu"));

    const char* names[5] = {Tr("Основное", "General"), Tr("Визуал", "Visuals"), Tr("Уведомления", "Alerts"),
                            Tr("Разное", "Misc"), Tr("Профиль", "Profile")};

    for (int i = 0; i < 5; ++i) {
        const ImVec2 tMin = wp + ImVec2(14.0f * s, (108.0f + 46.0f * i) * s);
        const ImVec2 tSize(182.0f * s, 40.0f * s);
        ImGui::PushID(2000 + i);
        bool hovered = false;
        if (Hit("##tab", tMin, tSize, &hovered)) g.tab = i;
        ImGui::PopID();

        ItemAnim& anim = g.tabs[i];
        Animate(anim.hovered, hovered, 14.0f * g.animSpeed);
        Animate(anim.active, g.tab == i, 14.0f * g.animSpeed);

        if (anim.active > 0.01f || anim.hovered > 0.01f) {
            dl->AddRectFilled(tMin, tMin + tSize, Col(255, 255, 255, (14 * anim.active + 8 * anim.hovered), false), 10.0f * s);
        }
        if (anim.active > 0.01f) {
            dl->AddRectFilled(ImVec2(tMin.x, tMin.y + 11.0f * s), ImVec2(tMin.x + 3.0f * s, tMin.y + tSize.y - 11.0f * s),
                              Accent(255.0f * anim.active), 2.0f * s);
        }

        // Иконка-точка
        dl->AddCircleFilled(tMin + ImVec2(22.0f * s, tSize.y * 0.5f), 3.2f * s,
                            Col(255, 255, 255, 70 + 185 * anim.active, false), 12);
        dl->AddText(FontOr(fontReg), SzRow(), tMin + ImVec2(38.0f * s, tSize.y * 0.5f - SzRow() * 0.62f),
                    Col(255, 255, 255, 110 + 145 * std::max(anim.active, anim.hovered * 0.6f), false), names[i]);
    }
}

void DrawLanguageSwitch(ImDrawList* dl, ImVec2 wp) {
    const float s = S();
    const ImVec2 min = wp + ImVec2(22.0f * s, (kHeight - 56.0f) * s);
    const ImVec2 size(112.0f * s, 34.0f * s);
    dl->AddRectFilled(min, min + size, Col(26, 26, 29, 255, false), 9.0f * s);
    dl->AddRect(min, min + size, Col(255, 255, 255, 12, false), 9.0f * s);

    const char* names[2] = {"RU", "EN"};
    for (int i = 0; i < 2; ++i) {
        const ImVec2 segMin(min.x + size.x * 0.5f * i, min.y);
        const ImVec2 segMax(segMin.x + size.x * 0.5f, min.y + size.y);
        ImGui::PushID(3000 + i);
        bool hovered = false;
        if (Hit("##lang", segMin, ImVec2(size.x * 0.5f, size.y), &hovered)) g.language = i;
        ImGui::PopID();

        const std::string key = std::string("lang") + names[i];
        ItemAnim& anim = g.items[key];
        Animate(anim.active, g.language == i, 14.0f * g.animSpeed);
        Animate(anim.hovered, hovered, 14.0f * g.animSpeed);

        if (anim.active > 0.01f) {
            dl->AddRectFilled(segMin + ImVec2(3.0f * s, 3.0f * s), segMax - ImVec2(3.0f * s, 3.0f * s),
                              Col(48, 48, 53, 255 * anim.active, false), 7.0f * s);
        }
        TextCenter(dl, fontReg, SzRow(), segMin, segMax, Col(255, 255, 255, 110 + 145 * anim.active, false), names[i]);
    }
}

// ---------------------------------------------------------------------------
// Содержимое вкладок
// ---------------------------------------------------------------------------
void TabGeneral(Column& l, Column& r) {
    CardBegin(l, Tr("HUD", "HUD"));
    Toggle(l, Tr("Часы", "Clock"), &g.showClock);
    Toggle(l, Tr("Счётчик FPS", "FPS counter"), &g.showFps);
    Toggle(l, Tr("Компактный режим", "Compact mode"), &g.compactHud);
    SliderFloat(l, Tr("Прозрачность", "Opacity"), &g.hudOpacity, 10.0f, 100.0f, "%.0f %%");
    CardEnd(l);

    CardBegin(l, Tr("Анимации", "Animations"));
    Toggle(l, Tr("Включить", "Enabled"), &g.animations);
    SliderFloat(l, Tr("Скорость", "Speed"), &g.animSpeed, 0.4f, 2.0f, "%.1fx");
    CardEnd(l);

    CardBegin(l, Tr("Окно", "Window"));
    Toggle(l, Tr("Тень", "Shadow"), &g.shadow);
    Toggle(l, Tr("Скруглённые углы", "Rounded corners"), &g.rounded);
    CardEnd(l);

    CardBegin(r, Tr("Водяной знак", "Watermark"));
    Toggle(r, Tr("Включить", "Enabled"), &g.watermark);
    const char* styles[] = {Tr("Текст", "Text"), Tr("Плашка", "Badge"), "3D"};
    Segmented(r, Tr("Стиль", "Style"), &g.watermarkStyle, styles, 3);
    Toggle(r, Tr("Тень текста", "Text shadow"), &g.watermarkShadow);
    ColorRow(r, Tr("Цвет акцента", "Accent color"), g.accent);
    Toggle(r, Tr("Иконка", "Icon"), &g.watermarkIcon);
    const char* corners[] = {Tr("Слева", "Left"), Tr("По центру", "Center"), Tr("Справа", "Right")};
    Segmented(r, Tr("Положение", "Position"), &g.watermarkPos, corners, 3);
    SliderFloat(r, Tr("Масштаб знака", "Watermark scale"), &g.watermarkScale, 0.6f, 2.0f, "%.1fx");
    CardEnd(r);
}

void TabVisuals(Column& l, Column& r) {
    CardBegin(l, Tr("Тема", "Theme"));
    const char* themes[] = {Tr("Тёмная", "Dark"), Tr("Графит", "Graphite"), Tr("Контраст", "Contrast")};
    Segmented(l, "", &g.theme, themes, 3);
    ColorRow(l, Tr("Акцент", "Accent"), g.accent);
    SliderFloat(l, Tr("Плотность карточек", "Card opacity"), &g.cardOpacity, 50.0f, 100.0f, "%.0f %%");
    CardEnd(l);

    CardBegin(l, Tr("Эффекты", "Effects"));
    Toggle(l, Tr("Свечение акцента", "Accent glow"), &g.glow);
    Toggle(l, Tr("Разделители", "Separators"), &g.separators);
    CardEnd(l);

    CardBegin(r, Tr("Размер интерфейса", "Interface size"));
    SliderFloat(r, Tr("Масштаб меню", "Menu scale"), &g.scale, 0.75f, 1.35f, "%.2fx");
    CaptionRow(r, Tr("Меню перестраивается под выбранный масштаб.", "The menu rescales instantly."));
    CardEnd(r);

    CardBegin(r, Tr("Предпросмотр", "Preview"));
    InfoRow(r, Tr("Карточек на вкладке", "Cards on tab"), "4");
    InfoRow(r, Tr("Анимации", "Animations"), g.animations ? Tr("вкл", "on") : Tr("выкл", "off"));
    InfoRow(r, Tr("Тема", "Theme"), themes[g.theme]);
    CardEnd(r);
}

void TabAlerts(Column& l, Column& r) {
    CardBegin(l, Tr("Уведомления", "Notifications"));
    Toggle(l, Tr("Включить", "Enabled"), &g.notifications);
    Toggle(l, Tr("Звук", "Sound"), &g.notifySound);
    SliderFloat(l, Tr("Время показа", "Display time"), &g.notifyTime, 1.0f, 10.0f, "%.1f c");
    CardEnd(l);

    CardBegin(l, Tr("События", "Events"));
    Toggle(l, Tr("Изменение настроек", "Settings changed"), &g.notifyConfig);
    Toggle(l, Tr("Горячие клавиши", "Hotkeys"), &g.notifyKeys);
    CardEnd(l);

    CardBegin(r, Tr("Расположение", "Placement"));
    const char* corners[] = {Tr("Слева сверху", "Top left"), Tr("Справа сверху", "Top right"), Tr("Снизу", "Bottom")};
    Segmented(r, Tr("Угол экрана", "Screen corner"), &g.notifyCorner, corners, 3);
    ColorRow(r, Tr("Цвет полосы", "Bar color"), g.accent);
    CaptionRow(r, Tr("Уведомления рисуются поверх HUD.", "Notifications are drawn above the HUD."));
    CardEnd(r);
}

void TabMisc(Column& l, Column& r) {
    CardBegin(l, Tr("Управление", "Controls"));
    KeyRow(l, Tr("Открыть меню", "Open menu"), &g.toggleKey);
    CaptionRow(l, Tr("Нажмите на поле и выберите клавишу. ESC — отмена.", "Click the field and press a key. ESC cancels."));
    if (ButtonRow(l, Tr("Сбросить клавишу", "Reset key"))) g.toggleKey = VK_INSERT;
    CardEnd(l);

    CardBegin(l, Tr("Поведение", "Behaviour"));
    Toggle(l, Tr("Сохранять при выходе", "Save on exit"), &g.saveOnExit);
    Toggle(l, Tr("Показывать курсор", "Show cursor"), &g.showCursor);
    Toggle(l, Tr("Плавное закрытие", "Fade on close"), &g.fadeOnClose);
    CardEnd(l);

    CardBegin(r, Tr("Сброс", "Reset"));
    CaptionRow(r, Tr("Вернуть все параметры интерфейса к значениям по умолчанию.",
                     "Restore all interface options to defaults."));
    if (ButtonRow(r, Tr("Сбросить настройки", "Reset settings"))) {
        const int key = g.toggleKey;
        const int lang = g.language;
        const float scale = g.scale;
        g = State();
        g.open = true;
        g.alpha = 1.0f;
        g.toggleKey = key;
        g.language = lang;
        g.scale = scale;
    }
    CardEnd(r);
}

void TabProfile(Column& l, Column& r) {
    CardBegin(l, Tr("Профиль", "Profile"));
    InfoRow(l, Tr("Имя", "Name"), "default");
    InfoRow(l, Tr("Язык", "Language"), g.language == 1 ? "English" : "Русский");
    InfoRow(l, Tr("Клавиша меню", "Menu key"), KeyName(g.toggleKey));
    CardEnd(l);

    CardBegin(r, Tr("О программе", "About"));
    InfoRow(r, Tr("Название", "Name"), "Mirick Menu");
    InfoRow(r, Tr("Версия", "Version"), "2.0");
    InfoRow(r, Tr("Платформа", "Platform"), "DirectX 9 / Win32");
    CaptionRow(r, Tr("Только интерфейс: игровых и чит-функций нет.", "Interface only: no gameplay or cheat features."));
    CardEnd(r);
}

void DrawBody(ImDrawList* dl, ImVec2 wp) {
    const float s = S();

    if (g.tab != g.prevTab) {
        g.tabFade = 0.0f;
        g.prevTab = g.tab;
    }
    Animate(g.tabFade, true, 10.0f * g.animSpeed);

    Column left{dl, wp.x + kColLeftX * s, wp.y + kContentTop * s, kColLeftW * s, 0.0f, false};
    Column right{dl, wp.x + kColRightX * s, wp.y + kContentTop * s, kColRightW * s, 0.0f, false};

    switch (g.tab) {
        case 0: TabGeneral(left, right); break;
        case 1: TabVisuals(left, right); break;
        case 2: TabAlerts(left, right); break;
        case 3: TabMisc(left, right); break;
        default: TabProfile(left, right); break;
    }
}

} // namespace

// ---------------------------------------------------------------------------
// Публичный интерфейс
// ---------------------------------------------------------------------------
void Menu::Toggle() { g.open = !g.open; }

bool Menu::IsOpen() { return g.open || g.alpha > 0.01f; }

int Menu::ToggleKey() { return g.toggleKey; }

void Menu::SetFonts(ImFont* big, ImFont* regular, ImFont* small) {
    fontBig = big;
    fontReg = regular;
    fontSmall = small;
}

void Menu::Draw() {
    Animate(g.alpha, g.open, (g.animations && g.fadeOnClose) ? 11.0f : 1000.0f);
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

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
                             ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                             ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground |
                             ImGuiWindowFlags_NoBringToFrontOnFocus;
    if (!g.dragging) flags |= ImGuiWindowFlags_NoMove;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::SetNextWindowBgAlpha(0.0f);
    ImGui::Begin("##mirick_root", nullptr, flags);

    const ImVec2 wp = ImGui::GetWindowPos() + ImVec2(kShadow, kShadow);
    const ImVec2 wmax = wp + ImVec2(kWidth * s, kHeight * s);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float round = g.rounded ? 16.0f * s : 0.0f;

    // Перетаскивание за верхнюю полосу (шапку)
    const bool overHeader = io.MousePos.x >= wp.x && io.MousePos.x <= wmax.x && io.MousePos.y >= wp.y &&
                            io.MousePos.y <= wp.y + 90.0f * s && io.MousePos.x > wp.x + kSidebar * s;
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) g.dragging = false;
    else if (overHeader && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) g.dragging = true;

    // Тень и корпус
    if (g.shadow) {
        for (int i = 8; i > 0; --i) {
            const float o = (float)i * 3.0f * s;
            dl->AddRect(wp - ImVec2(o, o), wmax + ImVec2(o, o), Col(0, 0, 0, 9, false), round + o, 0, 2.0f * s);
        }
    }
    dl->AddRectFilled(wp, wmax, Col(11, 11, 12, 252, false), round);
    dl->AddRect(wp, wmax, Col(255, 255, 255, 14, false), round);

    DrawSidebar(dl, wp);
    DrawHeader(dl, wp);
    DrawBody(dl, wp);
    DrawLanguageSwitch(dl, wp);

    ImGui::End();
    ImGui::PopStyleVar(2);
}
