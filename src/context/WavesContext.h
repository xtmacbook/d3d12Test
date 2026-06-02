
#pragma once
#include "FrameResourceContext.h"
#include "../common/Util.h"
#include "Waves.h"

#include <unordered_map>


enum class RenderLayer : int
{
	Opaque = 0,
	Count
};

class Waves;

class WavesContext : public FrameResourceContextInterface
{
public:

	virtual bool InitDirect3D()override;

	virtual void DrawFrameResource(ID3D12CommandAllocator*) override;

	virtual void Update(const GameTimer& gt)override;

protected:
	
	virtual void BuildShadersAndInputLayout();
	virtual void BuildFrameResources()override ;
	virtual void BuildGeometry() ;
	virtual void BuildRenderItems();

	void BuildMaterial();
	void BuildRootSignature();
	void BuildPSO();

	void UpdateObjectCBS(const GameTimer& gt);
	void UpdateMainPassCB(const GameTimer& gt);
	void UpdateWaves(const GameTimer& gt);
	void UpdateMaterialCBs(const GameTimer&gt);

	void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems);

private:
	float GetHillsHeight(float x, float z)const;
	DirectX::XMFLOAT3 GetHillsNormal(float x, float z)const;

	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3DBlob>>				m_Shaders;
	std::vector<D3D12_INPUT_ELEMENT_DESC>											m_InputLayout;
	Microsoft::WRL::ComPtr<ID3D12RootSignature>										m_RootSignature = nullptr;
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12PipelineState>>	m_PSOs;


	//material
	std::unordered_map<std::string, std::unique_ptr<Material>>						m_Materials;

	RenderItem*																		m_WavesRitem = nullptr;
	std::vector<RenderItem*>														m_RitemLayer[(int)RenderLayer::Count];

	std::unique_ptr<Waves>															m_Waves;

	std::vector < std::unique_ptr<RenderItem> >										m_AllRitems;
	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>>					m_Geometries;

	PassConstantsWithLight															m_MainPassCB;

	float m_SunTheta = 1.25f * DirectX::XM_PI;
	float m_SunPhi = DirectX::XM_PIDIV4;

};

