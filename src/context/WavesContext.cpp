#include "WavesContext.h"
#include "../common/GeometryGenerator.h"
#include "../common/Geometry.h"
#include "../common/App.h"
#include "../common/d3dx12.h"

using namespace DirectX;
using namespace DirectX::PackedVector;
using Microsoft::WRL::ComPtr;

class FrameResourceWithLight : public FrameResourceInterface
{
public:

	FrameResourceWithLight(ID3D12Device* device, UINT passCount, UINT objectCount,
		UINT materialCount, UINT waveVertCount)
	{
		ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
			IID_PPV_ARGS(m_CmdListAlloc.GetAddressOf())));


		m_ObjectCB = std::make_unique<UploadBuffer<ObjectConstants>>(device, objectCount, true);
		m_PassCB = std::make_unique<UploadBuffer<PassConstantsWithLight>>(device, objectCount, true);
		m_MaterialCB = std::make_unique<UploadBuffer<MaterialConstants>>(device, materialCount, true);
		m_WavesVB = std::make_unique<UploadBuffer<VertexN>>(device, waveVertCount, false);

	}

	FrameResourceWithLight(const FrameResourceWithLight& rhs) = delete;
	FrameResourceWithLight& operator=(const FrameResourceWithLight& rhs) = delete;
	~FrameResourceWithLight()
	{

	}



	virtual void CopyConstData(int elementIndex, void* data) override
	{
		ObjectConstants* content = static_cast<ObjectConstants*>(data);
		m_ObjectCB->CopyData(elementIndex, *content);
	}
	virtual void CopyPassData(int elementIndex, void* data) override
	{
		PassConstantsWithLight* content = static_cast<PassConstantsWithLight*>(data);
		m_PassCB->CopyData(elementIndex, *content);
	}
	virtual void CopyMaterialData(int elementIndex, void* data) {
		MaterialConstants* content = static_cast<MaterialConstants*>(data);
		m_MaterialCB->CopyData(elementIndex, *content);
	};

	virtual D3D12_GPU_VIRTUAL_ADDRESS getConstGpuAddress() override
	{
		return m_ObjectCB->Resource()->GetGPUVirtualAddress();
	}
	virtual D3D12_GPU_VIRTUAL_ADDRESS getPassGpuAddress() override
	{
		return m_PassCB->Resource()->GetGPUVirtualAddress();
	}
	virtual D3D12_GPU_VIRTUAL_ADDRESS getMaterialGpuAddress() override
	{
		return m_WavesVB->Resource()->GetGPUVirtualAddress();
	}

	std::unique_ptr<UploadBuffer<PassConstantsWithLight>>    m_PassCB = nullptr;
	std::unique_ptr<UploadBuffer<ObjectConstants>>           m_ObjectCB = nullptr;
	std::unique_ptr<UploadBuffer<MaterialConstants>>		 m_MaterialCB = nullptr;
	std::unique_ptr<UploadBuffer<VertexN>>					 m_WavesVB = nullptr;


};


bool WavesContext::InitDirect3D()
{
	if (!D3DContext::InitDirect3D()) return false;

	// Reset the command list to prep for initialization commands.
	ThrowIfFailed(m_CommandList->Reset(m_DirectCmdListAlloc.Get(), nullptr));

	m_Waves = std::make_unique<Waves>(128, 128, 1.0f, 0.03f, 4.0f, 0.2f);

	BuildGeometry();
	BuildMaterial();
	BuildRenderItems();
	BuildFrameResources();
	BuildShadersAndInputLayout();
	BuildRootSignature();
	BuildPSO();
	
	// Execute the initialization commands.
	ThrowIfFailed(m_CommandList->Close());
	ID3D12CommandList* cmdsLists[] = { m_CommandList.Get() };
	m_CommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Wait until initialization is complete.
	FlushCommandQueue();

	return true;
}

