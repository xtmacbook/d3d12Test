#pragma once

#include "common/Util.h"
#include "common/BufferStruct.h"
#include "common/GameTimer.h"

#include <memory>
#include <functional>
#include <unordered_map>

class ShadowMapRes;
class D3DContext;
class FrameResourceInterface;

class ShadowInterface
{
public:
    struct ShadowMapUpdateData
    {
        DirectX::XMFLOAT3       m_lightDir;
        DirectX::BoundingSphere m_sceneBounds;
    };

    struct ShadowMapDrawData
    {
        using DrawSceneForShadowMapFunc = std::function<void(ID3D12GraphicsCommandList*)>;

        DrawSceneForShadowMapFunc m_drawCb;
        UINT m_shadowPassRootParameterIndx;
        D3D12_GPU_VIRTUAL_ADDRESS m_shadowPassAddress;
    };


    ShadowInterface(D3DContext *);

    virtual void InitSceneBounds();

    void BuildDrawScenePSO(ID3D12RootSignature*);

    void BuildShadowMap();

    void BuildDescriptors(
        CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv,
        CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv,
        CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuDsv);

    ID3D12PipelineState* GetDrawSceneToShadowMapPSO();

    virtual void UpdateShadowTransform(ShadowMapUpdateData data);

    virtual void DrawSceneToShadowMap(ID3D12GraphicsCommandList*, ShadowMapDrawData data);
    
    void UpdateShadowPass(const GameTimer& gt, FrameResourceInterface*,int idx);

    inline DirectX::XMFLOAT4X4     getShadowTransform() { return m_ShadowTransform; }

protected:
    std::shared_ptr<ShadowMapRes>                    m_ShadowMap;
    D3DContext *                                     m_d3dContext;
    DirectX::XMFLOAT3                                m_LightPosW;
    DirectX::XMFLOAT4X4                              m_LightView;
    DirectX::XMFLOAT4X4                              m_LightProj;
    DirectX::XMFLOAT4X4                              m_ShadowTransform;
    PassConstantsWithNLightAndShadow                 m_shadowPass;

    Microsoft::WRL::ComPtr<ID3D12PipelineState>      m_ShadowMapDrawPSO;

};