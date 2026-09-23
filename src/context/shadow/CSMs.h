

 /*
 Adding shadows to a title is a process. 
 The first step is to get basic shadow maps working. 
 The second is to ensure all basic calculations are done optimally: 

 1) frusta fit as tightly as possible, 
 2) near/far planes fit tightly,
 3) slope-scaled bias is used, and so on.

 Slope-Scale Depth Bias
As previously mentioned, self-shadowing can lead to shadow acne. 
Adding too much bias can result in Peter Panning. Additionally,
polygons with steep slopes (relative to the light) suffer more from projective aliasing than polygons with shallow
slopes (relative to the light). Because of this, each depth map value may need a different offset depending on the polygon's slope relative to the light.


目前是view frumstum不变的，下面说的远近plane,都是在说light space的frustum
The depth buffer can be 16-bit, 24-bit, or 32-bit, with values between 0 and 1.
Generally, depth buffers are fixed point, with the values close to the near plane grouped more closely together 
than the values close to the far plane. The degree of precision available to the depth buffer 
is determined by the ratio of the near plane to the far plane. Using the tightest possible
near/far plane could allow use of a 16-bit depth buffer. A 16-bit depth buffer could reduce
the use of memory while increasing processing speed.


An easy and naive way to calculate the near plane and far plane is to transform the scene's bounding volume
into light space. The smallest Z-coordinate value is the near plane and the largest Z-coordinate value is the far plane.

 

        vLightCameraOrthographicMin /= vWorldUnitsPerTexel;
        vLightCameraOrthographicMin = XMVectorFloor( vLightCameraOrthographicMin );
        vLightCameraOrthographicMin *= vWorldUnitsPerTexel;

        vLightCameraOrthographicMax /= vWorldUnitsPerTexel;
        vLightCameraOrthographicMax = XMVectorFloor( vLightCameraOrthographicMax );
        vLightCameraOrthographicMax *= vWorldUnitsPerTexel;

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

struct ShadowHeapDescriptor2
{
	UINT m_shadowMapHeapOffset;
	UINT m_sdkMeshModelTextureHeapOffset;
	UINT m_nullHeapOffset;

	CD3DX12_GPU_DESCRIPTOR_HANDLE m_nullSrvGpuHandle;
};

class CascadedShadowsManager;
struct CSMConfig;

class CSMMapContext :public D3DContext,
	public FrameResourceContextInterface
{
public:
	virtual bool InitDirect3D()override;

	void BuildDescriptorHeaps();

	void CreateDsvDescriptorHeap()override;

	virtual void BuildRootSignature();

	void OnResize(int width, int heigh) override;

	virtual void BuildFrameResources()override;
	virtual void BuildShapeGeometry(ID3D12Device*, ID3D12GraphicsCommandList* mCommandList);

	void ShowCustomImguiWin() override;
	
	void BuildPSOs();
	void BuildTextures();
	void BuildResourceView();

	void Update(const GameTimer& gt)override;
	virtual void UpdateMaterialCBs(const GameTimer& gt);
	void UpdateMainPassCB(const GameTimer& gt);
	void UpdateObjectCBs(const GameTimer& gt);


	void Draw(const GameTimer& gt)override;
	void DrawFrameResource(ID3D12CommandAllocator*)override;

	void OnKeyboardInput(const GameTimer& gt)override;

private:

	std::shared_ptr<SDKMesh::SDKMeshModel>											m_sdkMeshModel = nullptr;

	std::unordered_map<std::string, std::unique_ptr<Material>>						m_Materials;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>									m_SrvDescriptorHeap = nullptr; //for texture source
	Microsoft::WRL::ComPtr<ID3D12RootSignature>										m_RootSignature = nullptr;
	
	Camera																			m_shadowLightCamera;
	std::shared_ptr< CascadedShadowsManager>										m_cascadedShadowsMgr = nullptr;
	ShadowHeapDescriptor2															m_HeapDescriptorOffsets;
	std::shared_ptr< CSMConfig>														m_csmConfig;

	//csm config
	float																			m_fPCFOffset = 0.002f;
	FLOAT																			m_fBlurBetweenCascadesAmount = 0.005f;

};


/*




*/