void WavesContext::DrawFrameResource(ID3D12CommandAllocator*allocator)
{
	ThrowIfFailed(m_CommandList->Reset(allocator, m_PSOs["opaque"].Get()));
	BEFORE_DRAW_SET;

	/*m_CommandList->RSSetViewports(1, &m_ScreenViewport); 
	m_CommandList->RSSetScissorRects(1, &m_ScissorRect); 
	m_CommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET)); 
	m_CommandList->ClearRenderTargetView(CurrentCPUBackBufferView(), Colors::LightSteelBlue, 0, nullptr); 
	m_CommandList->ClearDepthStencilView(DepthStencilCPUView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr); 
	m_CommandList->OMSetRenderTargets(1, &CurrentCPUBackBufferView(), true, &DepthStencilCPUView());
	m_CommandList->SetGraphicsRootSignature(m_RootSignature.Get());*/

	m_CommandList->SetGraphicsRootConstantBufferView(2, m_currFrameResource->getPassGpuAddress());

	DrawRenderItems(m_CommandList.Get(), m_RitemLayer[(int)RenderLayer::Opaque]);

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

void WavesContext::Update(const GameTimer& gt)
{
	FrameResourceContextInterface::Update(gt);

	UpdateObjectCBS(gt);
	UpdateMaterialCBs(gt);
	UpdateMainPassCB(gt);
	UpdateWaves(gt);
}

void WavesContext::BuildShadersAndInputLayout()
{
	m_Shaders["standardVS"] = D3DUtil::CompileShader(L"Shaders\\Waves.hlsl", nullptr, "VS", "vs_5_1");
	m_Shaders["opaquePS"] = D3DUtil::CompileShader(L"Shaders\\Waves.hlsl", nullptr, "PS", "ps_5_1");

	m_InputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};
}

void WavesContext::BuildFrameResources()
{
	for (int i = 0; i < m_NumFrameResources; i++)
		m_frameResources.emplace_back(std::make_unique<FrameResourceWithLight>(m_d3dDevice.Get(), 
			1, m_AllRitems.size(), (UINT)m_Materials.size(), m_Waves->VertexCount()));
}

void WavesContext::BuildGeometry()
{
	//land Geo
	GeometryGenerator geoGen;
	GeometryGenerator::MeshData grid = geoGen.CreateGrid(160.0f, 160.0f, 50, 50);

	std::vector<VertexN> vertices(grid.Vertices.size());
	for (size_t i = 0; i < grid.Vertices.size(); ++i)
	{
		auto& p = grid.Vertices[i].Position;
		vertices[i].Pos = p;
		vertices[i].Pos.y = GetHillsHeight(p.x, p.z);
		vertices[i].Normal = GetHillsNormal(p.x, p.z);
	}

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(VertexN);

	std::vector<std::uint16_t> indices = grid.GetIndices16();
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "landGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->m_VertexBufferCPU));
	CopyMemory(geo->m_VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->m_IndexBufferCPU));
	CopyMemory(geo->m_IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->m_VertexBufferGPU = D3DUtil::CreateDefaultBuffer(m_d3dDevice.Get(),
		m_CommandList.Get(), vertices.data(), vbByteSize, geo->m_VertexBufferUploader);

	geo->m_IndexBufferGPU = D3DUtil::CreateDefaultBuffer(m_d3dDevice.Get(),
		m_CommandList.Get(), indices.data(), ibByteSize, geo->m_IndexBufferUploader);

	geo->m_VertexByteStride = sizeof(VertexN);
	geo->m_VertexBufferByteSize = vbByteSize;
	geo->m_IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->m_IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.m_IndexCount = (UINT)indices.size();
	submesh.m_StartIndexLocation = 0;
	submesh.m_BaseVertexLocation = 0;

	geo->m_DrawArgs["grid"] = submesh;

	m_Geometries["landGeo"] = std::move(geo);

	//wave Geo
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

		UINT vbByteSize = m_Waves->VertexCount() * sizeof(VertexN);
		UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

		auto geo = std::make_unique<MeshGeometry>();
		geo->Name = "waterGeo";

		// Set dynamically.
		geo->m_VertexBufferCPU = nullptr;
		geo->m_VertexBufferGPU = nullptr;

		ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->m_IndexBufferCPU));
		CopyMemory(geo->m_IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

		geo->m_IndexBufferGPU = D3DUtil::CreateDefaultBuffer(m_d3dDevice.Get(),
			m_CommandList.Get(), indices.data(), ibByteSize, geo->m_IndexBufferUploader);

		geo->m_VertexByteStride = sizeof(VertexN);
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
}

