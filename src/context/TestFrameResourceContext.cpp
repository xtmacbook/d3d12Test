#include "TestFrameResourceContext.h"

#include "common/App.h"
#include "common/GeometryGenerator.h"
#include "common/Geometry.h"


using namespace DirectX;
using namespace DirectX::PackedVector;
using Microsoft::WRL::ComPtr;


bool DefaultFrameResourceContext::InitDirect3D()
{
	if (!D3DContext::InitDirect3D()) return false;

	// Reset the command list to prep for initialization commands.
	ThrowIfFailed(m_CommandList->Reset(m_DirectCmdListAlloc.Get(), nullptr));

	BuildRootSignature();
	BuildShadersAndInputLayout();
	BuildGeometry();
	BuildRenderItems();
	BuildFrameResources();
	BuildDescriptorHeaps();
	BuildConstantBufferViews();
	BuildPSO();

	// Execute the initialization commands.
	ThrowIfFailed(m_CommandList->Close());
	ID3D12CommandList* cmdsLists[] = { m_CommandList.Get() };
	m_CommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Wait until initialization is complete.
	FlushCommandQueue();

	return true;
}

void DefaultFrameResourceContext::Update(const GameTimer& gt)
{
	D3DContext::Update(gt);
	FrameResourceContextInterface::Update(gt,m_Fence.Get());
	UpdateObjectCBs(gt);
	UpdateMainPassCB(gt);
}

void DefaultFrameResourceContext::Draw(const GameTimer& gt)
{
	FrameResourceContextInterface::Draw(gt, m_CurrentFence, m_Fence.Get(), m_CommandQueue.Get());
}

void DefaultFrameResourceContext::UpdateObjectCBs(const GameTimer& gt)
{
	for (auto& item : m_AllRitems)
	{
		// Only update the cbuffer data if the constants have changed.  
		// This needs to be tracked per frame resource.
		if (item->m_NumFramesDirty > 0)
		{
			XMMATRIX world = XMLoadFloat4x4(&item->m_World);

			ObjectConstants objConstants;
			XMStoreFloat4x4(&objConstants.WorldViewProj, XMMatrixTranspose(world));

			m_currFrameResource->CopyConstData(item->m_ObjCBIndex, &objConstants);
			item->m_NumFramesDirty--;
		}
	}
}

void DefaultFrameResourceContext::UpdateMainPassCB(const GameTimer& gt)
{
	XMMATRIX view = XMLoadFloat4x4(&m_View);
	XMMATRIX proj = XMLoadFloat4x4(&m_Proj);

	XMMATRIX viewProj = XMMatrixMultiply(view, proj);
	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
	XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
	XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

	XMStoreFloat4x4(&m_MainPassCB.m_View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&m_MainPassCB.m_InvView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&m_MainPassCB.m_Proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&m_MainPassCB.m_InvProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&m_MainPassCB.m_ViewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&m_MainPassCB.m_InvViewProj, XMMatrixTranspose(invViewProj));
	m_MainPassCB.m_EyePosW = m_EyePos;
	m_MainPassCB.m_RenderTargetSize = XMFLOAT2((float)m_win->Width(), (float)m_win->Height());
	m_MainPassCB.m_InvRenderTargetSize = XMFLOAT2(1.0f / m_win->Width(), 1.0f / m_win->Height());
	m_MainPassCB.m_NearZ = 1.0f;
	m_MainPassCB.m_FarZ = 1000.0f;
	m_MainPassCB.m_TotalTime = gt.TotalTime();
	m_MainPassCB.m_DeltaTime = gt.DeltaTime();

	m_currFrameResource->CopyPassData(0, &m_MainPassCB);
}


void DefaultFrameResourceContext::BuildFrameResources()
{
	for (int i = 0; i < m_NumFrameResources; i++)
	{
		m_frameResources.emplace_back(std::make_unique<FrameResource <ObjectConstants,PassConstants>  >(m_d3dDevice.Get(), 1, m_AllRitems.size()));
	}
}

