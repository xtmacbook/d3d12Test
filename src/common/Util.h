#include <Windows.h>
#include <wrl.h>

#include <dxgi1_4.h>
#include <d3d12.h>

#include <D3Dcompiler.h>
#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <DirectXColors.h>
#include <DirectXCollision.h>

#pragma once

#include "d3dx12.h"
#include "MathHelper.h"

#include <string>
 
#define MaxLights 16

inline std::wstring AnsiToWstring(const std::string& str)
{
    WCHAR buffer[512];
    MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, buffer, 512);
    return std::wstring(buffer);
}

inline std::string WStringToString(const std::wstring& wstr)
{
    if (wstr.empty()) return "";
    // 获取需要缓冲区大小
    int bufSize = WideCharToMultiByte(
        CP_UTF8,        // 输出编码：UTF-8，改用CP_ACP为系统ANSI
        0,
        wstr.c_str(),
        static_cast<int>(wstr.size()),
        nullptr,
        0,
        nullptr,
        nullptr
    );
    std::string res(bufSize, 0);
    WideCharToMultiByte(
        CP_UTF8,
        0,
        wstr.c_str(),
        static_cast<int>(wstr.size()),
        &res[0],
        bufSize,
        nullptr,
        nullptr
    );
    return res;
}

std::wstring SourcePath();

class D3DUtil
{
public:
    static UINT CalcConstantBufferByteSize(UINT byteSize)
    {
        // Constant buffers must be a multiple of the minimum hardware
        // allocation size (usually 256 bytes).  So round up to nearest
        // multiple of 256.  We do this by adding 255 and then masking off
        // the lower 2 bytes which store all bits < 256.
        // Example: Suppose byteSize = 300.
        // (300 + 255) & ~255
        // 555 & ~255
        // 0x022B & ~0x00ff
        // 0x022B & 0xff00
        // 0x0200
        // 512
        return (byteSize + 255) & ~255;
    }

    /*
      1.Gpu上创建buffer;
      2.创建uploader buffer;
      3.通过map 将cpu数据(MemcpySubresource)放入到uploader buffer,
      4.通过pCmdList->CopyBufferRegion将uploader buffer数据推送到第一步创建的buffer中

    */
    static Microsoft::WRL::ComPtr<ID3D12Resource> CreateDefaultBuffer(
        ID3D12Device* device,
        ID3D12GraphicsCommandList* cmdList,
        const void* initData,
        UINT64 byteSize,
        Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer);

    static Microsoft::WRL::ComPtr<ID3DBlob> CompileShader(
        const std::wstring& filename,
        const D3D_SHADER_MACRO* defines,
        const std::string& entrypoint,
        const std::string& target);
};

class DxException
{
public:
    DxException() = default;

    DxException(HRESULT hr, const std::wstring & functionName, const std::wstring & filename, int lineNumber);

    std::wstring toString()const;

    HRESULT         m_ErrorCode = S_OK;
    std::wstring    m_functionName;
    std::wstring    m_filename;
    int             m_lineNumber = -1;
};


struct RootParameterIndexs
{
    UINT m_CONST_RootParameterIndex;
    UINT m_PASS_RootParameterIndex;
    UINT m_Material_RootParameterIndex;
};


#ifndef ThrowIfFailed
#define ThrowIfFailed(x)                                              \
{                                                                     \
    HRESULT hr__ = (x);                                               \
    std::wstring wfn = AnsiToWstring(__FILE__);                       \
    if(FAILED(hr__)) {                                                 \
    throw DxException(hr__, L#x, wfn, __LINE__);                        \
    } \
}
#endif

void errorExit();

#define UPDATE_MAIN_PASS  XMMATRIX view = XMLoadFloat4x4(&m_View);\
XMMATRIX proj = XMLoadFloat4x4(&m_Proj);\
XMMATRIX viewProj = XMMatrixMultiply(view, proj);\
XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);\
XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);\
XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);\
XMStoreFloat4x4(&m_MainPassCB.m_View, XMMatrixTranspose(view));\
XMStoreFloat4x4(&m_MainPassCB.m_InvView, XMMatrixTranspose(invView));\
XMStoreFloat4x4(&m_MainPassCB.m_Proj, XMMatrixTranspose(proj));\
XMStoreFloat4x4(&m_MainPassCB.m_InvProj, XMMatrixTranspose(invProj));\
XMStoreFloat4x4(&m_MainPassCB.m_ViewProj, XMMatrixTranspose(viewProj));\
XMStoreFloat4x4(&m_MainPassCB.m_InvViewProj, XMMatrixTranspose(invViewProj));\
m_MainPassCB.m_EyePosW = m_EyePos;\
m_MainPassCB.m_RenderTargetSize = XMFLOAT2((float)m_win->Width(), (float)m_win->Height());\
m_MainPassCB.m_InvRenderTargetSize = XMFLOAT2(1.0f / m_win->Width(), 1.0f / m_win->Height());\
m_MainPassCB.m_NearZ = 1.0f;\
m_MainPassCB.m_FarZ = 1000.0f;\
m_MainPassCB.m_TotalTime = gt.TotalTime();\
m_MainPassCB.m_DeltaTime = gt.DeltaTime(); 

#define BEFORE_DRAW_SET 	m_CommandList->RSSetViewports(1, &m_ScreenViewport);\
m_CommandList->RSSetScissorRects(1, &m_ScissorRect);\
m_CommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));\
m_CommandList->ClearRenderTargetView(CurrentCPUBackBufferView(), Colors::LightSteelBlue, 0, nullptr);\
m_CommandList->ClearDepthStencilView(DepthStencilCPUView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);\
m_CommandList->OMSetRenderTargets(1, &CurrentCPUBackBufferView(), true, &DepthStencilCPUView());\
m_CommandList->SetGraphicsRootSignature(m_RootSignature.Get());
