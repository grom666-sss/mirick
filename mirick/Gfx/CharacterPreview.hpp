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

    // Selects a registered runtime model. This never changes the player ped.
    void SelectModel(int modelId);
    void SetRotation(float yawDegrees);
    void AdjustPose(PosePart part, float deltaDegrees);
    void ResetPose();

    // Renders the standalone clump into the camera texture and returns the
    // underlying D3D9 texture accepted by ImGui's DX9 backend.
    IDirect3DTexture9* Render();

    State GetState();
    int GetModelId();

    // RenderWare camera textures live in the D3D default pool.
    void OnDeviceLost();
    void Shutdown();
}
