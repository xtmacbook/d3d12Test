#include "GSContext.h"
#include "../common/Geometry.h"

using namespace DirectX;
using namespace DirectX::PackedVector;
using Microsoft::WRL::ComPtr;

void GSContext::BuildShadersAndInputLayout()
{
	BlendContext::BuildShadersAndInputLayout();

	BuildGeometryShader();
	m_TreeSpriteInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "SIZE", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};
}

void GSContext::BuildShapeGeometry(ID3D12Device*device, ID3D12GraphicsCommandList* mCommandList)
{
	BlendContext::BuildShapeGeometry(device,mCommandList);
	BuildSprites(device, mCommandList);
}

void GSContext::BuildMaterials()
{
	BlendContext::BuildMaterials();

	auto treeSprites = std::make_unique<Material>();
	treeSprites->Name = "treeSprites";
	treeSprites->MatCBIndex = 3;
	treeSprites->DiffuseSrvHeapIndex = 3;
	treeSprites->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	treeSprites->FresnelR0 = XMFLOAT3(0.01f, 0.01f, 0.01f);
	treeSprites->Roughness = 0.125f;

	m_Materials["treeSprites"] = std::move(treeSprites);
}

void GSContext::BuildRenderItems()
{
	BlendContext::BuildRenderItems();

	auto spritePoint = std::make_unique<RenderItemWithTex>();
	spritePoint->m_ObjCBIndex = 2;
	XMStoreFloat4x4(&spritePoint->m_World, XMMatrixTranslation(3.0f, 2.0f, -9.0f));
	spritePoint->m_Material = m_Materials["treeSprites"].get();
	spritePoint->m_Geo = m_Geometries["treeSpritesGeo"].get();
	spritePoint->m_PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;

	spritePoint->m_IndexCount = spritePoint->m_Geo->m_DrawArgs["box"].m_IndexCount;
	spritePoint->m_StartIndexLocation = spritePoint->m_Geo->m_DrawArgs["box"].m_StartIndexLocation;
	spritePoint->m_BaseVertexLocation = spritePoint->m_Geo->m_DrawArgs["box"].m_BaseVertexLocation;
	m_AllRitems.push_back(std::move(spritePoint));
}

void GSContext::BuildGeometryShader()
{
	const D3D_SHADER_MACRO alphaTestDefines[] =
	{
		"FOG", "1",
		"ALPHA_TEST", "1",
		NULL, NULL
	};

	m_Shaders["treeVS"] = D3DUtil::CompileShader(SourcePath() + L"/Shaders/TreeSprite.hlsl", nullptr, "VS", "vs_5_1");
	m_Shaders["treeGS"] = D3DUtil::CompileShader(SourcePath() + L"/Shaders/TreeSprite.hlsl", nullptr, "PS", "ps_5_1");
	m_Shaders["treePS"] = D3DUtil::CompileShader(SourcePath() + L"/Shaders/TreeSprite.hlsl", alphaTestDefines, "PS", "ps_5_1");
}
