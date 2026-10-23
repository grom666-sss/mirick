#include "Gfx/CharacterPreview.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

#include "Utils/Sdk.hpp"

#include "CBaseModelInfo.h"
#include "CModelInfo.h"
#include "CScene.h"
#include "CStreaming.h"
#include "CVisibilityPlugins.h"
#include "enums/eModelInfoType.h"
#include "enums/ePedBones.h"

namespace
{
    constexpr int kPreviewWidth = 384;
    constexpr int kPreviewHeight = 512;
    constexpr int kD3D9RasterExtOffsetAddress = 0xB4E9E0;

    struct RasterExtension
    {
        IDirect3DTexture9* texture;
    };

    RwCamera* camera = nullptr;
    RwFrame* cameraFrame = nullptr;
    RwRaster* colorRaster = nullptr;
    RwRaster* depthRaster = nullptr;
    RpLight* ambientLight = nullptr;
    RpClump* modelClump = nullptr;
    CBaseModelInfo* modelInfo = nullptr;
    RpHAnimHierarchy* hierarchy = nullptr;
    std::vector<RwMatrix> bindPose;

    int requestedModelId = -1;
    int activeModelId = -1;
    bool streamingRequested = false;
    float yaw = 0.0f;
    float headPose = 0.0f;
    float armPose = 0.0f;
    float legPose = 0.0f;
    CharacterPreview::State state = CharacterPreview::State::Idle;

    RwV3d xAxis{ 1.0f, 0.0f, 0.0f };
    RwV3d zAxis{ 0.0f, 0.0f, 1.0f };

    void ReleaseClump()
    {
        bindPose.clear();
        hierarchy = nullptr;
        if (modelClump)
        {
            RpClumpDestroy(modelClump);
            modelClump = nullptr;
        }
        if (modelInfo)
        {
            modelInfo->RemoveRef();
            modelInfo = nullptr;
        }
        activeModelId = -1;
    }

    void ReleaseScene()
    {
        ReleaseClump();

        if (ambientLight)
        {
            RpLightDestroy(ambientLight);
            ambientLight = nullptr;
        }
        if (Scene.m_pWorld && camera)
        {
            RpWorldRemoveCamera(Scene.m_pWorld, camera);
        }
        if (camera)
        {
            RwCameraSetFrame(camera, nullptr);
            RwCameraSetRaster(camera, nullptr);
            RwCameraSetZRaster(camera, nullptr);
            RwCameraDestroy(camera);
            camera = nullptr;
        }
        if (cameraFrame)
        {
            RwFrameDestroy(cameraFrame);
            cameraFrame = nullptr;
        }
        if (colorRaster)
        {
            RwRasterDestroy(colorRaster);
            colorRaster = nullptr;
        }
        if (depthRaster)
        {
            RwRasterDestroy(depthRaster);
            depthRaster = nullptr;
        }
    }

    bool CreateScene()
    {
        if (camera && colorRaster && depthRaster && ambientLight)
        {
            return true;
        }

        ReleaseScene();
        if (!Scene.m_pWorld)
        {
            return false;
        }

        colorRaster = RwRasterCreate(kPreviewWidth, kPreviewHeight, 32,
            rwRASTERTYPECAMERATEXTURE | rwRASTERFORMAT8888);
        depthRaster = RwRasterCreate(kPreviewWidth, kPreviewHeight, 0, rwRASTERTYPEZBUFFER);
        camera = RwCameraCreate();
        cameraFrame = RwFrameCreate();
        ambientLight = RpLightCreate(rpLIGHTAMBIENT);

        if (!colorRaster || !depthRaster || !camera || !cameraFrame || !ambientLight)
        {
            ReleaseScene();
            return false;
        }

        RwCameraSetRaster(camera, colorRaster);
        RwCameraSetZRaster(camera, depthRaster);
        RwCameraSetFrame(camera, cameraFrame);
        RwCameraSetNearClipPlane(camera, 0.01f);
        RwCameraSetFarClipPlane(camera, 100.0f);
        RwCameraSetProjection(camera, rwPERSPECTIVE);
        RwV2d viewWindow{ 0.38f * (static_cast<float>(kPreviewWidth) / kPreviewHeight), 0.38f };
        RwCameraSetViewWindow(camera, &viewWindow);

        // RenderWare cameras initially look along +Z. GTA peds use Z-up, so
        // rotate the isolated camera to look towards negative Y.
        RwFrameRotate(cameraFrame, &xAxis, 90.0f, rwCOMBINEREPLACE);
        RpWorldAddCamera(Scene.m_pWorld, camera);

        RwRGBAReal lightColor{ 1.0f, 1.0f, 1.0f, 1.0f };
        RpLightSetColor(ambientLight, &lightColor);
        return true;
    }

