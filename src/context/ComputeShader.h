

#include "BlendContext.h"
#include "interface/ComputeShaderInterface.h"
#include <array>


class ComputeShaderContext : public BlendContext
{
public:
	
	void CreateComputePipeLineState();

	void CreateStructedBuffer(ID3D12Device*device, ID3D12GraphicsCommandList* mCommandList);

	void CopyCSResultToSysMemory(ID3D12Device* device, ID3D12GraphicsCommandList* mCommandList);

	void BuildComputeShaderRootSignature();

	void BindOutputResources(ID3D12Device*);

	Microsoft::WRL::ComPtr<ID3D12RootSignature>							m_BlurSignature = nullptr;


};
