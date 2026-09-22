#include "CascadedShadowsManager.h"
#include "common/ShadowMapRes.h"
#include "common/SDKMeshModel.h"
#include "common/Camera.h"

using namespace DirectX;
using namespace DirectX::PackedVector;
using Microsoft::WRL::ComPtr;


static const XMVECTORF32 g_vFLTMAX = { FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX };
static const XMVECTORF32 g_vFLTMIN = { -FLT_MAX, -FLT_MAX, -FLT_MAX, -FLT_MAX };
static const XMVECTORF32 g_vHalfVector = { 0.5f, 0.5f, 0.5f, 0.5f };
static const XMVECTORF32 g_vMultiplySetzwToZero = { 1.0f, 1.0f, 0.0f, 0.0f };
static const XMVECTORF32 g_vZero = { 0.0f, 0.0f, 0.0f, 0.0f };

bool CascadedShadowsManager::init(ID3D12Device* device,const DirectX::BoundingBox& sceneBox,
	const Camera* viewCamra, const Camera* shadowCamera, CSMConfig* config)
{
    m_d3dDevice = device;
    m_csmConfig = config;
    m_copyCsmConfig = *m_csmConfig;
    m_sceneBox = sceneBox;

	//init shaders
	m_ShadowVSShader = D3DUtil::CompileShader(SourcePath() + L"/Shaders/CSM/csm_shadowMap.hlsl", nullptr, "VS", "vs_5_1");

    D3D_SHADER_MACRO defines[] =
    {
        "CASCADE_COUNT_FLAG", "1",
        "USE_DERIVATIVES_FOR_DEPTH_OFFSET_FLAG", "0",
        "BLEND_BETWEEN_CASCADE_LAYERS_FLAG", "0",
        "SELECT_CASCADE_BY_INTERVAL_FLAG", "0",
        nullptr, nullptr
    };

    char cCascadeDefinition[32];
    char cDerivativeDefinition[32];
    char cBlendDefinition[32];
    char cIntervalDefinition[32];

    for (INT iCascadeIndex = 0; iCascadeIndex < MAX_CASCADES; ++iCascadeIndex)
    {
        // There is just one vertex shader for the scene.                
        sprintf_s(cCascadeDefinition, "%d", iCascadeIndex + 1);
        defines[0].Definition = cCascadeDefinition;
        defines[1].Definition = "0";
        defines[2].Definition = "0";
        defines[3].Definition = "0";


        m_ppsRenderSceneVSShadersBlob[iCascadeIndex] =
            D3DUtil::CompileShader(SourcePath() + L"/Shaders/CSM/csm_scene.hlsl", defines, "VS", "vs_5_1");

        for (INT iDerivativeIndex = 0; iDerivativeIndex < 2; ++iDerivativeIndex)
        {
            for (INT iBlendIndex = 0; iBlendIndex < 2; ++iBlendIndex)
            {
                for (INT iIntervalIndex = 0; iIntervalIndex < 2; ++iIntervalIndex)
                {
                    sprintf_s(cCascadeDefinition, "%d", iCascadeIndex + 1);
                    sprintf_s(cDerivativeDefinition, "%d", iDerivativeIndex);
                    sprintf_s(cBlendDefinition, "%d", iBlendIndex);
                    sprintf_s(cIntervalDefinition, "%d", iIntervalIndex);

                    defines[0].Definition = cCascadeDefinition;
                    defines[1].Definition = cDerivativeDefinition;
                    defines[2].Definition = cBlendDefinition;
                    defines[3].Definition = cIntervalDefinition;

                    m_ppsRenderScenePSShadersBlob[iCascadeIndex][iDerivativeIndex][iBlendIndex][iIntervalIndex] =
                        D3DUtil::CompileShader(SourcePath() + L"/Shaders/CSM/csm_scene.hlsl", defines, "PS", "ps_5_1");
                }
            }
        }
    }


	return true;
}

void CascadedShadowsManager::BuildShadowMap()
{
    m_ShadowMap = std::make_shared<ShadowMapRes>(m_d3dDevice, m_copyCsmConfig.m_iBufferSize * m_copyCsmConfig.m_nCascadeLevels,
        m_copyCsmConfig.m_iBufferSize);
}

