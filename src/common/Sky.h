#pragma once
#include "Util.h"

#include <vector>
#include <unordered_map>
#include <memory>

/*
最后绘制sky,而不是之前先绘制sky
需要设置depth comparison function

reflections via environment mapping do not work well for flat surfaces.

*/

class D3DContext;
class MeshGeometry;
class Texture;

class Sky
{
    public:
    
    Sky (D3DContext*);

    void BuildPSO();
    void BuildSkyGeometry();
    void BuildResource();
    void BuildRootSignature();
    void BuildDescriptors(
        CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuDescriptor,
        CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuDescriptor,
        UINT descriptorSize);

    void BuildLayout();

    void DrawSky(ID3D12GraphicsCommandList* cmdList,
        ID3D12CommandAllocator*allocator, CD3DX12_GPU_DESCRIPTOR_HANDLE samplerHandle,
        D3D12_GPU_VIRTUAL_ADDRESS passHandle, ID3D12DescriptorHeap*descriptorHeaps[]);

	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3DBlob>>				                    m_Shaders;

    Microsoft::WRL::ComPtr<ID3D12PipelineState>                                                         m_skyPSO;        
    std::shared_ptr<MeshGeometry>                                                                       m_skyGeo = nullptr;
    D3DContext*                                                                                         m_context;

    CD3DX12_CPU_DESCRIPTOR_HANDLE			                                                            m_CubeMapCPUSrv;
    CD3DX12_GPU_DESCRIPTOR_HANDLE			                                                            m_CubeMapGpuSRrv;
    Microsoft::WRL::ComPtr<ID3D12RootSignature>			                                                m_skySignature = nullptr;

    std::unique_ptr<Texture>                                                                            m_TextureResource = nullptr;
};