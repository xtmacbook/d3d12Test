

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
    DirectX::XMFLOAT4       m_vShadowTextureOffset;
	DirectX::XMFLOAT4       m_vShadowTextureScale;

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
        CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuRtv,

        CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv1,
        CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv1,
        CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuDsv);

    void UpdateMainPassData(VSMPassConstants& constData);
    void UpdateShadowPassData(VSMPassConstants &constData);

    void BuildPOS(SDKMesh::SDKMeshModel *, D3D12_GRAPHICS_PIPELINE_STATE_DESC);

    ID3D12PipelineState* GetSceneEffect();
    ID3D12PipelineState* GetShadowEffect();

	void RenderScene(ID3D12GraphicsCommandList* cmmandList, VSMShadowMapDrawData& data);
    void RenderShadows(ID3D12GraphicsCommandList* mCommandList,
        VSMShadowMapDrawData& data);

    void BuildShadowMap();

    void UpdateFrame(const GameTimer& gt, Camera* viewCamera, Camera* lightCamer);
    
    inline DirectX::XMMATRIX getShadowProjMatrix() const { return m_matShadowProj; }

    void CalculateFrustum(int iCascadeIndex, Camera* viewCamera, Camera* lightCamer,
    DirectX::XMVECTOR& vLightCameraOrthographicMin, 
    DirectX::XMVECTOR& vLightCameraOrthographicMax);
private:

    Microsoft::WRL::ComPtr<ID3DBlob> m_ppsRenderSceneVSShadersBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> m_ppsRenderScenePSShadersBlob;

    Microsoft::WRL::ComPtr<ID3DBlob> m_ppsRenderShadowVSShadersBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> m_ppsRenderShadowPSShadersBlob;

    std::shared_ptr<ShadowMapRes> m_ShadowMap;
    std::shared_ptr<SDKMesh::Effect>  m_sceneEffect; // 目前所有的mesh中的part使用相同的pso，具体模型可能不同
    std::shared_ptr<SDKMesh::Effect>  m_shadowEffect; // 目前所有的mesh中的part使用相同的pso，具体模型可能不同

    std::shared_ptr<ShadowMapRes> m_TmpDepthMap;
    
    DirectX::BoundingBox m_sceneBox;

    VSMConfig *m_vsmConfig; // 用户可能修改了
    VSMConfig m_copyVsmConfig;
    ID3D12Device *m_d3dDevice = nullptr;

    DirectX::XMMATRIX m_matShadowProj;
};