void CascadedShadowsManager::BuildPOS(SDKMesh::SDKMeshModel*meshModel, D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc)
{
  
    for (INT iCascadeIndex = 0; iCascadeIndex < MAX_CASCADES; ++iCascadeIndex)
    {
        for (INT iDerivativeIndex = 0; iDerivativeIndex < 2; ++iDerivativeIndex)
        {
            for (INT iBlendIndex = 0; iBlendIndex < 2; ++iBlendIndex)
            {
                for (INT iIntervalIndex = 0; iIntervalIndex < 2; ++iIntervalIndex)
                {
                    SDKMesh::EffectPipelineStateDescription epsd;
                    epsd.standardVS = m_ppsRenderSceneVSShadersBlob[iCascadeIndex];
                    epsd.opaquesPS = m_ppsRenderScenePSShadersBlob[iCascadeIndex][iDerivativeIndex][iBlendIndex][iIntervalIndex];
                    epsd.alphaPS = nullptr;
                    epsd.device = m_d3dDevice;
                    epsd.desc = psoDesc;
                    m_effects.effectCluster[iCascadeIndex][iDerivativeIndex][iBlendIndex][iIntervalIndex] = meshModel->CreateOnlyOneEffect(epsd);
                }
            }
        }
    }

    psoDesc.RasterizerState.DepthBias = 100000;
    psoDesc.RasterizerState.DepthBiasClamp = 0.0f;
    psoDesc.RasterizerState.SlopeScaledDepthBias = 1.0f;

    // Shadow map pass does not have a render target.
    psoDesc.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;
    psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    psoDesc.NumRenderTargets = 0;

    SDKMesh::EffectPipelineStateDescription epsd;
    epsd.standardVS = m_ShadowVSShader;
    epsd.opaquesPS = nullptr;
    epsd.alphaPS = nullptr;
    epsd.device = m_d3dDevice;
    epsd.desc = psoDesc;
    m_shadowDrawPSO = meshModel->CreateOnlyOneEffect(epsd)->m_PSO;
}


void CascadedShadowsManager::CreateFrustumPointsFromCascadeInterval(float fCascadeIntervalBegin,
	FLOAT fCascadeIntervalEnd, DirectX::CXMMATRIX vProjection,
	DirectX::XMVECTOR* pvCornerPointsWorld)
{
    BoundingFrustum vViewFrust(vProjection);
    vViewFrust.Near = fCascadeIntervalBegin;
    vViewFrust.Far = fCascadeIntervalEnd;

    static const XMVECTORU32 vGrabY = { 0x00000000,0xFFFFFFFF,0x00000000,0x00000000 };
    static const XMVECTORU32 vGrabX = { 0xFFFFFFFF,0x00000000,0x00000000,0x00000000 };

    XMVECTORF32 vRightTop = { vViewFrust.RightSlope,vViewFrust.TopSlope,1.0f,1.0f };
    XMVECTORF32 vLeftBottom = { vViewFrust.LeftSlope,vViewFrust.BottomSlope,1.0f,1.0f };
    XMVECTORF32 vNear = { vViewFrust.Near,vViewFrust.Near,vViewFrust.Near,1.0f };
    XMVECTORF32 vFar = { vViewFrust.Far,vViewFrust.Far,vViewFrust.Far,1.0f };

    XMVECTOR vRightTopNear = XMVectorMultiply(vRightTop, vNear);
    XMVECTOR vRightTopFar = XMVectorMultiply(vRightTop, vFar);
    XMVECTOR vLeftBottomNear = XMVectorMultiply(vLeftBottom, vNear);
    XMVECTOR vLeftBottomFar = XMVectorMultiply(vLeftBottom, vFar);

    pvCornerPointsWorld[0] = vRightTopNear;
    pvCornerPointsWorld[1] = XMVectorSelect(vRightTopNear, vLeftBottomNear, vGrabX);
    pvCornerPointsWorld[2] = vLeftBottomNear;
    pvCornerPointsWorld[3] = XMVectorSelect(vRightTopNear, vLeftBottomNear, vGrabY);

    pvCornerPointsWorld[4] = vRightTopFar;
    pvCornerPointsWorld[5] = XMVectorSelect(vRightTopFar, vLeftBottomFar, vGrabX);
    pvCornerPointsWorld[6] = vLeftBottomFar;
    pvCornerPointsWorld[7] = XMVectorSelect(vRightTopFar, vLeftBottomFar, vGrabY);
}

void CascadedShadowsManager::UpdateFrame(const GameTimer& gt, Camera* viewCamera, Camera* lightCamer)
{
    ReleasePreFrameResource();

    //update cascades
    
    XMMATRIX matLightCameraView = lightCamer->GetView();

    XMFLOAT3 sceneBoxCorners[8];
    m_sceneBox.GetCorners(sceneBoxCorners);
    
    // Transform the scene AABB to Light space.
    XMVECTOR vSceneAABBPointsLightSpace[8];
    for (int index = 0; index < 8; ++index)
    {
        XMVECTOR v = XMLoadFloat3(&sceneBoxCorners[index]);
        vSceneAABBPointsLightSpace[index] = XMVector3Transform(v, matLightCameraView);
    }

    XMVECTOR vLightCameraOrthographicMin;
    XMVECTOR vLightCameraOrthographicMax;

    for (int iCascadeIndex = 0; iCascadeIndex < m_copyCsmConfig.m_nCascadeLevels; iCascadeIndex++)
    {
        //计算cascade frustum
        XMVECTOR vWorldUnitsPerTexel = CalculateCascadeFrumstum(iCascadeIndex, viewCamera, lightCamer,
            vLightCameraOrthographicMin, vLightCameraOrthographicMax);

        if (m_bMoveLightTexelSize)
        {
            // We snape the camera to 1 pixel increments so that moving the camera does not cause the shadows to jitter.
            // This is a matter of integer dividing by the world space size of a texel
            vLightCameraOrthographicMin /= vWorldUnitsPerTexel; //Moving the Light in Texel-Sized Increments (注意这里)
            vLightCameraOrthographicMin = XMVectorFloor(vLightCameraOrthographicMin);
            vLightCameraOrthographicMin *= vWorldUnitsPerTexel;

            vLightCameraOrthographicMax /= vWorldUnitsPerTexel;
            vLightCameraOrthographicMax = XMVectorFloor(vLightCameraOrthographicMax);
            vLightCameraOrthographicMax *= vWorldUnitsPerTexel;
        }

        //计算远近裁剪面
        FLOAT fNearPlane = 0.0f, fFarPlane = 10000.f;
        CalculateCascadeNearAndFarPlane(vSceneAABBPointsLightSpace, vLightCameraOrthographicMin,
            vLightCameraOrthographicMax, fNearPlane, fFarPlane);

        //获取cascade的 orthographic project
        m_matShadowProj[iCascadeIndex] =
            XMMatrixOrthographicOffCenterLH(XMVectorGetX(vLightCameraOrthographicMin),
                                            XMVectorGetX(vLightCameraOrthographicMax),
                                            XMVectorGetY(vLightCameraOrthographicMin),
                                            XMVectorGetY(vLightCameraOrthographicMax),
                                            fNearPlane, fFarPlane);


    }

    m_matShadowView = lightCamer->GetView();
}

