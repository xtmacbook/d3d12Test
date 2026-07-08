#include "LayoutInterface.h"

void LayoutInterface::BuildShaders()
{
	const D3D_SHADER_MACRO defines[] =
	{
		"FOG", "1",
		NULL, NULL
	};

	const D3D_SHADER_MACRO alphaTestDefines[] =
	{
		"FOG", "1",
		"ALPHA_TEST", "1",
		NULL, NULL
	};

	m_Shaders["standardVS"] = D3DUtil::CompileShader(SourcePath() +  L"/Shaders/Default.hlsl", nullptr, "VS", "vs_5_1");
	m_Shaders["opaquePS"] = D3DUtil::CompileShader(SourcePath() + L"/Shaders/Default.hlsl", defines, "PS", "ps_5_1");
	m_Shaders["alphaTestedPS"] = D3DUtil::CompileShader(SourcePath() + L"/Shaders/Default.hlsl", alphaTestDefines, "PS", "ps_5_1");
}

void LayoutInterface::BuildLayout()
{
	m_InputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};
}

void LayoutInterface::BuildDynamicShaders()
{
	const D3D_SHADER_MACRO defines[] =
	{
		"FOG", "1",
		NULL, NULL
	};

	const D3D_SHADER_MACRO alphaTestDefines[] =
	{
		"FOG", "1",
		"ALPHA_TEST", "1",
		NULL, NULL
	};

	m_Shaders["standardVS"] = D3DUtil::CompileShader(SourcePath() +  L"Shaders/DynamicIndex.hlsl", nullptr, "VS", "vs_5_1");
	m_Shaders["opaquePS"] = D3DUtil::CompileShader(SourcePath() + L"Shaders/DynamicIndex.hlsl", defines, "PS", "ps_5_1");
	m_Shaders["alphaTestedPS"] = D3DUtil::CompileShader(SourcePath() + L"Shaders/DynamicIndex.hlsl", alphaTestDefines, "PS", "ps_5_1");
}
