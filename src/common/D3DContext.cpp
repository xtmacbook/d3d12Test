#include "D3DContext.h"
#include "App.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

D3DContext::D3DContext():
	m_FrustumCullingEnabled(true)
{
}

D3DContext::~D3DContext()
{
	if (m_d3dDevice)
	{
		FlushCommandQueue();
	}
}

bool D3DContext::InitDirect3D()
{
#if defined(DEBUG) || defined(_DEBUG) 
	// Enable the D3D12 debug layer.
	{
		ComPtr<ID3D12Debug> debugController;
		ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)));
		debugController->EnableDebugLayer();
	}
#endif

	ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&m_dxgiFactory)));

	// ------------------------------------------------ CreateDevice ------------------------------------------------

	HRESULT hardwareResult = D3D12CreateDevice(
		nullptr,             // default adapter
		D3D_FEATURE_LEVEL_11_0,
		IID_PPV_ARGS(&m_d3dDevice));

	if (FAILED(hardwareResult))
	{
		ComPtr<IDXGIAdapter> pWarpAdapter;
		ThrowIfFailed(m_dxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&pWarpAdapter)));

		ThrowIfFailed(D3D12CreateDevice(
			pWarpAdapter.Get(),
			D3D_FEATURE_LEVEL_11_0,
			IID_PPV_ARGS(&m_d3dDevice)));
	}

	// ------------------------------------------------ CreateFence ------------------------------------------------

	ThrowIfFailed(m_d3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE,
		IID_PPV_ARGS(&m_Fence)));

	// ------------------------------------------------ Get Descriptor Size ------------------------------------------------
	m_RtvDescriptorSize = m_d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	m_DsvDescriptorSize = m_d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	m_CbvSrvUavDescriptorSize = m_d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);


	D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msQualityLevels;
	msQualityLevels.Format = m_BackBufferFormat;
	msQualityLevels.SampleCount = 4;
	msQualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
	msQualityLevels.NumQualityLevels = 0;
	ThrowIfFailed(m_d3dDevice->CheckFeatureSupport(
		D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,
		&msQualityLevels,
		sizeof(msQualityLevels)));

	m_4xMsaaQuality = msQualityLevels.NumQualityLevels;

	CreateCommandObjects();
	CreateSwapChain();
	CreateRtvAndDsvDescriptorHeaps();

	return true;
}

void D3DContext::CreateRtvAndDsvDescriptorHeaps()
{
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
	rtvHeapDesc.NumDescriptors = SwapChainBufferCount;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	rtvHeapDesc.NodeMask = 0;

	ThrowIfFailed(m_d3dDevice->CreateDescriptorHeap(
		&rtvHeapDesc, IID_PPV_ARGS(m_RtvHeap.GetAddressOf())));


	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
	dsvHeapDesc.NumDescriptors = 1;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	dsvHeapDesc.NodeMask = 0;

	ThrowIfFailed(m_d3dDevice->CreateDescriptorHeap(
		&dsvHeapDesc, IID_PPV_ARGS(m_DsvHeap.GetAddressOf())));
}