void CascadedShadowsManager::UpdateMainPassData(CSMPassConstants& constData)
{
    XMStoreFloat4x4(&constData.m_Shadow, XMMatrixTranspose(m_matShadowView));
    constData.m_nCascadeLevels = m_copyCsmConfig.m_nCascadeLevels;

    constData.m_fMaxBorderPadding = (float)(m_csmConfig->m_iBufferSize - 1.0f) /
        (float)m_csmConfig->m_iBufferSize;
    constData.m_fMinBorderPadding = (float)(1.0f) /
        (float)m_csmConfig->m_iBufferSize;
    
    constData.m_fShadowPartitionSize = 1.0f / (float)m_copyCsmConfig.m_nCascadeLevels;
    constData.m_fTexelSize = 1.0f / (float)m_copyCsmConfig.m_iBufferSize; ;
    constData.m_fNativeTexelSizeInX = constData.m_fTexelSize / (float)m_copyCsmConfig.m_nCascadeLevels; ;
    
    XMMATRIX matTextureScale = XMMatrixScaling(0.5f, -0.5f, 1.0f);
    XMMATRIX matTextureTranslation = XMMatrixTranslation(.5f, .5f, 0.f);

    for (UINT iCascadeIndex = 0; iCascadeIndex < m_copyCsmConfig.m_nCascadeLevels; iCascadeIndex++)
    {
        XMMATRIX mShadowTexture = m_matShadowProj[iCascadeIndex] * matTextureScale * matTextureTranslation;
        constData.m_vCascadeScale[iCascadeIndex].x = XMVectorGetX(mShadowTexture.r[0]);
        constData.m_vCascadeScale[iCascadeIndex].y = XMVectorGetY(mShadowTexture.r[1]);
        constData.m_vCascadeScale[iCascadeIndex].z = XMVectorGetZ(mShadowTexture.r[2]);
        constData.m_vCascadeScale[iCascadeIndex].w = 1;

        XMStoreFloat3(reinterpret_cast<XMFLOAT3*>(&constData.m_vCascadeOffset[iCascadeIndex]), mShadowTexture.r[3]);
        constData.m_vCascadeOffset[iCascadeIndex].w = 0;
    }

    memcpy(constData.m_fCascadeFrustumsEyeSpaceDepths,
        m_fCascadePartitionsFrustum, MAX_CASCADES * 4);
    for (int index = 0; index < MAX_CASCADES; ++index)
    {
        constData.m_fCascadeFrustumsEyeSpaceDepthsFloat4[index].x = m_fCascadePartitionsFrustum[index];
    }
}

void CascadedShadowsManager::UpdateShadowPassData(UINT iCascadeIndex, CSMPassConstants& constData)
{
    XMMATRIX WorldViewProj = m_matShadowView * m_matShadowProj[iCascadeIndex];
    XMStoreFloat4x4(&constData.m_WorldViewProj, XMMatrixTranspose(WorldViewProj));
}

