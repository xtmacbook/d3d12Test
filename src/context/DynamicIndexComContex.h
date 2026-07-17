
#pragma once

#include "ComContext.h"

class DynamicIndexComContext : public ComContext
{
public:
	virtual void BuildRootSignature()override;
	virtual void BuildShadersAndInputLayout()override;
	virtual void BuildFrameResources()override;
	virtual void UpdateObjectCBs(const GameTimer& gt)override;
	virtual void UpdateMaterialCBs(const GameTimer& gt)override;
	virtual void DrawRenderItem(ID3D12GraphicsCommandList* cmdList, const RenderItem* ritems)override;

	virtual void BuildLayout()override;

};
