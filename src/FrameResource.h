
#pragma once
#include "common/Util.h"
#include "common/UploadBuffer.h"
#include "Data.h"
#include <memory>

/*
 With frame resources, we modify our render loop so that we do not have to flush the command queue every frame;
*/

struct FrameResource
{
public:

	FrameResource(ID3D12Device* device, UINT passCount, UINT objectCount);
	FrameResource(const FrameResource& rhs) = delete;
	FrameResource& operator=(const FrameResource& rhs) = delete;
	~FrameResource();

    // We cannot reset the allocator until the GPU is done processing the commands.
    // So each frame needs their own allocator.
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator>      m_CmdListAlloc;

    // We cannot update a cbuffer until the GPU is done processing the commands
    // that reference it.  So each frame needs their own cbuffers.
    std::unique_ptr<UploadBuffer<PassConstants>>        m_PassCB = nullptr;
    std::unique_ptr<UploadBuffer<ObjectConstants>>      m_ObjectCB = nullptr;

    // Fence value to mark commands up to this fence point.  This lets us
    // check if these frame resources are still in use by the GPU.
    UINT64                                              m_Fence = 0;

};