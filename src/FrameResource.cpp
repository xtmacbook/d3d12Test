#include "FrameResource.h"

FrameResource::FrameResource(ID3D12Device* device, UINT passCount, UINT objectCount)
{
	ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(m_CmdListAlloc.GetAddressOf())));


	m_ObjectCB = std::make_unique<UploadBuffer<ObjectConstants>>(device, objectCount, true);
	m_PassCB = std::make_unique<UploadBuffer<PassConstants>>(device, objectCount, true);
}

FrameResource::~FrameResource()
{
}
