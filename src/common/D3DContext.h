#pragma once

#include "Util.h"
#include "GameTimer.h"
#include "MathHelper.h"

#pragma comment(lib,"d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")
#pragma comment(lib, "dxgi.lib")

class App;

class D3DContext
{
public:

	D3DContext();

	virtual ~D3DContext();

	virtual bool InitDirect3D();
	virtual void CreateRtvAndDsvDescriptorHeaps();
	virtual void OnResize();

	void setApp(App*);

	inline ID3D12Device* device() { return m_d3dDevice.Get(); }

	/*同步CPU和GPU,但是目前是阻止CPU的运行,GPU完成提交的命令后继续CPU的执行*/
	void FlushCommandQueue();

	virtual void Update(const GameTimer& gt);
	virtual void Draw(const GameTimer& gt) {};


	void setWireFrame(bool);

protected:
	void CreateCommandObjects();
	void CreateSwapChain();

protected:

	D3D12_CPU_DESCRIPTOR_HANDLE CurrentCPUBackBufferView()const;
	D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilCPUView()const;
	ID3D12Resource* CurrentBackBuffer()const;
	void UpdateCamera(const GameTimer& gt);

protected:

	DirectX::XMFLOAT4X4 m_World								= MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 m_View								= MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 m_Proj								= MathHelper::Identity4x4();
	DirectX::XMFLOAT3	m_EyePos							= { .0,.0,.0 };
	DirectX::BoundingFrustum								m_CamFrustum;
	bool													m_FrustumCullingEnabled;
protected:

	Microsoft::WRL::ComPtr<IDXGIFactory4>					m_dxgiFactory;
	Microsoft::WRL::ComPtr<IDXGISwapChain>					m_SwapChain;
	Microsoft::WRL::ComPtr<ID3D12Device>					m_d3dDevice;


	Microsoft::WRL::ComPtr<ID3D12Fence>						m_Fence;
	UINT64													m_CurrentFence = 0;

	DXGI_FORMAT												m_BackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	DXGI_FORMAT												m_DepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	UINT													m_4xMsaaQuality = 0;      // quality level of 4X MSAA
	bool													m_4xMsaaState = false;    // 4X MSAA enabled

	Microsoft::WRL::ComPtr<ID3D12CommandQueue>				m_CommandQueue;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator>			m_DirectCmdListAlloc;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>		m_CommandList;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>			m_RtvHeap;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>			m_DsvHeap;

	UINT													m_RtvDescriptorSize = 0;
	UINT													m_DsvDescriptorSize = 0;
	UINT													m_CbvSrvUavDescriptorSize = 0;


	D3D12_VIEWPORT											m_ScreenViewport;
	D3D12_RECT												m_ScissorRect;

	static const int										SwapChainBufferCount = 2;

	Microsoft::WRL::ComPtr<ID3D12Resource>					m_SwapChainBuffer[SwapChainBufferCount];
	Microsoft::WRL::ComPtr<ID3D12Resource>					m_DepthStencilBuffer;
	int														m_CurrBackBuffer = 0;
	App*													m_win = nullptr;

	bool													m_IsWireframe = false;

	friend class Sky;
};