    IDirect3DTexture9* GetD3DTexture()
    {
        if (!colorRaster)
        {
            return nullptr;
        }

        RwRaster* root = colorRaster;
        while (root->parent && root->parent != root)
        {
            root = root->parent;
        }

        const int offset = *reinterpret_cast<const int*>(kD3D9RasterExtOffsetAddress);
        if (offset <= 0 || offset > 256)
        {
            return nullptr;
        }

        auto* extension = reinterpret_cast<RasterExtension*>(reinterpret_cast<unsigned char*>(root) + offset);
        return extension->texture;
    }

    bool CreateClump()
    {
        if (activeModelId == requestedModelId && modelClump)
        {
            return true;
        }

        ReleaseClump();
        if (requestedModelId < 0 || requestedModelId >= CModelInfo::ms_modelInfoCount)
        {
            state = CharacterPreview::State::InvalidId;
            return false;
        }

        CBaseModelInfo* info = CModelInfo::GetModelInfo(requestedModelId);
        if (!info)
        {
            state = CharacterPreview::State::Unavailable;
            return false;
        }
        if (info->GetModelType() != MODEL_INFO_PED)
        {
            state = CharacterPreview::State::NotPed;
            return false;
        }

        if (!info->m_pRwObject)
        {
            if (!streamingRequested)
            {
                CStreaming::RequestModel(requestedModelId, GAME_REQUIRED);
                streamingRequested = true;
            }
            state = CharacterPreview::State::Loading;
            return false;
        }

        RwObject* instance = info->CreateInstance();
        if (!instance || instance->type != rpCLUMP)
        {
            state = CharacterPreview::State::Unavailable;
            return false;
        }

        modelClump = reinterpret_cast<RpClump*>(instance);
        modelInfo = info;
        modelInfo->AddRef();
        activeModelId = requestedModelId;
        streamingRequested = false;

        RwFrame* root = RpClumpGetFrame(modelClump);
        hierarchy = root ? RpHAnimFrameGetHierarchy(root) : nullptr;
        if (hierarchy && hierarchy->pMatrixArray && hierarchy->numNodes > 0)
        {
            bindPose.assign(hierarchy->pMatrixArray, hierarchy->pMatrixArray + hierarchy->numNodes);
        }
        state = CharacterPreview::State::Ready;
        return true;
    }

    void RotateBone(int boneId, const RwV3d& axis, float degrees)
    {
        if (!hierarchy || bindPose.empty() || std::fabs(degrees) < 0.01f)
        {
            return;
        }

        const int index = RpHAnimIDGetIndex(hierarchy, boneId);
        if (index < 0 || index >= hierarchy->numNodes)
        {
            return;
        }

        RwMatrix& matrix = hierarchy->pMatrixArray[index];
        const RwV3d position = matrix.pos;
        matrix.pos = { 0.0f, 0.0f, 0.0f };
        RwMatrixRotate(&matrix, &axis, degrees, rwCOMBINEPOSTCONCAT);
        matrix.pos = position;
    }

    void ApplyPose()
    {
        if (!hierarchy || bindPose.size() != static_cast<size_t>(hierarchy->numNodes))
        {
            return;
        }

        std::memcpy(hierarchy->pMatrixArray, bindPose.data(), bindPose.size() * sizeof(RwMatrix));
        RotateBone(BONE_HEAD, xAxis, headPose);
        RotateBone(BONE_LEFTSHOULDER, xAxis, armPose);
        RotateBone(BONE_RIGHTSHOULDER, xAxis, -armPose);
        RotateBone(BONE_LEFTHIP, xAxis, legPose);
        RotateBone(BONE_RIGHTHIP, xAxis, -legPose);
    }