void WavesContext::BuildRenderItems()
{
	auto wavesRitem = std::make_unique<RenderItemWithMaterial>();
	wavesRitem->m_World = MathHelper::Identity4x4();
	wavesRitem->m_ObjCBIndex = 0;
	wavesRitem->m_Material = m_Materials["water"].get();
	wavesRitem->m_Geo = m_Geometries["waterGeo"].get();
	wavesRitem->m_PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	wavesRitem->m_IndexCount = wavesRitem->m_Geo->m_DrawArgs["grid"].m_IndexCount;
	wavesRitem->m_StartIndexLocation = wavesRitem->m_Geo->m_DrawArgs["grid"].m_StartIndexLocation;
	wavesRitem->m_BaseVertexLocation = wavesRitem->m_Geo->m_DrawArgs["grid"].m_BaseVertexLocation;

	m_WavesRitem = wavesRitem.get();

	m_RitemLayer[(int)RenderLayer::Opaque].push_back(wavesRitem.get());

	auto gridRitem = std::make_unique<RenderItemWithMaterial>();
	gridRitem->m_World = MathHelper::Identity4x4();
	gridRitem->m_ObjCBIndex = 1;
	gridRitem->m_Material = m_Materials["grass"].get();
	gridRitem->m_Geo = m_Geometries["landGeo"].get();
	gridRitem->m_PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	gridRitem->m_IndexCount = gridRitem->m_Geo->m_DrawArgs["grid"].m_IndexCount;
	gridRitem->m_StartIndexLocation = gridRitem->m_Geo->m_DrawArgs["grid"].m_StartIndexLocation;
	gridRitem->m_BaseVertexLocation = gridRitem->m_Geo->m_DrawArgs["grid"].m_BaseVertexLocation;

	m_RitemLayer[(int)RenderLayer::Opaque].push_back(gridRitem.get());

	m_AllRitems.push_back(std::move(wavesRitem));
	m_AllRitems.push_back(std::move(gridRitem));
}

void WavesContext::BuildMaterial()
{
	auto grass = std::make_unique<Material>();
	grass->Name = "grass";
	grass->MatCBIndex = 0;
	grass->DiffuseAlbedo = XMFLOAT4(0.2f, 0.6f, 0.2f, 1.0f);
	grass->FresnelR0 = XMFLOAT3(0.01f, 0.01f, 0.01f);
	grass->Roughness = 0.125f;

	// This is not a good water material definition, but we do not have all the rendering
	// tools we need (transparency, environment reflection), so we fake it for now.
	auto water = std::make_unique<Material>();
	water->Name = "water";
	water->MatCBIndex = 1;
	water->DiffuseAlbedo = XMFLOAT4(0.0f, 0.2f, 0.6f, 1.0f);
	water->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
	water->Roughness = 0.0f;

	m_Materials["grass"] = std::move(grass);
	m_Materials["water"] = std::move(water);
}

void WavesContext::BuildRootSignature()
{
	D3D12_ROOT_PARAMETER slotRootParameter[3];

	slotRootParameter[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	slotRootParameter[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	slotRootParameter[0].Descriptor.RegisterSpace = 0;
	slotRootParameter[0].Descriptor.ShaderRegister = 0;

	slotRootParameter[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	slotRootParameter[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	slotRootParameter[1].Descriptor.RegisterSpace = 0;
	slotRootParameter[1].Descriptor.ShaderRegister = 1;

	slotRootParameter[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	slotRootParameter[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	slotRootParameter[2].Descriptor.RegisterSpace = 0;
	slotRootParameter[2].Descriptor.ShaderRegister = 2;

	// A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(3, slotRootParameter, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	// create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

	if (errorBlob != nullptr)
	{
		::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	}
	ThrowIfFailed(hr);

	ThrowIfFailed(m_d3dDevice->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(m_RootSignature.GetAddressOf())));
}

void WavesContext::BuildPSO()
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePsoDesc;
	ZeroMemory(&opaquePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));

	opaquePsoDesc.pRootSignature = m_RootSignature.Get();
	opaquePsoDesc.InputLayout = {m_InputLayout.data(),(UINT)m_InputLayout.size()};

	opaquePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	opaquePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	opaquePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	opaquePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);

	opaquePsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(m_Shaders["standardVS"]->GetBufferPointer()),
		m_Shaders["standardVS"]->GetBufferSize()
	};
	opaquePsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(m_Shaders["opaquePS"]->GetBufferPointer()),
		m_Shaders["opaquePS"]->GetBufferSize()
	};

	opaquePsoDesc.SampleMask = UINT_MAX;
	opaquePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	opaquePsoDesc.NumRenderTargets = 1;
	opaquePsoDesc.RTVFormats[0] = m_BackBufferFormat;
	opaquePsoDesc.SampleDesc.Count = m_4xMsaaState ? 4 : 1;
	opaquePsoDesc.SampleDesc.Quality = m_4xMsaaState ? (m_4xMsaaQuality - 1) : 0;
	opaquePsoDesc.DSVFormat = m_DepthStencilFormat;
	ThrowIfFailed(m_d3dDevice->CreateGraphicsPipelineState(&opaquePsoDesc, IID_PPV_ARGS(&m_PSOs["opaque"])));
}

