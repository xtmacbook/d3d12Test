
#pragma once

#include "common/Util.h"

#include <unordered_map>
#include <map>
#include <memory>
#include <string>
#include <array>

struct Texture;

class LayoutInterface 
{

public:

	virtual void BuildShaders();

	virtual void BuildLayout();

	virtual void BuildDynamicShaders();

	virtual void BuildInstanceShaders();


protected:
	std::vector<D3D12_INPUT_ELEMENT_DESC>											m_InputLayout;
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3DBlob>>				m_Shaders;

};
