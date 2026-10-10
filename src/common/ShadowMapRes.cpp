#include "ShadowMapRes.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;

ShadowMapRes::ShadowMapRes(ID3D12Device *device, UINT width, UINT height, SHADOW_TEXTURE_FORMAT formate,
	 BOOL dsvOrRtv) 
    : m_d3dDevice(device), m_Width(width), m_Height(height), m_Format(formate),
    m_dsvOrRtv(dsvOrRtv)
{
    m_Viewport.TopLeftX = 0.0f;
    m_Viewport.TopLeftY = 0.0f;
    m_Viewport.Width = static_cast<float>(m_Width);
    m_Viewport.Height = static_cast<float>(m_Height);
    m_Viewport.MinDepth = 0.0f;
    m_Viewport.MaxDepth = 1.0f;

    m_ScissorRect = {0, 0, static_cast<LONG>(m_Width), static_cast<LONG>(m_Height)};

    BuildResource();
}

UINT ShadowMapRes::Width() const
{
    return m_Width;
}

UINT ShadowMapRes::Height() const
{
    return m_Height;
}

ID3D12Resource *ShadowMapRes::Resource()
{
    return m_ShadowMap.Get();
}

CD3DX12_GPU_DESCRIPTOR_HANDLE ShadowMapRes::Srv() const
{
    return m_hGpuSrv;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE ShadowMapRes ::Dsv() const
{
    return m_hCpuDsvOrRtv;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE ShadowMapRes::Rtv() const
{
    return m_hCpuDsvOrRtv;
}

D3D12_VIEWPORT ShadowMapRes::Viewport() const
{
    return m_Viewport;
}

D3D12_RECT ShadowMapRes::ScissorRect() const
{
    return m_ScissorRect;
}

void ShadowMapRes::BuildDescriptors(
    CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv,
    CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv,
    CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuDsvOrRtv)
{
    m_hCpuSrv = hCpuSrv;
    m_hGpuSrv = hGpuSrv;
    m_hCpuDsvOrRtv = hCpuDsvOrRtv;

    BuildDescriptors();
}

void ShadowMapRes::SetDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv,
                                  CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv, CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuDsvOrRtv)
{
    m_hCpuSrv = hCpuSrv;
    m_hGpuSrv = hGpuSrv;
    m_hCpuDsvOrRtv = hCpuDsvOrRtv;
}

void ShadowMapRes::DepthBias()
{
}

void ShadowMapRes::orthProj()
{
}

void ShadowMapRes::DepthFilter()
{
    // we should not average depth values and use the Percentage closer filter(PCF),point filtering(MIN_MAG_MIP_POINT)
    // bilinearly interpolate the shadow map result

    // Direct3D 11开始通过SampleCmpLevelZero方法 支持PCF

    /*
     only the following formats support comparison
        filters : R32_FLOAT_X8X24_TYPELESS, R32_FLOAT, R24_UNORM_X8_TYPELESS, R16_UNORM.
    */
    // depth sampler desc for shadow mapping
}

void ShadowMapRes::OnResize(UINT newWidth, UINT newHeight)
{
    if ((m_Width != newWidth) || (m_Height != newHeight))
    {
        m_Width = newWidth;
        m_Height = newHeight;

        BuildResource();
        BuildDescriptors();
    }
}

void ShadowMapRes::BuildDescriptors()
{
    DXGI_FORMAT SRVfmt = DXGI_FORMAT_R32_FLOAT;
    DXGI_FORMAT DSVfmt = DXGI_FORMAT_D32_FLOAT;  //注意这里是一个"D"
    DXGI_FORMAT RtVfmt = DXGI_FORMAT_D32_FLOAT;

    switch (m_Format)
    {
    case SHADOW_DXGI_FORMAT_R32_TYPELESS:
        SRVfmt = DXGI_FORMAT_R32_FLOAT;
        DSVfmt = DXGI_FORMAT_D32_FLOAT;
        break;
    case SHADOW_DXGI_FORMAT_R24G8_TYPELESS:
        SRVfmt = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
        DSVfmt = DXGI_FORMAT_D24_UNORM_S8_UINT;
        break;
    case SHADOW_DXGI_FORMAT_R16_TYPELESS:
        SRVfmt = DXGI_FORMAT_R16_UNORM;
        DSVfmt = DXGI_FORMAT_D16_UNORM;
        break;
    case SHADOW_DXGI_FORMAT_R8_TYPELESS:
        SRVfmt = DXGI_FORMAT_R8_UNORM;
        DSVfmt = DXGI_FORMAT_R8_UNORM;
        break;
    case SHADOW_DXGI_FORMAT_R32G32_TYPELESS:
        SRVfmt = DXGI_FORMAT_R32G32_FLOAT;
		break;
    case SHADOW_DXGI_FORMAT_R16G16_TYPELESS:
        SRVfmt = DXGI_FORMAT_R16G16_FLOAT;
    }

    RtVfmt = SRVfmt;

    // Create SRV to resource so we can sample the shadow map in a shader program.
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = SRVfmt;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = 1;
    srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
    srvDesc.Texture2D.PlaneSlice = 0;
    m_d3dDevice->CreateShaderResourceView(m_ShadowMap.Get(), &srvDesc, m_hCpuSrv);

    if (m_dsvOrRtv)
    {
        // Create DSV to resource so we can render to the shadow map.
        D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
        dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
        dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        dsvDesc.Format = DSVfmt;
        dsvDesc.Texture2D.MipSlice = 0;
        m_d3dDevice->CreateDepthStencilView(m_ShadowMap.Get(), &dsvDesc, m_hCpuDsvOrRtv);
    }
    else
    {
        D3D12_RENDER_TARGET_VIEW_DESC rtvDesc;
		rtvDesc.Format = RtVfmt;
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		rtvDesc.Texture2D.MipSlice = 0;
		rtvDesc.Texture2D.PlaneSlice = 0;
        m_d3dDevice->CreateRenderTargetView(m_ShadowMap.Get(), &rtvDesc, m_hCpuDsvOrRtv);
    }
}

void ShadowMapRes::BuildResource()
{
    DXGI_FORMAT texturefmt = DXGI_FORMAT_R32_TYPELESS;
    DXGI_FORMAT clearfmt = DXGI_FORMAT_R32_FLOAT;

    D3D12_RESOURCE_FLAGS resFlags = D3D12_RESOURCE_FLAG_NONE;

    switch (m_Format)
    {
    case SHADOW_DXGI_FORMAT_R32_TYPELESS:
        texturefmt = DXGI_FORMAT_R32_TYPELESS;
        clearfmt = DXGI_FORMAT_R32_FLOAT;
        break;
    case SHADOW_DXGI_FORMAT_R24G8_TYPELESS:
        texturefmt = DXGI_FORMAT_R24G8_TYPELESS;
        clearfmt = DXGI_FORMAT_D24_UNORM_S8_UINT;
        break;
    case SHADOW_DXGI_FORMAT_R16_TYPELESS:
        texturefmt = DXGI_FORMAT_R16_TYPELESS;
        clearfmt = DXGI_FORMAT_R16_UNORM;
        break;
    case SHADOW_DXGI_FORMAT_R8_TYPELESS:
        texturefmt = DXGI_FORMAT_R8_TYPELESS;
        clearfmt = DXGI_FORMAT_R8_UNORM;
        break;
	case SHADOW_DXGI_FORMAT_R32G32_TYPELESS:
		texturefmt = DXGI_FORMAT_R32G32_TYPELESS;
		clearfmt = DXGI_FORMAT_R32G32_FLOAT;
        break;
	case SHADOW_DXGI_FORMAT_R16G16_TYPELESS:
		texturefmt = DXGI_FORMAT_R16G16_TYPELESS;
		clearfmt = DXGI_FORMAT_R16G16_FLOAT;
        break;
    }

    if (m_dsvOrRtv) resFlags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	else resFlags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;   


    D3D12_RESOURCE_DESC texDesc = {};
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Alignment = 0;
    texDesc.Width = m_Width;
    texDesc.Height = m_Height;
    texDesc.DepthOrArraySize = 1;
    texDesc.MipLevels = 1;
    texDesc.Format = texturefmt;
    texDesc.SampleDesc.Count = 1;
    texDesc.SampleDesc.Quality = 0;
    texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    texDesc.Flags = resFlags;

    D3D12_CLEAR_VALUE optClear;

    optClear.Format = clearfmt;
    if (m_dsvOrRtv)
    {
        optClear.DepthStencil.Depth = 1.0f;
        optClear.DepthStencil.Stencil = 0;
    }
    else
    {
        optClear.Color[0] = 0.0f;
        optClear.Color[1] = 0.0f;
        optClear.Color[2] = 0.0f;
        optClear.Color[3] = 0.0f;
    }

    m_d3dDevice->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &texDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        &optClear,
        IID_PPV_ARGS(&m_ShadowMap));
}
