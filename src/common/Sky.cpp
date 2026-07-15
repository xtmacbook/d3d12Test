#include "Sky.h"
#include "D3DContext.h"
#include "GeometryGenerator.h"
#include "Geometry.h"
#include "Struct.h"
#include "UploadBuffer.h"
#include "common/DDSTextureLoader.h"
#include "context/interface/GeomtryContextInterface.h"

using namespace DirectX;
using namespace DirectX::PackedVector;
using Microsoft::WRL::ComPtr;

Sky::Sky(D3DContext *context):m_context(context)
{
}

void Sky::BuildPSO()
{
    std::vector<D3D12_INPUT_ELEMENT_DESC>	 InputLayout ={
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

    D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePsoDesc;
	ZeroMemory(&opaquePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	opaquePsoDesc.InputLayout = { InputLayout.data(), (UINT)InputLayout.size() };
	opaquePsoDesc.pRootSignature = m_skySignature.Get();
	opaquePsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(m_Shaders["skyVS"]->GetBufferPointer()),
		m_Shaders["skyVS"]->GetBufferSize()
	};
	opaquePsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(m_Shaders["skyPS"]->GetBufferPointer()),
		m_Shaders["skyPS"]->GetBufferSize()
	};

	opaquePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	opaquePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	opaquePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    opaquePsoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    opaquePsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	opaquePsoDesc.SampleMask = UINT_MAX;
	opaquePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	opaquePsoDesc.NumRenderTargets = 1;
	opaquePsoDesc.RTVFormats[0] = m_context->m_BackBufferFormat;
	opaquePsoDesc.SampleDesc.Count = m_context->m_4xMsaaState ? 4 : 1;
	opaquePsoDesc.SampleDesc.Quality = m_context->m_4xMsaaState ? (m_context->m_4xMsaaQuality - 1) : 0;
	opaquePsoDesc.DSVFormat = m_context->m_DepthStencilFormat;
	ThrowIfFailed(m_context->m_d3dDevice->CreateGraphicsPipelineState(&opaquePsoDesc, IID_PPV_ARGS(&m_skyPSO)));
}

std::shared_ptr<MeshGeometry> BuildSphere(ID3D12Device* device,
	ID3D12GraphicsCommandList* mCommandList, SphereProfile profile)
{
	GeometryGenerator geoGen;
	GeometryGenerator::MeshData sphere = geoGen.CreateSphere(profile.radius, profile.sliceCount, profile.stackCount);

	SubmeshGeometry boxSubmesh;
	boxSubmesh.m_IndexCount = (UINT)sphere.Indices32.size();
	boxSubmesh.m_StartIndexLocation = 0;
	boxSubmesh.m_BaseVertexLocation = 0;


	std::vector<VertexNT> vertices(sphere.Vertices.size());

	for (size_t i = 0; i < sphere.Vertices.size(); ++i)
	{
		vertices[i].Pos = sphere.Vertices[i].Position;
		vertices[i].Normal = sphere.Vertices[i].Normal;
		vertices[i].TexC = sphere.Vertices[i].TexC;
	}

	std::vector<std::uint16_t> indices = sphere.GetIndices16();

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(VertexNT);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "sphereGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->m_VertexBufferCPU));
	CopyMemory(geo->m_VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->m_IndexBufferCPU));
	CopyMemory(geo->m_IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->m_VertexBufferGPU = D3DUtil::CreateDefaultBuffer(device,
		mCommandList, vertices.data(), vbByteSize, geo->m_VertexBufferUploader);

	geo->m_IndexBufferGPU = D3DUtil::CreateDefaultBuffer(device,
		mCommandList, indices.data(), ibByteSize, geo->m_IndexBufferUploader);

	geo->m_VertexByteStride = sizeof(VertexNT);
	geo->m_VertexBufferByteSize = vbByteSize;
	geo->m_IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->m_IndexBufferByteSize = ibByteSize;

	geo->m_DrawArgs["sphere"] = boxSubmesh;
	return geo;
}


void Sky::BuildSkyGeometry()
{
	SphereProfile profile{ 0.5f, 20, 20 };

	m_skyGeo = BuildSphere(m_context->m_d3dDevice.Get(),
		m_context->m_CommandList.Get(), profile);

}

void Sky::BuildResource()
{
	auto cubeMapTexture = std::make_unique<Texture>();

	cubeMapTexture->m_Name = "cubeMap";
	cubeMapTexture->m_Filename = SourcePath() + L"/Textures/grasscube1024.dds";

	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(m_context->m_d3dDevice.Get(),
		m_context->m_CommandList.Get(), cubeMapTexture->m_Filename.c_str(),
		cubeMapTexture->m_Resource, cubeMapTexture->m_UploadHeap));

	m_TextureResource = std::move(cubeMapTexture);

}