void CascadedShadowsManager::RenderShadowsForAllCascades(ID3D12GraphicsCommandList* mCommandList,
	CSMShadowMapDrawData& data)
{
    if (!data.m_drawCb)
        return;

    UINT passCBByteSize = D3DUtil::CalcConstantBufferByteSize(sizeof(CSMPassConstants));


    // Change to DEPTH_WRITE.
    mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
        m_ShadowMap->Resource(),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        D3D12_RESOURCE_STATE_DEPTH_WRITE));

    // Clear the back buffer and depth buffer.
    mCommandList->ClearDepthStencilView(m_ShadowMap->Dsv(),
        D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
        1.0f, 0, 0, nullptr);

    mCommandList->OMSetRenderTargets(0, nullptr, false, &m_ShadowMap->Dsv()); // 不给渲染目标，只给深度缓冲区

    for (UINT iCascdeIndex = 0; iCascdeIndex < m_copyCsmConfig.m_nCascadeLevels; iCascdeIndex++)
    {
        D3D12_VIEWPORT viewPort = m_ShadowMap->Viewport();
        viewPort.TopLeftX = viewPort.TopLeftX += iCascdeIndex * m_copyCsmConfig.m_iBufferSize;
        viewPort.Width = m_copyCsmConfig.m_iBufferSize;

        mCommandList->RSSetViewports(1, &viewPort);
        
        D3D12_RECT scissorRect;
        scissorRect.top = 0;
        scissorRect.left = viewPort.TopLeftX;
        scissorRect.right = scissorRect.left + m_copyCsmConfig.m_iBufferSize;
        scissorRect.bottom = m_copyCsmConfig.m_iBufferSize;
        mCommandList->RSSetScissorRects(1, &scissorRect);


        D3D12_GPU_VIRTUAL_ADDRESS shadowPassAddress = data.m_shadowPassAddress +
            iCascdeIndex * passCBByteSize;

        mCommandList->SetGraphicsRootConstantBufferView(data.m_shadowPassRootParameterIndx, shadowPassAddress);

        data.m_drawCb(mCommandList);
       
    }

    mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
        m_ShadowMap->Resource(),
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        D3D12_RESOURCE_STATE_GENERIC_READ));
}

void CascadedShadowsManager::RenderScene(ID3D12GraphicsCommandList* cmmandList, 
	CSMShadowMapDrawData& data)
{
    if (data.m_drawCb)
        data.m_drawCb(cmmandList);
}

void CascadedShadowsManager::ReleasePreFrameResource()
{
}

XMVECTOR CascadedShadowsManager::CalculateCascadeFrumstum(int iCascadeIndex, Camera* viewCamera, Camera* lightCamer,
    DirectX::XMVECTOR& vLightCameraOrthographicMin, DirectX::XMVECTOR& vLightCameraOrthographicMax)
{
    XMMATRIX matViewCamerProject = viewCamera->GetProj();
    XMMATRIX matViewCameraView = viewCamera->GetView();
    XMMATRIX matLightCameraView = lightCamer->GetView();
    XMMATRIX matInverseViewCamera = XMMatrixInverse(nullptr, matViewCameraView);

    FLOAT fCameraNearFarRange = viewCamera->GetFarZ() - viewCamera->GetNearZ();

    FLOAT fFrustumIntervalBegin;
    FLOAT fFrustumIntervalEnd;

    if (m_eSelectedCascadesFit == FIT_TO_CASCADES)
        FrustumFitCascade(iCascadeIndex, fFrustumIntervalBegin,fFrustumIntervalEnd);
    else if (m_eSelectedCascadesFit == FIT_TO_SCENE)
        FrustumFitScene(iCascadeIndex, fFrustumIntervalBegin, fFrustumIntervalEnd);
    else {}

    fFrustumIntervalBegin /= (FLOAT)m_iCascadePartitionsMax; //这个时候就是百分比
    fFrustumIntervalEnd /= (FLOAT)m_iCascadePartitionsMax;

    fFrustumIntervalBegin = fFrustumIntervalBegin * fCameraNearFarRange;
    fFrustumIntervalEnd = fFrustumIntervalEnd * fCameraNearFarRange;

    //1. 计算不同cascade frustum在view camera坐标系下的8个点
    XMVECTOR vFrustumPoints[8];
    // This function takes the began and end intervals along with the projection matrix and returns the 8
    // points that repreresent the cascade Interval
    CreateFrustumPointsFromCascadeInterval(fFrustumIntervalBegin, fFrustumIntervalEnd,
        matViewCamerProject, vFrustumPoints);

    vLightCameraOrthographicMin = g_vFLTMAX;
    vLightCameraOrthographicMax = g_vFLTMIN; //每个view camera的不同cascade在light space中的orthographic 


    //2. 计算不同cascade frustum在 light view camera坐标系下的8个点
    XMVECTOR vTempTranslatedCornerPoint;
    // This next section of code calculates the min and max values for the orthographic projection.
    for (int icpIndex = 0; icpIndex < 8; ++icpIndex)
    {
        // Transform the frustum from camera view space to world space.
        vFrustumPoints[icpIndex] = XMVector4Transform(vFrustumPoints[icpIndex], matInverseViewCamera);
        // Transform the point from world space to Light Camera Space.
        vTempTranslatedCornerPoint = XMVector4Transform(vFrustumPoints[icpIndex], matLightCameraView);
        // Find the closest point.
        vLightCameraOrthographicMin = XMVectorMin(vTempTranslatedCornerPoint, vLightCameraOrthographicMin);
        vLightCameraOrthographicMax = XMVectorMax(vTempTranslatedCornerPoint, vLightCameraOrthographicMax);
    }

    m_fCascadePartitionsFrustum[iCascadeIndex] = fFrustumIntervalEnd;


    //外扩cascade frustum,并返回worldunitperTexel
    return UnShimmeringAndReturnWorldUnitsPerTexel(vFrustumPoints,vLightCameraOrthographicMin, vLightCameraOrthographicMax);
}

void CascadedShadowsManager::FrustumFitScene(int iCascadeIndex, FLOAT& fFrustumIntervalBegin,
    FLOAT& fFrustumIntervalEnd)
{
    fFrustumIntervalBegin = 0.0f;
    fFrustumIntervalEnd = (FLOAT)m_iCascadePartitionsZeroToOne[iCascadeIndex];
}

