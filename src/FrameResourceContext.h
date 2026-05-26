#pragma once
#include "common/D3DContext.h"
#include "Data.h"

#include <vector>
#include <memory>
#include <unordered_map>
#include <string>

struct FrameResource;
struct RenderItem;

class FrameResourceContext : public D3DContext
{
public:

	virtual bool InitDirect3D();

	virtual void Update(const GameTimer& gt);

	virtual void Draw(const GameTimer& gt);

	virtual void UpdateObjectCBs(const GameTimer& gt);

	virtual void UpdateMainPassCB(const GameTimer& gt);

	static const int						m_NumFrameResources;

protected:
	virtual void BuildFrameResources();
	virtual void BuildDescriptorHeaps();
	virtual void BuildRenderItems();
	virtual void BuildConstantBufferViews();
	virtual void BuildRootSignature();
	virtual void BuildShadersAndInputLayout();
	virtual void BuildGeometry();
	virtual void BuildPSO();
	virtual void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems);
private:
	
	std::vector< std::unique_ptr<FrameResource> >									m_frameResources;
	FrameResource*																	m_currFrameResource;
	int																				m_CurrFrameResourceIndex = 0;

	std::vector < std::unique_ptr<RenderItem> >										m_AllRitems;
	std::vector<RenderItem*>														m_OpaqueRitems; //Render items divided by PSO.
	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>>					m_Geometries;


	PassConstants																	m_MainPassCB;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>									m_ObjCbvHeap = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>									m_passCbvHeap = nullptr;

	Microsoft::WRL::ComPtr<ID3D12RootSignature>										m_RootSignature = nullptr;

	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3DBlob>>				m_Shaders;
	std::vector<D3D12_INPUT_ELEMENT_DESC>											m_InputLayout;
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12PipelineState> >	m_PSOs;

	UINT																			m_PassCbvOffset = 0;


};

