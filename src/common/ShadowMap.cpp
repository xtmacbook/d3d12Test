#include "ShadowMap.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;

ShadowMap::ShadowMap(ID3D12Device* device, UINT width, UINT height)
    : m_d3dDevice(device)
    , m_Width(width)
    , m_Height(height)
{
    m_Viewport.TopLeftX = 0.0f;
    m_Viewport.TopLeftY = 0.0f;
    m_Viewport.Width = static_cast<float>(m_Width);
    m_Viewport.Height = static_cast<float>(m_Height);
    m_Viewport.MinDepth = 0.0f;
    m_Viewport.MaxDepth = 1.0f;

    m_ScissorRect = { 0, 0, static_cast<LONG>(m_Width), static_cast<LONG>(m_Height) };

    BuildResource();
}

UINT ShadowMap::Width() const
{
    return m_Width;
}

UINT ShadowMap::Height() const
{
    return m_Height;
}

ID3D12Resource* ShadowMap::Resource()
{
    return m_ShadowMap.Get();
}

CD3DX12_GPU_DESCRIPTOR_HANDLE ShadowMap::Srv() const
{
    return m_hGpuSrv;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE ShadowMap::Dsv() const
{
    return m_hCpuDsv;
}

D3D12_VIEWPORT ShadowMap::Viewport() const
{
    return m_Viewport;
}

D3D12_RECT ShadowMap::ScissorRect() const
{
    return m_ScissorRect;
}

void ShadowMap::BuildDescriptors(
    CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv,
    CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv,
    CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuDsv)
{
    m_hCpuSrv = hCpuSrv;
    m_hGpuSrv = hGpuSrv;
    m_hCpuDsv = hCpuDsv;

    BuildDescriptors();
}

void ShadowMap::DepthBias()
{
}

void ShadowMap::orthProj()
{
}

void ShadowMap::DepthFilter()
{
 // we should not average depth values and use the Percentage closer filter(PCF),point filtering(MIN_MAG_MIP_POINT)
    //bilinearly interpolate the shadow map result

    //Direct3D 11开始通过SampleCmpLevelZero方法 支持PCF
 
    /*
     only the following formats support comparison
        filters : R32_FLOAT_X8X24_TYPELESS, R32_FLOAT, R24_UNORM_X8_TYPELESS, R16_UNORM.
    */
    //depth sampler desc for shadow mapping
}

void ShadowMap::OnResize(UINT newWidth, UINT newHeight)
{
    if ((m_Width != newWidth) || (m_Height != newHeight))
    {
        m_Width = newWidth;
        m_Height = newHeight;

        BuildResource();
        BuildDescriptors();
    }
}

void ShadowMap::BuildDescriptors()
{
    // Create SRV to resource so we can sample the shadow map in a shader program.
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = 1;
    srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
    srvDesc.Texture2D.PlaneSlice = 0;
    m_d3dDevice->CreateShaderResourceView(m_ShadowMap.Get(), &srvDesc, m_hCpuSrv);

    // Create DSV to resource so we can render to the shadow map.
    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
    dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dsvDesc.Texture2D.MipSlice = 0;
    m_d3dDevice->CreateDepthStencilView(m_ShadowMap.Get(), &dsvDesc, m_hCpuDsv);
}

void ShadowMap::BuildResource()
{
    D3D12_RESOURCE_DESC texDesc = {};
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Alignment = 0;
    texDesc.Width = m_Width;
    texDesc.Height = m_Height;
    texDesc.DepthOrArraySize = 1;
    texDesc.MipLevels = 1;
    texDesc.Format = m_Format;
    texDesc.SampleDesc.Count = 1;
    texDesc.SampleDesc.Quality = 0;
    texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE optClear;
    optClear.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    optClear.DepthStencil.Depth = 1.0f;
    optClear.DepthStencil.Stencil = 0;

    m_d3dDevice->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &texDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        &optClear,
        IID_PPV_ARGS(&m_ShadowMap));
}
 
 
