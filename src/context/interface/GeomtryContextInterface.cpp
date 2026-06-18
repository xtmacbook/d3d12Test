#include "GeomtryContextInterface.h"
#include "../../common/GeometryGenerator.h"
#include "../../common/Geometry.h"
#include "../../common/GameTimer.h"
#include "../../common/data.h"
#include "../Waves.h"

#include <fstream>

using namespace DirectX;
using namespace DirectX::PackedVector;
using Microsoft::WRL::ComPtr;

GeometryContextInterface::GeometryContextInterface()
{
	m_Waves = std::make_unique<Waves>(128, 128, 1.0f, 0.03f, 4.0f, 0.2f);
}

void GeometryContextInterface::BuildShapeGeometry(ID3D12Device* md3dDevice, ID3D12GraphicsCommandList* mCommandList)
{
	BuildBox(md3dDevice,mCommandList,BoxProfile());
}

void GeometryContextInterface::updateGeometry(const GameTimer& gt)
{
	if (m_Waves) updateWave(gt);
}

void GeometryContextInterface::BuildBox(ID3D12Device* md3dDevice, ID3D12GraphicsCommandList* mCommandList, BoxProfile profile)
{
	GeometryGenerator geoGen;
	GeometryGenerator::MeshData box = geoGen.CreateBox(profile.width,profile.height,profile.depth , 3);

	SubmeshGeometry boxSubmesh;
	boxSubmesh.m_IndexCount = (UINT)box.Indices32.size();
	boxSubmesh.m_StartIndexLocation = 0;
	boxSubmesh.m_BaseVertexLocation = 0;


	std::vector<VertexNT> vertices(box.Vertices.size());

	for (size_t i = 0; i < box.Vertices.size(); ++i)
	{
		vertices[i].Pos = box.Vertices[i].Position;
		vertices[i].Normal = box.Vertices[i].Normal;
		vertices[i].TexC = box.Vertices[i].TexC;
	}

	std::vector<std::uint16_t> indices = box.GetIndices16();

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(VertexNT);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "boxGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->m_VertexBufferCPU));
	CopyMemory(geo->m_VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->m_IndexBufferCPU));
	CopyMemory(geo->m_IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->m_VertexBufferGPU = D3DUtil::CreateDefaultBuffer(md3dDevice,
		mCommandList, vertices.data(), vbByteSize, geo->m_VertexBufferUploader);

	geo->m_IndexBufferGPU = D3DUtil::CreateDefaultBuffer(md3dDevice,
		mCommandList, indices.data(), ibByteSize, geo->m_IndexBufferUploader);

	geo->m_VertexByteStride = sizeof(VertexNT);
	geo->m_VertexBufferByteSize = vbByteSize;
	geo->m_IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->m_IndexBufferByteSize = ibByteSize;

	geo->m_DrawArgs["box"] = boxSubmesh;
	m_Geometries[geo->Name] = std::move(geo);
}

float GetHillsHeight(float x, float z) 
{
	return 0.3f * (z * sinf(0.1f * x) + x * cosf(0.1f * z));
}

DirectX::XMFLOAT3 GetHillsNormal(float x, float z)
{
	// n = (-df/dx, 1, -df/dz)
	DirectX::XMFLOAT3 n(
		-0.03f * z * cosf(0.1f * x) - 0.3f * cosf(0.1f * z),
		1.0f,
		-0.3f * sinf(0.1f * x) + 0.03f * x * sinf(0.1f * z));

	XMVECTOR unitNormal = XMVector3Normalize(XMLoadFloat3(&n));
	XMStoreFloat3(&n, unitNormal);

	return n;
}

void GeometryContextInterface::BuildLand(ID3D12Device* md3dDevice, ID3D12GraphicsCommandList* mCommandList)
{
	GeometryGenerator geoGen;
	GeometryGenerator::MeshData grid = geoGen.CreateGrid(160.0f, 160.0f, 50, 50);

	std::vector<VertexNT> vertices(grid.Vertices.size());
	for (size_t i = 0; i < grid.Vertices.size(); ++i)
	{
		auto& p = grid.Vertices[i].Position;
		vertices[i].Pos = p;
		vertices[i].Pos.y = GetHillsHeight(p.x, p.z);
		vertices[i].Normal = GetHillsNormal(p.x, p.z);
		vertices[i].TexC = grid.Vertices[i].TexC;
	}

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(VertexNT);

	std::vector<std::uint16_t> indices = grid.GetIndices16();
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "landGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->m_VertexBufferCPU));
	CopyMemory(geo->m_VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->m_IndexBufferCPU));
	CopyMemory(geo->m_IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->m_VertexBufferGPU = D3DUtil::CreateDefaultBuffer(md3dDevice,
		mCommandList, vertices.data(), vbByteSize, geo->m_VertexBufferUploader);

	geo->m_IndexBufferGPU = D3DUtil::CreateDefaultBuffer(md3dDevice,
		mCommandList, indices.data(), ibByteSize, geo->m_IndexBufferUploader);

	geo->m_VertexByteStride = sizeof(VertexNT);
	geo->m_VertexBufferByteSize = vbByteSize;
	geo->m_IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->m_IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.m_IndexCount = (UINT)indices.size();
	submesh.m_StartIndexLocation = 0;
	submesh.m_BaseVertexLocation = 0;

	geo->m_DrawArgs["grid"] = submesh;

	m_Geometries["landGeo"] = std::move(geo);
}

