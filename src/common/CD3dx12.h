
#include <d3d12.h>

struct CD3DX12_CPU_DESCRIPTOR_HANDLE : public D3D12_CPU_DESCRIPTOR_HANDLE
{

    CD3DX12_CPU_DESCRIPTOR_HANDLE& Offset(INT offsetInDescriptors, UINT descriptorIncrementSize)
    {
        ptr += offsetInDescriptors * descriptorIncrementSize;
        return *this;
    }
};