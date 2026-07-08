#pragma once

#include "common/D3DContext.h"
#include "common/UploadBuffer.h"

#include <memory>
#include <vector>

struct MeshGeometry;
struct ObjectConstants;

class MainD3DContext :public D3DContext
{
public:

	virtual bool InitDirect3D();

	virtual void Update(const GameTimer& gt);
	virtual void Draw(const GameTimer& gt);

protected:
	virtual void BuildDescriptorHeaps();
	virtual void BuildConstantBuffers();

	virtual void BuildRootSignature();
	virtual void BuildShadersAndInputLayout();

	virtual void BuildPSO();

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