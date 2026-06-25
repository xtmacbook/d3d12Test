#pragma once

#include "ComContext.h"
#include <array>


/*
* 
	
A:	Stencil Test:
	if(Stecil Ref & StencilReadMask  (operator Func)  value & StencilReadMask)
		accept pixel
	else
		reject pixel


B:	Depth Setting:
		DepthEnable:  如果为false,则无论顺序和是否遮挡,都会绘制,elements in the depth buffer are not updated either, regardless of the DepthWriteMask setting.

		DepthWriteMask: 
			D3D12_DEPTH_WRITE_MASK_ZERO ：
				不允许写入到depth buffer, 但如果DepthEnable为true,depth testing 会发生
			D3D12_DEPTH_WRITE_MASK_ALL:
				允许写入到depth buffer,
		 DepthFunc:  define the depth test comparison function.


C:  Stencil Setting:
	StencilEnable:
	StencilReadMask:
	StencilWriteMask: 
	FrontFace:  
    BackFace:

	D3D12_DEPTH_STENCILOP_DESC:
		StencilFailOp:如果stencil test失败后的stencil buffer如何操作 
		StencilDepthFailOp: stencil test 成功但是depth test失败后stencil buffer如何的操作
		StencilPassOp: depth test和stencil test都成功的stencil buffer如何操作
		StencilFunc:  


	1. Render the floor, walls, and skull to the back buffer as normal
	2. Clear the stencil buffer to 0
	3. Render the mirror only to the stencil buffer. We can disable color writes to the back buffer  and we can disable writes to the depth buffer
	4. Now we render the reflected skull to the back buffer and stencil buffer this is done using a StencilRef of 1, and the stencil operator D3D12_COMPARISON_EQUAL. 
	5. Finally, we render the mirror to the back buffer as normal. However,  we need to render the mirror with transparency blending. 

*/

class MirrorWithStencil : public ComContext
{

public:

	virtual bool InitDirect3D()override;
	virtual void BuildShapeGeometry(ID3D12Device*, ID3D12GraphicsCommandList* mCommandList)override;
	virtual void BuildFrameResources()override;

	virtual void BuildPSOs();
	virtual void BuildMaterials();
	virtual void BuildRenderItems();

	virtual void initTextures(ID3D12Device*, ID3D12GraphicsCommandList* mCommandList);

	virtual void Update(const GameTimer& gt) override;
	void UpdateMainPassCB(const GameTimer& gt);

	virtual void Draw(const GameTimer& gt);
	virtual void DrawFrameResource(ID3D12CommandAllocator*) override;
	
	//void DrawRenderItem(ID3D12GraphicsCommandList* cmdList, const RenderItem* ritems);

protected:
	

	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12PipelineState>>	m_PSOs;
	
	PassConstantsWithFrog															m_MainPassCB;

};
