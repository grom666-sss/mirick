#include "Game/Features.h"

#include "Game/Visuals/Esp.hpp"

#include "Menu/Menu.hpp"

#include "imgui.h"
#include "CColModel.h"
#include "CPools.h"
#include "CSprite.h"
#include "ePedBones.h"
#include "eWeaponType.h"

#include <array>
#include <cmath>
#include <cstdio>
#include <vector>

namespace
{
    const char* WeaponName(eWeaponType type)
    {
        switch (type)
        {
        case WEAPONTYPE_UNARMED: return "Кулаки";
        case WEAPONTYPE_BRASSKNUCKLE: return "Кастет";
        case WEAPONTYPE_GOLFCLUB: return "Клюшка";
        case WEAPONTYPE_NIGHTSTICK: return "Дубинка";
        case WEAPONTYPE_KNIFE: return "Нож";
        case WEAPONTYPE_BASEBALLBAT: return "Бита";
        case WEAPONTYPE_SHOVEL: return "Лопата";
        case WEAPONTYPE_POOLCUE: return "Бильярдный кий";
        case WEAPONTYPE_KATANA: return "Катана";
        case WEAPONTYPE_CHAINSAW: return "Бензопила";
        case WEAPONTYPE_DILDO1: return "Предмет";
        case WEAPONTYPE_DILDO2: return "Предмет 2";
        case WEAPONTYPE_VIBE1: return "Предмет 3";
        case WEAPONTYPE_VIBE2: return "Предмет 4";
        case WEAPONTYPE_FLOWERS: return "Цветы";
        case WEAPONTYPE_CANE: return "Трость";
        case WEAPONTYPE_GRENADE: return "Граната";
        case WEAPONTYPE_TEARGAS: return "Слезоточивый газ";
        case WEAPONTYPE_MOLOTOV: return "Коктейль Молотова";
        case WEAPONTYPE_PISTOL: return "Пистолет";
        case WEAPONTYPE_PISTOL_SILENCED: return "Пистолет с глушителем";
        case WEAPONTYPE_DESERT_EAGLE: return "Дезерт Игл";
        case WEAPONTYPE_SHOTGUN: return "Дробовик";
        case WEAPONTYPE_SAWNOFF: return "Обрез";
        case WEAPONTYPE_SPAS12: return "СПАС-12";
        case WEAPONTYPE_MICRO_UZI: return "Микро-Узи";
        case WEAPONTYPE_MP5: return "МП-5";
        case WEAPONTYPE_AK47: return "АК-47";
        case WEAPONTYPE_M4: return "М4";
        case WEAPONTYPE_TEC9: return "ТЕК-9";
        case WEAPONTYPE_COUNTRYRIFLE: return "Охотничья винтовка";
        case WEAPONTYPE_SNIPERRIFLE: return "Снайперская винтовка";
        case WEAPONTYPE_RLAUNCHER: return "РПГ";
        case WEAPONTYPE_RLAUNCHER_HS: return "РПГ с наведением";
        case WEAPONTYPE_FTHROWER: return "Огнемёт";
        case WEAPONTYPE_MINIGUN: return "Миниган";
        case WEAPONTYPE_SATCHEL_CHARGE: return "Взрывчатка";
        case WEAPONTYPE_DETONATOR: return "Детонатор";
        case WEAPONTYPE_SPRAYCAN: return "Баллончик";
        case WEAPONTYPE_EXTINGUISHER: return "Огнетушитель";
        case WEAPONTYPE_CAMERA: return "Камера";
        case WEAPONTYPE_NIGHTVISION: return "Прибор ночного видения";
        case WEAPONTYPE_INFRARED: return "Тепловизор";
        case WEAPONTYPE_PARACHUTE: return "Парашют";
        default: return "Неизвестное оружие";
        }
    }