void D3DContext::OnResize()
{
	FlushCommandQueue();

	ThrowIfFailed(m_CommandList->Reset(m_DirectCmdListAlloc.Get(), nullptr));

	// ------------------------------------------------ Release the previous resources we will be recreating.------------------------------------------------
	for (int i = 0; i < SwapChainBufferCount; ++i)
		m_SwapChainBuffer[i].Reset();
	m_DepthStencilBuffer.Reset();

	// Resize the swap chain.
	ThrowIfFailed(m_SwapChain->ResizeBuffers(
		SwapChainBufferCount,
		m_win->Width(), m_win->Height(),
		m_BackBufferFormat,
		DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH));

	m_CurrBackBuffer = 0;

	//------------------------------------------------Create Render target view ------------------------------------------------
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHeapHandle(m_RtvHeap->GetCPUDescriptorHandleForHeapStart());
	for (UINT i = 0; i < SwapChainBufferCount; i++)
	{
		ThrowIfFailed(m_SwapChain->GetBuffer(i, IID_PPV_ARGS(&m_SwapChainBuffer[i])));
		m_d3dDevice->CreateRenderTargetView(m_SwapChainBuffer[i].Get(), nullptr, rtvHeapHandle);
		rtvHeapHandle.Offset(1, m_RtvDescriptorSize);
	}

	// ------------------------------------------------ set the depth/stencil buffer desc ------------------------------------------------
	D3D12_RESOURCE_DESC depthStencilDesc;

	depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	depthStencilDesc.Alignment = 0;

	depthStencilDesc.Width = m_win->Width();
	depthStencilDesc.Height = m_win->Height();

	depthStencilDesc.DepthOrArraySize = 1;
	depthStencilDesc.MipLevels = 1;
	depthStencilDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;

	depthStencilDesc.SampleDesc.Count = (m_4xMsaaState) ? 4 : 1;
	depthStencilDesc.SampleDesc.Quality = (m_4xMsaaState) ? (m_4xMsaaQuality - 1) : 0;

	depthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	// ------------------------------------------------Create the depth/stencil buffer resource and init heap ------------------------------------------------
	D3D12_CLEAR_VALUE optClear;
	optClear.Format = m_DepthStencilFormat;
	optClear.DepthStencil.Depth = 1.0f;
	optClear.DepthStencil.Stencil = 0;
	ThrowIfFailed(m_d3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&depthStencilDesc,
		D3D12_RESOURCE_STATE_COMMON,
		&optClear,
		IID_PPV_ARGS(m_DepthStencilBuffer.GetAddressOf())));

	// ------------------------------------------------ Create descriptor(view) to mip level 0 of entire resource  ------------------------------------------------
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Format = m_DepthStencilFormat;
	dsvDesc.Texture2D.MipSlice = 0;
	m_d3dDevice->CreateDepthStencilView(m_DepthStencilBuffer.Get(), &dsvDesc, DepthStencilCPUView());

	// ------------------------------------------------ Create resource barrier for depth buffer Transition from common to depth write  ------------------------------------------------

	// Transition the resource from its initial state to be used as a depth buffer.
	m_CommandList->ResourceBarrier(1,
		&CD3DX12_RESOURCE_BARRIER::Transition(m_DepthStencilBuffer.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE));

	// Execute the resize commands.
	ThrowIfFailed(m_CommandList->Close());
	ID3D12CommandList* cmdsLists[] = { m_CommandList.Get() };
	m_CommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Wait until resize is complete.
	FlushCommandQueue();

	// Update the viewport transform to cover the client area.
	m_ScreenViewport.TopLeftX = 0;
	m_ScreenViewport.TopLeftY = 0;
	m_ScreenViewport.Width = static_cast<float>(m_win->Width());
	m_ScreenViewport.Height = static_cast<float>(m_win->Height());
	m_ScreenViewport.MinDepth = 0.0f;
	m_ScreenViewport.MaxDepth = 1.0f;

	m_ScissorRect = { 0, 0, m_win->Width(), m_win->Height() };

	// The window resized, so update the aspect ratio and recompute the projection matrix.
	XMMATRIX P = XMMatrixPerspectiveFovLH(0.25f * MathHelper::Pi, m_win->AspectRatio(), 1.0f, 1000.0f);
	DirectX::XMStoreFloat4x4(&m_Proj, P);

	//从proj 获取视锥体的frustum
	BoundingFrustum::CreateFromMatrix(m_CamFrustum, XMLoadFloat4x4(&m_Proj));
}

void D3DContext::setApp(App* app)
{
	m_win = app;
}

void D3DContext::setWireFrame(bool frame)
{
	m_IsWireframe = frame;
}