void CascadedShadowsManager::FrustumFitCascade(int iCascadeIndex, FLOAT&fFrustumIntervalBegin,
    FLOAT& fFrustumIntervalEnd)
{
    if (iCascadeIndex == 0) fFrustumIntervalBegin = 0.0f;
    else fFrustumIntervalBegin = (FLOAT)m_iCascadePartitionsZeroToOne[iCascadeIndex - 1];
    fFrustumIntervalEnd = (FLOAT)m_iCascadePartitionsZeroToOne[iCascadeIndex];
}

DirectX::XMVECTOR CascadedShadowsManager::UnShimmeringAndReturnWorldUnitsPerTexel(XMVECTOR* vFrustumPoints,DirectX::XMVECTOR& vLightCameraOrthographicMin,
    DirectX::XMVECTOR& vLightCameraOrthographicMax)
{
    XMVECTOR vWorldUnitsPerTexel = g_vZero;

    if (m_eSelectedCascadesFit == FIT_TO_CASCADES)
    {
        //计算project因为pfc 边界偏移和vWorldUnitsPerTexel
        // This code removes the shimmering effect along the edges of shadows due to the light changing to fit the camera.
        // We calculate a looser bound based on the size of the PCF blur.  This ensures us that we're 
        // sampling within the correct map.
        float fScaleDuetoBlureAMT = ((float)(m_iPCFBlurSize * 2 + 1) / (float)m_copyCsmConfig.m_iBufferSize);
        XMVECTORF32 vScaleDuetoBlureAMT = { fScaleDuetoBlureAMT, fScaleDuetoBlureAMT, 0.0f, 0.0f };

        float fNormalizeByBufferSize = (1.0f / (float)m_copyCsmConfig.m_iBufferSize);
        XMVECTOR vNormalizeByBufferSize = XMVectorSet(fNormalizeByBufferSize, fNormalizeByBufferSize, 0.0f, 0.0f);

        // We calculate the offsets as a percentage of the bound.
        XMVECTOR vBoarderOffset = vLightCameraOrthographicMax - vLightCameraOrthographicMin;
        vBoarderOffset *= g_vHalfVector;
        vBoarderOffset *= vScaleDuetoBlureAMT;
        vLightCameraOrthographicMax += vBoarderOffset;
        vLightCameraOrthographicMin -= vBoarderOffset;

        // The world units per texel are used to snap  the orthographic projection
        // to texel sized increments.  
        // Because we're fitting tighly to the cascades, the shimmering shadow edges will still be present when the 
        // camera rotates.  However, when zooming in or strafing the shadow edge will not shimmer.
        vWorldUnitsPerTexel = vLightCameraOrthographicMax - vLightCameraOrthographicMin;
        vWorldUnitsPerTexel *= vNormalizeByBufferSize;
    }
    else if (m_eSelectedCascadesFit == FIT_TO_SCENE)
    {
        //Pad the projection to be the size of the diagonal of the Frustum partition.
          //  To do this, we pad the ortho transform so that it is always big enough to cover the entire camera view frustum.

        //fit to scene和上面fit to cascae采取的防止shimmer不同

        // Fit the ortho projection to the cascades far plane and a near plane of zero. 
            // Pad the projection to be the size of the diagonal of the Frustum partition. 
            // 
            // To do this, we pad the ortho transform so that it is always big enough to cover 
            // the entire camera view frustum.
         //vFrustumPoints：是被切割后的 view camera frustum,但如果转到light space后这个frutum可能因为 光源和镜头之间夹角关系而没有view camera frustum的(物理上)大
        XMVECTOR vDiagonal = vFrustumPoints[0] - vFrustumPoints[6];
        vDiagonal = XMVector3Length(vDiagonal);

        // The bound is the length of the diagonal of the frustum interval.
        FLOAT fCascadeBound = XMVectorGetX(vDiagonal);

        // The offset calculated will pad the ortho projection so that it is always the same size 
        // and big enough to cover the entire cascade interval.
        XMVECTOR vBoarderOffset = (vDiagonal -
            (vLightCameraOrthographicMax - vLightCameraOrthographicMin))
            * g_vHalfVector;
        // Set the Z and W components to zero.
        vBoarderOffset *= g_vMultiplySetzwToZero;

        // Add the offsets to the projection.
        vLightCameraOrthographicMax += vBoarderOffset;
        vLightCameraOrthographicMin -= vBoarderOffset;

        // The world units per texel are used to snap the shadow the orthographic projection
        // to texel sized increments.  This keeps the edges of the shadows from shimmering.
        FLOAT fWorldUnitsPerTexel = fCascadeBound / (float)m_copyCsmConfig.m_iBufferSize;

        vWorldUnitsPerTexel = XMVectorSet(fWorldUnitsPerTexel, fWorldUnitsPerTexel, 0.0f, 0.0f);

    }
    return vWorldUnitsPerTexel;
}

