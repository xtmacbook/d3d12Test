#pragma once

#include "Util.h"

namespace SDKMesh
{
    struct Effect
    {
        Microsoft::WRL::ComPtr<ID3D12PipelineState> m_PSO;
    };

    struct EffectPipelineStateDescription
    {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC desc;
        Microsoft::WRL::ComPtr<ID3DBlob> standardVS;
        Microsoft::WRL::ComPtr<ID3DBlob> opaquesPS;
        Microsoft::WRL::ComPtr<ID3DBlob> alphaPS;
        ID3D12Device* device;
    };

}