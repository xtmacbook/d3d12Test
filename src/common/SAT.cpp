#include "SAT.h"

#include "context/interface/TexContextInterface.h"

#include <cmath>
#include <array>
#include <algorithm>

using namespace DirectX;
using namespace DirectX::PackedVector;
using Microsoft::WRL::ComPtr;

static const UINT kGroupSize = 1024;   // must match GROUP_SIZE in sat.hlsl

struct SATConstBuffer
{
    UINT m_Width;         // row length
    UINT m_Height;        // column count
    UINT m_TilesPerRow;   // ceil(g_Width / GROUP_SIZE)
    UINT m_TilesPerCol;   // ceil(g_Height / GROUP_SIZE)
    UINT m_Slice;
};

SAT::SAT(ID3D12Device *device, UINT width, UINT height, DXGI_FORMAT format, UINT depthOrArraySize)
    : m_Width(width),
      m_Height(height),
      m_DepthOrArraySize(depthOrArraySize),
      m_d3dDevice(device),
      m_Format(format)
{
    BuildResources();
}

void SAT::OnResize(UINT newWidth, UINT newHeight)
{
    if ((m_Width != newWidth) || (m_Height != newHeight))
    {
        m_Width = newWidth;
        m_Height = newHeight;
        BuildResources();
        // descriptors need to be re-run externally via UdateDescriptors after resize.
    }
}

// Create the result SAT texture + three scan scratch buffers.
// Note: scratch buffers are RWStructuredBuffer<float2> (8 bytes / element).
void SAT::BuildResources()
{
    // ---- result SAT texture (RWTexture2DArray<float2>, R32G32_FLOAT) ----
    TextureOutDes satDesc;
    satDesc.Width    = (int)m_Width;
    satDesc.Height   = (int)m_Height;
    satDesc.ArraySize = (int)m_DepthOrArraySize;
    satDesc.Format   = m_Format;
    TexContextInterface::BuildUAVTextureResouce(m_d3dDevice, satDesc, m_GeneratedSATSource);

    const UINT tilesPerRow  = (UINT)std::ceil((float)m_Width  / (float)kGroupSize);
    const UINT tilesPerCol  = (UINT)std::ceil((float)m_Height / (float)kGroupSize);

    // per layer, the block buffer count = max(row tile size, col tile size);
    // equal for square maps.
    const UINT blockPerLayer = (std::max)(tilesPerRow * m_Height, m_Width * tilesPerCol);

    auto createStructuredBuffer = [this](ComPtr<ID3D12Resource>& out, UINT64 byteSize)
    {
        ThrowIfFailed(m_d3dDevice->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(byteSize),
            D3D12_RESOURCE_STATE_COMMON,
            nullptr,
            IID_PPV_ARGS(&out)));
    };

    const UINT64 float2Size = sizeof(XMFLOAT2);   // 8 bytes
    createStructuredBuffer(m_RowBufSource,  (UINT64)m_Width * m_Height * m_DepthOrArraySize * float2Size);
    createStructuredBuffer(m_BlockSumSource,(UINT64)blockPerLayer * m_DepthOrArraySize * float2Size);
    createStructuredBuffer(m_BlockOffSource,(UINT64)blockPerLayer * m_DepthOrArraySize * float2Size);
}

void SAT::BuildCSShader()
{
    D3D_SHADER_MACRO defines[] =
    {
        "USE_ARRAY", "1",
        nullptr, nullptr
    };

    m_Shader["RowStage1"] = D3DUtil::CompileShader(SourcePath() + L"/Shaders/sat.hlsl", defines, "RowStage1", "cs_5_1");
    m_Shader["RowStage2"] = D3DUtil::CompileShader(SourcePath() + L"/Shaders/sat.hlsl", defines, "RowStage2", "cs_5_1");
    m_Shader["RowStage3"] = D3DUtil::CompileShader(SourcePath() + L"/Shaders/sat.hlsl", defines, "RowStage3", "cs_5_1");
    m_Shader["ColStage1"] = D3DUtil::CompileShader(SourcePath() + L"/Shaders/sat.hlsl", defines, "ColStage1", "cs_5_1");
    m_Shader["ColStage2"] = D3DUtil::CompileShader(SourcePath() + L"/Shaders/sat.hlsl", defines, "ColStage2", "cs_5_1");
    m_Shader["ColStage3"] = D3DUtil::CompileShader(SourcePath() + L"/Shaders/sat.hlsl", defines, "ColStage3", "cs_5_1");
}

