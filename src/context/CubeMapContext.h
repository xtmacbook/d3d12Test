
#pragma once

#include "ComContext.h"

class Sky;

class CubeMapContext : public ComContext
{
public:

	virtual bool InitDirect3D()override;

	virtual void BuildRootSignature()override;
	virtual void BuildShadersAndInputLayout()override;
	virtual void BuildFrameResources()override;
	virtual void BuildShapeGeometry(ID3D12Device*, ID3D12GraphicsCommandList* mCommandList)override;
	virtual void BuildRenderItems();
	virtual void BuildMaterials();

	void BuildTexture();

	void Update(const GameTimer& gt)override;
	virtual void UpdateMaterialCBs(const GameTimer& gt)override;

	virtual void DrawRenderItem(ID3D12GraphicsCommandList* cmdList, const RenderItem* ritems)override;
	virtual void DrawFrameResource(ID3D12CommandAllocator*)override;

	std::shared_ptr< Sky>  m_sky;

};