void D3DContext::CreateCommandObjects()
{
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	ThrowIfFailed(m_d3dDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_CommandQueue)));

	ThrowIfFailed(m_d3dDevice->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(m_DirectCmdListAlloc.GetAddressOf())));

	ThrowIfFailed(m_d3dDevice->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		m_DirectCmdListAlloc.Get(), // Associated command allocator
		nullptr,                   // Initial PipelineStateObject
		IID_PPV_ARGS(m_CommandList.GetAddressOf())));

	// Start off in a closed state.  This is because the first time we refer 
	// to the command list we will Reset it, and it needs to be closed before
	// calling Reset.
	m_CommandList->Close();
}

void D3DContext::CreateSwapChain()
{
	m_SwapChain.Reset();

	DXGI_SWAP_CHAIN_DESC sd;
	sd.BufferDesc.Width = m_win->Width();
	sd.BufferDesc.Height = m_win->Height();

	sd.BufferDesc.RefreshRate.Numerator = 60;
	sd.BufferDesc.RefreshRate.Denominator = 1;

	sd.BufferDesc.Format = m_BackBufferFormat;

	sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

	sd.SampleDesc.Count = (m_4xMsaaState) ? 4 : 1;
	sd.SampleDesc.Quality = m_4xMsaaQuality ? (m_4xMsaaQuality - 1) : 0;

	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.BufferCount = SwapChainBufferCount;

	sd.OutputWindow = m_win->MainWnd();
	sd.Windowed = true;

	sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	ThrowIfFailed(m_dxgiFactory->CreateSwapChain(
		m_CommandQueue.Get(),
		&sd,
		m_SwapChain.GetAddressOf()));

}

void D3DContext::FlushCommandQueue()
{
	// Advance the fence value to mark commands up to this fence point.
	m_CurrentFence++;

	// Add an instruction to the command queue to set a new fence point.  Because we 
	// are on the GPU timeline, the new fence point won't be set until the GPU finishes
	// processing all the commands prior to this Signal().
	ThrowIfFailed(m_CommandQueue->Signal(m_Fence.Get(), m_CurrentFence)); //更新m_Fence为特定的值

	// Wait until the GPU has completed commands up to this fence point.
	if (m_Fence->GetCompletedValue() < m_CurrentFence)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);

		// Fire event when GPU hits current fence.  
		ThrowIfFailed(m_Fence->SetEventOnCompletion(m_CurrentFence, eventHandle));

		// Wait until the GPU hits current fence event is fired.
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}

}

void D3DContext::Update(const GameTimer& gt)
{
	UpdateCamera(gt);
}

D3D12_CPU_DESCRIPTOR_HANDLE D3DContext::CurrentCPUBackBufferView() const
{
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(
		m_RtvHeap->GetCPUDescriptorHandleForHeapStart(),
		m_CurrBackBuffer,
		m_RtvDescriptorSize);
}

D3D12_CPU_DESCRIPTOR_HANDLE D3DContext::DepthStencilCPUView() const
{
	return m_DsvHeap->GetCPUDescriptorHandleForHeapStart();
}

ID3D12Resource* D3DContext::CurrentBackBuffer() const
{
	return m_SwapChainBuffer[m_CurrBackBuffer].Get();
}

void D3DContext::UpdateCamera(const GameTimer& gt)
{
	float radius, phi, theta;
	m_win->GetViewState(theta, phi, radius);

	// Convert Spherical to Cartesian coordinates.
	m_EyePos.x = radius * sinf(phi) * cosf(theta);
	m_EyePos.z = radius * sinf(phi) * sinf(theta);
	m_EyePos.y = radius * cosf(phi);

	// Build the view matrix.
	XMVECTOR pos = XMVectorSet(m_EyePos.x, m_EyePos.y, m_EyePos.z, 1.0f);
	XMVECTOR target = XMVectorZero();
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
	XMStoreFloat4x4(&m_View, view);
}


 