void SAT::BuildRootSignature()
{
    // 0: constants (b0, 5 DWORDs, filled with SetComputeRoot32BitConstants)
    // 1: SRV table (t0, input)
    // 2: UAV table (u0..u3, g_SAT / RowBuf / BlockSum / BlockOff)
    CD3DX12_DESCRIPTOR_RANGE srvTable;
    srvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

    CD3DX12_DESCRIPTOR_RANGE uavTable;
    uavTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 4, 0);   // 4 consecutive UAVs

    CD3DX12_ROOT_PARAMETER slotRootParameter[3];
    // 5 uints (Width/Height/TilesPerRow/TilesPerCol/Slice), register b0
    slotRootParameter[0].InitAsConstants(5, 0);
    slotRootParameter[1].InitAsDescriptorTable(1, &srvTable);
    slotRootParameter[2].InitAsDescriptorTable(1, &uavTable);

    D3D12_ROOT_SIGNATURE_DESC descRootSignature;
    descRootSignature.NumStaticSamplers = 0;
    descRootSignature.pStaticSamplers = nullptr;
    descRootSignature.pParameters = slotRootParameter;
    descRootSignature.NumParameters = _countof(slotRootParameter);
    descRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

    ComPtr<ID3DBlob> serializedRootSig = nullptr;
    ComPtr<ID3DBlob> errorBlob = nullptr;
    HRESULT hr = D3D12SerializeRootSignature(&descRootSignature, D3D_ROOT_SIGNATURE_VERSION_1,
        serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

    if (errorBlob != nullptr)
        ::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
    ThrowIfFailed(hr);

    ThrowIfFailed(m_d3dDevice->CreateRootSignature(0,
        serializedRootSig->GetBufferPointer(), serializedRootSig->GetBufferSize(),
        IID_PPV_ARGS(m_PSignature.GetAddressOf())));
}

void SAT::BuildComputePipeLineState()
{
    D3D12_COMPUTE_PIPELINE_STATE_DESC computePsoDesc = {};
    ZeroMemory(&computePsoDesc, sizeof(D3D12_COMPUTE_PIPELINE_STATE_DESC));
    computePsoDesc.pRootSignature = m_PSignature.Get();
    computePsoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

    const char* stageNames[] = { "RowStage1", "RowStage2", "RowStage3", "ColStage1", "ColStage2", "ColStage3" };
    for (const char* name : stageNames)
    {
        computePsoDesc.CS = {
            reinterpret_cast<BYTE*>(m_Shader[name]->GetBufferPointer()),
            m_Shader[name]->GetBufferSize()
        };
        ThrowIfFailed(m_d3dDevice->CreateComputePipelineState(&computePsoDesc, IID_PPV_ARGS(&m_PSOs[name])));
    }
}