void GeometryContextInterface::BuildWave(ID3D12Device* md3dDevice, ID3D12GraphicsCommandList* mCommandList)
{
	std::vector<std::uint16_t> indices(3 * m_Waves->TriangleCount()); // 3 indices per face

	// Iterate over each quad.
	int m = m_Waves->RowCount();
	int n = m_Waves->ColumnCount();
	int k = 0;
	for (int i = 0; i < m - 1; ++i)
	{
		for (int j = 0; j < n - 1; ++j)
		{
			indices[k] = i * n + j;
			indices[k + 1] = i * n + j + 1;
			indices[k + 2] = (i + 1) * n + j;

			indices[k + 3] = (i + 1) * n + j;
			indices[k + 4] = i * n + j + 1;
			indices[k + 5] = (i + 1) * n + j + 1;

			k += 6; // next quad
		}
	}

	UINT vbByteSize = m_Waves->VertexCount() * sizeof(VertexNT);
	UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "waterGeo";

	// Set dynamically.
	geo->m_VertexBufferCPU = nullptr;
	geo->m_VertexBufferGPU = nullptr;

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->m_IndexBufferCPU));
	CopyMemory(geo->m_IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->m_IndexBufferGPU = D3DUtil::CreateDefaultBuffer(md3dDevice,
		mCommandList, indices.data(), ibByteSize, geo->m_IndexBufferUploader);

	geo->m_VertexByteStride = sizeof(VertexNT);
	geo->m_VertexBufferByteSize = vbByteSize;
	geo->m_IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->m_IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.m_IndexCount = (UINT)indices.size();
	submesh.m_StartIndexLocation = 0;
	submesh.m_BaseVertexLocation = 0;

	geo->m_DrawArgs["grid"] = submesh;

	m_Geometries["waterGeo"] = std::move(geo);
}

void GeometryContextInterface::BuildSkull(ID3D12Device*device, ID3D12GraphicsCommandList* mCommandList)
{
	std::ifstream fin(SourcePath() + L"/Models/skull.txt");

	if (!fin)
	{
		MessageBox(0, L"Models/skull.txt not found.", 0, 0);
		return;
	}

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

		// Model does not have texture coordinates, so just zero them out.
		vertices[i].TexC = { 0.0f, 0.0f };
	}

	fin >> ignore;
	fin >> ignore;
	fin >> ignore;

	std::vector<std::int32_t> indices(3 * tcount);
	for (UINT i = 0; i < tcount; ++i)
	{
		fin >> indices[i * 3 + 0] >> indices[i * 3 + 1] >> indices[i * 3 + 2];
	}

	fin.close();

	//
	// Pack the indices of all the meshes into one index buffer.
	//

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(VertexNT);

	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::int32_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "skullGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->m_VertexBufferCPU));
	CopyMemory(geo->m_VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->m_IndexBufferCPU));
	CopyMemory(geo->m_IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->m_VertexBufferGPU = D3DUtil::CreateDefaultBuffer(device,
		mCommandList, vertices.data(), vbByteSize, geo->m_VertexBufferUploader);

	geo->m_IndexBufferGPU = D3DUtil::CreateDefaultBuffer(device,
		mCommandList, indices.data(), ibByteSize, geo->m_IndexBufferUploader);

	geo->m_VertexByteStride = sizeof(VertexNT);
	geo->m_VertexBufferByteSize = vbByteSize;
	geo->m_IndexFormat = DXGI_FORMAT_R32_UINT;
	geo->m_IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.m_IndexCount = (UINT)indices.size();
	submesh.m_StartIndexLocation = 0;
	submesh.m_BaseVertexLocation = 0;

	geo->m_DrawArgs["skull"] = submesh;

	m_Geometries[geo->Name] = std::move(geo);
}