void Sky::BuildRootSignature()
{
	CD3DX12_ROOT_PARAMETER slotRootParameter[3];
	//pass
	slotRootParameter[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	slotRootParameter[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	slotRootParameter[0].Descriptor.RegisterSpace = 0;
	slotRootParameter[0].Descriptor.ShaderRegister = 0;

	//texture cub map
	D3D12_DESCRIPTOR_RANGE texTable[1];
	texTable[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	texTable[0].NumDescriptors = 1;
	texTable[0].BaseShaderRegister = 0;
	texTable[0].RegisterSpace = 0;
	texTable[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	slotRootParameter[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	slotRootParameter[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;//change
	slotRootParameter[1].DescriptorTable.NumDescriptorRanges = 1;
	slotRootParameter[1].DescriptorTable.pDescriptorRanges = texTable;

	//sample
	D3D12_DESCRIPTOR_RANGE samplerTable[1];
	samplerTable[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
	samplerTable[0].NumDescriptors = 1;
	samplerTable[0].BaseShaderRegister = 0;
	samplerTable[0].RegisterSpace = 0;
	samplerTable[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	slotRootParameter[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	slotRootParameter[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	slotRootParameter[2].DescriptorTable.NumDescriptorRanges = 1;
	slotRootParameter[2].DescriptorTable.pDescriptorRanges = samplerTable;


	D3D12_ROOT_SIGNATURE_DESC descRootSignature;
	descRootSignature.NumStaticSamplers = 0;
	descRootSignature.pStaticSamplers = nullptr;
	descRootSignature.pParameters = slotRootParameter;
	descRootSignature.NumParameters = _countof(slotRootParameter);
	descRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&descRootSignature, D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

	if (errorBlob != nullptr)
	{
		::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	}
	ThrowIfFailed(hr);

	ThrowIfFailed(m_context->m_d3dDevice->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(m_skySignature.GetAddressOf())));
}

void Sky::BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuDescriptor,
	CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuDescriptor, UINT descriptorSize)
{
	m_CubeMapCPUSrv = hCpuDescriptor;
	m_CubeMapGpuSRrv = hGpuDescriptor;


	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = m_TextureResource->m_Resource->GetDesc().Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	srvDesc.TextureCube.MostDetailedMip = 0;
	srvDesc.TextureCube.MipLevels = m_TextureResource->m_Resource->GetDesc().MipLevels;
	srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
	m_context->m_d3dDevice->CreateShaderResourceView(m_TextureResource->m_Resource.Get(), &srvDesc, m_CubeMapCPUSrv);
}

void Sky::BuildLayout()
{
    m_Shaders["skyVS"] = D3DUtil::CompileShader(SourcePath() +  L"/Shaders/Sky.hlsl", nullptr, "VS", "vs_5_1");
	m_Shaders["skyPS"] = D3DUtil::CompileShader(SourcePath() + L"/Shaders/Sky.hlsl", nullptr, "PS", "ps_5_1");

}

void Sky::DrawSky(ID3D12GraphicsCommandList* cmdList,  
	ID3D12CommandAllocator* allocator, 
	CD3DX12_GPU_DESCRIPTOR_HANDLE samplerHandle,
	D3D12_GPU_VIRTUAL_ADDRESS passGPUAddress, ID3D12DescriptorHeap* descriptorHeaps[])
{
	cmdList->SetPipelineState(m_skyPSO.Get());
	cmdList->SetGraphicsRootSignature(m_skySignature.Get());

	//cmdList->SetDescriptorHeaps(2, descriptorHeaps);


	cmdList->IASetVertexBuffers(0, 1, &m_skyGeo->VertexBufferView());
	cmdList->IASetIndexBuffer(&m_skyGeo->IndexBufferView());
	cmdList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	
	cmdList->SetGraphicsRootConstantBufferView(0, passGPUAddress);//2
	cmdList->SetGraphicsRootDescriptorTable(1, m_CubeMapGpuSRrv);
	cmdList->SetGraphicsRootDescriptorTable(2, samplerHandle);

	//draw
	cmdList->DrawIndexedInstanced(m_skyGeo->m_DrawArgs["sphere"].m_IndexCount,
		1, m_skyGeo->m_DrawArgs["sphere"].m_StartIndexLocation,
		m_skyGeo->m_DrawArgs["sphere"].m_BaseVertexLocation, 0);
}
