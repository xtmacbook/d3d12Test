#pragma once

#include "common/D3DContext.h"
#include "common/UploadBuffer.h"
#include "common/MathHelper.h"
#include "Geometry.h"

#include <memory>

struct MeshGeometry;

struct ObjectConstants
{
	XMFLOAT4X4 WorldViewProj = MathHelper::Identity4x4();
};

class MainD3DContext :public D3DContext
{
public:

	virtual bool InitDirect3D();

	virtual void Update(const GameTimer& gt);
	virtual void Draw(const GameTimer& gt);

protected:
	void BuildDescriptorHeaps();
	void BuildConstantBuffers();

	void BuildRootSignature();
	void BuildShadersAndInputLayout();

	void BuildPSO();

	virtual void BuildGeometry();
	
private:
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>	m_CbvHeap = nullptr;
	std::unique_ptr<UploadBuffer<ObjectConstants>>	m_ObjectCB = nullptr;

	std::unique_ptr<MeshGeometry>					m_BoxGeo = nullptr;

	Microsoft::WRL::ComPtr<ID3DBlob>				m_vsByteCode = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob>				m_psByteCode = nullptr;

	Microsoft::WRL::ComPtr<ID3D12RootSignature>		m_RootSignature = nullptr;
	Microsoft::WRL::ComPtr<ID3D12PipelineState>		m_PSO = nullptr;

	std::vector<D3D12_INPUT_ELEMENT_DESC>			m_InputLayout;



};