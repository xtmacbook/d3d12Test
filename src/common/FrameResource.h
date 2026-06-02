
#pragma once
#include "Util.h"
#include "UploadBuffer.h"
#include "Data.h"
#include <memory>

class FrameResourceInterface
{
public:

    FrameResourceInterface();
    FrameResourceInterface(const FrameResourceInterface& rhs) = delete;
    FrameResourceInterface& operator=(const FrameResourceInterface& rhs) = delete;
    ~FrameResourceInterface();

    virtual void CopyConstData(int elementIndex,  void* data)=0;
    virtual void CopyPassData(int elementIndex, void* data) = 0;
    virtual void CopyMaterialData(int elementIndex, void* data) {};

    virtual D3D12_GPU_VIRTUAL_ADDRESS getConstGpuAddress() = 0;
    virtual D3D12_GPU_VIRTUAL_ADDRESS getPassGpuAddress() = 0;
    virtual D3D12_GPU_VIRTUAL_ADDRESS getMaterialGpuAddress();

    // We cannot reset the allocator until the GPU is done processing the commands.
    // So each frame needs their own allocator.
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator>      m_CmdListAlloc;
  
    // Fence value to mark commands up to this fence point.  This lets us
    // check if these frame resources are still in use by the GPU.
    UINT64                                              m_Fence = 0;

};


/*
 With frame resources, we modify our render loop so that we do not have to flush the command queue every frame;
*/

class FrameResource : public FrameResourceInterface
{
public:

	FrameResource(ID3D12Device* device, UINT passCount, UINT objectCount);
	FrameResource(const FrameResource& rhs) = delete;
	FrameResource& operator=(const FrameResource& rhs) = delete;
	~FrameResource();

    virtual void CopyConstData(int elementIndex,  void* data) override;
    virtual void CopyPassData(int elementIndex,  void* data) override;

    virtual D3D12_GPU_VIRTUAL_ADDRESS getConstGpuAddress() override;
    virtual D3D12_GPU_VIRTUAL_ADDRESS getPassGpuAddress() override;

    // We cannot update a cbuffer until the GPU is done processing the commands
    // that reference it.  So each frame needs their own cbuffers.
    std::unique_ptr<UploadBuffer<PassConstants>>           m_PassCB = nullptr;
    std::unique_ptr<UploadBuffer<ObjectConstants>>           m_ObjectCB = nullptr;

};


