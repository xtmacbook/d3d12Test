#pragma once

#include "ComContext.h"
#include <array>

/*
*
*/

class GSContext : public ComContext
{

public:

	virtual void BuildShaders()override;

	virtual void BuildLayout()override;

	virtual bool InitDirect3D()override;
	virtual void BuildShapeGeometry(ID3D12Device*, ID3D12GraphicsCommandList* mCommandList)override;
	virtual void BuildFrameResources()override;

	void BuildPSOs();
	void BuildMaterials();
	void BuildRenderItems();

	virtual void Update(const GameTimer& gt) override;
	void UpdateMainPassCB(const GameTimer& gt);

	virtual void Draw(const GameTimer& gt);
	virtual void DrawFrameResource(ID3D12CommandAllocator*) override;

protected:
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12PipelineState>>	m_PSOs;

	PassConstantsWithFrog															m_MainPassCB;

};
