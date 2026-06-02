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


struct MaterialConstants
{
    DirectX::XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
    DirectX::XMFLOAT3 FresnelR0 = { 0.01f, 0.01f, 0.01f };
    float Roughness = 0.25f;

    // Used in texture mapping.
    DirectX::XMFLOAT4X4 MatTransform = MathHelper::Identity4x4();
};

// Simple struct to represent a material for our demos.  A production 3D engine
// would likely create a class hierarchy of Materials.
struct Material
{
    // Unique material name for lookup.
    std::string Name;

    // Index into constant buffer corresponding to this material.
    int MatCBIndex = -1;

    // Index into SRV heap for diffuse texture.
    int DiffuseSrvHeapIndex = -1;

    // Index into SRV heap for normal texture.
    int NormalSrvHeapIndex = -1;

    // Dirty flag indicating the material has changed and we need to update the constant buffer.
    // Because we have a material constant buffer for each FrameResource, we have to apply the
    // update to each FrameResource.  Thus, when we modify a material we should set 
    // NumFramesDirty = gNumFrameResources so that each frame resource gets the update.
    int NumFramesDirty = 3;

    // Material constant buffer data used for shading.
    DirectX::XMFLOAT4   DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
    DirectX::XMFLOAT3   FresnelR0 = { 0.01f, 0.01f, 0.01f };
    float               Roughness = .25f; //0-1,0:代表完全光滑 shininess = 1 – roughness
    DirectX::XMFLOAT4X4 MatTransform = MathHelper::Identity4x4();
};
 
struct Light
{
    DirectX::XMFLOAT3 Strength = { 0.5f, 0.5f, 0.5f };
    float FalloffStart = 1.0f;                          // point/spot light only
    DirectX::XMFLOAT3 Direction = { 0.0f, -1.0f, 0.0f };// directional/spot light only
    float FalloffEnd = 10.0f;                           // point/spot light only
    DirectX::XMFLOAT3 Position = { 0.0f, 0.0f, 0.0f };  // point/spot light only
    float SpotPower = 64.0f;                            // spot light only
};

#ifndef ThrowIfFailed
#define ThrowIfFailed(x)                                              \
{                                                                     \
    HRESULT hr__ = (x);                                               \
    std::wstring wfn = AnsiToWstring(__FILE__);                       \
    if(FAILED(hr__)) { throw DxException(hr__, L#x, wfn, __LINE__); } \
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
