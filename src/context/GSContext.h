#pragma once

#include "BlendContext.h"

#include <array>


class GSContext : public BlendContext
{
public:
	
	void BuildShadersAndInputLayout()override;

	void BuildShapeGeometry(ID3D12Device*, ID3D12GraphicsCommandList* mCommandList)override;

	void BuildMaterials()override;

	void BuildRenderItems() override;


protected:
	void BuildGeometryShader();

	private:
	std::vector<D3D12_INPUT_ELEMENT_DESC> m_TreeSpriteInputLayout ;

};



//geoemtry
//texture
//layout
//gshader


//update camera