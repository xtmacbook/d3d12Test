#pragma once
#include "interface/FrameResourceContextInterface.h"
#include "interface/TexContextInterface.h"
#include "interface/GeomtryContextInterface.h"
#include "interface/BlendContextInterface.h"
#include "interface/LayoutInterface.h"

#include <array>

class BlendContext :
    public FrameResourceContextInterface,
	public TexContextInterface,
	public GeometryContextInterface,
	public BlendContextInterface,
	public LayoutInterface,
	public D3DContext
{

public:

	enum class RenderLayer : int
	{
		Opaque = 0,
		Transparent,
		AlphaTested,
		Count
	};

	virtual bool InitDirect3D();
	virtual void BuildFrameResources()override;
	virtual void BuildShadersAndInputLayout();
	virtual void BuildShapeGeometry(ID3D12Device*, ID3D12GraphicsCommandList* mCommandList)override;

	virtual void initTextures(ID3D12Device*device, ID3D12GraphicsCommandList* mCommandList);

	void BuildRootSignature();
	void BuildPSOs();
	virtual void BuildMaterials();
	virtual void BuildRenderItems();

	virtual void Update(const GameTimer& gt) override;
	void UpdateObjectCBs(const GameTimer& gt);
	void UpdateMaterialCBs(const GameTimer& gt);
	void UpdateMainPassCB(const GameTimer& gt);

	virtual void Draw(const GameTimer& gt);
	virtual void DrawFrameResource(ID3D12CommandAllocator*) override;
	
	void DrawRenderItem(ID3D12GraphicsCommandList* cmdList, const RenderItem* ritems);

protected:
	
	std::unordered_map<std::string, std::unique_ptr<Material>>						m_Materials;
	std::unordered_map<std::string, std::unique_ptr<Texture>>						m_Textures;

	std::vector<std::unique_ptr<RenderItem>>										m_AllRitems;
	std::vector<RenderItem*>														m_RitemLayer[(int)RenderLayer::Count];
	RenderItem*																		m_WavesRitem = nullptr;

	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12PipelineState>>	m_PSOs;

	Microsoft::WRL::ComPtr<ID3D12RootSignature>										m_RootSignature = nullptr;
	
	PassConstantsWithFrog															m_MainPassCB;

	RootParameterIndexs																m_rpi;
};
