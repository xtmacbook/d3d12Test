
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

	virtual void CopyConstData(int elementIndex, void* data) = 0;
	virtual void CopyPassData(int elementIndex, void* data) = 0;
	virtual void CopyMaterialData(int elementIndex, void* data) {};
	virtual void CopyWaveData(int elementIndex, void* data) {};
 
	virtual D3D12_GPU_VIRTUAL_ADDRESS getConstGpuAddress() = 0;
	virtual D3D12_GPU_VIRTUAL_ADDRESS getPassGpuAddress() = 0;
	virtual D3D12_GPU_VIRTUAL_ADDRESS getMaterialGpuAddress();
	virtual D3D12_GPU_VIRTUAL_ADDRESS getWaveGpuAddress();

	virtual ID3D12Resource* getWaveResouce();

	Microsoft::WRL::ComPtr<ID3D12CommandAllocator>      m_CmdListAlloc;

	// Fence value to mark commands up to this fence point.  This lets us
	// check if these frame resources are still in use by the GPU.
	UINT64                                              m_Fence = 0;

};


/*
 With frame resources, we modify our render loop so that we do not have to flush the command queue every frame;
*/
template <typename OBJECTCONST ,typename PASSCONST>
class FrameResource : public FrameResourceInterface
{
public:

	FrameResource(ID3D12Device* device, UINT passCount, UINT objectCount);
	FrameResource(const FrameResource& rhs) = delete;
	FrameResource& operator=(const FrameResource& rhs) = delete;
	~FrameResource();

	virtual void CopyConstData(int elementIndex, void* data) override;
	virtual void CopyPassData(int elementIndex, void* data) override;

	virtual D3D12_GPU_VIRTUAL_ADDRESS getConstGpuAddress() override;
	virtual D3D12_GPU_VIRTUAL_ADDRESS getPassGpuAddress() override;

	std::unique_ptr<UploadBuffer<PASSCONST>>           m_PassCB = nullptr;
	std::unique_ptr<UploadBuffer<OBJECTCONST>>         m_ObjectCB = nullptr;

};

template<typename OBJECTCONST, typename PASSCONST>
inline FrameResource<OBJECTCONST, PASSCONST>::FrameResource(ID3D12Device* device, UINT passCount, UINT objectCount)
{
	ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(m_CmdListAlloc.GetAddressOf())));


	m_ObjectCB = std::make_unique<UploadBuffer<OBJECTCONST>>(device, objectCount, true);
	m_PassCB = std::make_unique<UploadBuffer<PASSCONST>>(device, objectCount, true);
}

template<typename OBJECTCONST, typename PASSCONST>
inline FrameResource<OBJECTCONST, PASSCONST>::~FrameResource()
{
}

template<typename OBJECTCONST, typename PASSCONST>
inline void FrameResource<OBJECTCONST, PASSCONST>::CopyConstData(int elementIndex, void* data)
{
	OBJECTCONST* content = static_cast<OBJECTCONST*>(data);
	m_ObjectCB->CopyData(elementIndex, *content);
}

template<typename OBJECTCONST, typename PASSCONST>
inline void FrameResource<OBJECTCONST, PASSCONST>::CopyPassData(int elementIndex, void* data)
{
	PASSCONST* content = static_cast<PASSCONST*>(data);
	m_PassCB->CopyData(elementIndex, *content);
}

template<typename OBJECTCONST, typename PASSCONST>
inline D3D12_GPU_VIRTUAL_ADDRESS FrameResource<OBJECTCONST, PASSCONST>::getConstGpuAddress()
{
	return m_ObjectCB->Resource()->GetGPUVirtualAddress();
}

template<typename OBJECTCONST, typename PASSCONST>
inline D3D12_GPU_VIRTUAL_ADDRESS FrameResource<OBJECTCONST, PASSCONST>::getPassGpuAddress()
{
	return m_PassCB->Resource()->GetGPUVirtualAddress();
}

template <typename OBJECTCONST, typename PASSCONST,typename MATERIALCONST>
class FrameResourceWithMaterial : public FrameResource<OBJECTCONST,PASSCONST>
{
public:

	FrameResourceWithMaterial(ID3D12Device* device, UINT passCount, UINT objectCount,
		UINT materialCount);

	FrameResourceWithMaterial(const FrameResourceWithMaterial& rhs) = delete;
	FrameResourceWithMaterial& operator=(const FrameResourceWithMaterial& rhs) = delete;
	~FrameResourceWithMaterial() {};

	virtual void CopyMaterialData(int elementIndex, void* data)
	{
		MATERIALCONST* content = static_cast<MATERIALCONST*>(data);
		m_MaterialCB->CopyData(elementIndex, *content);
	}

	virtual D3D12_GPU_VIRTUAL_ADDRESS getMaterialGpuAddress() override
	{
		return m_MaterialCB->Resource()->GetGPUVirtualAddress();
	}

	std::unique_ptr<UploadBuffer<MATERIALCONST>>			m_MaterialCB = nullptr;

};

template<typename OBJECTCONST, typename PASSCONST, typename MATERIALCONST>
inline FrameResourceWithMaterial<OBJECTCONST, PASSCONST, MATERIALCONST>::
FrameResourceWithMaterial(ID3D12Device* device, UINT passCount, UINT objectCount, UINT materialCount):
	FrameResource< OBJECTCONST, PASSCONST>(device,passCount,objectCount)
{
	m_MaterialCB = std::make_unique<UploadBuffer<MATERIALCONST>>(device, materialCount, true);
}
