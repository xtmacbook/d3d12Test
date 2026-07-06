#pragma once

#include "ComContext.h"

#include <array>

class Waves;

class TessllationContext : public ComContext
{
public:
	virtual bool InitDirect3D()override;
	virtual void BuildShapeGeometry(ID3D12Device*, ID3D12GraphicsCommandList* mCommandList)override;
	void BuildFrameResources()override;
	void BuildRootSignature()override;
	void BuildShadersAndInputLayout()override;

	virtual void BuildPSOs();
	virtual void BuildMaterials();
	virtual void BuildRenderItems();

	virtual void initTextures(ID3D12Device*, ID3D12GraphicsCommandList* mCommandList);

	virtual void Update(const GameTimer& gt) override;
	void UpdateMainPassCB(const GameTimer& gt);

	virtual void Draw(const GameTimer& gt);
	virtual void DrawFrameResource(ID3D12CommandAllocator*) override;
protected:
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12PipelineState>>	m_PSOs;
	PassConstantsWithLight															m_MainPassCB;

private:

};
