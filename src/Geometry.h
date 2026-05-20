
#pragma once

#include "common/MathHelper.h"
#include "common/Util.h"

#include <unordered_map>
#include <string>


using namespace DirectX;


struct SubmeshGeometry
{
	UINT m_IndexCount = 0;
	UINT m_StartIndexLocation = 0;
	INT  m_BaseVertexLocation = 0;
	// Bounding box of the geometry defined by this submesh. 
	DirectX::BoundingBox m_Bounds;
};

struct MeshGeometry
{
	std::string Name;

	// System memory copies
	Microsoft::WRL::ComPtr<ID3DBlob> m_VertexBufferCPU = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> m_IndexBufferCPU = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> m_VertexBufferGPU = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_IndexBufferGPU = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> m_VertexBufferUploader = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_IndexBufferUploader = nullptr;

	UINT m_VertexByteStride = 0;
	UINT m_VertexBufferByteSize = 0;
	DXGI_FORMAT m_IndexFormat = DXGI_FORMAT_R16_UINT;
	UINT m_IndexBufferByteSize = 0;

	// A MeshGeometry may store multiple geometries in one vertex/index buffer.
	// Use this container to define the Submesh geometries so we can draw
	// the Submeshes individually.
	std::unordered_map<std::string, SubmeshGeometry> m_DrawArgs;

	D3D12_VERTEX_BUFFER_VIEW VertexBufferView()const
	{
		D3D12_VERTEX_BUFFER_VIEW vbv;
		vbv.BufferLocation = m_VertexBufferGPU->GetGPUVirtualAddress();
		vbv.StrideInBytes = m_VertexByteStride;
		vbv.SizeInBytes = m_VertexBufferByteSize;

		return vbv;
	}

	D3D12_INDEX_BUFFER_VIEW IndexBufferView()const
	{
		D3D12_INDEX_BUFFER_VIEW ibv;
		ibv.BufferLocation = m_IndexBufferGPU->GetGPUVirtualAddress();
		ibv.Format = m_IndexFormat;
		ibv.SizeInBytes = m_IndexBufferByteSize;

		return ibv;
	}

	// We can free this memory after we finish upload to the GPU.
	void DisposeUploaders()
	{
		m_VertexBufferUploader = nullptr;
		m_IndexBufferUploader = nullptr;
	}
};


struct BuildGeometry
{

};