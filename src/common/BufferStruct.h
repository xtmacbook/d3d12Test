#pragma once
#include "Util.h"
#include "MathHelper.h"
#include "Struct.h"

///////////////////////////////////////////////Const buffer structures///////////////////////////////////////////////
/*
	和每个object有关系,当object的world matrix变化才更新
*/
struct ObjectConstants
{
	DirectX::XMFLOAT4X4 WorldViewProj = MathHelper::Identity4x4();
};

struct ObjectConstantsWithTexTran : public ObjectConstants
{
	DirectX::XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();
};


struct MaterialConstants
{
	DirectX::XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
	DirectX::XMFLOAT3 FresnelR0 = { 0.01f, 0.01f, 0.01f };
	float			  Roughness = 0.25f;
};

struct MaterialConstantsWithTexTran : public MaterialConstants
{
	// Used in texture mapping.
	DirectX::XMFLOAT4X4 MatTransform = MathHelper::Identity4x4();
};


/*
 只是每个pass进行更新
*/
struct PassConstants
{
	DirectX::XMFLOAT4X4			m_View = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4			m_InvView = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4			m_Proj = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4			m_InvProj = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4			m_ViewProj = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4			m_InvViewProj = MathHelper::Identity4x4();

	DirectX::XMFLOAT3			m_EyePosW = { 0.0f, 0.0f, 0.0f };

	float						m_cbPerObjectPad1 = 0.0f;
	
	DirectX::XMFLOAT2			m_RenderTargetSize = { 0.0f, 0.0f };
	DirectX::XMFLOAT2			m_InvRenderTargetSize = { 0.0f, 0.0f };
	
	float						m_NearZ = 0.0f;
	float						m_FarZ = 0.0f;
	
	float						m_TotalTime = 0.0f;
	float						m_DeltaTime = 0.0f;
};

struct PassConstantsWithLight : public PassConstants
{
	DirectX::XMFLOAT4			m_AmbientLight = { 0.0f, 0.0f, 0.0f, 1.0f };
	Light						m_Lights[MaxLights];
};

struct PassConstantsWithFrog : public PassConstants
{
	DirectX::XMFLOAT4			m_AmbientLight = { 0.0f, 0.0f, 0.0f, 1.0f };

	DirectX::XMFLOAT4           FogColor = { 0.7f, 0.7f, 0.7f, 1.0f };
	float                       gFogStart = 5.0f;
	float                       gFogRange = 150.0f;
	DirectX::XMFLOAT2           cbPerObjectPad2;

	Light						m_Lights[MaxLights];

};