void CascadedShadowsManager::CalculateCascadeNearAndFarPlane(XMVECTOR* vSceneAABBPointsLightSpace, DirectX::XMVECTOR& vLightCameraOrthographicMin,
    DirectX::XMVECTOR& vLightCameraOrthographicMax, FLOAT& fNearPlane, FLOAT& fFarPlane)
{
    if (m_eSelectedNearFarFit == FIT_NEARFAR_AABB)
        FitNearFarWithAABB(vSceneAABBPointsLightSpace, vLightCameraOrthographicMin, vLightCameraOrthographicMax, fNearPlane, fFarPlane);
    else if(m_eSelectedNearFarFit == FIT_NEARFAR_SCENE_AABB 
        || m_eSelectedNearFarFit == FIT_NEARFAR_PANCAKING)
        FitNearFarWithScene(vSceneAABBPointsLightSpace, vLightCameraOrthographicMin, vLightCameraOrthographicMax, fNearPlane, fFarPlane);
}

void CascadedShadowsManager::FitNearFarWithAABB(XMVECTOR* vSceneAABBPointsLightSpace,
    DirectX::XMVECTOR& vLightCameraOrthographicMin, DirectX::XMVECTOR& vLightCameraOrthographicMax, FLOAT& fNearPlane, FLOAT& fFarPlane)
{
    XMVECTOR vLightSpaceSceneAABBminValue = g_vFLTMAX;  // world space scene aabb 
    XMVECTOR vLightSpaceSceneAABBmaxValue = g_vFLTMIN;
    // We calculate the min and max vectors of the scene in light space. The min and max "Z" values of the  
    // light space AABB can be used for the near and far plane. This is easier than intersecting the scene with the AABB
    // and in some cases provides similar results.
    for (int index = 0; index < 8; ++index)
    {
        vLightSpaceSceneAABBminValue = XMVectorMin(vSceneAABBPointsLightSpace[index], vLightSpaceSceneAABBminValue);
        vLightSpaceSceneAABBmaxValue = XMVectorMax(vSceneAABBPointsLightSpace[index], vLightSpaceSceneAABBmaxValue);
    }

    // The min and max z values are the near and far planes.
    fNearPlane = XMVectorGetZ(vLightSpaceSceneAABBminValue);
    fFarPlane = XMVectorGetZ(vLightSpaceSceneAABBmaxValue);
}

struct Triangle
{
    XMVECTOR pt[3];
    bool culled;
};


void CascadedShadowsManager::FitNearFarWithScene(XMVECTOR* vSceneAABBPointsLightSpace,
    DirectX::XMVECTOR& vLightCameraOrthographicMin, DirectX::XMVECTOR& vLightCameraOrthographicMax, FLOAT& fNearPlane, FLOAT& fFarPlane)
{
    ComputeNearAndFar(fNearPlane, fFarPlane, vLightCameraOrthographicMin,
        vLightCameraOrthographicMax, vSceneAABBPointsLightSpace);

    if (m_eSelectedNearFarFit == FIT_NEARFAR_PANCAKING)
    {
        /*if (fLightCameraOrthographicMinZ > fNearPlane)
        {
            fNearPlane = fLightCameraOrthographicMinZ;
        }*/
    }
}

void CascadedShadowsManager::ComputeNearAndFar(FLOAT& fNearPlane, FLOAT& fFarPlane, DirectX::FXMVECTOR vLightCameraOrthographicMin, 
    DirectX::FXMVECTOR vLightCameraOrthographicMax, DirectX::XMVECTOR* pvPointsInCameraView)

