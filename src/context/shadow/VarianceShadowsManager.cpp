#include "VarianceShadowsManager.h"
#include "common/SDKMeshModel.h"
#include "common/Camera.h"
using namespace DirectX;
using namespace DirectX::PackedVector;
using Microsoft::WRL::ComPtr;

static const XMVECTORF32 g_vHalfVector = {0.5f, 0.5f, 0.5f, 0.5f};
static const XMVECTORF32 g_vMultiplySetzwToZero = {1.0f, 1.0f, 0.0f, 0.0f};

/**
 *
 *
m_pCascadedShadowMapVarianceTextureArray: 数组(cascade,R32G32 float array)------

                            -m_pCascadedShadowMapVarianceRTVArrayAll[cascade](targetview : CreateRenderTargetView, texture formate)
                        ------m_pCascadedShadowMapVarianceSRVArraySingle(shader resouce view)
                        ------ m_pCascadedShadowMapVarianceSRVArrayAll[cascade] (shader resouce view)

m_pCascadedShadowMapTempBlurTexture: width ,height不是数组 ---------m_pCascadedShadowMapTempBlurRTV(target view)
                             ------------ m_pCascadedShadowMapTempBlurSRV(shader resouce view)


m_pTemporaryShadowDepthBufferTexture（32 float） ---m_pTemporaryShadowDepthBufferDSV

1. to, targetview: m_pCascadedShadowMapVarianceRTVArrayAll
        depthview : m_pTemporaryShadowDepthBufferDSV

2. blur shadow:
     shader: vs: quadblur
       for( cascadelevel)
    targetview : m_pCascadedShadowMapTempBlurRTV ,
    depthview : nullview
    ps: psblurx
    shaderRource: m_pCascadedShadowMapVarianceSRVArrayAll[cascade]
    //draw

    targetView : m_pCascadedShadowMapVarianceRTVArrayAll[cascade]
    depthveiew : nullview
    shaderRource: m_pCascadedShadowMapTempBlurSRV
    //draw

 *
 */

bool VarianceShadowsManager::init(ID3D12Device *device, const DirectX::BoundingBox &sceneBox,
                                  const Camera *viewCamra, const Camera *shadowCamera, VSMConfig *config)
{
    m_d3dDevice = device;
    m_csmConfig = config;
    m_copyCsmConfig = *m_csmConfig;
    m_sceneBox = sceneBox;

    m_ppsRenderSceneVSShadersBlob =
        D3DUtil::CompileShader(SourcePath() + L"/Shaders/shadow/vsm_scene.hlsl", nullptr, "VS", "vs_5_1");

    m_ppsRenderScenePSShadersBlob =
        D3DUtil::CompileShader(SourcePath() + L"/Shaders/shadow/vsm_scene.hlsl", nullptr, "PS", "ps_5_1");

    m_ppsRenderShadowVSShadersBlob =
        D3DUtil::CompileShader(SourcePath() + L"/Shaders/shadow/vsm_shadow.hlsl", nullptr, "VS", "vs_5_1");

    m_ppsRenderShadowPSShadersBlob =
        D3DUtil::CompileShader(SourcePath() + L"/Shaders/shadow/vsm_shadow.hlsl", nullptr, "PS", "ps_5_1");

    return true;
}

UINT VarianceShadowsManager::GetResouceViewCount() const
{
    return 1;
}

void VarianceShadowsManager::BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv,
                                              CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv,
                                              CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuRtv)
{
    m_ShadowMap->BuildDescriptors(hCpuSrv, hGpuSrv, hCpuRtv);
}

void VarianceShadowsManager::UpdateMainPassData(VSMPassConstants &constData)
{
    XMMATRIX matTextureScale = XMMatrixScaling(0.5f, -0.5f, 1.0f);
    XMMATRIX matTextureTranslation = XMMatrixTranslation(.5f, .5f, 0.f);

    XMMATRIX mShadowTexture = m_matShadowProj * matTextureScale * matTextureTranslation;
    constData.m_vShadowTextureScale.x = XMVectorGetX(mShadowTexture.r[0]);
    constData.m_vShadowTextureScale.y = XMVectorGetY(mShadowTexture.r[1]);
    constData.m_vShadowTextureScale.z = XMVectorGetZ(mShadowTexture.r[2]);
    constData.m_vShadowTextureScale.w = 1;

    XMStoreFloat3(reinterpret_cast<XMFLOAT3 *>(&constData.m_vShadowTextureOffset), mShadowTexture.r[3]);
    constData.m_vShadowTextureOffset.w = 0;
}