void GeometryContextInterface::BuildMirror(ID3D12Device*device, ID3D12GraphicsCommandList* mCommandList)
{
	// Create and specify geometry.  For this sample we draw a floor
// and a wall with a mirror on it.  We put the floor, wall, and
// mirror geometry in one vertex buffer.
//
//   |--------------|
//   |              |
//   |----|----|----|
//   |Wall|Mirr|Wall|
//   |    | or |    |
//   /--------------/
//  /   Floor      /
// /--------------/

	std::array<VertexNT, 20> vertices =
	{
		// Floor: Observe we tile texture coordinates.
		VertexNT(-3.5f, 0.0f, -10.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f), // 0 
		VertexNT(-3.5f, 0.0f,   0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f),
		VertexNT(7.5f, 0.0f,   0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f),
		VertexNT(7.5f, 0.0f, -10.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f),

		// Wall: Observe we tile texture coordinates, and that we
		// leave a gap in the middle for the mirror.
		VertexNT(-3.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 2.0f), // 4
		VertexNT(-3.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f),
		VertexNT(-2.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.5f, 0.0f),
		VertexNT(-2.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.5f, 2.0f),

		VertexNT(2.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 2.0f), // 8 
		VertexNT(2.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f),
		VertexNT(7.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 2.0f, 0.0f),
		VertexNT(7.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 2.0f, 2.0f),

		VertexNT(-3.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f), // 12
		VertexNT(-3.5f, 6.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f),
		VertexNT(7.5f, 6.0f, 0.0f, 0.0f, 0.0f, -1.0f, 6.0f, 0.0f),
		VertexNT(7.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 6.0f, 1.0f),

		// Mirror
		VertexNT(-2.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f), // 16
		VertexNT(-2.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f),
		VertexNT(2.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f),
		VertexNT(2.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f)
	};

	std::array<std::int16_t, 30> indices =
	{
		// Floor
		0, 1, 2,
		0, 2, 3,

		// Walls
		4, 5, 6,
		4, 6, 7,

		8, 9, 10,
		8, 10, 11,

		12, 13, 14,
		12, 14, 15,

		// Mirror
		16, 17, 18,
		16, 18, 19
	};

	SubmeshGeometry floorSubmesh;
	floorSubmesh.m_IndexCount = 6;
	floorSubmesh.m_StartIndexLocation = 0;
	floorSubmesh.m_BaseVertexLocation = 0;

	SubmeshGeometry wallSubmesh;
	wallSubmesh.m_IndexCount = 18;
	wallSubmesh.m_StartIndexLocation = 6;
	wallSubmesh.m_BaseVertexLocation = 0;

	SubmeshGeometry mirrorSubmesh;
	mirrorSubmesh.m_IndexCount = 6;
	mirrorSubmesh.m_StartIndexLocation = 24;
	mirrorSubmesh.m_BaseVertexLocation = 0;

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(VertexNT);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "roomGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->m_VertexBufferCPU));
	CopyMemory(geo->m_VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->m_IndexBufferCPU));
	CopyMemory(geo->m_IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->m_VertexBufferGPU = D3DUtil::CreateDefaultBuffer(device,
		mCommandList, vertices.data(), vbByteSize, geo->m_VertexBufferUploader);

	geo->m_IndexBufferGPU = D3DUtil::CreateDefaultBuffer(device,
		mCommandList, indices.data(), ibByteSize, geo->m_IndexBufferUploader);

	geo->m_VertexByteStride = sizeof(VertexNT);
	geo->m_VertexBufferByteSize = vbByteSize;
	geo->m_IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->m_IndexBufferByteSize = ibByteSize;

	geo->m_DrawArgs["floor"] = floorSubmesh;
	geo->m_DrawArgs["wall"] = wallSubmesh;
	geo->m_DrawArgs["mirror"] = mirrorSubmesh;

	m_Geometries[geo->Name] = std::move(geo);
}

Waves* GeometryContextInterface::GetWave()
{
	if(m_Waves)
		return m_Waves.get();
	return nullptr;
}

void GeometryContextInterface::updateWave(const GameTimer& gt)
{
	static float t_base = 0.0f;
	if ((gt.TotalTime() - t_base) >= 0.25f)
	{
		t_base += 0.25f;

		int i = MathHelper::Rand(4, m_Waves->RowCount() - 5);
		int j = MathHelper::Rand(4, m_Waves->ColumnCount() - 5);

		float r = MathHelper::RandF(0.2f, 0.5f);

		m_Waves->Disturb(i, j, r);
	}

	// Update the wave simulation.
	m_Waves->Update(gt.DeltaTime());
}
