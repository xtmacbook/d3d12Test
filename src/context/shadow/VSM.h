
/**
 *
 *  VSMs enable direct shadow map filtering , all of the power of the texture-filtering hardware can be used. 
 
    Additionally, VSMs can be blurred directly through convolution. 
    No use PCF, the shadows are blurred in a two-pass separable convolution.

    VSMs do have some drawbacks; two channels of depth data must be stored (depth and depth squared). 
    When shadows overlap, light-bleeding is common. 

    //seam
    Using gradients with CSMs can produce a seam along the border between two cascades as seen in Figure 17. 
    The sample instruction uses derivatives between pixels to calculate information, such as the mipmap level, needed by the filter. 
    This causes a problem in particular for mipmap selection or anisotropic filtering.
    When pixels in a quad take different branches in the shader, the derivatives calculated by the GPU hardware are invalid.
    This results in a jagged seam along the shadow map.

    This problem is solved by computing the derivatives on the position in light-view space; 
    the light-view space coordinate is not specific to the selected cascade. 
    The computed derivatives can be scaled by the scale portion of the projection-texture matrix to the correct mipmap level.

    float3 vShadowTexCoordDDX = ddx( vShadowMapTextureCoordViewSpace );
        vShadowTexCoordDDX *= m_vCascadeScale[iCascade].xyz;
        float3 vShadowTexCoordDDY = ddy( vShadowMapTextureCoordViewSpace );
        vShadowTexCoordDDY *= m_vCascadeScale[iCascade].xyz;

        mapDepth += g_txShadow.SampleGrad( g_samShadow, vShadowTexCoord.xyz,
        vShadowTexCoordDDX, vShadowTexCoordDDY );
    

    Both VSMs and PCF attempt to approximate the fraction of pixel area that would pass the depth test. 
    VSMs work with filtering hardware and can be blurred with separable kernels. 
    Separable convolution kernels are considerably cheaper to implement than a full kernel. 
    Additionally, VSMs compare one light-space depth against one value in the light-space depth map.
    This means that VSMs do not have the same offset problems as PCF. 
    Technically, VSMs are sampling depth over a greater area, as well as performing a statistical analysis. 
    This is less precise than PCF. In practice, VSMs do a very good job of blending, which results in less offset being necessary. 

    VSMs and PCF represent a trade-off between GPU compute power and GPU texture bandwidth. 
    VSMs require more math to be performed to calculate the variance. 
    PCF requires more texture memory bandwidth. Large PCF kernels can quickly become bottlenecked by texture bandwidth. 
    With GPU computation power growing more rapidly than GPU bandwidth, VSMs are becoming the more practical of the two algorithms. 
    VSMs also look better with lower resolution shadow maps due to blending and filtering.

 * 
 */

 
#pragma once

#include "common/D3DContext.h"
#include "interface/FrameResourceContextInterface.h"
#include "common/util.h"
#include "common/BufferStruct.h"

namespace SDKMesh
{
	struct SDKMeshModel;
}

struct VSMShadowHeapDescriptor
{
	UINT m_shadowMapHeapOffset;
	UINT m_sdkMeshModelTextureHeapOffset;
	UINT m_nullHeapOffset;

	CD3DX12_GPU_DESCRIPTOR_HANDLE m_nullSrvGpuHandle;
};

class VarianceShadowsManager;
class ShadowMapRes;
struct VSMConfig;


class VSMMapContext :public D3DContext,
	public FrameResourceContextInterface
{
public:
	virtual bool InitDirect3D()override;

	void BuildDescriptorHeaps();

	void CreateRtvDescriptorHeap() override;

	virtual void BuildRootSignature();

	void OnResize(int width, int heigh) override;

	virtual void BuildFrameResources()override;
	virtual void BuildShapeGeometry(ID3D12Device*, ID3D12GraphicsCommandList* mCommandList);

	void ShowCustomImguiWin() override;
	
	void BuildPSOs();
	void BuildTextures(const std::wstring textureDir);
	void BuildResourceView();

	void Update(const GameTimer& gt)override;
	virtual void UpdateMaterialCBs(const GameTimer& gt);
	void UpdatePassCB(const GameTimer& gt);
	void UpdateObjectCBs(const GameTimer& gt);

	void Draw(const GameTimer& gt)override;
	void DrawFrameResource(ID3D12CommandAllocator*)override;
private:

	std::shared_ptr<SDKMesh::SDKMeshModel>											m_sdkMeshModel = nullptr;

	std::unordered_map<std::string, std::unique_ptr<Material>>						m_Materials;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>									m_SrvDescriptorHeap = nullptr; //for texture source
	Microsoft::WRL::ComPtr<ID3D12RootSignature>										m_RootSignature = nullptr;
	std::shared_ptr<ShadowMapRes>													m_ShadowMap;

	Camera																			m_shadowLightCamera;
	std::shared_ptr< VarianceShadowsManager>										m_varianceShadowsMgr = nullptr;
	VSMShadowHeapDescriptor															m_HeapDescriptorOffsets;
	std::shared_ptr< VSMConfig>														m_vsmConfig;

	//vsm config
	float																			m_fPCFOffset = 0.002f;
	FLOAT																			m_fBlurBetweenCascadesAmount = 0.005f;

};
