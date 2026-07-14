#include "FrameResource.h"

FrameResourceInterface::FrameResourceInterface()
{
}

FrameResourceInterface::~FrameResourceInterface()
{
}

D3D12_GPU_VIRTUAL_ADDRESS FrameResourceInterface::getInstanceGpuAddress()
{
	return D3D12_GPU_VIRTUAL_ADDRESS();
}

D3D12_GPU_VIRTUAL_ADDRESS FrameResourceInterface::getMaterialGpuAddress()
{
	return D3D12_GPU_VIRTUAL_ADDRESS();
}

D3D12_GPU_VIRTUAL_ADDRESS FrameResourceInterface::getWaveGpuAddress()
{
	return D3D12_GPU_VIRTUAL_ADDRESS();
}

ID3D12Resource* FrameResourceInterface::getWaveResouce()
{
	return nullptr;
}