void VarianceShadowsManager::UpdateShadowPassData(VSMPassConstants &constData)
{
}

void VarianceShadowsManager::BuildPOS(SDKMesh::SDKMeshModel *meshModel, D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc)
{
    SDKMesh::EffectPipelineStateDescription epsd;
    epsd.standardVS = m_ppsRenderSceneVSShadersBlob;
    epsd.opaquesPS = m_ppsRenderScenePSShadersBlob;
    epsd.alphaPS = nullptr;
    epsd.device = m_d3dDevice;
    epsd.desc = psoDesc;
    m_sceneEffect = meshModel->CreateOnlyOneEffect(epsd);

    // 设置shadow pso

    psoDesc.RTVFormats[0] = DXGI_FORMAT_R32G32_FLOAT;
    psoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;

    epsd.standardVS = m_ppsRenderShadowVSShadersBlob;
    epsd.opaquesPS = m_ppsRenderShadowPSShadersBlob;
    epsd.alphaPS = nullptr;
    epsd.device = m_d3dDevice;
    epsd.desc = psoDesc;
    m_shadowEffect = meshModel->CreateOnlyOneEffect(epsd);
}

ID3D12PipelineState *VarianceShadowsManager::GetSceneEffect()
{
    return m_sceneEffect->m_PSO.Get();
}

ID3D12PipelineState *VarianceShadowsManager::GetShadowEffect()
{
    return m_shadowEffect->m_PSO.Get();
}

void VarianceShadowsManager::RenderScene(ID3D12GraphicsCommandList *cmmandList, VSMShadowMapDrawData &data)
{
    if (data.m_drawCb)
        data.m_drawCb(cmmandList);
}

void VarianceShadowsManager::RenderShadows(ID3D12GraphicsCommandList *mCommandList,
                                           VSMShadowMapDrawData &data)
{
    UINT passCBByteSize = D3DUtil::CalcConstantBufferByteSize(sizeof(VSMPassConstants));

    // 渲染前
    mCommandList->SetPipelineState(m_shadowEffect->m_PSO.Get()); // 已配置 RTVFormats[0]=R32G32_FLOAT

    // 状态转换：GENERIC_READ -> RENDER_TARGET
    mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
                                         m_ShadowMap->Resource(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET));

    // 清除 (1,1,0,0)
    const float clearMoments[4] = {1.0f, 1.0f, 0.0f, 0.0f};
    mCommandList->ClearRenderTargetView(m_ShadowMap->Rtv(), clearMoments, 0, nullptr);

    // 绑定：只有 RTV，没有 DSV
    mCommandList->OMSetRenderTargets(1, &m_ShadowMap->Rtv(), FALSE, nullptr);

    mCommandList->RSSetViewports(1, &m_ShadowMap->Viewport());
    mCommandList->RSSetScissorRects(1, &m_ShadowMap->ScissorRect());

    mCommandList->SetGraphicsRootConstantBufferView(2, data.m_PassAddress);

    data.m_drawCb(mCommandList);

    mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
                                         m_ShadowMap->Resource(),
                                         D3D12_RESOURCE_STATE_RENDER_TARGET,
                                         D3D12_RESOURCE_STATE_GENERIC_READ));
}

void VarianceShadowsManager::UpdateFrame(const GameTimer &gt, Camera *viewCamera, Camera *lightCamer)
{
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

    // 计算不同cascade frustum在 light view camera坐标系下的8个点
    CalculateFrustum(0, viewCamera, lightCamer, vLightCameraOrthographicMin, vLightCameraOrthographicMax);
    // 计算远近裁剪面
    FLOAT fNearPlane = 0.0f, fFarPlane = 10000.f;
    CalculateNearFarWithAABB(vSceneAABBPointsLightSpace, vLightCameraOrthographicMin,
                             vLightCameraOrthographicMax, fNearPlane, fFarPlane);

    // 获取cascade的 orthographic project
    m_matShadowProj =
        XMMatrixOrthographicOffCenterLH(XMVectorGetX(vLightCameraOrthographicMin),
                                        XMVectorGetX(vLightCameraOrthographicMax),
                                        XMVectorGetY(vLightCameraOrthographicMin),
                                        XMVectorGetY(vLightCameraOrthographicMax),
                                        fNearPlane, fFarPlane);
}