void SAT::UdateDescriptors(SATHeapDescriptor heapDescriptor)
{
    m_HeapDescriptor = heapDescriptor;

    const UINT descriptorSize = m_d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    auto cpuStart = heapDescriptor.m_descriptorHeap->GetCPUDescriptorHandleForHeapStart();
    auto gpuStart = heapDescriptor.m_descriptorHeap->GetGPUDescriptorHandleForHeapStart();

    // input SRV: caller creates the shadow-map SRV at this offset; we only record the GPU handle.
    m_InputSrv = CD3DX12_GPU_DESCRIPTOR_HANDLE(gpuStart, heapDescriptor.m_InputSrvOffset, descriptorSize);

    // ---- SAT texture UAV ----
    CD3DX12_CPU_DESCRIPTOR_HANDLE satCpu(cpuStart, heapDescriptor.m_GenerateSATUavOffset, descriptorSize);
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = m_Format;
    if (m_DepthOrArraySize > 1)
    {
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
        uavDesc.Texture2DArray.MipSlice = 0;
        uavDesc.Texture2DArray.FirstArraySlice = 0;
        uavDesc.Texture2DArray.ArraySize = m_DepthOrArraySize;
    }
    else
    {
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        uavDesc.Texture2D.MipSlice = 0;
    }
    m_d3dDevice->CreateUnorderedAccessView(m_GeneratedSATSource.Get(), nullptr, &uavDesc, satCpu);
    m_GenerateSATUav = CD3DX12_GPU_DESCRIPTOR_HANDLE(gpuStart, heapDescriptor.m_GenerateSATUavOffset, descriptorSize);

    // ---- three structured-buffer UAVs ----
    const UINT tilesPerRow = (UINT)std::ceil((float)m_Width  / (float)kGroupSize);
    const UINT tilesPerCol = (UINT)std::ceil((float)m_Height / (float)kGroupSize);
    const UINT blockPerLayer = (std::max)(tilesPerRow * m_Height, m_Width * tilesPerCol);

    auto createBufferUav = [&](ComPtr<ID3D12Resource>& res, CD3DX12_CPU_DESCRIPTOR_HANDLE cpu,
                               CD3DX12_GPU_DESCRIPTOR_HANDLE& gpu, UINT offset, UINT numElements)
    {
        D3D12_UNORDERED_ACCESS_VIEW_DESC bvd = {};
        bvd.Format = DXGI_FORMAT_UNKNOWN;
        bvd.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
        bvd.Buffer.FirstElement = 0;
        bvd.Buffer.NumElements = numElements;
        bvd.Buffer.StructureByteStride = sizeof(XMFLOAT2);   // 8
        bvd.Buffer.CounterOffsetInBytes = 0;
        m_d3dDevice->CreateUnorderedAccessView(res.Get(), nullptr, &bvd, cpu);
        gpu = CD3DX12_GPU_DESCRIPTOR_HANDLE(gpuStart, offset, descriptorSize);
    };

    CD3DX12_CPU_DESCRIPTOR_HANDLE rowCpu(cpuStart, heapDescriptor.m_RowBufUavOffset, descriptorSize);
    createBufferUav(m_RowBufSource, rowCpu, m_RowBufUav,
        heapDescriptor.m_RowBufUavOffset, (UINT)m_Width * m_Height * m_DepthOrArraySize);

    CD3DX12_CPU_DESCRIPTOR_HANDLE sumCpu(cpuStart, heapDescriptor.m_BlockSumUavOffset, descriptorSize);
    createBufferUav(m_BlockSumSource, sumCpu, m_BlockSumUav,
        heapDescriptor.m_BlockSumUavOffset, blockPerLayer * m_DepthOrArraySize);

    CD3DX12_CPU_DESCRIPTOR_HANDLE offCpu(cpuStart, heapDescriptor.m_BlockOffUavOffset, descriptorSize);
    createBufferUav(m_BlockOffSource, offCpu, m_BlockOffUav,
        heapDescriptor.m_BlockOffUavOffset, blockPerLayer * m_DepthOrArraySize);

    // Note: the four UAV descriptors must be CONSECUTIVE starting at
    // m_GenerateSATUavOffset, so SetComputeRootDescriptorTable(2, m_GenerateSATUav)
    // can be used as u0..u3.
}

