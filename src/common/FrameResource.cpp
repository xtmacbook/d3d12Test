#include "FrameResource.h"

FrameResourceInterface::FrameResourceInterface()
{
}

FrameResourceInterface::~FrameResourceInterface()
{
}

D3D12_GPU_VIRTUAL_ADDRESS FrameResourceInterface::getMaterialGpuAddress()
{
    return D3D12_GPU_VIRTUAL_ADDRESS();
}

FrameResource::FrameResource(ID3D12Device* device, UINT passCount, UINT objectCount):
    FrameResourceInterface()
{
    ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_PPV_ARGS(m_CmdListAlloc.GetAddressOf())));


    m_ObjectCB = std::make_unique<UploadBuffer<ObjectConstants>>(device, objectCount, true);
    m_PassCB = std::make_unique<UploadBuffer<PassConstants>>(device, objectCount, true);
}

FrameResource::~FrameResource()
{
}

void FrameResource::CopyConstData(int elementIndex,  void* data)
{
    ObjectConstants* content = static_cast< ObjectConstants*>(data);
    m_ObjectCB->CopyData(elementIndex, *content);
}

void FrameResource::CopyPassData(int elementIndex,  void* data)
{
    PassConstants* content = static_cast<PassConstants*>(data);
    m_PassCB->CopyData(elementIndex, *content);
}

D3D12_GPU_VIRTUAL_ADDRESS FrameResource::getConstGpuAddress()
{
    return m_ObjectCB->Resource()->GetGPUVirtualAddress();
}

D3D12_GPU_VIRTUAL_ADDRESS FrameResource::getPassGpuAddress()
{
    return m_PassCB->Resource()->GetGPUVirtualAddress();
}
