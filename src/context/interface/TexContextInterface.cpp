#include "TexContextInterface.h"
#include "../../common/DDSTextureLoader.h"
#include "../../common/data.h"

bool TexContextInterface::loadTextures(ID3D12Device* md3dDevice, 
	ID3D12GraphicsCommandList* mCommandList, std::unordered_map<std::string, std::wstring>&files)
{

	for (auto item : files)
	{
		auto woodCrateTex = std::make_unique<Texture>();
		woodCrateTex->m_Name = item.first;;
		woodCrateTex->m_Filename = item.second;

		ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice,
			mCommandList, woodCrateTex->m_Filename.c_str(),
			woodCrateTex->m_Resource, woodCrateTex->m_UploadHeap));

		m_Textures[woodCrateTex->m_Name] = std::move(woodCrateTex);
	}

	return true;
}

void TexContextInterface::BuildSampleDescriptorHeap(ID3D12Device* md3dDevice)
{
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.NumDescriptors = 1;
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(
		&srvHeapDesc, IID_PPV_ARGS(&m_SamplerDescriptorHeap)));
}

void TexContextInterface::BuildSampleDescriptor(ID3D12Device* md3dDevice, ID3D12GraphicsCommandList* mCommandList)
{

	D3D12_SAMPLER_DESC samplerDesc = {};
	samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplerDesc.MinLOD = 0;
	samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
	samplerDesc.MipLODBias = 0.0f;
	samplerDesc.MaxAnisotropy = 1;
	samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	md3dDevice->CreateSampler(&samplerDesc,
		m_SamplerDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
}

void TexContextInterface::BuildSRVDescriptorHeap(ID3D12Device* md3dDevice)
{
	//create srv descriptor headp
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.NumDescriptors = m_Textures.size();
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(
		&srvHeapDesc, IID_PPV_ARGS(&m_SrvDescriptorHeap)));
}

void TexContextInterface::BuildSRCDescript(ID3D12Device* md3dDevice, int CbvSrvUavDescriptorSize)
{
	/*
		 MostDetailedMip: Specifies the index of the most detailed mipmap level to
		view.This will be a number between 0 and MipCount - 1.

		 MipLevels : The number of mipmap levels to view, starting at MostDetailedMip.
		This field, along with MostDetailedMip allows us to specify a subrange of
		mipmap levels to view.You can specify - 1 to indicate to view all mipmap levels
		from MostDetailedMip down to the last mipmap level.

		 ResourceMinLODClamp : Specifies the minimum mipmap level that can be
		accessed. 0.0 means all the mipmap levels can be accessed.Specifying 3.0
		means mipmap levels 3.0 to MipCount - 1 can be accessed.
	*/

	// Suppose the following texture resources are already created.

	int idx = 0;
	for (auto iter = m_Textures.begin(); iter != m_Textures.end(); iter++)
	{

		CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(
			m_SrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

		hDescriptor.Offset(idx, CbvSrvUavDescriptorSize);

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = iter->second->m_Resource->GetDesc().Format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = -1;
		md3dDevice->CreateShaderResourceView(iter->second->m_Resource.Get(), &srvDesc, hDescriptor);

		idx++;
	}
}

std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> TexContextInterface::getStaticSamplerDescriptor()
{
	/*

	It turns out that a graphics application usually only uses a handful of samplers.
		Therefore, Direct3D provides a special shortcut to define an array of samplers
		and set them without going through the process of creating a sampler heap.The
		Init function of the CD3DX12_ROOT_SIGNATURE_DESC class has two parameters that
		allow you to define an array of so - called static samplers your application can use.

		这个时候可能不需要创建sampler heap
	*/

	//和D3D12_SAMPLER_DESC非常相似
	 //D3D12_STATIC_SAMPLER_DESC staticSamplerDesc = {};

	const CD3DX12_STATIC_SAMPLER_DESC pointWrap(
		0, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP, // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP, // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW
	const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
		1, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP, // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP, // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW
	const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
		2, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP, // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP, // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW
	const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
		3, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP, // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP, // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW
	const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
		4, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP, // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP, // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP, // addressW
		0.0f, // mipLODBias
		8); // maxAnisotropy
	const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(
		5, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP, // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP, // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP, // addressW
		0.0f, // mipLODBias
		8); // maxAnisotropy
	return {
	pointWrap, pointClamp,
	linearWrap, linearClamp,
	anisotropicWrap, anisotropicClamp };

}
 