    void PositionClump()
    {
        RwFrame* root = RpClumpGetFrame(modelClump);
        if (!root)
        {
            return;
        }

        float radius = 0.55f;
        float centerZ = 0.9f;
        if (modelInfo && modelInfo->m_pColModel)
        {
            radius = std::max(0.35f, modelInfo->m_pColModel->m_boundSphere.m_fRadius);
            centerZ = (modelInfo->m_pColModel->m_boundBox.m_vecMin.z
                + modelInfo->m_pColModel->m_boundBox.m_vecMax.z) * 0.5f;
        }

        RwMatrix matrix{};
        RwMatrixSetIdentity(&matrix);
        RwMatrixRotate(&matrix, &zAxis, yaw, rwCOMBINEPOSTCONCAT);
        matrix.pos = { 0.0f, -radius * 3.5f, -centerZ };
        RwFrameTransform(root, &matrix, rwCOMBINEREPLACE);
        RwFrameUpdateObjects(root);
        ApplyPose();
    }
}

void CharacterPreview::SelectModel(int modelId)
{
    if (requestedModelId == modelId)
    {
        return;
    }

    requestedModelId = modelId;
    streamingRequested = false;
    state = State::Idle;
    ReleaseClump();
}

void CharacterPreview::SetRotation(float yawDegrees)
{
    yaw = std::fmod(yawDegrees, 360.0f);
}

void CharacterPreview::AdjustPose(PosePart part, float deltaDegrees)
{
    float* value = &headPose;
    if (part == PosePart::Arms)
    {
        value = &armPose;
    }
    else if (part == PosePart::Legs)
    {
        value = &legPose;
    }
    *value = std::clamp(*value + deltaDegrees, -55.0f, 55.0f);
}

void CharacterPreview::ResetPose()
{
    headPose = 0.0f;
    armPose = 0.0f;
    legPose = 0.0f;
    yaw = 0.0f;
}

IDirect3DTexture9* CharacterPreview::Render()
{
    if (!CreateScene() || !CreateClump())
    {
        return nullptr;
    }

    IDirect3DDevice9* device = nullptr;
    IDirect3DTexture9* texture = GetD3DTexture();
    if (!texture || FAILED(texture->GetDevice(&device)) || !device)
    {
        state = State::RenderError;
        return nullptr;
    }

    IDirect3DStateBlock9* stateBlock = nullptr;
    if (FAILED(device->CreateStateBlock(D3DSBT_ALL, &stateBlock)))
    {
        device->Release();
        state = State::RenderError;
        return nullptr;
    }
    stateBlock->Capture();

    PositionClump();
    CVisibilityPlugins::SetRenderWareCamera(camera);
    const RwRGBA clear{ 7, 10, 17, 0 };
    const bool began = RwCameraClear(camera, const_cast<RwRGBA*>(&clear), rwCAMERACLEARIMAGE | rwCAMERACLEARZ)
        && RwCameraBeginUpdate(camera);
    if (began)
    {
        RpWorldAddLight(Scene.m_pWorld, ambientLight);
        RwRenderStateSet(rwRENDERSTATEZTESTENABLE, reinterpret_cast<void*>(TRUE));
        RwRenderStateSet(rwRENDERSTATEZWRITEENABLE, reinterpret_cast<void*>(TRUE));
        RwRenderStateSet(rwRENDERSTATEFOGENABLE, reinterpret_cast<void*>(FALSE));
        RwRenderStateSet(rwRENDERSTATECULLMODE, reinterpret_cast<void*>(rwCULLMODECULLBACK));
        RpClumpRender(modelClump);
        RwCameraEndUpdate(camera);
        RpWorldRemoveLight(Scene.m_pWorld, ambientLight);
    }

    if (Scene.m_pCamera)
    {
        CVisibilityPlugins::SetRenderWareCamera(Scene.m_pCamera);
    }
    stateBlock->Apply();
    stateBlock->Release();
    device->Release();

    state = began ? State::Ready : State::RenderError;
    return began ? texture : nullptr;
}

CharacterPreview::State CharacterPreview::GetState()
{
    return state;
}

int CharacterPreview::GetModelId()
{
    return activeModelId;
}

void CharacterPreview::OnDeviceLost()
{
    ReleaseScene();
    state = State::Idle;
    streamingRequested = false;
}

void CharacterPreview::Shutdown()
{
    ReleaseScene();
    requestedModelId = -1;
    state = State::Idle;
    streamingRequested = false;
}
