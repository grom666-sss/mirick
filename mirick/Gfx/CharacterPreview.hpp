#pragma once

#include <d3d9.h>

namespace CharacterPreview
{
    enum class PosePart
    {
        Head,
        Arms,
        Legs
    };

    enum class State
    {
        Idle,
        Loading,
        Ready,
        InvalidId,
        NotPed,
        Unavailable,
        RenderError
    };

    // Hooks preview rendering into GTA's RenderWare render phase. RenderWare
    // work must never run from IDirect3DDevice9::Present.
    void Initialize();
    void SetVisible(bool visible);

    // Selects a registered runtime model. This never changes the player ped.
    void SelectModel(int modelId);
    void SetRotation(float yawDegrees);
    void AdjustPose(PosePart part, float deltaDegrees);
    void ResetPose();

    // Returns the camera texture produced during GTA's previous render phase.
    // It is safe to pass this pointer to ImGui; no RenderWare calls happen here.
    IDirect3DTexture9* GetTexture();

    State GetState();
    int GetModelId();

    void OnDeviceLost();
    void Shutdown();
}
