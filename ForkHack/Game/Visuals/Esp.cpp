#include "Game/Features.h"

#include "Game/Visuals/Esp.hpp"

#include "Menu/Menu.hpp"

#include "imgui.h"
#include "CColModel.h"
#include "CPools.h"
#include "CSprite.h"
#include "ePedBones.h"

#include <array>
#include <cmath>
#include <cstdio>

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
    const ImU32 armorCol = g_cfg.armorcol.to_color().as_imcolor();
    const ImU32 textCol = g_cfg.distcol.to_color().as_imcolor();
    const ImU32 skelCol = g_cfg.skelcol.to_color().as_imcolor();
    const ImU32 skelOccludedCol = g_cfg.skeloccludedcol.to_color().as_imcolor();
    const ImU32 snapCol = g_cfg.snapcol.to_color().as_imcolor();

    const int poolSize = CPools::ms_pPedPool->m_nSize;

    for (int i = 0; i < poolSize; i++)
    {
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

        if (g_cfg.wh_flags & WH_SNAP)
        {
            const RwV3d epos = { foot.x, foot.y, foot.z - 1.1f };
            RwV3d escr{};
            float ew = 0.0f, eh = 0.0f;
            ImVec2 to;

            if (CSprite::CalcScreenCoors(epos, &escr, &ew, &eh, true, true))
            {
                to = ImVec2(escr.x, escr.y);
            }
            else
            {
                CMatrix& cm = TheCamera.m_mCameraMatrix;
                float dx = foot.x - cm.pos.x;
                float dy = foot.y - cm.pos.y;
                float dz = foot.z - cm.pos.z;
                float rx = dx * cm.right.x + dy * cm.right.y + dz * cm.right.z;
                float fx = dx * cm.at.x + dy * cm.at.y + dz * cm.at.z;
                float ang = atan2f(rx, fx);
                float cx = ImGui::GetIO().DisplaySize.x * 0.5f;
                float cy = ImGui::GetIO().DisplaySize.y * 0.5f;
                float rr = (cx < cy ? cx : cy) * 0.9f;
                to = ImVec2(cx + sinf(ang) * rr, cy - cosf(ang) * rr);
            }

            const RwV3d lpos = { localPos.x, localPos.y, localPos.z - 1.1f };
            RwV3d lscr{};
            float lw = 0.0f, lh = 0.0f;
            ImVec2 from;

            if (CSprite::CalcScreenCoors(lpos, &lscr, &lw, &lh, true, true))
            {
                from = ImVec2(lscr.x, lscr.y);
            }
            else
            {
                from = ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y);
            }

            draw->AddLine(from, to, snapCol, 2.0f);
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

        if (g_cfg.wh_flags & WH_BOX)
        {
            draw->AddRect(min - ImVec2(1.0f, 1.0f), max + ImVec2(1.0f, 1.0f), backCol, 0.0f, 0, 1.0f);
            draw->AddRect(min, max, boxCol, 0.0f, 0, 1.0f);
            draw->AddRect(min + ImVec2(1.0f, 1.0f), max - ImVec2(1.0f, 1.0f), backCol, 0.0f, 0, 1.0f);
        }

        if (g_cfg.wh_flags & WH_HP)
        {
            const float hp = std::clamp(ped->m_fHealth / 100.0f, 0.0f, 1.0f);
            const ImVec2 barMin(min.x - 5.0f * s, min.y);
            const ImVec2 barMax(min.x - 2.0f * s, max.y);
            const ImU32 hpFill = g_cfg.hpcol.to_color().multiply(c_color(255, 30, 30), 1.0f - hp).as_imcolor();
            draw->AddRect(barMin - ImVec2(1.0f, 1.0f), barMax + ImVec2(1.0f, 1.0f), backCol, 0.0f, 0, 1.0f);
            draw->AddRectFilled(barMin, barMax, IM_COL32(0, 0, 0, 150), 0.0f);
            draw->AddRectFilled(ImVec2(barMin.x, barMax.y - (barMax.y - barMin.y) * hp), barMax, hpFill, 0.0f);
        }

        if ((g_cfg.wh_flags & WH_ARMOR) && ped->m_fArmour > 0.0f)
        {
            const float ap = std::clamp(ped->m_fArmour / 100.0f, 0.0f, 1.0f);
            const ImVec2 barMin(max.x + 2.0f * s, min.y);
            const ImVec2 barMax(max.x + 5.0f * s, max.y);
            draw->AddRect(barMin - ImVec2(1.0f, 1.0f), barMax + ImVec2(1.0f, 1.0f), backCol, 0.0f, 0, 1.0f);
            draw->AddRectFilled(barMin, barMax, IM_COL32(0, 0, 0, 150), 0.0f);
            draw->AddRectFilled(ImVec2(barMin.x, barMax.y - (barMax.y - barMin.y) * ap), barMax, armorCol, 0.0f);
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

                bone.occluded = IsOccluded(bone.world);
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
                    // Locate the cover edge along the limb instead of changing
                    // the entire segment. Four visibility samples give a stable
                    // boundary without doing excessive collision traces.
                    RwV3d sameSide = first.world;
                    RwV3d otherSide = second.world;
                    for (int step = 0; step < 4; ++step)
                    {
                        const RwV3d middleWorld{
                            (sameSide.x + otherSide.x) * 0.5f,
                            (sameSide.y + otherSide.y) * 0.5f,
                            (sameSide.z + otherSide.z) * 0.5f,
                        };

                        if (IsOccluded(middleWorld) == first.occluded)
                        {
                            sameSide = middleWorld;
                        }
                        else
                        {
                            otherSide = middleWorld;
                        }
                    }

                    const RwV3d boundaryWorld{
                        (sameSide.x + otherSide.x) * 0.5f,
                        (sameSide.y + otherSide.y) * 0.5f,
                        (sameSide.z + otherSide.z) * 0.5f,
                    };
                    RwV3d boundaryScreen{};
                    float boundaryW = 0.0f, boundaryH = 0.0f;
                    ImVec2 boundary((p0.x + p1.x) * 0.5f, (p0.y + p1.y) * 0.5f);
                    if (CSprite::CalcScreenCoors(boundaryWorld, &boundaryScreen, &boundaryW, &boundaryH, false, true))
                    {
                        boundary = ImVec2(boundaryScreen.x, boundaryScreen.y);
                    }

                    draw->AddLine(p0, boundary, col0, 1.0f);
                    draw->AddLine(boundary, p1, col1, 1.0f);
                }
            }
        }

    }
}