void DefaultFrameResourceContext::DrawFrameResource(ID3D12CommandAllocator* cmdListAlloc)
{

	if (m_IsWireframe)
	{
		ThrowIfFailed(m_CommandList->Reset(cmdListAlloc, m_PSOs["opaque_wireframe"].Get()));
	}
	else
	{
		ThrowIfFailed(m_CommandList->Reset(cmdListAlloc, m_PSOs["opaque"].Get()));
	}

	m_CommandList->RSSetViewports(1, &m_ScreenViewport);
	m_CommandList->RSSetScissorRects(1, &m_ScissorRect);

	// Indicate a state transition on the resource usage.
	m_CommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	// Clear the back buffer and depth buffer.
	m_CommandList->ClearRenderTargetView(CurrentCPUBackBufferView(), Colors::LightSteelBlue, 0, nullptr);
	m_CommandList->ClearDepthStencilView(DepthStencilCPUView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	// Specify the buffers we are going to render to.
	m_CommandList->OMSetRenderTargets(1, &CurrentCPUBackBufferView(), true, &DepthStencilCPUView());

	ID3D12DescriptorHeap* descriptorHeaps[] = { m_ObjCbvHeap.Get() };
	m_CommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	m_CommandList->SetGraphicsRootSignature(m_RootSignature.Get());

	int passCbvIndex = m_PassCbvOffset + m_CurrFrameResourceIndex;
	auto passCbvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(m_ObjCbvHeap->GetGPUDescriptorHandleForHeapStart());
	passCbvHandle.Offset(passCbvIndex, m_CbvSrvUavDescriptorSize);
	m_CommandList->SetGraphicsRootDescriptorTable(1, passCbvHandle);

	DrawRenderItems(m_CommandList.Get(), m_OpaqueRitems);

	// Indicate a state transition on the resource usage.
	m_CommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	// Done recording commands.
	ThrowIfFailed(m_CommandList->Close());

	// Add the command list to the queue for execution.
	ID3D12CommandList* cmdsLists[] = { m_CommandList.Get() };
	m_CommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Swap the back and front buffers
	ThrowIfFailed(m_SwapChain->Present(0, 0));
	m_CurrBackBuffer = (m_CurrBackBuffer + 1) % SwapChainBufferCount;
}

void DefaultFrameResourceContext::BuildDescriptorHeaps()
{

	UINT objCount = (UINT)m_OpaqueRitems.size();

	// Need a CBV descriptor for each object for each frame resource,
  // +1 for the perPass CBV for each frame resource.
	UINT numDescriptors = (objCount + 1) * m_NumFrameResources;

	m_PassCbvOffset = objCount * m_NumFrameResources;

	D3D12_DESCRIPTOR_HEAP_DESC objCbvHeapDesc;
	objCbvHeapDesc.NumDescriptors = numDescriptors;
	objCbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	objCbvHeapDesc.NodeMask = 0;
	objCbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

	ThrowIfFailed(m_d3dDevice->CreateDescriptorHeap(&objCbvHeapDesc,IID_PPV_ARGS(&m_ObjCbvHeap)));
}

void DefaultFrameResourceContext::BuildRenderItems()
{
	auto boxRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&boxRitem->m_World, XMMatrixScaling(2.0f, 2.0f, 2.0f) * XMMatrixTranslation(0.0f, 0.5f, 0.0f));
	boxRitem->m_ObjCBIndex = 0;
	boxRitem->m_Geo = m_Geometries["shapeGeo"].get();
	boxRitem->m_PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	boxRitem->m_IndexCount = boxRitem->m_Geo->m_DrawArgs["box"].m_IndexCount;
	boxRitem->m_StartIndexLocation = boxRitem->m_Geo->m_DrawArgs["box"].m_StartIndexLocation;
	boxRitem->m_BaseVertexLocation = boxRitem->m_Geo->m_DrawArgs["box"].m_BaseVertexLocation;
	m_AllRitems.push_back(std::move(boxRitem));

	auto gridRitem = std::make_unique<RenderItem>();
	gridRitem->m_World = MathHelper::Identity4x4();
	gridRitem->m_ObjCBIndex = 1;
	gridRitem->m_Geo = m_Geometries["shapeGeo"].get();
	gridRitem->m_PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	gridRitem->m_IndexCount = gridRitem->m_Geo->m_DrawArgs["grid"].m_IndexCount;
	gridRitem->m_StartIndexLocation = gridRitem->m_Geo->m_DrawArgs["grid"].m_StartIndexLocation;
	gridRitem->m_BaseVertexLocation = gridRitem->m_Geo->m_DrawArgs["grid"].m_BaseVertexLocation;
	m_AllRitems.push_back(std::move(gridRitem));

	UINT objCBIndex = 2;
	for (int i = 0; i < 5; ++i)
	{
		auto leftCylRitem = std::make_unique<RenderItem>();
		auto rightCylRitem = std::make_unique<RenderItem>();
		auto leftSphereRitem = std::make_unique<RenderItem>();
		auto rightSphereRitem = std::make_unique<RenderItem>();

		XMMATRIX leftCylWorld = XMMatrixTranslation(-5.0f, 1.5f, -10.0f + i * 5.0f);
		XMMATRIX rightCylWorld = XMMatrixTranslation(+5.0f, 1.5f, -10.0f + i * 5.0f);

		XMMATRIX leftSphereWorld = XMMatrixTranslation(-5.0f, 3.5f, -10.0f + i * 5.0f);
		XMMATRIX rightSphereWorld = XMMatrixTranslation(+5.0f, 3.5f, -10.0f + i * 5.0f);

		XMStoreFloat4x4(&leftCylRitem->m_World, rightCylWorld);
		leftCylRitem->m_ObjCBIndex = objCBIndex++;
		leftCylRitem->m_Geo = m_Geometries["shapeGeo"].get();
		leftCylRitem->m_PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		leftCylRitem->m_IndexCount = leftCylRitem->m_Geo->m_DrawArgs["cylinder"].m_IndexCount;
		leftCylRitem->m_StartIndexLocation = leftCylRitem->m_Geo->m_DrawArgs["cylinder"].m_StartIndexLocation;
		leftCylRitem->m_BaseVertexLocation = leftCylRitem->m_Geo->m_DrawArgs["cylinder"].m_BaseVertexLocation;

		XMStoreFloat4x4(&rightCylRitem->m_World, leftCylWorld);
		rightCylRitem->m_ObjCBIndex = objCBIndex++;
		rightCylRitem->m_Geo = m_Geometries["shapeGeo"].get();
		rightCylRitem->m_PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		rightCylRitem->m_IndexCount = rightCylRitem->m_Geo->m_DrawArgs["cylinder"].m_IndexCount;
		rightCylRitem->m_StartIndexLocation = rightCylRitem->m_Geo->m_DrawArgs["cylinder"].m_StartIndexLocation;
		rightCylRitem->m_BaseVertexLocation = rightCylRitem->m_Geo->m_DrawArgs["cylinder"].m_BaseVertexLocation;

		XMStoreFloat4x4(&leftSphereRitem->m_World, leftSphereWorld);
		leftSphereRitem->m_ObjCBIndex = objCBIndex++;
		leftSphereRitem->m_Geo = m_Geometries["shapeGeo"].get();
		leftSphereRitem->m_PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		leftSphereRitem->m_IndexCount = leftSphereRitem->m_Geo->m_DrawArgs["sphere"].m_IndexCount;
		leftSphereRitem->m_StartIndexLocation = leftSphereRitem->m_Geo->m_DrawArgs["sphere"].m_StartIndexLocation;
		leftSphereRitem->m_BaseVertexLocation = leftSphereRitem->m_Geo->m_DrawArgs["sphere"].m_BaseVertexLocation;

		XMStoreFloat4x4(&rightSphereRitem->m_World, rightSphereWorld);
		rightSphereRitem->m_ObjCBIndex = objCBIndex++;
		rightSphereRitem->m_Geo = m_Geometries["shapeGeo"].get();
		rightSphereRitem->m_PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		rightSphereRitem->m_IndexCount = rightSphereRitem->m_Geo->m_DrawArgs["sphere"].m_IndexCount;
		rightSphereRitem->m_StartIndexLocation = rightSphereRitem->m_Geo->m_DrawArgs["sphere"].m_StartIndexLocation;
		rightSphereRitem->m_BaseVertexLocation = rightSphereRitem->m_Geo->m_DrawArgs["sphere"].m_BaseVertexLocation;

		m_AllRitems.push_back(std::move(leftCylRitem));
		m_AllRitems.push_back(std::move(rightCylRitem));
		m_AllRitems.push_back(std::move(leftSphereRitem));
		m_AllRitems.push_back(std::move(rightSphereRitem));
	}

	// All the render items are opaque.
	for (auto& e : m_AllRitems)
		m_OpaqueRitems.push_back(e.get());
}

