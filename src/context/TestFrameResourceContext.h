#pragma once
#include "../common/D3DContext.h"
#include "../common/FrameResource.h"
#include "../common/Data.h"
#include "FrameResourceContext.h"

#include <vector>
#include <memory>
#include <unordered_map>
#include <string>
 
struct RenderItem;

class DefaultFrameResourceContext : public FrameResourceContextInterface
{
public:

	virtual bool InitDirect3D();

	virtual void Update(const GameTimer& gt);

	virtual void UpdateObjectCBs(const GameTimer& gt);

	virtual void UpdateMainPassCB(const GameTimer& gt);

	virtual void BuildFrameResources() override;
	virtual void DrawFrameResource(ID3D12CommandAllocator*) override;

protected: 
	virtual void BuildDescriptorHeaps();
	virtual void BuildRenderItems();
	virtual void BuildConstantBufferViews();
	virtual void BuildRootSignature();
	virtual void BuildShadersAndInputLayout();
	virtual void BuildGeometry();
	virtual void BuildPSO();
	virtual void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems);
protected:
	
	std::vector < std::unique_ptr<RenderItem> >										m_AllRitems;
	std::vector<RenderItem*>														m_OpaqueRitems; //Render items divided by PSO.
	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>>					m_Geometries;


	PassConstants																	m_MainPassCB;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>									m_ObjCbvHeap = nullptr;

	Microsoft::WRL::ComPtr<ID3D12RootSignature>										m_RootSignature = nullptr;

	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3DBlob>>				m_Shaders;
	std::vector<D3D12_INPUT_ELEMENT_DESC>											m_InputLayout;
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12PipelineState> >	m_PSOs;

	UINT																			m_PassCbvOffset = 0;


};

