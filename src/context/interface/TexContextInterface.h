
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
};

class TexContextInterface 
{

public:

	virtual bool loadTextures(ID3D12Device*, ID3D12GraphicsCommandList*,std::vector<TextureLoadDesc>&);

	virtual void BuildSampleDescriptorHeap(ID3D12Device*);

	virtual void BuildSampleDescriptor(ID3D12Device* md3dDevice, ID3D12GraphicsCommandList* mCommandList);
	
	void BuildSRVDescriptorHeap(ID3D12Device* md3dDevice);

	void BuildSRCDescript(ID3D12Device* md3dDevice, int CbvSrvUavDescriptorSize);

	void BuildUAVTexture(ID3D12Device*, TextureOutDes, TextureOutResouce&);

	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> getStaticSamplerDescriptor();

protected:

	std::unordered_map<std::string, std::unique_ptr<Texture>>						m_Textures;
	std::unordered_map<std::string, std::unique_ptr<Texture>>						m_TextureArrs;


	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>									m_SrvDescriptorHeap = nullptr; //for texture source
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>									m_SamplerDescriptorHeap = nullptr;
	
};
