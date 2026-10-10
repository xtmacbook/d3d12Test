#pragma once

#include "Util.h"

/**
 * 
The key to projective texturing is to generate texture coordinates for each
pixel in such a way that the applied texture looks like it has been projected onto
the geometry. We will call such generated texture coordinates projective texture
coordinates.

From Figure 20.4, we see that the texture coordinates (u, v) identify the texel
that should be projected onto the 3D point p. But the coordinates (u, v) precisely
identify the projection of p on the projection window, relative to a texture space
coordinate system on the projection window. 

So the strategy of generating projective texture coordinates is as follows:
1. Project the point p onto the light’s projection window and transform the
coordinates to NDC space.

2. Transform the projected coordinates from NDC space to texture space,
thereby effectively turning them into texture coordinates.

Step 1 can be implemented by thinking of the light projector as a camera. We
define a view matrix V and projection matrix P for the light projector. Together,
these matrices essentially define the position, orientation, and frustum of the light
projector in the world. The matrix V transforms coordinates from world space to



 */

enum SHADOW_TEXTURE_FORMAT 
{
    SHADOW_DXGI_FORMAT_R32_TYPELESS,
    SHADOW_DXGI_FORMAT_R24G8_TYPELESS,
    SHADOW_DXGI_FORMAT_R16_TYPELESS,
    SHADOW_DXGI_FORMAT_R8_TYPELESS,
    SHADOW_DXGI_FORMAT_R32G32_TYPELESS, //vsm中使用存储两个变量(depth和variance)
    SHADOW_DXGI_FORMAT_R16G16_TYPELESS //vsm中使用存储两个变量(depth和variance)

};

class ShadowMapRes
{
public:
    /*
    默认为dsv,否则为rtv
   */
    ShadowMapRes(ID3D12Device *device,UINT width, UINT height,
        SHADOW_TEXTURE_FORMAT format = 
        SHADOW_TEXTURE_FORMAT::SHADOW_DXGI_FORMAT_R24G8_TYPELESS,BOOL dsvOrRtv = true);

    ShadowMapRes(const ShadowMapRes &rhs) = delete;
    ShadowMapRes&operator=(const ShadowMapRes&rhs) = delete;
    ~ShadowMapRes() = default;

    UINT Width() const;
    UINT Height() const;
    
    ID3D12Resource *Resource();
    
    CD3DX12_GPU_DESCRIPTOR_HANDLE Srv() const;
    CD3DX12_CPU_DESCRIPTOR_HANDLE Dsv() const;
    CD3DX12_CPU_DESCRIPTOR_HANDLE Rtv() const;
    
    D3D12_VIEWPORT Viewport() const;
    D3D12_RECT ScissorRect() const;
    
    void BuildDescriptors(
        CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv,
        CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv,
        CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuDsvOrRtv);

    void SetDescriptors(
        CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv,
        CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv,
        CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuDsvOrRtv);


    void DepthBias();

    void orthProj();

    void DepthFilter();

    void OnResize(UINT newWidth, UINT newHeight);

private:
    void BuildDescriptors();
    void BuildResource();

private:
    ID3D12Device *              m_d3dDevice = nullptr;
    D3D12_VIEWPORT              m_Viewport;
    D3D12_RECT                  m_ScissorRect;
    UINT                        m_Width = 0;
    UINT                        m_Height = 0;
    SHADOW_TEXTURE_FORMAT       m_Format;

    CD3DX12_CPU_DESCRIPTOR_HANDLE m_hCpuSrv;
    CD3DX12_GPU_DESCRIPTOR_HANDLE m_hGpuSrv;

    CD3DX12_CPU_DESCRIPTOR_HANDLE m_hCpuDsvOrRtv;
    
    Microsoft::WRL::ComPtr<ID3D12Resource> m_ShadowMap = nullptr;
	BOOL                        m_dsvOrRtv; //true为dsv,false为rtv
};