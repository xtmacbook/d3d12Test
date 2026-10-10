#include "VarianceShadowsManager.h"
#include "common/SDKMeshModel.h"

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
}

void VarianceShadowsManager::UpdateShadowPassData(VSMPassConstants& constData)
{
   
}

void VarianceShadowsManager::BuildPOS(SDKMesh::SDKMeshModel * meshModel, D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc)
{
    SDKMesh::EffectPipelineStateDescription epsd;
    epsd.standardVS = m_ppsRenderSceneVSShadersBlob;
    epsd.opaquesPS = m_ppsRenderScenePSShadersBlob;
    epsd.alphaPS = nullptr;
    epsd.device = m_d3dDevice;
    epsd.desc = psoDesc;
    m_sceneEffect = meshModel->CreateOnlyOneEffect(epsd);


	//设置shadow pso

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

ID3D12PipelineState* VarianceShadowsManager::GetShadowEffect()
{
    return m_shadowEffect->m_PSO.Get();
}

void VarianceShadowsManager::RenderScene(ID3D12GraphicsCommandList *cmmandList, VSMShadowMapDrawData &data)
{
     if (data.m_drawCb)
        data.m_drawCb(cmmandList);
}

void VarianceShadowsManager::RenderShadows(ID3D12GraphicsCommandList* mCommandList, 
    VSMShadowMapDrawData& data)
{
    UINT passCBByteSize = D3DUtil::CalcConstantBufferByteSize(sizeof(VSMPassConstants));

    // 渲染前
    mCommandList->SetPipelineState(m_shadowEffect->m_PSO.Get());  // 已配置 RTVFormats[0]=R32G32_FLOAT

    // 状态转换：GENERIC_READ -> RENDER_TARGET
    mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
        m_ShadowMap->Resource(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET));

    // 清除 (1,1,0,0)
    const float clearMoments[4] = { 1.0f, 1.0f, 0.0f, 0.0f };
    mCommandList->ClearRenderTargetView(m_ShadowMap->Rtv(), clearMoments, 0, nullptr);

    // 绑定：只有 RTV，没有 DSV
    mCommandList->OMSetRenderTargets(1, &m_ShadowMap->Rtv(), FALSE, nullptr);

    mCommandList->SetGraphicsRootConstantBufferView(2, data.m_PassAddress);

    data.m_drawCb(mCommandList);

    mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
        m_ShadowMap->Resource(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_GENERIC_READ));
}

void VarianceShadowsManager::BuildShadowMap()
{
	//最有一个参数是false，表示不创建深度视图，因为vsm不需要深度视图
    m_ShadowMap = std::make_shared<ShadowMapRes>(m_d3dDevice, m_copyCsmConfig.m_iBufferSize ,
        m_copyCsmConfig.m_iBufferSize, m_copyCsmConfig.m_ShadowBufferFormat,false);

}