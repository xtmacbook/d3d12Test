#include "ModelIO.h"
#include "common/Struct.h"
#include "common/Geometry.h"
#include <vector>
#include <fstream>

using namespace DirectX;

std::unique_ptr<MeshGeometry> ModelIO::GetMeshFromTxt(std::string file, DirectX::BoundingBox& bounds)
{
	std::ifstream fin(file);

	if (!fin)
	{
		MessageBox(0, L"not found.", 0, 0);
		return nullptr;
	}

	XMFLOAT3 vMinf3(+MathHelper::Infinity, +MathHelper::Infinity,
		+MathHelper::Infinity);
	XMFLOAT3 vMaxf3(-MathHelper::Infinity, -MathHelper::Infinity,
		-MathHelper::Infinity);

	XMVECTOR vMin = XMLoadFloat3(&vMinf3);
	XMVECTOR vMax = XMLoadFloat3(&vMaxf3);

	UINT vcount = 0;
	UINT tcount = 0;
	std::string ignore;

	fin >> ignore >> vcount;
	fin >> ignore >> tcount;
	fin >> ignore >> ignore >> ignore >> ignore;

	std::vector<VertexNT> vertices(vcount);
	for (UINT i = 0; i < vcount; ++i)
	{
		fin >> vertices[i].Pos.x >> vertices[i].Pos.y >> vertices[i].Pos.z;
		fin >> vertices[i].Normal.x >> vertices[i].Normal.y >> vertices[i].Normal.z;

		vertices[i].TexC = { 0.0f, 0.0f };

		XMVECTOR P = XMLoadFloat3(&vertices[i].Pos);
	}

	fin >> ignore;
	fin >> ignore;
	fin >> ignore;

	std::vector<std::int32_t> indices(3 * tcount);
	for (UINT i = 0; i < tcount; ++i)
		fin >> indices[i * 3 + 0] >> indices[i * 3 + 1] >> indices[i * 3 + 2];

	fin.close();

	//后面这个box需要转到局部坐标系下的box,也可以使用 `oriented bounding box`.

	 
	//it is common for applications to keep a system memory copy around
	 //for things like this, as well as picking, and collision detection.

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(VertexNT);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::int32_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "skullGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->m_VertexBufferCPU));
	CopyMemory(geo->m_VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->m_IndexBufferCPU));
	CopyMemory(geo->m_IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->m_VertexBufferGPU = D3DUtil::CreateDefaultBuffer(m_device,
		m_cmdList, vertices.data(), vbByteSize, geo->m_VertexBufferUploader);

	geo->m_IndexBufferGPU = D3DUtil::CreateDefaultBuffer(m_device,
		m_cmdList, indices.data(), ibByteSize, geo->m_IndexBufferUploader);

	geo->m_VertexByteStride = sizeof(VertexNT);
	geo->m_VertexBufferByteSize = vbByteSize;
	geo->m_IndexFormat = DXGI_FORMAT_R32_UINT;
	geo->m_IndexBufferByteSize = ibByteSize;

	return geo;
}

void ModelIO::init(ID3D12Device* device, ID3D12GraphicsCommandList*cmd)
{
	m_device = device;
	m_cmdList = cmd;


}