void DefaultFrameResourceContext::BuildPSO()
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePsoDesc;
	ZeroMemory(&opaquePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));

	opaquePsoDesc.pRootSignature = m_RootSignature.Get();
	opaquePsoDesc.InputLayout = { m_InputLayout.data(),(UINT)m_InputLayout.size()};

	opaquePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	opaquePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	opaquePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	opaquePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);

	opaquePsoDesc.VS = { reinterpret_cast<BYTE*>(m_Shaders["standardVS"]->GetBufferPointer()),
		m_Shaders["standardVS"]->GetBufferSize() };

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

	D3D12_GRAPHICS_PIPELINE_STATE_DESC opaqueWireframePsoDesc = opaquePsoDesc;
	opaqueWireframePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
	ThrowIfFailed(m_d3dDevice->CreateGraphicsPipelineState(&opaqueWireframePsoDesc, IID_PPV_ARGS(&m_PSOs["opaque_wireframe"])));
}

void DefaultFrameResourceContext::BuildConstantBufferViews()
{
	UINT objCBByteSize = D3DUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));

	UINT objCount = (UINT)m_OpaqueRitems.size();

	for (int frameIndx = 0; frameIndx < m_NumFrameResources; frameIndx++)
	{
		for (UINT i = 0; i < objCount; ++i)
		{
			D3D12_GPU_VIRTUAL_ADDRESS cbAddress = m_frameResources[frameIndx]->getConstGpuAddress();

			cbAddress += i * objCBByteSize;

			// Offset to the object cbv in the descriptor heap.
			int heapIndex = frameIndx * objCount + i;
			auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_ObjCbvHeap->GetCPUDescriptorHandleForHeapStart());
			handle.Offset(heapIndex, m_CbvSrvUavDescriptorSize);

			D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
			cbvDesc.BufferLocation = cbAddress;
			cbvDesc.SizeInBytes = objCBByteSize;

			m_d3dDevice->CreateConstantBufferView(&cbvDesc, handle);
		}
	}

	UINT passCBByteSize = D3DUtil::CalcConstantBufferByteSize(sizeof(PassConstants));

	// Last three descriptors are the pass CBVs for each frame resource.
	for (int frameIndex = 0; frameIndex < m_NumFrameResources; ++frameIndex)
	{
		D3D12_GPU_VIRTUAL_ADDRESS cbAddress = m_frameResources[frameIndex]->getPassGpuAddress();

		// Offset to the pass cbv in the descriptor heap.
		int heapIndex = m_PassCbvOffset + frameIndex;
		auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_ObjCbvHeap->GetCPUDescriptorHandleForHeapStart());
		handle.Offset(heapIndex, m_CbvSrvUavDescriptorSize);

		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
		cbvDesc.BufferLocation = cbAddress;
		cbvDesc.SizeInBytes = passCBByteSize;

		m_d3dDevice->CreateConstantBufferView(&cbvDesc, handle);
	}


}