{
    fNearPlane = FLT_MAX;
    fFarPlane = -FLT_MAX;

    Triangle triangleList[16];
    INT iTriangleCnt = 1;

    triangleList[0].pt[0] = pvPointsInCameraView[0];
    triangleList[0].pt[1] = pvPointsInCameraView[1];
    triangleList[0].pt[2] = pvPointsInCameraView[2];
    triangleList[0].culled = false;

    // These are the indices used to tesselate an AABB into a list of triangles.
    static const INT iAABBTriIndexes[] =
    {
        0,1,2,  1,2,3,
        4,5,6,  5,6,7,
        0,2,4,  2,4,6,
        1,3,5,  3,5,7,
        0,1,4,  1,4,5,
        2,3,6,  3,6,7
    };

    INT iPointPassesCollision[3];

    // At a high level: 
    // 1. Iterate over all 12 triangles of the AABB.  
    // 2. Clip the triangles against each plane. Create new triangles as needed.
    // 3. Find the min and max z values as the near and far plane.

    //This is easier because the triangles are in camera spacing making the collisions tests simple comparisions.

    float fLightCameraOrthographicMinX = XMVectorGetX(vLightCameraOrthographicMin);
    float fLightCameraOrthographicMaxX = XMVectorGetX(vLightCameraOrthographicMax);
    float fLightCameraOrthographicMinY = XMVectorGetY(vLightCameraOrthographicMin);
    float fLightCameraOrthographicMaxY = XMVectorGetY(vLightCameraOrthographicMax);

    for (INT AABBTriIter = 0; AABBTriIter < 12; ++AABBTriIter)
    {

        triangleList[0].pt[0] = pvPointsInCameraView[iAABBTriIndexes[AABBTriIter * 3 + 0]];
        triangleList[0].pt[1] = pvPointsInCameraView[iAABBTriIndexes[AABBTriIter * 3 + 1]];
        triangleList[0].pt[2] = pvPointsInCameraView[iAABBTriIndexes[AABBTriIter * 3 + 2]];
        iTriangleCnt = 1;
        triangleList[0].culled = FALSE;

        // Clip each invidual triangle against the 4 frustums.  When ever a triangle is clipped into new triangles, 
        //add them to the list.
        for (INT frustumPlaneIter = 0; frustumPlaneIter < 4; ++frustumPlaneIter)
        {

            FLOAT fEdge;
            INT iComponent;

            if (frustumPlaneIter == 0)
            {
                fEdge = fLightCameraOrthographicMinX; // todo make float temp
                iComponent = 0;
            }
            else if (frustumPlaneIter == 1)
            {
                fEdge = fLightCameraOrthographicMaxX;
                iComponent = 0;
            }
            else if (frustumPlaneIter == 2)
            {
                fEdge = fLightCameraOrthographicMinY;
                iComponent = 1;
            }
            else
            {
                fEdge = fLightCameraOrthographicMaxY;
                iComponent = 1;
            }

            for (INT triIter = 0; triIter < iTriangleCnt; ++triIter)
            {
                // We don't delete triangles, so we skip those that have been culled.
                if (!triangleList[triIter].culled)
                {
                    INT iInsideVertCount = 0;
                    XMVECTOR tempOrder;
                    // Test against the correct frustum plane.
                    // This could be written more compactly, but it would be harder to understand.

                    if (frustumPlaneIter == 0)
                    {
                        for (INT triPtIter = 0; triPtIter < 3; ++triPtIter)
                        {
                            if (XMVectorGetX(triangleList[triIter].pt[triPtIter]) >
                                XMVectorGetX(vLightCameraOrthographicMin))
                            {
                                iPointPassesCollision[triPtIter] = 1;
                            }
                            else
                            {
                                iPointPassesCollision[triPtIter] = 0;
                            }
                            iInsideVertCount += iPointPassesCollision[triPtIter];
                        }
                    }
                    else if (frustumPlaneIter == 1)
                    {
                        for (INT triPtIter = 0; triPtIter < 3; ++triPtIter)
                        {
                            if (XMVectorGetX(triangleList[triIter].pt[triPtIter]) <
                                XMVectorGetX(vLightCameraOrthographicMax))
                            {
                                iPointPassesCollision[triPtIter] = 1;
                            }
                            else
                            {
                                iPointPassesCollision[triPtIter] = 0;
                            }
                            iInsideVertCount += iPointPassesCollision[triPtIter];
                        }
                    }
                    else if (frustumPlaneIter == 2)
                    {
                        for (INT triPtIter = 0; triPtIter < 3; ++triPtIter)
                        {
                            if (XMVectorGetY(triangleList[triIter].pt[triPtIter]) >
                                XMVectorGetY(vLightCameraOrthographicMin))
                            {
                                iPointPassesCollision[triPtIter] = 1;
                            }
                            else
                            {
                                iPointPassesCollision[triPtIter] = 0;
                            }
                            iInsideVertCount += iPointPassesCollision[triPtIter];
                        }
                    }
                    else
                    {
                        for (INT triPtIter = 0; triPtIter < 3; ++triPtIter)
                        {
                            if (XMVectorGetY(triangleList[triIter].pt[triPtIter]) <
                                XMVectorGetY(vLightCameraOrthographicMax))
                            {
                                iPointPassesCollision[triPtIter] = 1;
                            }
                            else
                            {
                                iPointPassesCollision[triPtIter] = 0;
                            }
                            iInsideVertCount += iPointPassesCollision[triPtIter];
                        }
                    }

                    // Move the points that pass the frustum test to the begining of the array.
                    if (iPointPassesCollision[1] && !iPointPassesCollision[0])
                    {
                        tempOrder = triangleList[triIter].pt[0];
                        triangleList[triIter].pt[0] = triangleList[triIter].pt[1];
                        triangleList[triIter].pt[1] = tempOrder;
                        iPointPassesCollision[0] = TRUE;
                        iPointPassesCollision[1] = FALSE;
                    }
                    if (iPointPassesCollision[2] && !iPointPassesCollision[1])
                    {
                        tempOrder = triangleList[triIter].pt[1];
                        triangleList[triIter].pt[1] = triangleList[triIter].pt[2];
                        triangleList[triIter].pt[2] = tempOrder;
                        iPointPassesCollision[1] = TRUE;
                        iPointPassesCollision[2] = FALSE;
                    }
                    if (iPointPassesCollision[1] && !iPointPassesCollision[0])
                    {
                        tempOrder = triangleList[triIter].pt[0];
                        triangleList[triIter].pt[0] = triangleList[triIter].pt[1];
                        triangleList[triIter].pt[1] = tempOrder;
                        iPointPassesCollision[0] = TRUE;
                        iPointPassesCollision[1] = FALSE;
                    }

                    if (iInsideVertCount == 0)
                    { // All points failed. We're done,  
                        triangleList[triIter].culled = true;
                    }
                    else if (iInsideVertCount == 1)
                    {// One point passed. Clip the triangle against the Frustum plane
                        triangleList[triIter].culled = false;

                        // 
                        XMVECTOR vVert0ToVert1 = triangleList[triIter].pt[1] - triangleList[triIter].pt[0];
                        XMVECTOR vVert0ToVert2 = triangleList[triIter].pt[2] - triangleList[triIter].pt[0];

                        // Find the collision ratio.
                        FLOAT fHitPointTimeRatio = fEdge - XMVectorGetByIndex(triangleList[triIter].pt[0], iComponent);
                        // Calculate the distance along the vector as ratio of the hit ratio to the component.
                        FLOAT fDistanceAlongVector01 = fHitPointTimeRatio / XMVectorGetByIndex(vVert0ToVert1, iComponent);
                        FLOAT fDistanceAlongVector02 = fHitPointTimeRatio / XMVectorGetByIndex(vVert0ToVert2, iComponent);
                        // Add the point plus a percentage of the vector.
                        vVert0ToVert1 *= fDistanceAlongVector01;
                        vVert0ToVert1 += triangleList[triIter].pt[0];
                        vVert0ToVert2 *= fDistanceAlongVector02;
                        vVert0ToVert2 += triangleList[triIter].pt[0];

                        triangleList[triIter].pt[1] = vVert0ToVert2;
                        triangleList[triIter].pt[2] = vVert0ToVert1;

                    }
                    else if (iInsideVertCount == 2)
                    { // 2 in  // tesselate into 2 triangles


                        // Copy the triangle\(if it exists) after the current triangle out of
                        // the way so we can override it with the new triangle we're inserting.
                        triangleList[iTriangleCnt] = triangleList[triIter + 1];

                        triangleList[triIter].culled = false;
                        triangleList[triIter + 1].culled = false;

                        // Get the vector from the outside point into the 2 inside points.
                        XMVECTOR vVert2ToVert0 = triangleList[triIter].pt[0] - triangleList[triIter].pt[2];
                        XMVECTOR vVert2ToVert1 = triangleList[triIter].pt[1] - triangleList[triIter].pt[2];

                        // Get the hit point ratio.
                        FLOAT fHitPointTime_2_0 = fEdge - XMVectorGetByIndex(triangleList[triIter].pt[2], iComponent);
                        FLOAT fDistanceAlongVector_2_0 = fHitPointTime_2_0 / XMVectorGetByIndex(vVert2ToVert0, iComponent);
                        // Calcaulte the new vert by adding the percentage of the vector plus point 2.
                        vVert2ToVert0 *= fDistanceAlongVector_2_0;
                        vVert2ToVert0 += triangleList[triIter].pt[2];

                        // Add a new triangle.
                        triangleList[triIter + 1].pt[0] = triangleList[triIter].pt[0];
                        triangleList[triIter + 1].pt[1] = triangleList[triIter].pt[1];
                        triangleList[triIter + 1].pt[2] = vVert2ToVert0;

                        //Get the hit point ratio.
                        FLOAT fHitPointTime_2_1 = fEdge - XMVectorGetByIndex(triangleList[triIter].pt[2], iComponent);
                        FLOAT fDistanceAlongVector_2_1 = fHitPointTime_2_1 / XMVectorGetByIndex(vVert2ToVert1, iComponent);
                        vVert2ToVert1 *= fDistanceAlongVector_2_1;
                        vVert2ToVert1 += triangleList[triIter].pt[2];
                        triangleList[triIter].pt[0] = triangleList[triIter + 1].pt[1];
                        triangleList[triIter].pt[1] = triangleList[triIter + 1].pt[2];
                        triangleList[triIter].pt[2] = vVert2ToVert1;
                        // Cncrement triangle count and skip the triangle we just inserted.
                        ++iTriangleCnt;
                        ++triIter;


                    }
                    else
                    { // all in
                        triangleList[triIter].culled = false;

                    }
                }// end if !culled loop            
            }
        }
        for (INT index = 0; index < iTriangleCnt; ++index)
        {
            if (!triangleList[index].culled)
            {
                // Set the near and far plan and the min and max z values respectivly.
                for (int vertind = 0; vertind < 3; ++vertind)
                {
                    float fTriangleCoordZ = XMVectorGetZ(triangleList[index].pt[vertind]);
                    if (fNearPlane > fTriangleCoordZ)
                    {
                        fNearPlane = fTriangleCoordZ;
                    }
                    if (fFarPlane < fTriangleCoordZ)
                    {
                        fFarPlane = fTriangleCoordZ;
                    }
                }
            }
        }
    }

}

void CascadedShadowsManager::BuildDescriptors(
	CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv,
	CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv,
	CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuDsv)
{
    m_ShadowMap->BuildDescriptors(hCpuSrv, hGpuSrv, hCpuDsv);
}


ID3D12PipelineState* CascadedShadowsManager::GetDrawSceneToShadowMapPSO()
{
	return m_shadowDrawPSO.Get();
}

ID3D12PipelineState* CascadedShadowsManager::GetEffect()
{
    return m_effects.effectCluster[2][m_iDerivativeBasedOffset]
        [m_iBlurBetweenCascades]
        [m_eSelectedCascadeSelection]->m_PSO.Get();
}