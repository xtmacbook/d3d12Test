#pragma once

#include "ComContext.h"
#include <array>


/*
	1. Render the floor, walls, and skull to the back buffer as normal
	2. Clear the stencil buffer to 0
	3. Render the mirror only to the stencil buffer. We can disable color writes to the back buffer  and we can disable writes to the depth buffer by setting
	4. Now we render the reflected skull to the back buffer and stencil buffer this is done using a StencilRef of 1, and the stencil operator D3D12_COMPARISON_EQUAL. 
	5. Finally, we render the mirror to the back buffer as normal. However,  we need to render the mirror with transparency blending. 

*/

class MirrorWithStencil : public ComContext
{

public:

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
	
	//void DrawRenderItem(ID3D12GraphicsCommandList* cmdList, const RenderItem* ritems);

protected:
	

	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12PipelineState>>	m_PSOs;
	
	PassConstantsWithFrog															m_MainPassCB;

};
