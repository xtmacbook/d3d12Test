#include "CascadedShadowsManager.h"
#include "common/SDKMeshModel.h"
#include "common/Camera.h"

using namespace DirectX;
using namespace DirectX::PackedVector;
using Microsoft::WRL::ComPtr;


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
	m_ShadowVSShader = D3DUtil::CompileShader(SourcePath() + L"/Shaders/shadow/csm_shadowMap.hlsl", nullptr, "VS", "vs_5_1");

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
            D3DUtil::CompileShader(SourcePath() + L"/Shaders/shadow/csm_scene.hlsl", defines, "VS", "vs_5_1");

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
                        D3DUtil::CompileShader(SourcePath() + L"/Shaders/shadow/csm_scene.hlsl", defines, "PS", "ps_5_1");
                }
            }
        }
    }


	return true;
}

void CascadedShadowsManager::BuildShadowMap()
{
    m_ShadowMap = std::make_shared<ShadowMapRes>(m_d3dDevice, m_copyCsmConfig.m_iBufferSize * m_copyCsmConfig.m_nCascadeLevels,
        m_copyCsmConfig.m_iBufferSize,m_copyCsmConfig.m_ShadowBufferFormat);
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

    psoDesc.RasterizerState.DepthBias = 0;
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

void CascadedShadowsManager::FitNearFarWithScene(XMVECTOR* vSceneAABBPointsLightSpace,
    DirectX::XMVECTOR& vLightCameraOrthographicMin, DirectX::XMVECTOR& vLightCameraOrthographicMax, FLOAT& fNearPlane, FLOAT& fFarPlane)
{
    CalculateNearFarWithAABB(vSceneAABBPointsLightSpace, vLightCameraOrthographicMin,
        vLightCameraOrthographicMax,fNearPlane, fFarPlane);

    if (m_eSelectedNearFarFit == FIT_NEARFAR_PANCAKING)
    {
        /*if (fLightCameraOrthographicMinZ > fNearPlane)
        {
            fNearPlane = fLightCameraOrthographicMinZ;
        }*/
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