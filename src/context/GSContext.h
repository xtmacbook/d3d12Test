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

	void BuildPSOs() override;

	void DrawFrameResource(ID3D12CommandAllocator*) override;

protected:
	void BuildGeometryShader();

	private:
	std::vector<D3D12_INPUT_ELEMENT_DESC> m_TreeSpriteInputLayout ;
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3DBlob>>				m_SpriteShaders;
};
