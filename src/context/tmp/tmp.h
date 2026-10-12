

#include "common/util.h"
#include "common/GameTimer.h"
#include "common/ShadowMapRes.h"

#include <vector>
#include <memory>
#include <functional>

class Camera;
class ShadowMapRes;
class FrameResourceInterface;

namespace SDKMesh
{
    struct Effect;
    struct SDKMeshModel;
}

struct VSMConfig
{
    INT m_nCascadeLevels;
    SHADOW_TEXTURE_FORMAT m_ShadowBufferFormat;
    INT m_iBufferSize;
};

struct VSMPassConstants
{
    DirectX::XMFLOAT4X4     m_WorldViewProj;
	DirectX::XMFLOAT4X4     m_World;
	DirectX::XMFLOAT4X4     m_WorldView;
	DirectX::XMFLOAT4X4     m_Shadow;
	DirectX::XMFLOAT4       m_vLightDir;
};

struct VSMShadowMapDrawData
{
	using DrawSceneForShadowMapFunc = std::function<void(ID3D12GraphicsCommandList*)>;

	DrawSceneForShadowMapFunc m_drawCb;
	UINT m_PassRootParameterIndx;
	D3D12_GPU_VIRTUAL_ADDRESS m_PassAddress;
};

class VarianceShadowsManager
{
public:
    bool init(ID3D12Device *, const DirectX::BoundingBox &sceneBox,
              const Camera *viewCamra, const Camera *shadowCamera,
              VSMConfig *config);

    UINT GetResouceViewCount() const;

    void BuildDescriptors(
        CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv,
        CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv,
        CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuRtv);

    void UpdateMainPassData(VSMPassConstants &constData);

    void BuildPOS(SDKMesh::SDKMeshModel *, D3D12_GRAPHICS_PIPELINE_STATE_DESC);

	ID3D12PipelineState* GetEffect();

	void RenderScene(ID3D12GraphicsCommandList* cmmandList, VSMShadowMapDrawData& data);

    void BuildShadowMap();

private:


    Microsoft::WRL::ComPtr<ID3DBlob> m_ppsRenderSceneVSShadersBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> m_ppsRenderScenePSShadersBlob;

    std::shared_ptr<ShadowMapRes> m_ShadowMap;
    std::shared_ptr<SDKMesh::Effect>  m_effect; // 目前所有的mesh中的part使用相同的pso，具体模型可能不同
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_shadowDrawPSO = nullptr;

    DirectX::BoundingBox m_sceneBox;

    VSMConfig *m_csmConfig; // 用户可能修改了
    VSMConfig m_copyCsmConfig;
    ID3D12Device *m_d3dDevice = nullptr;

    DirectX::XMMATRIX m_matShadowProj;
    DirectX::XMMATRIX m_matShadowView;
};