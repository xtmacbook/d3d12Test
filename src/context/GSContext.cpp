#include "GSContext.h"
#include "../common/Geometry.h"
#include "../common/d3dx12.h"

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
	spritePoint->m_ObjCBIndex = 3;
	spritePoint->m_World = MathHelper::Identity4x4();
	spritePoint->m_Material = m_Materials["treeSprites"].get();
	spritePoint->m_Geo = m_Geometries["treeSpritesGeo"].get();
	spritePoint->m_PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;
	spritePoint->m_IndexCount = spritePoint->m_Geo->m_DrawArgs["points"].m_IndexCount;
	spritePoint->m_StartIndexLocation = spritePoint->m_Geo->m_DrawArgs["points"].m_StartIndexLocation;
	spritePoint->m_BaseVertexLocation = spritePoint->m_Geo->m_DrawArgs["points"].m_BaseVertexLocation;
	m_AllRitems.push_back(std::move(spritePoint));
}

void GSContext::BuildPSOs()
{
	BlendContext::BuildPSOs();

	D3D12_GRAPHICS_PIPELINE_STATE_DESC spritePsoDesc;
	ZeroMemory(&spritePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));

	spritePsoDesc.InputLayout = { m_TreeSpriteInputLayout.data(), (UINT)m_TreeSpriteInputLayout.size() };
	spritePsoDesc.pRootSignature = m_RootSignature.Get();
	spritePsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(m_SpriteShaders["treeVS"]->GetBufferPointer()),
		m_SpriteShaders["treeVS"]->GetBufferSize()
	};
	spritePsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(m_SpriteShaders["treePS"]->GetBufferPointer()),
		m_SpriteShaders["treePS"]->GetBufferSize()
	};

	spritePsoDesc.GS =
	{
		reinterpret_cast<BYTE*>(m_SpriteShaders["treeGS"]->GetBufferPointer()),
		m_SpriteShaders["treeGS"]->GetBufferSize()
	};

	spritePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	spritePsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	spritePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	spritePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	spritePsoDesc.SampleMask = UINT_MAX;
	spritePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
	spritePsoDesc.NumRenderTargets = 1;
	spritePsoDesc.RTVFormats[0] = m_BackBufferFormat;
	spritePsoDesc.SampleDesc.Count = m_4xMsaaState ? 4 : 1;
	spritePsoDesc.SampleDesc.Quality = m_4xMsaaState ? (m_4xMsaaQuality - 1) : 0;
	spritePsoDesc.DSVFormat = m_DepthStencilFormat;
	ThrowIfFailed(m_d3dDevice->CreateGraphicsPipelineState(&spritePsoDesc, IID_PPV_ARGS(&m_PSOs["treeSpritePso"])));
}

void GSContext::DrawFrameResource(ID3D12CommandAllocator* allocator)
{
	ThrowIfFailed(m_CommandList->Reset(allocator, m_PSOs["opaque"].Get()));
	BEFORE_DRAW_SET;

	ID3D12DescriptorHeap* descriptorHeaps[] = { m_SrvDescriptorHeap.Get(), m_SamplerDescriptorHeap.Get() };
	m_CommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	//set pass buffer
	m_CommandList->SetGraphicsRootConstantBufferView(m_rpi.m_PASS_RootParameterIndex, m_currFrameResource->getPassGpuAddress());

	CD3DX12_GPU_DESCRIPTOR_HANDLE sampler(m_SamplerDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
	m_CommandList->SetGraphicsRootDescriptorTable(4, sampler);

	//land
	DrawRenderItem(m_CommandList.Get(), m_RitemLayer[(int)RenderLayer::Opaque][0]);

	//box
	m_CommandList->SetPipelineState(m_PSOs["alphaTested"].Get());
	DrawRenderItem(m_CommandList.Get(), m_RitemLayer[(int)RenderLayer::AlphaTested][0]);

	//tree sprite
	m_CommandList->SetPipelineState(m_PSOs["treeSpritePso"].Get());
	DrawRenderItem(m_CommandList.Get(), m_AllRitems.back().get());

	//water
	m_CommandList->SetPipelineState(m_PSOs["transparent"].Get());
	DrawRenderItem(m_CommandList.Get(), m_AllRitems[0].get());


	// Indicate a state transition on the resource usage.
	m_CommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	ThrowIfFailed(m_CommandList->Close());
	// Add the command list to the queue for execution.
	ID3D12CommandList* cmdsLists[] = { m_CommandList.Get() };
	m_CommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Swap the back and front buffers
	ThrowIfFailed(m_SwapChain->Present(0, 0));
	m_CurrBackBuffer = (m_CurrBackBuffer + 1) % SwapChainBufferCount;


}

void GSContext::BuildGeometryShader()
{
	const D3D_SHADER_MACRO alphaTestDefines[] =
	{
		"FOG", "1",
		"ALPHA_TEST", "1",
		NULL, NULL
	};

	m_SpriteShaders["treeVS"] = D3DUtil::CompileShader(SourcePath() + L"/Shaders/TreeSprite.hlsl", nullptr, "VS", "vs_5_1");
	m_SpriteShaders["treeGS"] = D3DUtil::CompileShader(SourcePath() + L"/Shaders/TreeSprite.hlsl", nullptr, "GS", "gs_5_1");
	m_SpriteShaders["treePS"] = D3DUtil::CompileShader(SourcePath() + L"/Shaders/TreeSprite.hlsl", alphaTestDefines, "PS", "ps_5_1");
}
