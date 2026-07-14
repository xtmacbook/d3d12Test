
#pragma once

#include "ComContext.h"

/*
1. structured buffer that contains the perinstance data for all of our instances ( For example, if we were going to instance an object 100 times, we would create a structured buffer with 100 per-instance data elements)
2. use SV_InstanceID which you can use in your vertex shader.
*/

class InstanceContext : public ComContext
{
public:

	virtual bool InitDirect3D()override;

	virtual void BuildRootSignature()override;
	virtual void BuildShadersAndInputLayout()override;
	virtual void BuildFrameResources()override;
	virtual void BuildShapeGeometry(ID3D12Device*, ID3D12GraphicsCommandList* mCommandList)override;
	virtual void BuildRenderItems();
	virtual void BuildMaterials();

	void Update(const GameTimer& gt)override;
	virtual void UpdateMaterialCBs(const GameTimer& gt)override;
	
	virtual void DrawRenderItem(ID3D12GraphicsCommandList* cmdList, const RenderItem* ritems)override;
	virtual void DrawFrameResource(ID3D12CommandAllocator*)override;


	/*
		the AABB of model is in local space,so we need transform the view frustum into the local space
		of each instance
	*/
	void UpdateSceneData();

};
