#include "Game/Features.h"

#include "Game/Misc/GodMode.hpp"

#include <algorithm>
#include <random>

namespace
{
    template <typename T>
    void SetProofFlags(T* entity, bool enabled)
    {
        if (!entity)
        {
            return;
        }

        entity->bBulletProof = enabled;
        entity->bFireProof = enabled;
        entity->bCollisionProof = enabled;
        entity->bMeleeProof = enabled;
        entity->bExplosionProof = enabled;
        entity->bInvulnerable = enabled;
    }
}

void GodMode::Update()
{
    CPed* ped = FindPlayerPed(-1);
    if (!ped)
    {
        return;
    }

    CVehicle* veh = FindPlayerVehicle(0, false);
    static bool protectedNow = true;
    static DWORD nextRoll = 0;
    static std::mt19937 random{ static_cast<unsigned int>(GetTickCount()) };

    bool enableProtection = g_cfg.godmode;
    if (enableProtection && g_cfg.godmode_random)
    {
        const DWORD now = GetTickCount();
        if (now >= nextRoll)
        {
            const int chance = std::clamp(g_cfg.godmode_chance, 0, 100);
            protectedNow = chance >= 100
                || (chance > 0 && std::uniform_int_distribution<int>(1, 100)(random) <= chance);
            nextRoll = now + 250;
        }
        enableProtection = protectedNow;
    }
    else
    {
        protectedNow = true;
        nextRoll = 0;
    }

    SetProofFlags(ped, enableProtection);
    SetProofFlags(veh, enableProtection);
}
