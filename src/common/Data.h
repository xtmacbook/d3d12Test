#pragma once
#include "Util.h"
#include "MathHelper.h"

struct MeshGeometry;

/*
	和每个object有关系,当object的world matrix变化才更新
*/
struct ObjectConstants
{
	DirectX::XMFLOAT4X4 WorldViewProj = MathHelper::Identity4x4();
};

/*
	The Data fixed over a given rendering pass such as 
	the eye position, 
	the view and projection matrices,
	and information about the screen (render target) dimensions; 
	game timing information, which is useful data to have access to in shader programs.

*/

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

/*
 只是每个pass进行更新
*/
struct PassConstants
{
	DirectX::XMFLOAT4X4			m_View = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4			m_InvView = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4			m_Proj = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4			m_InvProj = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4			m_ViewProj = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4			m_InvViewProj = MathHelper::Identity4x4();
	DirectX::XMFLOAT3			m_EyePosW = { 0.0f, 0.0f, 0.0f };
	float						m_cbPerObjectPad1 = 0.0f;
	DirectX::XMFLOAT2			m_RenderTargetSize = { 0.0f, 0.0f };
	DirectX::XMFLOAT2			m_InvRenderTargetSize = { 0.0f, 0.0f };
	float						m_NearZ = 0.0f;
	float						m_FarZ = 0.0f;
	float						m_TotalTime = 0.0f;
	float						m_DeltaTime = 0.0f;
};

struct PassConstantsWithLight : public PassConstants
{
	DirectX::XMFLOAT4			m_AmbientLight = { 0.0f, 0.0f, 0.0f, 1.0f };
	Light						m_Lights[MaxLights];
};

// Lightweight structure stores parameters to draw a shape.  This will
// vary from app-to-app.
struct RenderItem
{
	RenderItem() = default;

	// World matrix of the shape that describes the object's local space
	// relative to the world space, which defines the position, orientation,
	// and scale of the object in the world.
	DirectX::XMFLOAT4X4				m_World = MathHelper::Identity4x4();

	// Dirty flag indicating the object data has changed and we need to update the constant buffer.
	// Because we have an object cbuffer for each FrameResource, we have to apply the
	// update to each FrameResource.  Thus, when we modify obect data we should set 
	// NumFramesDirty = gNumFrameResources so that each frame resource gets the update.
	int								m_NumFramesDirty = 3;

	// Index into GPU constant buffer corresponding to the ObjectCB for this render item.
	UINT							m_ObjCBIndex = -1;

	MeshGeometry*					m_Geo = nullptr;

	// Primitive topology.
	D3D12_PRIMITIVE_TOPOLOGY		m_PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	// DrawIndexedInstanced parameters.
	UINT							m_IndexCount = 0;
	UINT							m_StartIndexLocation = 0;
	int								m_BaseVertexLocation = 0;
};

struct RenderItemWithMaterial : public RenderItem
{
	DirectX::XMFLOAT4X4 m_TexTransform = MathHelper::Identity4x4();

	Material* m_Material = nullptr;
};