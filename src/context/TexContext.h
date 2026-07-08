#pragma once
#include "interface/FrameResourceContextInterface.h"
#include "interface/TexContextInterface.h"
#include "interface/GeomtryContextInterface.h"
#include "common/BufferStruct.h"

#include <array>

class TexContext :
    public FrameResourceContextInterface,
	public TexContextInterface,
	public GeometryContextInterface,
	public D3DContext
{

public:

	virtual bool InitDirect3D();
	virtual void BuildFrameResources()override;
	virtual void BuildShadersAndInputLayout();
	void BuildRootSignature();
	void BuildPSOs();
	void BuildMaterials();
	void BuildRenderItems();

	virtual void Update(const GameTimer& gt) override;
	void UpdateObjectCBs(const GameTimer& gt);
	void UpdateMaterialCBs(const GameTimer& gt);
	void UpdateMainPassCB(const GameTimer& gt);

	virtual void Draw(const GameTimer& gt);
	virtual void DrawFrameResource(ID3D12CommandAllocator*) override;
	void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems);
	
protected:
	
	std::vector<D3D12_INPUT_ELEMENT_DESC>											m_InputLayout;
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3DBlob>>				m_Shaders;
	std::unordered_map<std::string, std::unique_ptr<Material>>						m_Materials;
	std::unordered_map<std::string, std::unique_ptr<Texture>>						m_Textures;
	std::vector<std::unique_ptr<RenderItem>>										m_AllRitems;
	std::vector<RenderItem*>														m_OpaqueRitems;
	Microsoft::WRL::ComPtr<ID3D12PipelineState>										m_OpaquePSO = nullptr;

	Microsoft::WRL::ComPtr<ID3D12RootSignature>										m_RootSignature = nullptr;
	
	PassConstantsWithLight															m_MainPassCB;

	
};