void VarianceShadowsManager::BuildShadowMap()
{
    // 最有一个参数是false，表示不创建深度视图，因为vsm不需要深度视图
    m_ShadowMap = std::make_shared<ShadowMapRes>(m_d3dDevice, m_copyCsmConfig.m_iBufferSize,
                                                 m_copyCsmConfig.m_iBufferSize, m_copyCsmConfig.m_ShadowBufferFormat, false);
}

void VarianceShadowsManager::CalculateFrustum(int iCascadeIndex, Camera *viewCamera, Camera *lightCamer,
                                              DirectX::XMVECTOR &vLightCameraOrthographicMin, DirectX::XMVECTOR &vLightCameraOrthographicMax)
{
    XMMATRIX matViewCamerProject = viewCamera->GetProj();
    XMMATRIX matViewCameraView = viewCamera->GetView();
    XMMATRIX matLightCameraView = lightCamer->GetView();
    XMMATRIX matInverseViewCamera = XMMatrixInverse(nullptr, matViewCameraView);

    FLOAT fCameraNearFarRange = viewCamera->GetFarZ() - viewCamera->GetNearZ();

    FLOAT fFrustumIntervalBegin;
    FLOAT fFrustumIntervalEnd;

    fFrustumIntervalBegin = 0;
    fFrustumIntervalEnd = fCameraNearFarRange;

    // 1. 计算不同cascade frustum在view camera坐标系下的8个点
    XMVECTOR vFrustumPoints[8];
    // This function takes the began and end intervals along with the projection matrix and returns the 8
    // points that repreresent the cascade Interval
    CreateFrustumPointsFromCascadeInterval(fFrustumIntervalBegin, fFrustumIntervalEnd,
                                           matViewCamerProject, vFrustumPoints);

    vLightCameraOrthographicMin = g_vFLTMAX;
    vLightCameraOrthographicMax = g_vFLTMIN; // 每个view camera的不同cascade在light space中的orthographic

    // 2. 计算不同cascade frustum在 light view camera坐标系下的8个点
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

    XMVECTOR vDiagonal = vFrustumPoints[0] - vFrustumPoints[6]; //world space
    vDiagonal = XMVector3Length(vDiagonal);

    // The bound is the length of the diagonal of the frustum interval.
    FLOAT fCascadeBound = XMVectorGetX(vDiagonal);

    // The offset calculated will pad the ortho projection so that it is always the same size
    // and big enough to cover the entire cascade interval.
    XMVECTOR vBoarderOffset = (vDiagonal -
                               (vLightCameraOrthographicMax - vLightCameraOrthographicMin)) *
                              g_vHalfVector;
    // Set the Z and W components to zero.
    vBoarderOffset *= g_vMultiplySetzwToZero;

    // Add the offsets to the projection.
    vLightCameraOrthographicMax += vBoarderOffset;
    vLightCameraOrthographicMin -= vBoarderOffset;

    FLOAT fWorldUnitsPerTexel = fCascadeBound / (float)m_copyCsmConfig.m_iBufferSize;
    XMVECTOR vWorldUnitsPerTexel = XMVectorSet(fWorldUnitsPerTexel, fWorldUnitsPerTexel, 0.0f, 0.0f);

    vLightCameraOrthographicMin /= vWorldUnitsPerTexel;
    vLightCameraOrthographicMin = XMVectorFloor(vLightCameraOrthographicMin);
    vLightCameraOrthographicMin *= vWorldUnitsPerTexel;

    vLightCameraOrthographicMax /= vWorldUnitsPerTexel;
    vLightCameraOrthographicMax = XMVectorFloor(vLightCameraOrthographicMax);
    vLightCameraOrthographicMax *= vWorldUnitsPerTexel;
}