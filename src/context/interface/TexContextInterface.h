
#pragma once

#include "../../common/Util.h"

#include <unordered_map>
#include <map>
#include <memory>
#include <string>
#include <array>

struct Texture;

struct TextureLoadDesc
{
	TextureLoadDesc() = default;

	TextureLoadDesc(std::string n, std::wstring fn, bool array = false);

	std::string Name;
	std::wstring FileName;
	bool TextureArray = false;
};

struct TextureOutDes
{
	int Width;
	int Height;
	DXGI_FORMAT Format;
};

struct TextureOutResouce
{
	Microsoft::WRL::ComPtr<ID3D12Resource> Texture;

	CD3DX12_CPU_DESCRIPTOR_HANDLE		  CPUSrvHndle;
	CD3DX12_CPU_DESCRIPTOR_HANDLE		  CPUUavHndle;
};

class TexContextInterface 
{

public:

	virtual bool loadTextures(ID3D12Device*, ID3D12GraphicsCommandList*,std::vector<TextureLoadDesc>&);

	virtual void BuildSampleDescriptorHeap(ID3D12Device*);

	virtual void BuildSampleDescriptor(ID3D12Device* md3dDevice, ID3D12GraphicsCommandList* mCommandList);
	
	virtual void BuildSRVDescriptorHeap(ID3D12Device* md3dDevice);

	virtual void BuildSRCDescript(ID3D12Device* md3dDevice, int CbvSrvUavDescriptorSize);

	static void BuildUAVTexture(ID3D12Device*, TextureOutDes, TextureOutResouce&);

	static void BuildUAVTextureResouce(ID3D12Device*, TextureOutDes, Microsoft::WRL::ComPtr<ID3D12Resource>&);

	static void BuildUAVTextureResouceView(ID3D12Device*, TextureOutDes, TextureOutResouce&);

	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> getStaticSamplerDescriptor();

protected:

	std::unordered_map<std::string, std::unique_ptr<Texture>>						m_Textures;
	std::unordered_map<std::string, std::unique_ptr<Texture>>						m_TextureArrs;


	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>									m_SrvDescriptorHeap = nullptr; //for texture source
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>									m_SamplerDescriptorHeap = nullptr;
	
};
