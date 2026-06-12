
#pragma once

#include "../../common/Util.h"

#include <unordered_map>
#include <map>
#include <memory>
#include <string>
#include <array>

struct Texture;

class TexContextInterface 
{

public:
	
	virtual bool loadTextures(ID3D12Device*, ID3D12GraphicsCommandList*,std::map<std::string,std::wstring>&);

	virtual void BuildSampleDescriptorHeap(ID3D12Device*);

	virtual void BuildSampleDescriptor(ID3D12Device* md3dDevice, ID3D12GraphicsCommandList* mCommandList);
	
	void BuildSRVDescriptorHeap(ID3D12Device* md3dDevice);

	void BuildSRCDescript(ID3D12Device* md3dDevice, int CbvSrvUavDescriptorSize);

	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> getStaticSamplerDescriptor();

protected:

	std::unordered_map<std::string, std::unique_ptr<Texture>>						m_Textures;


	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>									m_SrvDescriptorHeap = nullptr; //for texture source
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>									m_SamplerDescriptorHeap = nullptr;
	
};
