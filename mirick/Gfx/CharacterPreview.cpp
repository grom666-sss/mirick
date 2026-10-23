#include "Gfx/CharacterPreview.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <mutex>
#include <vector>

#include "Utils/Sdk.hpp"
#include "Gfx/Blur.hpp"

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

    RwCamera* camera = nullptr;
    RwFrame* cameraFrame = nullptr;
    RwRaster* colorRaster = nullptr;
    RwRaster* depthRaster = nullptr;
    RpLight* ambientLight = nullptr;
    RpClump* modelClump = nullptr;
    CBaseModelInfo* modelInfo = nullptr;
    RpHAnimHierarchy* hierarchy = nullptr;
    std::vector<RwMatrix> bindPose;
    IDirect3DTexture9* displayTexture = nullptr;
    std::vector<unsigned char> capturedPixels;
    std::mutex capturedPixelsMutex;
    unsigned int capturedGeneration = 0;
    unsigned int uploadedGeneration = 0;

    plugin::CdeclEvent<plugin::AddressList<0x53EA12, plugin::H_CALL>, plugin::PRIORITY_BEFORE,
        plugin::ArgPickNone, void()> previewRenderEvent;

    int requestedModelId = -1;
    int activeModelId = -1;
    bool streamingRequested = false;
    bool initialized = false;
    bool visible = false;
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
        if (displayTexture)
        {
            displayTexture->Release();
            displayTexture = nullptr;
        }
        {
            std::lock_guard<std::mutex> lock(capturedPixelsMutex);
            capturedPixels.clear();
            capturedGeneration = 0;
            uploadedGeneration = 0;
        }

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

namespace
{
    bool CapturePreviewPixels()
    {
        if (!colorRaster)
        {
            return false;
        }

        unsigned char* pixels = RwRasterLock(colorRaster, 0, rwRASTERLOCKREAD);
        if (!pixels || colorRaster->stride < kPreviewWidth * 4)
        {
            if (pixels)
            {
                RwRasterUnlock(colorRaster);
            }
            return false;
        }

        std::vector<unsigned char> next(static_cast<size_t>(kPreviewWidth) * kPreviewHeight * 4);
        for (int y = 0; y < kPreviewHeight; ++y)
        {
            std::memcpy(next.data() + static_cast<size_t>(y) * kPreviewWidth * 4,
                pixels + static_cast<size_t>(y) * colorRaster->stride,
                static_cast<size_t>(kPreviewWidth) * 4);
        }
        RwRasterUnlock(colorRaster);

        std::lock_guard<std::mutex> lock(capturedPixelsMutex);
        capturedPixels.swap(next);
        ++capturedGeneration;
        return true;
    }

    void RenderPreviewFrame()
    {
        if (!visible || requestedModelId < 0)
        {
            return;
        }

        if (!CreateScene() || !CreateClump())
        {
            return;
        }

        PositionClump();
        CVisibilityPlugins::SetRenderWareCamera(camera);
        RwRGBA clear{ 7, 10, 17, 0 };
        const bool began = RwCameraClear(camera, &clear, rwCAMERACLEARIMAGE | rwCAMERACLEARZ)
            && RwCameraBeginUpdate(camera);
        bool captured = false;
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
            captured = CapturePreviewPixels();
        }

        if (Scene.m_pCamera)
        {
            CVisibilityPlugins::SetRenderWareCamera(Scene.m_pCamera);
        }
        state = began && captured ? CharacterPreview::State::Ready : CharacterPreview::State::RenderError;
    }
}

void CharacterPreview::Initialize()
{
    if (initialized)
    {
        return;
    }

    // 0x53EA12 is immediately before GTA's main RenderWare scene render. At
    // this point RenderWare is initialized and owns the D3D device. Calling
    // these APIs from Present caused a D3D9 access violation on Next RP.
    previewRenderEvent += [] { RenderPreviewFrame(); };
    initialized = true;
}

void CharacterPreview::SetVisible(bool isVisible)
{
    visible = isVisible;
}

IDirect3DTexture9* CharacterPreview::GetTexture()
{
    if (state != State::Ready)
    {
        return nullptr;
    }

    IDirect3DDevice9* device = Blur::GetDevice();
    if (!device)
    {
        return nullptr;
    }
    if (!displayTexture && FAILED(device->CreateTexture(kPreviewWidth, kPreviewHeight, 1, 0,
        D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &displayTexture, nullptr)))
    {
        state = State::RenderError;
        return nullptr;
    }

    std::vector<unsigned char> pixels;
    unsigned int generation = 0;
    {
        std::lock_guard<std::mutex> lock(capturedPixelsMutex);
        generation = capturedGeneration;
        if (generation != uploadedGeneration)
        {
            pixels = capturedPixels;
        }
    }

    if (!pixels.empty())
    {
        D3DLOCKED_RECT locked{};
        if (FAILED(displayTexture->LockRect(0, &locked, nullptr, 0)))
        {
            state = State::RenderError;
            return nullptr;
        }
        for (int y = 0; y < kPreviewHeight; ++y)
        {
            std::memcpy(static_cast<unsigned char*>(locked.pBits) + static_cast<size_t>(y) * locked.Pitch,
                pixels.data() + static_cast<size_t>(y) * kPreviewWidth * 4,
                static_cast<size_t>(kPreviewWidth) * 4);
        }
        displayTexture->UnlockRect(0);
        uploadedGeneration = generation;
    }

    return uploadedGeneration ? displayTexture : nullptr;
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
