#include "VarianceShadowsManager.h"
#include "common/SDKMeshModel.h"

/**
 *
 *
m_pCascadedShadowMapVarianceTextureArray: 数组(cascade)-------m_pCascadedShadowMapVarianceRTVArrayAll[cascade](targetview )
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

    return false;
}

UINT VarianceShadowsManager::GetResouceViewCount() const
{
    return 3;
}

void VarianceShadowsManager::BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv,
                                              CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv, CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuDsv)
{
}

void VarianceShadowsManager::UpdateMainPassData(VSMPassConstants &constData)
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
    m_effect = meshModel->CreateOnlyOneEffect(epsd);
}

ID3D12PipelineState *VarianceShadowsManager::GetEffect()
{
    return m_effect->m_PSO.Get();
}

void VarianceShadowsManager::RenderScene(ID3D12GraphicsCommandList *cmmandList, VSMShadowMapDrawData &data)
{
     if (data.m_drawCb)
        data.m_drawCb(cmmandList);
}
