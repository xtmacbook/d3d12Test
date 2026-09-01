#pragma once
#include "Util.h"

#include <vector>
#include <memory>

struct MeshGeometry;
struct SubmeshGeometry;
struct InstanceData;

struct Material
{
	// Unique material name for lookup.
	std::string							Name;

	// Index into constant buffer corresponding to this material.
	int									MaterialCBIndex = -1;

	// Index into SRV heap for diffuse texture.
	int									DiffuseSrvHeapIndex = -1;

	// Index into SRV heap for normal texture.
	int									NormalSrvHeapIndex = -1;

	// Dirty flag indicating the material has changed and we need to update the constant buffer.
	// Because we have a material constant buffer for each FrameResource, we have to apply the
	// update to each FrameResource.  Thus, when we modify a material we should set 
	// NumFramesDirty = gNumFrameResources so that each frame resource gets the update.
	int									NumFramesDirty = 3;

	// Material constant buffer data used for shading.
	DirectX::XMFLOAT4   DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
	DirectX::XMFLOAT3   FresnelR0 = { 0.01f, 0.01f, 0.01f };
	float               Roughness = .25f; //0-1,0:代表完全光滑 shininess = 1 – roughness
};

struct MaterialWithTexTran : public Material
{
	DirectX::XMFLOAT4X4 MatTransform = MathHelper::Identity4x4();
};


struct PRBLight
{
	DirectX::XMFLOAT3		Strength = { 0.5f, 0.5f, 0.5f };
	float					FalloffStart = 1.0f;                          // point/spot light only
	DirectX::XMFLOAT3		Direction = { 0.0f, -1.0f, 0.0f };// directional/spot light only
	float					FalloffEnd = 10.0f;                           // point/spot light only
	DirectX::XMFLOAT3		Position = { 0.0f, 0.0f, 0.0f };  // point/spot light only
	float					SpotPower = 64.0f;                            // spot light only
};

struct Light
{
	DirectX::XMFLOAT4 lightDirection;
	DirectX::XMFLOAT4 lightSpecularColor = DirectX::XMFLOAT4{ 0.0f, 0.0f, 0.0f,0.f };
	DirectX::XMFLOAT4 lightDiffuseColor = DirectX::XMFLOAT4{ 0.0f, 0.0f, 0.0f, 0.0f };
};

struct VertexS
{
	DirectX::XMFLOAT3 Pos;
	DirectX::XMFLOAT2 Size;
};

struct VertexC
{
	DirectX::XMFLOAT3 Pos;
	DirectX::XMFLOAT4 Color;
};

struct VertexN
{
	DirectX::XMFLOAT3 Pos;
	DirectX::XMFLOAT3 Normal;
};

struct VertexNT
{
	VertexNT() = default;
	VertexNT(float x, float y, float z, float nx, float ny, float nz, float u, float v) :
		Pos(x, y, z),
		Normal(nx, ny, nz),
		TexC(u, v) {}

	DirectX::XMFLOAT3 Pos;
	DirectX::XMFLOAT3 Normal;
	DirectX::XMFLOAT2 TexC;
};

struct VertexNTU
{
	DirectX::XMFLOAT3 Pos;
	DirectX::XMFLOAT3 Normal;
	DirectX::XMFLOAT2 TexC;
	DirectX::XMFLOAT3 TangentU;
};

struct Texture
{
	std::string m_Name;
	std::wstring m_Filename;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_Resource = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_UploadHeap = nullptr;
};

// Lightweight structure stores parameters to draw a shape.  This will
// vary from app-to-app.
struct RenderItem
{
	RenderItem() = default;

	DirectX::XMFLOAT4X4				m_World = MathHelper::Identity4x4();

	// Dirty flag indicating the object data has changed and we need to update the constant buffer.
	// Because we have an object cbuffer for each FrameResource, we have to apply the
	// update to each FrameResource.  Thus, when we modify obect data we should set 
	// NumFramesDirty = gNumFrameResources so that each frame resource gets the update.
	int								m_NumFramesDirty = 3;

	// Index into GPU constant buffer corresponding to the ObjectCB for this render item.
	UINT							m_ObjCBIndex = -1;

	MeshGeometry*					m_Geo = nullptr;

	D3D12_PRIMITIVE_TOPOLOGY		m_PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	UINT							m_IndexCount = 0;
	UINT							m_StartIndexLocation = 0;
	int								m_BaseVertexLocation = 0;

	std::vector<std::shared_ptr<InstanceData> >	m_Instances;
	UINT										m_InstanceCount = 0;

	DirectX::BoundingBox						m_Bounds;


	void FillWithDrawArgs(const SubmeshGeometry*);

};

struct RenderItemWithMaterial : public RenderItem
{
	Material*				m_Material = nullptr;
};

struct RenderItemWithTex : public RenderItemWithMaterial
{
	DirectX::XMFLOAT4X4		m_TexTransform = MathHelper::Identity4x4();
};


