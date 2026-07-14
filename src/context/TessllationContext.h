#pragma once

#include "ComContext.h"
#include "common/BufferStruct.h"

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

	virtual void BuildPSOs()override;
	virtual void BuildMaterials();
	virtual void BuildRenderItems();

	virtual void initTextures(ID3D12Device*, ID3D12GraphicsCommandList* mCommandList);

	virtual void Update(const GameTimer& gt) override;
	void UpdateMainPassCB(const GameTimer& gt);

	virtual void DrawFrameResource(ID3D12CommandAllocator*) override;
protected:
	PassConstantsWithLight															m_MainPassCB;

private:

};
