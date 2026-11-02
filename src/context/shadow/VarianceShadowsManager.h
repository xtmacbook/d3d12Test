

#include "common/util.h"
#include "common/GameTimer.h"
#include "common/ShadowMapRes.h"

#include <vector>
#include <memory>
#include <functional>

class Camera;
class ShadowMapRes;
class FrameResourceInterface;
class BlurFilter;
class SAT;

#define MAX_VSM_CASCADES 3

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
	BOOL  m_bWithCSM; // 是否使用CSM
};

struct VSMShadowHeapDescriptor
{
    ID3D12DescriptorHeap*	        m_DescriptorHeap;
    ID3D12DescriptorHeap*			m_RtvHeap;
	ID3D12DescriptorHeap*			m_DsvHeap;

	UINT m_ShadowMapOffset = 0;
	UINT m_TmpDepthHeapOffset = 1;
	UINT m_BlurHeapOffset;
    UINT m_SATOffset;
};

struct VSMPassConstants
{
    DirectX::XMFLOAT4X4 m_WorldViewProj;
    DirectX::XMFLOAT4X4 m_World;
    DirectX::XMFLOAT4X4 m_WorldView;
    DirectX::XMFLOAT4X4 m_Shadow;

    DirectX::XMFLOAT4 m_vShadowTextureOffset[8];
    DirectX::XMFLOAT4 m_vShadowTextureScale[8];

    INT m_nCascadeLevels;     // Number of Cascades
    INT m_iVisualizeCascades; // 1 is to visualize the cascades in different colors. 0 is to just draw the scene.
    // For Map based selection scheme, this keeps the pixels inside of the the valid range.
    // When there is no boarder, these values are 0 and 1 respectivley.
    FLOAT m_fMinBorderPadding;
    FLOAT m_fMaxBorderPadding;

    FLOAT m_fCascadeBlendArea;                 // Amount to overlap when blending between cascades.
    DirectX::XMFLOAT2 m_fTexSize;                        // Shadow map texel size.
    FLOAT m_fNativeTexelSizeInX;               // Texel size in native map ( textures are packed ).
    
    FLOAT m_fCascadeFrustumsEyeSpaceDepths[8]; // The values along Z that seperate the cascades.
    DirectX::XMFLOAT4 m_vLightDir;
};

struct VSMShadowMapDrawData
{
    using DrawSceneForShadowMapFunc = std::function<void(ID3D12GraphicsCommandList *)>;

    DrawSceneForShadowMapFunc m_drawCb;
    UINT m_PassRootParameterIndx;
    D3D12_GPU_VIRTUAL_ADDRESS m_PassAddress;
};

class VarianceShadowsManager
{
public:
    VarianceShadowsManager();
    ~VarianceShadowsManager();

    bool init(ID3D12Device *, const DirectX::BoundingBox &sceneBox,
              const Camera *viewCamra, const Camera *shadowCamera,
              VSMConfig *config);

    UINT GetRtvDescriptorCount() const;
    // constant-buffer, shader-resource, and unordered-access 
    UINT GetCSUDescriptorCount() const;
    
    CD3DX12_GPU_DESCRIPTOR_HANDLE GetShadowMapGPUHandle();

    void BuildDescriptors(ID3D12DescriptorHeap* heapDescriptor, UINT heapOffset,
        ID3D12DescriptorHeap*RtvHeap,ID3D12DescriptorHeap*DsvHeap);

    void UpdateMainPassData(VSMPassConstants &constData);
    DirectX::XMMATRIX GetShadowPassData(UINT iCascadeIndex);

    void BuildPOS(SDKMesh::SDKMeshModel *, D3D12_GRAPHICS_PIPELINE_STATE_DESC);

    ID3D12PipelineState *GetSceneEffect(int idx = 0);
    ID3D12PipelineState *GetShadowEffect();

    void RenderScene(ID3D12GraphicsCommandList *cmmandList, VSMShadowMapDrawData &data);
    void RenderShadows(ID3D12GraphicsCommandList *mCommandList,
                       VSMShadowMapDrawData &data);

    void OnResize(int width, int heigh);

    void BuildRootSignature();

    void BuildShadowMap();

    void UpdateFrame(const GameTimer &gt, Camera *viewCamera, Camera *lightCamer);

    void CalculateFrustum(Camera *viewCamera, Camera *lightCamer,
                          DirectX::XMVECTOR &vLightCameraOrthographicMin,
                          DirectX::XMVECTOR &vLightCameraOrthographicMax);

    DirectX::XMVECTOR CalculateCascadeFrumstum(int iCascadeIndex, Camera* viewCamera, Camera* lightCamer,
        DirectX::XMVECTOR& vLightCameraOrthographicMin, DirectX::XMVECTOR& vLightCameraOrthographicMax);

    INT																				m_iCascadePartitionsZeroToOne[MAX_VSM_CASCADES]; //这个是用户设置的： Values are 0 to 100 and represent a percent of the frstum
    INT																				m_iCascadePartitionsMax;
    bool                                                                            m_blurShadow = false;
    bool                                                                            m_satShadow = false;
private:
    Microsoft::WRL::ComPtr<ID3DBlob> m_ppsRenderSceneVSShadersBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> m_ppsRenderScenePSShadersBlob[2];

    Microsoft::WRL::ComPtr<ID3DBlob> m_ppsRenderShadowVSShadersBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> m_ppsRenderShadowPSShadersBlob;

    std::shared_ptr<ShadowMapRes> m_ShadowMap;
    
    std::shared_ptr<SDKMesh::Effect> m_sceneEffect;  // 目前所有的mesh中的part使用相同的pso，具体模型可能不同
    std::shared_ptr<SDKMesh::Effect> m_sceneWithCSMEffect;  // 目前所有的mesh中的part使用相同的pso，具体模型可能不同
    std::shared_ptr<SDKMesh::Effect> m_shadowEffect; // 目前所有的mesh中的part使用相同的pso，具体模型可能不同

    std::shared_ptr<ShadowMapRes> m_TmpDepthMap;
    VSMShadowHeapDescriptor       m_heapDescriptor;
    DirectX::BoundingBox m_sceneBox;

    VSMConfig *m_vsmConfig; // 用户可能修改了
    ID3D12Device *m_d3dDevice = nullptr;
    
	FLOAT																			m_fCascadePartitionsFrustum[MAX_VSM_CASCADES]; //程序计算的真实值 Values are  between near and far
    DirectX::XMMATRIX m_matShadowProj[8];
	DirectX::XMMATRIX																m_matShadowView;
    
	std::unique_ptr<BlurFilter> m_BlurFilter;
	std::unique_ptr<SAT> m_Sat;

};