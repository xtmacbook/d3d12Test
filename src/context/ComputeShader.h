

#include "BlendContext.h"
#include "interface/ComputeShaderInterface.h"
#include "../common/BlurFilter.h"
#include <array>


class ComputeShaderContext : public BlendContext
{
public:
	
	bool InitDirect3D() override;

	void OnResize() override;

	void BuildSRVDescriptorHeap(ID3D12Device* md3dDevice) override;

	void BuildShadersAndInputLayout() override;

	void BuildRootSignature() override;

	void BuildPSOs() override;

	bool DrawPostProcessFrameResource(ID3D12CommandAllocator*) override;

	void CreateStructedBuffer(ID3D12Device*device, ID3D12GraphicsCommandList* mCommandList);

	void CopyCSResultToSysMemory(ID3D12Device* device, ID3D12GraphicsCommandList* mCommandList);


private:
	std::unique_ptr<BlurFilter> m_BlurFilter;
};
 