    bool WeaponUsesAmmo(eWeaponType type)
    {
        return (type >= WEAPONTYPE_GRENADE && type <= WEAPONTYPE_SATCHEL_CHARGE)
            || type == WEAPONTYPE_SPRAYCAN
            || type == WEAPONTYPE_EXTINGUISHER
            || type == WEAPONTYPE_CAMERA;
    }
}

void Esp::Update()
{
    if (!g_cfg.wh)
    {
        return;
    }

    CPed* pLocal = FindPlayerPed();

    if (!pLocal || !CPools::ms_pPedPool)
    {
        return;
    }

    const CVector localPos = pLocal->GetPosition();
    const float s = menu->GetScale();
    ImDrawList* draw = ImGui::GetForegroundDrawList();
    const ImU32 boxCol = g_cfg.whcol.to_color().as_imcolor();
    const ImU32 backCol = IM_COL32(0, 0, 0, 180);
    const ImU32 hpCol = g_cfg.hpcol.to_color().as_imcolor();
    const ImU32 armorCol = g_cfg.armorcol.to_color().as_imcolor();
    const ImU32 textCol = g_cfg.distcol.to_color().as_imcolor();
    const ImU32 skelCol = g_cfg.skelcol.to_color().as_imcolor();
    const ImU32 skelOccludedCol = g_cfg.skeloccludedcol.to_color().as_imcolor();
    const ImU32 weaponCol = g_cfg.weaponcol.to_color().as_imcolor();

    const int poolSize = CPools::ms_pPedPool->m_nSize;

    // Collision raycasts are much more expensive than projection/drawing. Keep
    // a per-pool-slot cache and update it incrementally with a fixed frame
    // budget, so turning toward a crowded area cannot cause a large FPS spike.
    struct VisibilityCache
    {
        CPed* owner = nullptr;
        std::array<bool, 17> occluded{};
        size_t nextBone = 0;
    };
    static std::vector<VisibilityCache> visibilityCache;
    static int visibilityStart = 0;
    visibilityCache.resize(poolSize);
    if (visibilityStart >= poolSize)
    {
        visibilityStart = 0;
    }
    int traceBudget = 12;
    int lastTracedSlot = -1;

    for (int poolOffset = 0; poolOffset < poolSize; ++poolOffset)
    {
        const int i = (visibilityStart + poolOffset) % poolSize;
        CPed* ped = CPools::ms_pPedPool->GetAt(i);

        if (!ped || ped == pLocal || ped->m_fHealth <= 0.0f)
        {
            continue;
        }

        const CVector foot = ped->GetPosition();
        const float dist = VecLength(CVector(foot.x - localPos.x, foot.y - localPos.y, foot.z - localPos.z));

        if (dist > g_cfg.whDistance)
        {
            continue;
        }

        float pedH = 1.8f;
        float footZ = foot.z;

        if (CColModel* col = ped->GetColModel())
        {
            const float h = col->m_boundBox.m_vecMax.z - col->m_boundBox.m_vecMin.z;

            if (h >= 0.5f && h <= 3.0f)
            {
                pedH = h;
                footZ = foot.z + col->m_boundBox.m_vecMin.z;
            }
        }

        const RwV3d foot3d = { foot.x, foot.y, footZ };
        const RwV3d head3d = { foot.x, foot.y, footZ + pedH };
        RwV3d footScr{}, headScr{};
        float w = 0.0f, h = 0.0f;

        if (!CSprite::CalcScreenCoors(foot3d, &footScr, &w, &h, false, true))
        {
            continue;
        }

        if (!CSprite::CalcScreenCoors(head3d, &headScr, &w, &h, false, true))
        {
            continue;
        }

        // Do not assume that the projected head is always above the feet. That
        // assumption breaks at steep camera pitches (most noticeably while
        // aiming), even though both points are still in front of the camera.
        const float boxTop = std::min(headScr.y, footScr.y);
        const float boxBottom = std::max(headScr.y, footScr.y);
        const float boxH = boxBottom - boxTop;

        if (!std::isfinite(boxH) || boxH < 1.0f)
        {
            continue;
        }

        const float boxW = boxH * 0.45f;
        const float boxCenterX = (headScr.x + footScr.x) * 0.5f;
        const ImVec2 min(boxCenterX - boxW * 0.5f, boxTop);
        const ImVec2 max(boxCenterX + boxW * 0.5f, boxBottom);

        const bool showHealth = (g_cfg.wh_flags & WH_HP) != 0;
        const bool showArmor = (g_cfg.wh_flags & WH_ARMOR) != 0;
        const bool showVitals = showHealth || showArmor;
        char hpText[20]{};
        char armorText[24]{};
        ImVec2 hpTextSize{};
        ImVec2 armorTextSize{};
        const ImVec2 separatorSize = ImGui::CalcTextSize("  |  ");
        const float vitalsPadY = 2.0f * s;
        float vitalsHeight = 0.0f;

        if (showHealth)
        {
            const int health = static_cast<int>(std::max(ped->m_fHealth, 0.0f) + 0.5f);
            snprintf(hpText, sizeof(hpText), "HP %d", health);
            hpTextSize = ImGui::CalcTextSize(hpText);
            vitalsHeight = hpTextSize.y + vitalsPadY * 2.0f;
        }
        if (showArmor)
        {
            const int armor = static_cast<int>(std::max(ped->m_fArmour, 0.0f) + 0.5f);
            snprintf(armorText, sizeof(armorText), "ARMOR %d", armor);
            armorTextSize = ImGui::CalcTextSize(armorText);
            vitalsHeight = std::max(vitalsHeight, armorTextSize.y + vitalsPadY * 2.0f);
        }

        if (g_cfg.wh_flags & WH_BOX)
        {
            draw->AddRect(min - ImVec2(1.0f, 1.0f), max + ImVec2(1.0f, 1.0f), backCol, 0.0f, 0, 1.0f);
            draw->AddRect(min, max, boxCol, 0.0f, 0, 1.0f);
            draw->AddRect(min + ImVec2(1.0f, 1.0f), max - ImVec2(1.0f, 1.0f), backCol, 0.0f, 0, 1.0f);
        }

        // Stack labels in a stable order: localized player label, HP/armor,
        // then weapon and current magazine. Next RP does not expose nicknames
        // through the available CPed API, so the local network-pool slot is
        // shown instead of inventing a name.
        char playerLabel[40]{};
        snprintf(playerLabel, sizeof(playerLabel), "ИГРОК #%d", i);
        const ImVec2 playerLabelSize = ImGui::CalcTextSize(playerLabel);

        const bool showWeapon = (g_cfg.wh_flags & WH_WEAPON) != 0;
        const char* weaponName = nullptr;
        char ammoLabel[40]{};
        ImVec2 weaponNameSize{};
        ImVec2 ammoSize{};
        bool showAmmo = false;
        if (showWeapon)
        {
            if (CWeapon* weapon = ped->GetWeapon())
            {
                const eWeaponType type = weapon->m_eWeaponType;
                weaponName = WeaponName(type);
                showAmmo = WeaponUsesAmmo(type);
                if (showAmmo)
                {
                    unsigned int clip = weapon->m_nAmmoInClip;
                    if (type >= WEAPONTYPE_PISTOL && type <= WEAPONTYPE_MINIGUN && clip > 0)
                    {
                        --clip;
                    }
                    snprintf(ammoLabel, sizeof(ammoLabel), "МАГАЗИН  %u", clip);
                    ammoSize = ImGui::CalcTextSize(ammoLabel);
                }
                weaponNameSize = ImGui::CalcTextSize(weaponName);
            }
        }

        const float labelGap = 3.0f * s;
        float labelsHeight = playerLabelSize.y;
        if (showVitals)
        {
            labelsHeight += labelGap + vitalsHeight;
        }
        if (weaponName)
        {
            labelsHeight += labelGap + weaponNameSize.y;
            if (showAmmo)
            {
                labelsHeight += 2.0f * s + ammoSize.y;
            }
        }

        float labelY = min.y - labelsHeight - 5.0f * s;
        const ImVec2 playerLabelPos(boxCenterX - playerLabelSize.x * 0.5f, labelY);
        draw->AddText(playerLabelPos + ImVec2(1.0f, 1.0f), IM_COL32(0, 0, 0, 220), playerLabel);
        draw->AddText(playerLabelPos, IM_COL32(242, 245, 255, 255), playerLabel);
        draw->AddLine(
            ImVec2(boxCenterX - playerLabelSize.x * 0.35f, labelY + playerLabelSize.y + 1.0f * s),
            ImVec2(boxCenterX + playerLabelSize.x * 0.35f, labelY + playerLabelSize.y + 1.0f * s),
            boxCol, 1.0f * s);
        labelY += playerLabelSize.y;

        if (showVitals)
        {
            labelY += labelGap;
            const float separatorWidth = showHealth && showArmor ? separatorSize.x : 0.0f;
            const float textWidth = hpTextSize.x + separatorWidth + armorTextSize.x;
            float textX = boxCenterX - textWidth * 0.5f;
            const float textY = labelY + vitalsPadY;
            if (showHealth)
            {
                draw->AddText(ImVec2(textX, textY), hpCol, hpText);
                textX += hpTextSize.x;
            }
            if (showHealth && showArmor)
            {
                draw->AddText(ImVec2(textX, textY), IM_COL32(145, 151, 166, 210), "  |  ");
                textX += separatorSize.x;
            }
            if (showArmor)
            {
                draw->AddText(ImVec2(textX, textY), armorCol, armorText);
            }
            labelY += vitalsHeight;
        }

        if (weaponName)
        {
            labelY += labelGap;
            const ImVec2 weaponPos(boxCenterX - weaponNameSize.x * 0.5f, labelY);
            draw->AddText(weaponPos + ImVec2(1.0f, 1.0f), IM_COL32(0, 0, 0, 210), weaponName);
            draw->AddText(weaponPos, IM_COL32(242, 245, 251, 255), weaponName);
            labelY += weaponNameSize.y;
            if (showAmmo)
            {
                labelY += 2.0f * s;
                const ImVec2 ammoPos(boxCenterX - ammoSize.x * 0.5f, labelY);
                draw->AddText(ammoPos + ImVec2(1.0f, 1.0f), IM_COL32(0, 0, 0, 210), ammoLabel);
                draw->AddText(ammoPos, weaponCol, ammoLabel);
            }
        }

        if (g_cfg.wh_flags & WH_DIST)
        {
            char buf[16]{};
            snprintf(buf, sizeof(buf), "%d m", (int)dist);
            const ImVec2 tp(min.x, max.y + 2.0f * s);
            draw->AddText(tp + ImVec2(1.0f, 1.0f), IM_COL32(0, 0, 0, 200), buf);
            draw->AddText(tp, textCol, buf);
        }

        if (g_cfg.wh_flags & WH_SKELETON)
        {
            struct BoneRenderData
            {
                RwV3d world{};
                RwV3d screen{};
                bool projected = false;
                bool occluded = false;
            };

            static constexpr int boneIds[] = {
                BONE_PELVIS, BONE_SPINE1, BONE_UPPERTORSO, BONE_NECK, BONE_HEAD,
                BONE_RIGHTSHOULDER, BONE_RIGHTELBOW, BONE_RIGHTWRIST,
                BONE_LEFTSHOULDER, BONE_LEFTELBOW, BONE_LEFTWRIST,
                BONE_RIGHTHIP, BONE_RIGHTKNEE, BONE_RIGHTANKLE,
                BONE_LEFTHIP, BONE_LEFTKNEE, BONE_LEFTANKLE,
            };
            static constexpr int segments[][2] = {
                { 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 4 },
                { 2, 5 }, { 5, 6 }, { 6, 7 },
                { 2, 8 }, { 8, 9 }, { 9, 10 },
                { 0, 11 }, { 11, 12 }, { 12, 13 },
                { 0, 14 }, { 14, 15 }, { 15, 16 },
            };

            std::array<BoneRenderData, sizeof(boneIds) / sizeof(boneIds[0])> bones{};
            const CCam& activeCam = TheCamera.m_aCams[TheCamera.m_nActiveCam];
            const CVector traceStart = activeCam.m_vecSource;
            const auto IsOccluded = [&](const RwV3d& point)
            {
                const CVector traceEnd(point.x, point.y, point.z);
                CColPoint visibilityHit{};
                CEntity* visibilityEntity = nullptr;
                return CWorld::ProcessLineOfSight(
                    traceStart,
                    traceEnd,
                    visibilityHit,
                    visibilityEntity,
                    true,  // buildings
                    true,  // vehicles
                    false, // peds (including the target)
                    true,  // objects
                    true,  // dummies
                    false,
                    false,
                    false);
            };

            // Test every joint independently. This keeps an arm/head in the
            // visible color when it sticks out while the torso remains covered.
            for (size_t bi = 0; bi < bones.size(); ++bi)
            {
                BoneRenderData& bone = bones[bi];
                ped->GetBonePosition(bone.world, static_cast<unsigned int>(boneIds[bi]), true);

                float sw = 0.0f, sh = 0.0f;
                bone.projected = CSprite::CalcScreenCoors(bone.world, &bone.screen, &sw, &sh, false, true);
                if (!bone.projected)
                {
                    continue;
                }

            }

            VisibilityCache& cache = visibilityCache[i];
            if (cache.owner != ped)
            {
                cache.owner = ped;
                cache.occluded.fill(true);
                cache.nextBone = 0;
            }

            // Update at most one joint of this ped in one frame. Cached values
            // keep the skeleton stable between updates while spreading work
            // across many players.
            int updatedForPed = 0;
            int attemptedBones = 0;
            while (traceBudget > 0 && updatedForPed < 1 && attemptedBones < (int)bones.size())
            {
                const size_t bi = cache.nextBone;
                cache.nextBone = (cache.nextBone + 1) % bones.size();
                ++attemptedBones;

                if (!bones[bi].projected)
                {
                    continue;
                }

                cache.occluded[bi] = IsOccluded(bones[bi].world);
                --traceBudget;
                ++updatedForPed;
                lastTracedSlot = i;
            }

            for (size_t bi = 0; bi < bones.size(); ++bi)
            {
                bones[bi].occluded = cache.occluded[bi];
            }

            for (const auto& segment : segments)
            {
                const BoneRenderData& first = bones[segment[0]];
                const BoneRenderData& second = bones[segment[1]];
                if (!first.projected || !second.projected)
                {
                    continue;
                }

                const ImVec2 p0(first.screen.x, first.screen.y);
                const ImVec2 p1(second.screen.x, second.screen.y);
                const ImU32 col0 = first.occluded ? skelOccludedCol : skelCol;
                const ImU32 col1 = second.occluded ? skelOccludedCol : skelCol;

                if (first.occluded == second.occluded)
                {
                    draw->AddLine(p0, p1, col0, 1.0f);
                }
                else
                {
                    // Split mixed-visibility limbs without extra collision
                    // queries; endpoint visibility comes from the cache.
                    const ImVec2 boundary((p0.x + p1.x) * 0.5f, (p0.y + p1.y) * 0.5f);
                    draw->AddLine(p0, boundary, col0, 1.0f);
                    draw->AddLine(boundary, p1, col1, 1.0f);
                }
            }
        }

    }

    if (poolSize > 0)
    {
        visibilityStart = lastTracedSlot >= 0
            ? (lastTracedSlot + 1) % poolSize
            : (visibilityStart + 1) % poolSize;
    }
}