void DefaultFrameResourceContext::BuildRootSignature()
{
	/*
	* 
	* shader需要的resources已经改变了,因为CBV需要根据跟新的频率,
	需要设置两个descriptor tables
	*/

	D3D12_DESCRIPTOR_RANGE cbvTable0;
	cbvTable0.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
	cbvTable0.NumDescriptors = 1;
	cbvTable0.BaseShaderRegister = 0;
	cbvTable0.RegisterSpace = 0;
	cbvTable0.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_DESCRIPTOR_RANGE cbvTable1;
	cbvTable1.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
	cbvTable1.NumDescriptors = 1;
	cbvTable1.BaseShaderRegister = 1;
	cbvTable1.RegisterSpace = 0;
	cbvTable1.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER slotRootParameter[2];
	slotRootParameter[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	slotRootParameter[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	slotRootParameter[0].DescriptorTable.NumDescriptorRanges = 1;
	slotRootParameter[0].DescriptorTable.pDescriptorRanges = &cbvTable0;

	slotRootParameter[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	slotRootParameter[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	slotRootParameter[1].DescriptorTable.NumDescriptorRanges = 1;
	slotRootParameter[1].DescriptorTable.pDescriptorRanges = &cbvTable1;

	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc;
	rootSignatureDesc.NumParameters = 2;
	rootSignatureDesc.pParameters = slotRootParameter;
	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	rootSignatureDesc.NumStaticSamplers = 0;
	rootSignatureDesc.pStaticSamplers = nullptr;

	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1,
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

void DefaultFrameResourceContext::BuildShadersAndInputLayout()
{
	m_Shaders["standardVS"] = D3DUtil::CompileShader(SourcePath() + L"/Shaders/shape.hlsl", nullptr, "VS", "vs_5_1");
	m_Shaders["opaquePS"] = D3DUtil::CompileShader(SourcePath() + L"/Shaders/shape.hlsl", nullptr, "PS", "ps_5_1");

	m_InputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};
}

void DefaultFrameResourceContext::BuildGeometry()
{
	/*
	 
	 1. 虽然绘制很多模型,但使用instance 很多只是使用不同的矩阵
	 2. 将所有的vertices和 indices压入到buffer,绘制不同模型使用buffer的子集进行绘制
	*/

	GeometryGenerator geoGen;
	GeometryGenerator::MeshData box = geoGen.CreateBox(1.5f, 0.5f, 1.5f, 3);
	GeometryGenerator::MeshData grid = geoGen.CreateGrid(20.0f, 30.0f, 60, 40);
	GeometryGenerator::MeshData sphere = geoGen.CreateSphere(0.5f, 20, 20);
	GeometryGenerator::MeshData cylinder = geoGen.CreateCylinder(0.5f, 0.3f, 3.0f, 20, 20);

	//
	// 把所有的mesh压入到一个buffer中
	//
	UINT boxVertexOffset = 0;
	UINT gridVertexOffset = (UINT)box.Vertices.size();
	UINT sphereVertexOffset = gridVertexOffset + (UINT)grid.Vertices.size();
	UINT cylinderVertexOffset = sphereVertexOffset + (UINT)sphere.Vertices.size();

	UINT boxIndexOffset = 0;
	UINT gridIndexOffset = (UINT)box.Indices32.size();
	UINT sphereIndexOffset = gridIndexOffset + (UINT)grid.Indices32.size();
	UINT cylinderIndexOffset = sphereIndexOffset + (UINT)sphere.Indices32.size();


	SubmeshGeometry boxSubmesh;
	boxSubmesh.m_IndexCount = (UINT)box.Indices32.size();
	boxSubmesh.m_StartIndexLocation = boxIndexOffset;
	boxSubmesh.m_BaseVertexLocation = boxVertexOffset;

	SubmeshGeometry gridSubmesh;
	gridSubmesh.m_IndexCount = (UINT)grid.Indices32.size();
	gridSubmesh.m_StartIndexLocation = gridIndexOffset;
	gridSubmesh.m_BaseVertexLocation = gridVertexOffset;

	SubmeshGeometry sphereSubmesh;
	sphereSubmesh.m_IndexCount = (UINT)sphere.Indices32.size();
	sphereSubmesh.m_StartIndexLocation = sphereIndexOffset;
	sphereSubmesh.m_BaseVertexLocation = sphereVertexOffset;

	SubmeshGeometry cylinderSubmesh;
	cylinderSubmesh.m_IndexCount = (UINT)cylinder.Indices32.size();
	cylinderSubmesh.m_StartIndexLocation = cylinderIndexOffset;
	cylinderSubmesh.m_BaseVertexLocation = cylinderVertexOffset;

	//
	// Extract the vertex elements we are interested in and pack the
	// vertices of all the meshes into one vertex buffer.
	//

	auto totalVertexCount =
		box.Vertices.size() +
		grid.Vertices.size() +
		sphere.Vertices.size() +
		cylinder.Vertices.size();

	std::vector<VertexC> vertices(totalVertexCount);

	UINT k = 0;
	for (size_t i = 0; i < box.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = box.Vertices[i].Position;
		vertices[k].Color = XMFLOAT4(DirectX::Colors::DarkGreen);
	}

	for (size_t i = 0; i < grid.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = grid.Vertices[i].Position;
		vertices[k].Color = XMFLOAT4(DirectX::Colors::ForestGreen);
	}

	for (size_t i = 0; i < sphere.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = sphere.Vertices[i].Position;
		vertices[k].Color = XMFLOAT4(DirectX::Colors::Crimson);
	}

	for (size_t i = 0; i < cylinder.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = cylinder.Vertices[i].Position;
		vertices[k].Color = XMFLOAT4(DirectX::Colors::SteelBlue);
	}

	std::vector<std::uint16_t> indices;
	indices.insert(indices.end(), std::begin(box.GetIndices16()), std::end(box.GetIndices16()));
	indices.insert(indices.end(), std::begin(grid.GetIndices16()), std::end(grid.GetIndices16()));
	indices.insert(indices.end(), std::begin(sphere.GetIndices16()), std::end(sphere.GetIndices16()));
	indices.insert(indices.end(), std::begin(cylinder.GetIndices16()), std::end(cylinder.GetIndices16()));

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(VertexC);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "shapeGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->m_VertexBufferCPU));
	CopyMemory(geo->m_VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->m_IndexBufferCPU));
	CopyMemory(geo->m_IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->m_VertexBufferGPU = D3DUtil::CreateDefaultBuffer(m_d3dDevice.Get(),
		m_CommandList.Get(), vertices.data(), vbByteSize, geo->m_VertexBufferUploader);

	geo->m_IndexBufferGPU = D3DUtil::CreateDefaultBuffer(m_d3dDevice.Get(),
		m_CommandList.Get(), indices.data(), ibByteSize, geo->m_IndexBufferUploader);

	geo->m_VertexByteStride = sizeof(VertexC);
	geo->m_VertexBufferByteSize = vbByteSize;
	geo->m_IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->m_IndexBufferByteSize = ibByteSize;

	geo->m_DrawArgs["box"] = boxSubmesh;
	geo->m_DrawArgs["grid"] = gridSubmesh;
	geo->m_DrawArgs["sphere"] = sphereSubmesh;
	geo->m_DrawArgs["cylinder"] = cylinderSubmesh;

	m_Geometries[geo->Name] = std::move(geo); //
}

void DefaultFrameResourceContext::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems)
{
	UINT objCBByteSize = D3DUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));

	// For each render item...
	for (size_t i = 0; i < ritems.size(); ++i)
	{
		auto ri = ritems[i];

		cmdList->IASetVertexBuffers(0, 1, &ri->m_Geo->VertexBufferView());
		cmdList->IASetIndexBuffer(&ri->m_Geo->IndexBufferView());
		cmdList->IASetPrimitiveTopology(ri->m_PrimitiveType);

		// Offset to the CBV in the descriptor heap for this object and for this frame resource.
		UINT cbvIndex = m_CurrFrameResourceIndex * (UINT)m_OpaqueRitems.size() + ri->m_ObjCBIndex;
		auto cbvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(m_ObjCbvHeap->GetGPUDescriptorHandleForHeapStart());
		cbvHandle.Offset(cbvIndex, m_CbvSrvUavDescriptorSize);

		cmdList->SetGraphicsRootDescriptorTable(0, cbvHandle);

		cmdList->DrawIndexedInstanced(ri->m_IndexCount, 1, ri->m_StartIndexLocation, ri->m_BaseVertexLocation, 0);
	}
}