void WavesContext::UpdateObjectCBS(const GameTimer& gt)
{
	for (auto& item : m_AllRitems)
	{
		RenderItemWithMaterial* itemWithM = static_cast<RenderItemWithMaterial*>(item.get());
		if (itemWithM->m_NumFramesDirty > 0)
		{
			XMMATRIX world = XMLoadFloat4x4(&itemWithM->m_World);
			XMMATRIX texTransform = XMLoadFloat4x4(&itemWithM->m_TexTransform);

			ObjectConstants objConstants;
			XMStoreFloat4x4(&objConstants.WorldViewProj, XMMatrixTranspose(world));
			m_currFrameResource->CopyConstData(itemWithM->m_ObjCBIndex, &objConstants);
			item->m_NumFramesDirty--;
		}
	}
}

void WavesContext::UpdateMainPassCB(const GameTimer& gt)
{
	UPDATE_MAIN_PASS;

	m_MainPassCB.m_AmbientLight = { 0.25f, 0.25f, 0.35f, 1.0f };

	XMVECTOR lightDir = -MathHelper::SphericalToCartesian(1.0f, m_SunTheta, m_SunPhi);

	XMStoreFloat3(&m_MainPassCB.m_Lights[0].Direction, lightDir);
	m_MainPassCB.m_Lights[0].Strength = { 1.0f, 1.0f, 0.9f };
	
	m_currFrameResource->CopyPassData(0, &m_MainPassCB);
}

void WavesContext::UpdateWaves(const GameTimer& gt)
{
	FrameResourceWithLight* currentResouce = static_cast < FrameResourceWithLight*>(m_currFrameResource);
	// Every quarter second, generate a random wave.
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

	// Update the wave vertex buffer with the new solution.
	auto currWavesVB = currentResouce->m_WavesVB.get();
	for (int i = 0; i < m_Waves->VertexCount(); ++i)
	{
		VertexN v;

		v.Pos = m_Waves->Position(i);
		v.Normal = m_Waves->Normal(i);

		currWavesVB->CopyData(i, v);
	}

	// Set the dynamic VB of the wave renderitem to the current frame VB.
	m_WavesRitem->m_Geo->m_VertexBufferGPU = currWavesVB->Resource();
}

void WavesContext::UpdateMaterialCBs(const GameTimer& gt)
{
	for (auto& item : m_Materials)
	{
		Material* material = item.second.get();
		if (material->NumFramesDirty > 0)
		{
			XMMATRIX matTransform = XMLoadFloat4x4(&material->MatTransform);

			MaterialConstants matConstants;
			matConstants.DiffuseAlbedo = material->DiffuseAlbedo;
			matConstants.FresnelR0 = material->FresnelR0;
			matConstants.Roughness = material->Roughness;

			m_currFrameResource->CopyMaterialData(material->MatCBIndex, &matConstants);

			material->NumFramesDirty--;
		}
	}
}

void WavesContext::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems)
{
	UINT objCBByteSize = D3DUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
	UINT matCBByteSize = D3DUtil::CalcConstantBufferByteSize(sizeof(MaterialConstants));

	// For each render item...
	for (size_t i = 0; i < ritems.size(); ++i)
	{

		const RenderItemWithMaterial* ri = static_cast<const RenderItemWithMaterial*>(ritems[i]);

		cmdList->IASetVertexBuffers(0, 1, &ri->m_Geo->VertexBufferView());
		cmdList->IASetIndexBuffer(&ri->m_Geo->IndexBufferView());
		cmdList->IASetPrimitiveTopology(ri->m_PrimitiveType);

		D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = m_currFrameResource->getConstGpuAddress() + ri->m_ObjCBIndex * objCBByteSize;
		D3D12_GPU_VIRTUAL_ADDRESS matCBAddress = m_currFrameResource->getMaterialGpuAddress() + ri->m_Material->MatCBIndex * matCBByteSize;

		cmdList->SetGraphicsRootConstantBufferView(0, objCBAddress);
		cmdList->SetGraphicsRootConstantBufferView(1, matCBAddress);

		cmdList->DrawIndexedInstanced(ri->m_IndexCount, 1, ri->m_StartIndexLocation, ri->m_BaseVertexLocation, 0);
	}
}

float WavesContext::GetHillsHeight(float x, float z) const
{
	return 0.3f * (z * sinf(0.1f * x) + x * cosf(0.1f * z));
}

DirectX::XMFLOAT3 WavesContext::GetHillsNormal(float x, float z)const
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