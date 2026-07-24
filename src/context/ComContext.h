#pragma once
#include "interface/FrameResourceContextInterface.h"
#include "interface/TexContextInterface.h"
#include "interface/GeomtryContextInterface.h"
#include "interface/BlendContextInterface.h"
#include "interface/LayoutInterface.h"
#include "common/BufferStruct.h"

#include <array>


class ComContext :
    public FrameResourceContextInterface,
	public TexContextInterface,
	public GeometryContextInterface,
	public BlendContextInterface,
	public LayoutInterface,
	public D3DContext
{

public:
	
	virtual void BuildShadersAndInputLayout();
	virtual void BuildRootSignature();
	virtual void BuildFrameResources()override;

	virtual void Update(const GameTimer& gt) override;
	virtual void UpdateObjectCBs(const GameTimer& gt);
	virtual void UpdateMaterialCBs(const GameTimer& gt);
	virtual void UpdateMainPassCB(const GameTimer& gt);
	virtual void UpdateInstance();


	virtual void Draw(const GameTimer& gt)override;
	virtual void DrawRenderItem(ID3D12GraphicsCommandList* cmdList, const RenderItem* ritems);

	virtual void BuildPSOs();

protected:
	PassConstantsWithFrog															m_MainPassCB;

	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12PipelineState>>	m_PSOs;
	RootParameterIndexs																m_rpi;
	Microsoft::WRL::ComPtr<ID3D12RootSignature>										m_RootSignature = nullptr;
	std::vector<std::unique_ptr<RenderItem>>										m_AllRitems;
	std::unordered_map<std::string, std::unique_ptr<Material>>						m_Materials;


};