void SAT::Execute(ID3D12GraphicsCommandList *cmdList, ID3D12Resource *input)
{
    const UINT tilesPerRow = (UINT)std::ceil((float)m_Width  / (float)kGroupSize);
    const UINT tilesPerCol = (UINT)std::ceil((float)m_Height / (float)kGroupSize);

    // Bring internal UAV resources from COMMON -> UNORDERED_ACCESS
    // (input stays GENERIC_READ / SRV, guaranteed by the caller).
    std::array<ID3D12Resource*, 4> uavs = {
        m_GeneratedSATSource.Get(), m_RowBufSource.Get(), m_BlockSumSource.Get(), m_BlockOffSource.Get()
    };
    {
        std::array<D3D12_RESOURCE_BARRIER, 4> barriers;
        for (int i = 0; i < 4; ++i)
            barriers[i] = CD3DX12_RESOURCE_BARRIER::Transition(uavs[i],
                D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        cmdList->ResourceBarrier(4, barriers.data());
    }

    cmdList->SetComputeRootSignature(m_PSignature.Get());
    cmdList->SetComputeRootDescriptorTable(1, m_InputSrv);
    cmdList->SetComputeRootDescriptorTable(2, m_GenerateSATUav);   // u0..u3

    auto uavBarrier = [&cmdList](ID3D12Resource* r)
    {
        D3D12_RESOURCE_BARRIER b = {};
        b.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
        b.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        b.UAV.pResource = r;
        cmdList->ResourceBarrier(1, &b);
    };

    for (UINT slice = 0; slice < m_DepthOrArraySize; ++slice)
    {
        SATConstBuffer cb;
        cb.m_Width       = m_Width;
        cb.m_Height      = m_Height;
        cb.m_TilesPerRow = tilesPerRow;
        cb.m_TilesPerCol = tilesPerCol;
        cb.m_Slice       = slice;
        cmdList->SetComputeRoot32BitConstants(0, 5, &cb, 0);

        // Row 3 stages
        cmdList->SetPipelineState(m_PSOs["RowStage1"].Get());
        cmdList->Dispatch(tilesPerRow, m_Height, 1);
        uavBarrier(m_GeneratedSATSource.Get()); uavBarrier(m_RowBufSource.Get());
        uavBarrier(m_BlockSumSource.Get()); uavBarrier(m_BlockOffSource.Get());

        cmdList->SetPipelineState(m_PSOs["RowStage2"].Get());
        cmdList->Dispatch(1, m_Height, 1);
        uavBarrier(m_GeneratedSATSource.Get()); uavBarrier(m_RowBufSource.Get());
        uavBarrier(m_BlockSumSource.Get()); uavBarrier(m_BlockOffSource.Get());

        cmdList->SetPipelineState(m_PSOs["RowStage3"].Get());
        cmdList->Dispatch(tilesPerRow, m_Height, 1);
        uavBarrier(m_GeneratedSATSource.Get()); uavBarrier(m_RowBufSource.Get());
        uavBarrier(m_BlockSumSource.Get()); uavBarrier(m_BlockOffSource.Get());

        // Col 3 stages
        cmdList->SetPipelineState(m_PSOs["ColStage1"].Get());
        cmdList->Dispatch(m_Width, tilesPerCol, 1);
        uavBarrier(m_GeneratedSATSource.Get()); uavBarrier(m_RowBufSource.Get());
        uavBarrier(m_BlockSumSource.Get()); uavBarrier(m_BlockOffSource.Get());

        cmdList->SetPipelineState(m_PSOs["ColStage2"].Get());
        cmdList->Dispatch(m_Width, 1, 1);
        uavBarrier(m_GeneratedSATSource.Get()); uavBarrier(m_RowBufSource.Get());
        uavBarrier(m_BlockSumSource.Get()); uavBarrier(m_BlockOffSource.Get());

        cmdList->SetPipelineState(m_PSOs["ColStage3"].Get());
        cmdList->Dispatch(m_Width, tilesPerCol, 1);
        uavBarrier(m_GeneratedSATSource.Get()); uavBarrier(m_RowBufSource.Get());
        uavBarrier(m_BlockSumSource.Get()); uavBarrier(m_BlockOffSource.Get());
    }

    // Back to COMMON so a subsequent CopyResource / dispatch can reuse it.
    {
        std::array<D3D12_RESOURCE_BARRIER, 4> barriers;
        for (int i = 0; i < 4; ++i)
            barriers[i] = CD3DX12_RESOURCE_BARRIER::Transition(uavs[i],
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON);
        cmdList->ResourceBarrier(4, barriers.data());
    }
}

ID3D12Resource *SAT::SATResource()
{
    return m_GeneratedSATSource.Get();
}
