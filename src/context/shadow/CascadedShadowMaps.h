
/*


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

class CascadedShadowMaps :public D3DContext,
	public FrameResourceContextInterface
{
public:
	virtual bool InitDirect3D()override;

	void BuildDescriptorHeaps();

	virtual void BuildRootSignature();
	virtual void BuildShadersAndInputLayout();
	virtual void BuildFrameResources()override;
	virtual void BuildShapeGeometry(ID3D12Device*, ID3D12GraphicsCommandList* mCommandList);
	virtual void BuildRenderItems();
	virtual void BuildMaterials();
	void BuildPSOs();
	void BuildTextures();
	void BuildTextureResourceView();
	void Update(const GameTimer& gt)override;
	virtual void UpdateMaterialCBs(const GameTimer& gt);
	void UpdateMainPassCB(const GameTimer& gt);
	void UpdateObjectCBs(const GameTimer& gt);

	void Draw(const GameTimer& gt)override;
	void DrawFrameResource(ID3D12CommandAllocator*)override;

	std::shared_ptr<SDKMesh::SDKMeshModel> m_sdkMeshModel = nullptr;	


	PassConstantsWithNLight															m_MainPassCB;
	std::unordered_map<std::string, std::unique_ptr<Material>>						m_Materials;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>									m_SrvDescriptorHeap = nullptr; //for texture source

	Microsoft::WRL::ComPtr<ID3D12RootSignature>										m_RootSignature = nullptr;
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3DBlob>>				m_Shaders;

};